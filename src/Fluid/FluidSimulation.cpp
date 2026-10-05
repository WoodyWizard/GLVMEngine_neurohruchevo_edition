// This file is part of Game Loop Versatile Modules (GLVM)
// License: http://opensource.org/licenses/MIT

#include "Fluid/FluidSimulation.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <random>
#include <stdexcept>

namespace GLVM::fluid
{
	namespace
	{
		constexpr uint32_t PARTICLE_GROUP = 128;
		constexpr uint32_t SCAN_BLOCK     = 1024;
		constexpr uint32_t MAX_NEIGHBORS  = 64;                              ///< Must match pbf_common.glsl
		constexpr double   PI             = 3.14159265358979323846;

		/// std140 mirror of SimParams in pbf_common.glsl.
		struct SimParams {
			float    gravityDt[4];
			float    domainMinH[4];
			float    domainMaxInvH[4];
			uint32_t gridDims[4];
			float    kernel[4];
			float    solver[4];
			float    motion[4];
			float    bodyParams[4];
			float    wall[4];
			uint32_t counts[4];
			float    drainMin[4];
			float    drainMax[4];
			float    emitterPosition[4];
			float    emitterVelocity[4];
			float    emitterColor[4];
		};

		enum Binding : uint32_t {
			PARAMETERS, POSITIONS_IN, VELOCITIES_IN, POSITIONS_OUT, VELOCITIES_OUT, COLORS_IN, COLORS_OUT, CELL_DATA,
			PARTICLE_CELL, PARTICLE_OFFSET, BLOCK_SUMS, LAMBDAS, OMEGAS, NEIGHBOR_COUNTS, BODIES, BODY_IMPULSES, COUNTERS,
			PREDICTED_IN, PREDICTED_OUT, NEIGHBORS, BINDING_COUNT
		};

		uint32_t groupsFor( uint32_t count, uint32_t groupSize ) {
			return (count + groupSize - 1) / groupSize;
		}

		double poly6( double r2, double h ) {
			const double h2 = h * h;
			if ( r2 >= h2 )
				return 0.0;
			return 315.0 / (64.0 * PI * std::pow( h, 9.0 )) * std::pow( h2 - r2, 3.0 );
		}
	}

	FluidSimulation::FluidSimulation( const GpuContext& context, const FluidSettings& settings )
		: context_(context), settings_(settings) {
		computeConstants();
		createBuffers();
		createPipelines();
	}

	FluidSimulation::~FluidSimulation() {
		vkDeviceWaitIdle( context_.device );
		if ( queryPool_ != VK_NULL_HANDLE )
			vkDestroyQueryPool( context_.device, queryPool_, nullptr );
		for ( Pipeline* pipeline : { &predict_, &scan_, &scanAdd_, &reorder_, &lambda_, &delta_, &neighborSearch_, &viscosity_, &vorticity_, &bodyStep_ } )
			destroyPipeline( context_, *pipeline );
		if ( descriptorPool_ != VK_NULL_HANDLE )
			vkDestroyDescriptorPool( context_.device, descriptorPool_, nullptr );
		if ( setLayout_ != VK_NULL_HANDLE )
			vkDestroyDescriptorSetLayout( context_.device, setLayout_, nullptr );
		for ( int k = 0; k < 2; ++k ) {
			destroyBuffer( context_, positions_[k] );
			destroyBuffer( context_, velocities_[k] );
			destroyBuffer( context_, colors_[k] );
			destroyBuffer( context_, predicted_[k] );
		}
		for ( GpuBuffer* buffer : { &parameters_, &cellData_, &particleCell_, &particleOffset_, &blockSums_, &lambdas_, &omegas_,
									&neighborCounts_, &neighbors_, &bodies_, &bodyImpulses_, &counters_ } )
			destroyBuffer( context_, *buffer );
	}

	/*
	  Constants of the rest state: a particle inside a cubic lattice of the particle spacing must have
	  exactly the rest density, so the rest density is the kernel sum of that lattice (unit masses).
	*/
	void FluidSimulation::computeConstants() {
		const double spacing = settings_.particleSpacing;
		const double h = settings_.kernelScale * spacing;
		constants_.kernelRadius   = (float)h;
		constants_.particleRadius = (float)(0.5 * spacing);
		constants_.particleMass   = (float)(settings_.waterDensity * spacing * spacing * spacing);

		const int reach = (int)std::ceil( h / spacing );
		double density = 0.0;
		for ( int x = -reach; x <= reach; ++x )
			for ( int y = -reach; y <= reach; ++y )
				for ( int z = -reach; z <= reach; ++z )
					density += poly6( (x * x + y * y + z * z) * spacing * spacing, h );
		constants_.restDensity = (float)density;

		const double spikyCoefficient = 45.0 / (PI * std::pow( h, 6.0 ));
		double gradientSum2 = 0.0;
		for ( int x = -reach; x <= reach; ++x )
			for ( int y = -reach; y <= reach; ++y )
				for ( int z = -reach; z <= reach; ++z ) {
					const double r = std::sqrt( (double)(x * x + y * y + z * z) ) * spacing;
					if ( r <= 0.0 || r >= h )
						continue;
					const double gradient = spikyCoefficient * (h - r) * (h - r) / density;
					gradientSum2 += gradient * gradient;                          ///< The self term sums to zero in a symmetric lattice
				}
		constants_.restGradientSum = (float)gradientSum2;

		for ( int axis = 0; axis < 3; ++axis ) {
			const float size = settings_.domainMax[axis] - settings_.domainMin[axis];
			if ( !(size > 2.0f * constants_.particleRadius) )
				throw std::runtime_error( "fluid domain is too small" );
			constants_.gridDims[axis] = std::max( 1u, (uint32_t)std::ceil( size / h ) );
		}
		constants_.cellCount = constants_.gridDims[0] * constants_.gridDims[1] * constants_.gridDims[2];
		if ( constants_.cellCount + 1 > SCAN_BLOCK * SCAN_BLOCK )
			throw std::runtime_error( "fluid grid has too many cells (" + std::to_string(constants_.cellCount) +
									  "), increase the particle spacing or reduce the domain" );
	}

	void FluidSimulation::createBuffers() {
		const VkDeviceSize particles = settings_.maxParticles;
		const VkBufferUsageFlags storage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
		parameters_ = createBuffer( context_, sizeof(SimParams), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, false );
		for ( int k = 0; k < 2; ++k ) {
			positions_[k]  = createBuffer( context_, particles * 16, storage, false );
			velocities_[k] = createBuffer( context_, particles * 16, storage, false );
			colors_[k]     = createBuffer( context_, particles * 16, storage, false );
			predicted_[k]  = createBuffer( context_, particles * 16, storage, false );
		}
		cellData_       = createBuffer( context_, (VkDeviceSize)(constants_.cellCount + 1) * 4, storage, false );
		particleCell_   = createBuffer( context_, particles * 4, storage, false );
		particleOffset_ = createBuffer( context_, particles * 4, storage, false );
		blockSums_      = createBuffer( context_, SCAN_BLOCK * 4, storage, false );
		lambdas_        = createBuffer( context_, particles * 4, storage, false );
		omegas_         = createBuffer( context_, particles * 16, storage, false );
		neighborCounts_ = createBuffer( context_, particles * 4, storage, false );
		neighbors_      = createBuffer( context_, particles * MAX_NEIGHBORS * 4, storage, false );
		bodies_         = createBuffer( context_, (VkDeviceSize)std::max( settings_.maxBodies, 1u ) * sizeof(GpuBody), storage, false );
		bodyImpulses_   = createBuffer( context_, (VkDeviceSize)std::max( settings_.maxBodies, 1u ) * 8 * 4, storage, false );
		counters_       = createBuffer( context_, 64, storage, false );

		context_.submitImmediate( [&]( VkCommandBuffer commandBuffer ) {
			vkCmdFillBuffer( commandBuffer, bodies_.buffer, 0, VK_WHOLE_SIZE, 0 );
			vkCmdFillBuffer( commandBuffer, bodyImpulses_.buffer, 0, VK_WHOLE_SIZE, 0 );
			vkCmdFillBuffer( commandBuffer, counters_.buffer, 0, VK_WHOLE_SIZE, 0 );
		});

		VkQueryPoolCreateInfo queryInfo{};
		queryInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
		queryInfo.queryType  = VK_QUERY_TYPE_TIMESTAMP;
		queryInfo.queryCount = QUERY_SLOTS * QUERIES_PER_SLOT;
		vkCheck( vkCreateQueryPool( context_.device, &queryInfo, nullptr, &queryPool_ ), "vkCreateQueryPool" );
		context_.submitImmediate( [&]( VkCommandBuffer commandBuffer ) {
			vkCmdResetQueryPool( commandBuffer, queryPool_, 0, QUERY_SLOTS * QUERIES_PER_SLOT );
		});

		std::vector<VkDescriptorType> types( BINDING_COUNT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER );
		types[PARAMETERS] = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		setLayout_ = createSetLayout( context_, types, VK_SHADER_STAGE_COMPUTE_BIT );
		descriptorPool_ = createDescriptorPool( context_, 4 );
		for ( uint32_t state = 0; state < 2; ++state ) {
			for ( uint32_t predicted = 0; predicted < 2; ++predicted ) {
				VkDescriptorSet set = allocateSet( context_, descriptorPool_, setLayout_ );
				const VkDescriptorType buffer = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
				updateSet( context_, set, {
					bufferWrite( PARAMETERS, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, parameters_ ),
					bufferWrite( POSITIONS_IN, buffer, positions_[state] ),
					bufferWrite( VELOCITIES_IN, buffer, velocities_[state] ),
					bufferWrite( POSITIONS_OUT, buffer, positions_[1 - state] ),
					bufferWrite( VELOCITIES_OUT, buffer, velocities_[1 - state] ),
					bufferWrite( COLORS_IN, buffer, colors_[state] ),
					bufferWrite( COLORS_OUT, buffer, colors_[1 - state] ),
					bufferWrite( CELL_DATA, buffer, cellData_ ),
					bufferWrite( PARTICLE_CELL, buffer, particleCell_ ),
					bufferWrite( PARTICLE_OFFSET, buffer, particleOffset_ ),
					bufferWrite( BLOCK_SUMS, buffer, blockSums_ ),
					bufferWrite( LAMBDAS, buffer, lambdas_ ),
					bufferWrite( OMEGAS, buffer, omegas_ ),
					bufferWrite( NEIGHBOR_COUNTS, buffer, neighborCounts_ ),
					bufferWrite( BODIES, buffer, bodies_ ),
					bufferWrite( BODY_IMPULSES, buffer, bodyImpulses_ ),
					bufferWrite( COUNTERS, buffer, counters_ ),
					bufferWrite( PREDICTED_IN, buffer, predicted_[predicted] ),
					bufferWrite( PREDICTED_OUT, buffer, predicted_[1 - predicted] ),
					bufferWrite( NEIGHBORS, buffer, neighbors_ ),
				});
				sets_[state][predicted] = set;
			}
		}
	}

	void FluidSimulation::createPipelines() {
		const std::vector<VkDescriptorSetLayout> layouts = { setLayout_ };
		predict_   = createComputePipeline( context_, "pbf_predict.comp.spv", layouts, 0 );
		scan_      = createComputePipeline( context_, "scan.comp.spv", layouts, 4 );
		scanAdd_   = createComputePipeline( context_, "scan_add.comp.spv", layouts, 0 );
		reorder_   = createComputePipeline( context_, "pbf_reorder.comp.spv", layouts, 0 );
		lambda_    = createComputePipeline( context_, "pbf_lambda.comp.spv", layouts, 0 );
		delta_     = createComputePipeline( context_, "pbf_delta.comp.spv", layouts, 0 );
		neighborSearch_ = createComputePipeline( context_, "pbf_neighbors.comp.spv", layouts, 0 );
		viscosity_ = createComputePipeline( context_, "pbf_viscosity.comp.spv", layouts, 0 );
		vorticity_ = createComputePipeline( context_, "pbf_vorticity.comp.spv", layouts, 0 );
		bodyStep_  = createComputePipeline( context_, "pbf_bodies.comp.spv", layouts, 0 );
	}

	/*
	  ===================================================
	  Particles and bodies
	  ===================================================
	*/
	void FluidSimulation::reset() {
		vkDeviceWaitIdle( context_.device );
		particleCount_  = 0;
		nextParticleId_ = 0;
		bodyCount_      = 0;
		stateParity_    = 0;
		pump_           = FluidPump();
		context_.submitImmediate( [&]( VkCommandBuffer commandBuffer ) {
			vkCmdFillBuffer( commandBuffer, bodies_.buffer, 0, VK_WHOLE_SIZE, 0 );
			vkCmdFillBuffer( commandBuffer, bodyImpulses_.buffer, 0, VK_WHOLE_SIZE, 0 );
		});
	}

	uint32_t FluidSimulation::addParticles( const std::vector<FluidParticle>& particles ) {
		const uint32_t count = std::min<uint32_t>( (uint32_t)particles.size(), settings_.maxParticles - particleCount_ );
		if ( count == 0 )
			return 0;
		std::vector<float> positions( count * 4 ), velocities( count * 4 ), colors( count * 4 );
		for ( uint32_t i = 0; i < count; ++i ) {
			std::memcpy( &positions[i * 4], particles[i].position, 16 );
			std::memcpy( &velocities[i * 4], particles[i].velocity, 16 );
			std::memcpy( &colors[i * 4], particles[i].color, 16 );
			velocities[i * 4 + 3] = (float)nextParticleId_++;
		}
		const VkDeviceSize offset = (VkDeviceSize)particleCount_ * 16;
		vkDeviceWaitIdle( context_.device );
		uploadToBuffer( context_, positions_[stateParity_], positions.data(), count * 16, offset );
		uploadToBuffer( context_, velocities_[stateParity_], velocities.data(), count * 16, offset );
		uploadToBuffer( context_, colors_[stateParity_], colors.data(), count * 16, offset );
		particleCount_ += count;
		return count;
	}

	uint32_t FluidSimulation::addBlock( const std::array<float, 3>& minimum, const std::array<float, 3>& maximum,
										const std::array<float, 3>& velocity, const std::array<float, 3>& color ) {
		const float spacing = settings_.particleSpacing;
		std::mt19937 random( 12345u + particleCount_ );
		std::uniform_real_distribution<float> jitter( -0.01f * spacing, 0.01f * spacing );    ///< A perfect lattice is an artificial state
		std::vector<FluidParticle> particles;
		for ( float z = minimum[2] + 0.5f * spacing; z < maximum[2]; z += spacing )
			for ( float y = minimum[1] + 0.5f * spacing; y < maximum[1]; y += spacing )
				for ( float x = minimum[0] + 0.5f * spacing; x < maximum[0]; x += spacing ) {
					FluidParticle particle = {};
					particle.position[0] = x + jitter( random );
					particle.position[1] = y + jitter( random );
					particle.position[2] = z + jitter( random );
					std::copy( velocity.begin(), velocity.end(), particle.velocity );
					std::copy( color.begin(), color.end(), particle.color );
					particles.push_back( particle );
				}
		return addParticles( particles );
	}

	uint32_t FluidSimulation::addSphere( const std::array<float, 3>& center, float radius, const std::array<float, 3>& velocity,
										 const std::array<float, 3>& color ) {
		const float spacing = settings_.particleSpacing;
		std::vector<FluidParticle> particles;
		for ( float z = -radius; z <= radius; z += spacing )
			for ( float y = -radius; y <= radius; y += spacing )
				for ( float x = -radius; x <= radius; x += spacing ) {
					if ( x * x + y * y + z * z > radius * radius )
						continue;
					FluidParticle particle = {};
					particle.position[0] = center[0] + x;
					particle.position[1] = center[1] + y;
					particle.position[2] = center[2] + z;
					std::copy( velocity.begin(), velocity.end(), particle.velocity );
					std::copy( color.begin(), color.end(), particle.color );
					particles.push_back( particle );
				}
		return addParticles( particles );
	}

	GpuBody FluidSimulation::toGpuBody( const FluidBody& body ) const {
		GpuBody gpu = {};
		const bool isSphere = body.type == BodyType::SPHERE;
		const float hx = body.halfExtents[0], hy = isSphere ? hx : body.halfExtents[1], hz = isSphere ? hx : body.halfExtents[2];
		gpu.positionType[0] = body.position[0];
		gpu.positionType[1] = body.position[1];
		gpu.positionType[2] = body.position[2];
		gpu.positionType[3] = (float)body.type;
		std::copy( body.rotation.begin(), body.rotation.end(), gpu.rotation );
		gpu.halfExtents[0] = hx;
		gpu.halfExtents[1] = hy;
		gpu.halfExtents[2] = hz;
		gpu.halfExtents[3] = isSphere ? hx : std::sqrt( hx * hx + hy * hy + hz * hz );
		std::copy( body.velocity.begin(), body.velocity.end(), gpu.velocityInverseMass );
		std::copy( body.angularVelocity.begin(), body.angularVelocity.end(), gpu.angularVelocity );

		if ( body.density > 0.0f ) {
			const float volume = isSphere ? 4.0f / 3.0f * 3.14159265f * hx * hx * hx : 8.0f * hx * hy * hz;
			const float mass = body.density * volume;
			float inertia[3];
			if ( isSphere ) {
				inertia[0] = inertia[1] = inertia[2] = 0.4f * mass * hx * hx;
			} else {
				inertia[0] = mass / 3.0f * (hy * hy + hz * hz);                     ///< m/12 (b^2 + c^2) with full sizes b = 2 hy, c = 2 hz
				inertia[1] = mass / 3.0f * (hx * hx + hz * hz);
				inertia[2] = mass / 3.0f * (hx * hx + hy * hy);
			}
			gpu.velocityInverseMass[3] = 1.0f / mass;
			for ( int k = 0; k < 3; ++k )
				gpu.inverseInertia[k] = 1.0f / inertia[k];
		}
		gpu.inverseInertia[3] = body.damping;
		std::copy( body.color.begin(), body.color.end(), gpu.color );
		return gpu;
	}

	void FluidSimulation::setBody( uint32_t index, const FluidBody& body ) {
		if ( index >= settings_.maxBodies )
			throw std::runtime_error( "fluid body index " + std::to_string(index) + " is out of range" );
		const GpuBody gpu = toGpuBody( body );
		vkDeviceWaitIdle( context_.device );
		uploadToBuffer( context_, bodies_, &gpu, sizeof(gpu), (VkDeviceSize)index * sizeof(GpuBody) );
		bodyCount_ = std::max( bodyCount_, index + 1 );
	}

	void FluidSimulation::recordBodyUpdate( VkCommandBuffer commandBuffer, uint32_t index, const FluidBody& body ) {
		if ( index >= settings_.maxBodies )
			return;
		const GpuBody gpu = toGpuBody( body );
		memoryBarrier( commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
					   VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT );
		vkCmdUpdateBuffer( commandBuffer, bodies_.buffer, (VkDeviceSize)index * sizeof(GpuBody), sizeof(gpu), &gpu );
		memoryBarrier( commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
					   VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT );
		bodyCount_ = std::max( bodyCount_, index + 1 );
	}

	/*
	  ===================================================
	  Simulation step
	  ===================================================
	*/
	void FluidSimulation::writeParameters( VkCommandBuffer commandBuffer, float substepTime ) {
		const float h = constants_.kernelRadius;
		const double kernelPoly6 = 315.0 / (64.0 * PI * std::pow( (double)h, 9.0 ));
		const double kernelSpiky = 45.0 / (PI * std::pow( (double)h, 6.0 ));
		const float epsilon = settings_.relaxation * constants_.restGradientSum;
		const double wDq = poly6( std::pow( settings_.artificialPressureDistance * h, 2.0 ), h );
		const double spacing = settings_.particleSpacing;

		SimParams p = {};
		p.gravityDt[0] = settings_.gravity[0];
		p.gravityDt[1] = settings_.gravity[1];
		p.gravityDt[2] = settings_.gravity[2];
		p.gravityDt[3] = substepTime;
		for ( int k = 0; k < 3; ++k ) {
			p.domainMinH[k]    = settings_.domainMin[k];
			p.domainMaxInvH[k] = settings_.domainMax[k];
			p.gridDims[k]      = constants_.gridDims[k];
		}
		p.domainMinH[3]    = h;
		p.domainMaxInvH[3] = 1.0f / h;
		p.gridDims[3]      = particleCount_;
		p.kernel[0] = (float)kernelPoly6;
		p.kernel[1] = (float)kernelSpiky;
		p.kernel[2] = constants_.restDensity;
		p.kernel[3] = 1.0f / constants_.restDensity;
		p.solver[0] = epsilon;
		p.solver[1] = settings_.artificialPressure / (constants_.restGradientSum + epsilon);
		p.solver[2] = (float)(1.0 / wDq);
		p.solver[3] = constants_.particleRadius;
		p.motion[0] = settings_.viscosity;
		p.motion[1] = settings_.vorticity;
		p.motion[2] = settings_.maxSpeed;
		p.motion[3] = settings_.wallFriction;
		/// Impulses are summed as integers: one unit is 1e-4 of a particle momentum at 1 m/s
		p.bodyParams[0] = (float)bodyCount_;
		p.bodyParams[1] = 1.0f / (constants_.particleMass * 1e-4f);
		p.bodyParams[2] = constants_.particleMass;
		p.bodyParams[3] = settings_.bodyRestitution;
		/// Wall density coefficient: number density / rest density * pi * K_poly6 / 4 (see wallDensity in pbf_common.glsl)
		p.wall[0] = (float)(1.0 / (spacing * spacing * spacing) / constants_.restDensity * PI * kernelPoly6 / 4.0);
		p.wall[1] = settings_.dyeDiffusion;
		p.counts[0] = constants_.cellCount + 1;
		p.counts[1] = groupsFor( constants_.cellCount + 1, SCAN_BLOCK );

		const int perRow = std::max( (int)(2.0f * pump_.nozzleRadius / settings_.particleSpacing), 1 );
		p.counts[2] = pump_.isEnabled ? (uint32_t)(perRow * perRow) : 0u;
		p.counts[3] = settings_.maxParticles;
		for ( int k = 0; k < 3; ++k ) {
			p.drainMin[k]        = pump_.drainMin[k];
			p.drainMax[k]        = pump_.drainMax[k];
			p.emitterPosition[k] = pump_.nozzle[k];
			p.emitterVelocity[k] = pump_.velocity[k];
			p.emitterColor[k]    = pump_.color[k];
		}
		p.drainMin[3]        = pump_.isEnabled ? 1.0f : 0.0f;
		p.emitterPosition[3] = pump_.nozzleRadius;
		p.emitterVelocity[3] = settings_.particleSpacing;

		memoryBarrier( commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_UNIFORM_READ_BIT,
					   VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT );
		vkCmdUpdateBuffer( commandBuffer, parameters_.buffer, 0, sizeof(p), &p );
		memoryBarrier( commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
					   VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_UNIFORM_READ_BIT );
	}

	void FluidSimulation::dispatch( VkCommandBuffer commandBuffer, const Pipeline& pipeline, uint32_t stateParity, uint32_t predictedParity,
									uint32_t groups, const void* push, uint32_t pushSize ) {
		vkCmdBindPipeline( commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline.pipeline );
		vkCmdBindDescriptorSets( commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline.layout, 0, 1,
								 &sets_[stateParity][predictedParity], 0, nullptr );
		if ( push != nullptr )
			vkCmdPushConstants( commandBuffer, pipeline.layout, VK_SHADER_STAGE_ALL, 0, pushSize, push );
		vkCmdDispatch( commandBuffer, groups, 1, 1 );
		computeBarrier( commandBuffer );
	}

	/*
	  ===================================================
	  Profiling: a timestamp after every stage, stage durations are summed over the substeps
	  ===================================================
	*/
	enum Stage : uint8_t { STAGE_SORT, STAGE_NEIGHBORS, STAGE_SOLVER, STAGE_VELOCITY, STAGE_BODIES, STAGE_START = 255 };

	void FluidSimulation::writeTimestamp( VkCommandBuffer commandBuffer, uint32_t stage ) {
		if ( !isProfiling_ || queryCount_[querySlot_] >= QUERIES_PER_SLOT )
			return;
		const uint32_t index = queryCount_[querySlot_]++;
		queryStage_[querySlot_][index] = (uint8_t)stage;
		vkCmdWriteTimestamp( commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, queryPool_, querySlot_ * QUERIES_PER_SLOT + index );
	}

	FluidTimings FluidSimulation::timings() {
		const double nanoseconds = context_.properties.limits.timestampPeriod;
		for ( uint32_t age = 1; age < QUERY_SLOTS; ++age ) {                       ///< Newest finished frame first
			const uint32_t slot = (querySlot_ + QUERY_SLOTS - age) % QUERY_SLOTS;
			const uint32_t count = queryCount_[slot];
			if ( count < 2 )
				continue;
			uint64_t stamps[QUERIES_PER_SLOT] = {};
			if ( vkGetQueryPoolResults( context_.device, queryPool_, slot * QUERIES_PER_SLOT, count, sizeof(stamps), stamps,
										sizeof(uint64_t), VK_QUERY_RESULT_64_BIT ) != VK_SUCCESS )
				continue;
			FluidTimings result;
			float* stages[] = { &result.sort, &result.neighborSearch, &result.solver, &result.velocity, &result.bodies };
			for ( uint32_t q = 1; q < count; ++q ) {
				const float milliseconds = (float)((double)(stamps[q] - stamps[q - 1]) * nanoseconds * 1e-6);
				if ( queryStage_[slot][q] < 5 )
					*stages[queryStage_[slot][q]] += milliseconds;
			}
			result.total = (float)((double)(stamps[count - 1] - stamps[0]) * nanoseconds * 1e-6);
			timings_ = result;
			break;
		}
		return timings_;
	}

	void FluidSimulation::recordStep( VkCommandBuffer commandBuffer, float frameTime ) {
		const uint32_t substeps = std::max( settings_.substeps, 1u );
		const float substepTime = std::clamp( frameTime, 1e-4f, 1.0f / 30.0f ) / (float)substeps;    ///< Long frames slow the simulation down
		if ( isProfiling_ ) {
			querySlot_ = (querySlot_ + 1) % QUERY_SLOTS;
			queryCount_[querySlot_] = 0;
			vkCmdResetQueryPool( commandBuffer, queryPool_, querySlot_ * QUERIES_PER_SLOT, QUERIES_PER_SLOT );
		}
		writeParameters( commandBuffer, substepTime );
		writeTimestamp( commandBuffer, STAGE_START );

		const uint32_t particleGroups = groupsFor( particleCount_, PARTICLE_GROUP );
		const uint32_t scanBlocks = groupsFor( constants_.cellCount + 1, SCAN_BLOCK );
		for ( uint32_t substep = 0; substep < substeps; ++substep ) {
			if ( particleCount_ > 0 ) {
				memoryBarrier( commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
							   VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT );
				vkCmdFillBuffer( commandBuffer, cellData_.buffer, 0, VK_WHOLE_SIZE, 0 );
				vkCmdFillBuffer( commandBuffer, counters_.buffer, 0, VK_WHOLE_SIZE, 0 );
				memoryBarrier( commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
							   VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT );

				/// Predict into predicted[1], sort everything by cell into the other state buffers and predicted[0].
				dispatch( commandBuffer, predict_, stateParity_, 0, particleGroups );
				const uint32_t localScan = 0, blockScan = 1;
				dispatch( commandBuffer, scan_, stateParity_, 0, scanBlocks, &localScan, 4 );
				dispatch( commandBuffer, scan_, stateParity_, 0, 1, &blockScan, 4 );
				dispatch( commandBuffer, scanAdd_, stateParity_, 0, groupsFor( constants_.cellCount + 1, 256 ) );
				dispatch( commandBuffer, reorder_, stateParity_, 1, particleGroups );
				stateParity_ = 1 - stateParity_;
				writeTimestamp( commandBuffer, STAGE_SORT );

				/// The first iteration builds the neighbor lists and computes lambda in one pass.
				uint32_t predictedParity = 0;
				for ( uint32_t iteration = 0; iteration < std::max( settings_.iterations, 1u ); ++iteration ) {
					if ( iteration == 0 ) {
						dispatch( commandBuffer, neighborSearch_, stateParity_, predictedParity, particleGroups );
						writeTimestamp( commandBuffer, STAGE_NEIGHBORS );
					} else {
						dispatch( commandBuffer, lambda_, stateParity_, predictedParity, particleGroups );
					}
					dispatch( commandBuffer, delta_, stateParity_, predictedParity, particleGroups );
					predictedParity = 1 - predictedParity;
				}
				writeTimestamp( commandBuffer, STAGE_SOLVER );
				dispatch( commandBuffer, viscosity_, stateParity_, predictedParity, particleGroups );
				dispatch( commandBuffer, vorticity_, stateParity_, predictedParity, particleGroups );
				writeTimestamp( commandBuffer, STAGE_VELOCITY );
			}
			if ( bodyCount_ > 0 ) {
				dispatch( commandBuffer, bodyStep_, stateParity_, 0, 1 );
				writeTimestamp( commandBuffer, STAGE_BODIES );
			}
		}

		memoryBarrier( commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT,
					   VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT |
					   VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_TRANSFER_READ_BIT );
	}

	std::vector<FluidParticle> FluidSimulation::downloadParticles() const {
		std::vector<FluidParticle> particles( particleCount_ );
		if ( particleCount_ == 0 )
			return particles;
		std::vector<float> positions( particleCount_ * 4 ), velocities( particleCount_ * 4 ), colors( particleCount_ * 4 );
		vkDeviceWaitIdle( context_.device );
		downloadFromBuffer( context_, positions_[stateParity_], positions.data(), particleCount_ * 16 );
		downloadFromBuffer( context_, velocities_[stateParity_], velocities.data(), particleCount_ * 16 );
		downloadFromBuffer( context_, colors_[stateParity_], colors.data(), particleCount_ * 16 );
		for ( uint32_t i = 0; i < particleCount_; ++i ) {
			std::memcpy( particles[i].position, &positions[i * 4], 16 );
			std::memcpy( particles[i].velocity, &velocities[i * 4], 16 );
			std::memcpy( particles[i].color, &colors[i * 4], 16 );
		}
		return particles;
	}

	std::vector<GpuBody> FluidSimulation::downloadBodies() const {
		std::vector<GpuBody> result( bodyCount_ );
		if ( bodyCount_ == 0 )
			return result;
		vkDeviceWaitIdle( context_.device );
		downloadFromBuffer( context_, bodies_, result.data(), bodyCount_ * sizeof(GpuBody) );
		return result;
	}
}
