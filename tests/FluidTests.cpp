// This file is part of Game Loop Versatile Modules (GLVM)
// License: http://opensource.org/licenses/MIT

/*
  Tests of the GPU fluid simulation (src/Fluid), run without a window:
  - one substep on the GPU against a CPU reference implementation of the same equations,
  - physical checks: a resting pool stays at rest and keeps its volume, a dam break stays bounded and
    settles, a light box floats with the expected submerged fraction, the pump keeps the particle count,
  - timings of a frame for several particle counts.

    fluidTests [--shaders <dir>] [--benchmark]
*/

#include "Fluid/FluidSimulation.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <functional>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

using namespace GLVM::fluid;

namespace
{
	int checksFailed = 0;
	int checksPassed = 0;

	void check( bool condition, const std::string& what ) {
		if ( condition ) {
			++checksPassed;
		} else {
			++checksFailed;
			std::printf( "  FAIL %s\n", what.c_str() );
		}
	}

	/*
	  ===================================================
	  Headless Vulkan device
	  ===================================================
	*/
	struct HeadlessGpu {
		VkInstance instance = VK_NULL_HANDLE;
		GpuContext context;

		explicit HeadlessGpu( const std::string& shaderDirectory ) {
			VkApplicationInfo application{};
			application.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
			application.pApplicationName = "GLVM fluid tests";
			application.apiVersion = VK_API_VERSION_1_3;
			VkInstanceCreateInfo instanceInfo{};
			instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
			instanceInfo.pApplicationInfo = &application;
			vkCheck( vkCreateInstance( &instanceInfo, nullptr, &instance ), "vkCreateInstance" );

			uint32_t count = 0;
			vkEnumeratePhysicalDevices( instance, &count, nullptr );
			std::vector<VkPhysicalDevice> devices( count );
			vkEnumeratePhysicalDevices( instance, &count, devices.data() );
			if ( devices.empty() )
				throw std::runtime_error( "no Vulkan device" );
			context.physicalDevice = devices[0];
			for ( VkPhysicalDevice device : devices ) {
				VkPhysicalDeviceProperties properties;
				vkGetPhysicalDeviceProperties( device, &properties );
				if ( properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU )
					context.physicalDevice = device;
			}
			vkGetPhysicalDeviceProperties( context.physicalDevice, &context.properties );
			vkGetPhysicalDeviceMemoryProperties( context.physicalDevice, &context.memoryProperties );

			uint32_t familyCount = 0;
			vkGetPhysicalDeviceQueueFamilyProperties( context.physicalDevice, &familyCount, nullptr );
			std::vector<VkQueueFamilyProperties> families( familyCount );
			vkGetPhysicalDeviceQueueFamilyProperties( context.physicalDevice, &familyCount, families.data() );
			for ( uint32_t i = 0; i < familyCount; ++i ) {
				if ( (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && (families[i].queueFlags & VK_QUEUE_COMPUTE_BIT) ) {
					context.queueFamily = i;
					break;
				}
			}

			const float priority = 1.0f;
			VkDeviceQueueCreateInfo queueInfo{};
			queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			queueInfo.queueFamilyIndex = context.queueFamily;
			queueInfo.queueCount       = 1;
			queueInfo.pQueuePriorities = &priority;
			VkDeviceCreateInfo deviceInfo{};
			deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
			deviceInfo.queueCreateInfoCount = 1;
			deviceInfo.pQueueCreateInfos    = &queueInfo;
			vkCheck( vkCreateDevice( context.physicalDevice, &deviceInfo, nullptr, &context.device ), "vkCreateDevice" );
			vkGetDeviceQueue( context.device, context.queueFamily, 0, &context.queue );

			VkCommandPoolCreateInfo poolInfo{};
			poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
			poolInfo.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
			poolInfo.queueFamilyIndex = context.queueFamily;
			vkCheck( vkCreateCommandPool( context.device, &poolInfo, nullptr, &context.commandPool ), "vkCreateCommandPool" );
			context.shaderDirectory = shaderDirectory;
		}

		~HeadlessGpu() {
			vkDestroyCommandPool( context.device, context.commandPool, nullptr );
			vkDestroyDevice( context.device, nullptr );
			vkDestroyInstance( instance, nullptr );
		}

		/// Runs frames of the simulation, returns the GPU time of one frame in milliseconds (average).
		double run( FluidSimulation& simulation, uint32_t frames, float frameTime ) {
			VkQueryPool queries = VK_NULL_HANDLE;
			VkQueryPoolCreateInfo queryInfo{};
			queryInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
			queryInfo.queryType  = VK_QUERY_TYPE_TIMESTAMP;
			queryInfo.queryCount = 2;
			vkCheck( vkCreateQueryPool( context.device, &queryInfo, nullptr, &queries ), "vkCreateQueryPool" );
			double totalMilliseconds = 0.0;
			for ( uint32_t frame = 0; frame < frames; ++frame ) {
				context.submitImmediate( [&]( VkCommandBuffer commandBuffer ) {
					vkCmdResetQueryPool( commandBuffer, queries, 0, 2 );
					vkCmdWriteTimestamp( commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, queries, 0 );
					simulation.recordStep( commandBuffer, frameTime );
					vkCmdWriteTimestamp( commandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, queries, 1 );
				});
				uint64_t stamps[2] = {};
				vkGetQueryPoolResults( context.device, queries, 0, 2, sizeof(stamps), stamps, sizeof(uint64_t), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT );
				totalMilliseconds += (double)(stamps[1] - stamps[0]) * context.properties.limits.timestampPeriod * 1e-6;
			}
			vkDestroyQueryPool( context.device, queries, nullptr );
			return frames > 0 ? totalMilliseconds / frames : 0.0;
		}
	};

	/*
	  ===================================================
	  CPU reference of one substep (same equations as the compute shaders, brute force neighbors)
	  ===================================================
	*/
	struct Vec3 {
		double x = 0, y = 0, z = 0;
		Vec3 operator+( const Vec3& o ) const { return { x + o.x, y + o.y, z + o.z }; }
		Vec3 operator-( const Vec3& o ) const { return { x - o.x, y - o.y, z - o.z }; }
		Vec3 operator*( double s ) const { return { x * s, y * s, z * s }; }
		double& operator[]( int i ) { return i == 0 ? x : (i == 1 ? y : z); }
		double operator[]( int i ) const { return i == 0 ? x : (i == 1 ? y : z); }
	};
	double dot( const Vec3& a, const Vec3& b ) { return a.x * b.x + a.y * b.y + a.z * b.z; }
	Vec3 cross( const Vec3& a, const Vec3& b ) { return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x }; }
	double length( const Vec3& a ) { return std::sqrt( dot( a, a ) ); }

	struct ReferenceParticle {
		Vec3 position, velocity, predicted, color;
		double foam = 0.0;
	};

	struct Reference {
		const FluidSettings& settings;
		const FluidConstants& constants;
		double h, h2, poly6K, spikyK, restDensity, epsilon, sCorr, inverseWdq, radius, wallCoefficient;
		Vec3 low, high;

		Reference( const FluidSettings& s, const FluidConstants& c ) : settings(s), constants(c) {
			const double pi = 3.14159265358979323846;
			h = c.kernelRadius;
			h2 = h * h;
			poly6K = 315.0 / (64.0 * pi * std::pow( h, 9.0 ));
			spikyK = 45.0 / (pi * std::pow( h, 6.0 ));
			restDensity = c.restDensity;
			epsilon = s.relaxation * c.restGradientSum;
			sCorr = s.artificialPressure / (c.restGradientSum + epsilon);
			const double dq2 = std::pow( s.artificialPressureDistance * h, 2.0 );
			inverseWdq = 1.0 / (poly6K * std::pow( h2 - dq2, 3.0 ));
			radius = c.particleRadius;
			const double spacing = s.particleSpacing;
			wallCoefficient = 1.0 / (spacing * spacing * spacing) / restDensity * pi * poly6K / 4.0;
			low  = { s.domainMin[0], s.domainMin[1], s.domainMin[2] };
			high = { s.domainMax[0], s.domainMax[1], s.domainMax[2] };
		}

		double poly6( double r2 ) const { return r2 >= h2 ? 0.0 : poly6K * std::pow( h2 - r2, 3.0 ); }
		Vec3 spiky( const Vec3& r, double len ) const {
			if ( len >= h || len < 1e-9 )
				return {};
			return r * (-spikyK * (h - len) * (h - len) / len);
		}
		double wall( double d, double& derivative ) const {
			if ( d >= h ) {
				derivative = 0.0;
				return 0.0;
			}
			d = std::max( d, 0.0 );
			const double F = [&]( double z ) { return h2 * h2 * h2 * h2 * z - 4.0 / 3.0 * std::pow( h, 6.0 ) * z * z * z + 1.2 * h2 * h2 * std::pow( z, 5.0 )
														- 4.0 / 7.0 * h2 * std::pow( z, 7.0 ) + std::pow( z, 9.0 ) / 9.0; }( d );
			derivative = -wallCoefficient * std::pow( h2 - d * d, 4.0 );
			return wallCoefficient * (128.0 / 315.0 * std::pow( h, 9.0 ) - F);
		}
		Vec3 clampToDomain( Vec3 p ) const {
			for ( int k = 0; k < 3; ++k )
				p[k] = std::clamp( p[k], low[k] + radius, high[k] - radius );
			return p;
		}

		void substep( std::vector<ReferenceParticle>& particles, double dt ) const {
			const size_t n = particles.size();
			for ( ReferenceParticle& p : particles ) {
				p.velocity = p.velocity + Vec3{ settings.gravity[0], settings.gravity[1], settings.gravity[2] } * dt;
				const double speed = length( p.velocity );
				if ( speed > settings.maxSpeed )
					p.velocity = p.velocity * (settings.maxSpeed / speed);
				p.predicted = clampToDomain( p.position + p.velocity * dt );
			}

			/// Neighbor lists of the predicted positions, used by every later pass of the substep (as on the GPU)
			std::vector<std::vector<size_t>> lists( n );
			for ( size_t i = 0; i < n; ++i ) {
				for ( size_t j = 0; j < n; ++j ) {
					const Vec3 r = particles[i].predicted - particles[j].predicted;
					if ( j != i && dot( r, r ) < h2 )
						lists[i].push_back( j );
				}
			}

			std::vector<double> lambdas( n );
			for ( uint32_t iteration = 0; iteration < settings.iterations; ++iteration ) {
				for ( size_t i = 0; i < n; ++i ) {
					double density = poly6( 0.0 ), sumGradient2 = 0.0;
					Vec3 gradientI;
					for ( size_t j : lists[i] ) {
						const Vec3 r = particles[i].predicted - particles[j].predicted;
						const double r2 = dot( r, r );
						if ( r2 >= h2 )
							continue;
						density += poly6( r2 );
						const Vec3 g = spiky( r, std::sqrt( r2 ) ) * (1.0 / restDensity);
						gradientI = gradientI + g;
						sumGradient2 += dot( g, g );
					}
					double wallRatio = 0.0;
					for ( int axis = 0; axis < 3; ++axis ) {
						double dLow, dHigh;
						wallRatio += wall( particles[i].predicted[axis] - low[axis], dLow ) + wall( high[axis] - particles[i].predicted[axis], dHigh );
						gradientI[axis] += dLow - dHigh;
					}
					const double constraint = std::max( density / restDensity + wallRatio - 1.0, 0.0 );
					sumGradient2 += dot( gradientI, gradientI );
					lambdas[i] = -constraint / (sumGradient2 + epsilon);
				}
				std::vector<Vec3> corrected( n );
				for ( size_t i = 0; i < n; ++i ) {
					Vec3 delta;
					for ( size_t j : lists[i] ) {
						const Vec3 r = particles[i].predicted - particles[j].predicted;
						const double r2 = dot( r, r );
						if ( r2 >= h2 )
							continue;
						const double ratio = poly6( r2 ) * inverseWdq;
						const double ratio4 = std::pow( ratio, 4.0 );
						const double correction = -sCorr * (lambdas[i] + lambdas[j] < 0.0 ? ratio4 : std::max( ratio4 - 1.0, 0.0 ));
						delta = delta + spiky( r, std::sqrt( r2 ) ) * (lambdas[i] + lambdas[j] + correction);
					}
					delta = delta * (1.0 / restDensity);
					for ( int axis = 0; axis < 3; ++axis ) {
						double dLow, dHigh;
						wall( particles[i].predicted[axis] - low[axis], dLow );
						wall( high[axis] - particles[i].predicted[axis], dHigh );
						delta[axis] += lambdas[i] * (dLow - dHigh);
					}
					const double deltaLength = length( delta );
					if ( deltaLength > 0.25 * h )
						delta = delta * (0.25 * h / deltaLength);
					corrected[i] = clampToDomain( particles[i].predicted + delta );
				}
				for ( size_t i = 0; i < n; ++i )
					particles[i].predicted = corrected[i];
			}

			std::vector<Vec3> temp( n );
			for ( size_t i = 0; i < n; ++i ) {
				Vec3 v = (particles[i].predicted - particles[i].position) * (1.0 / dt);
				for ( int axis = 0; axis < 3; ++axis ) {
					if ( particles[i].predicted[axis] - low[axis] < radius * 1.05 || high[axis] - particles[i].predicted[axis] < radius * 1.05 ) {
						for ( int other = 0; other < 3; ++other )
							if ( other != axis )
								v[other] *= 1.0 - settings.wallFriction;
					}
				}
				temp[i] = v;
			}
			std::vector<Vec3> omegas( n ), velocities( n );
			std::vector<double> omegaLength( n );
			for ( size_t i = 0; i < n; ++i ) {
				Vec3 viscosity, omega;
				double weightSum = poly6( 0.0 );
				for ( size_t j : lists[i] ) {
					const Vec3 r = particles[i].predicted - particles[j].predicted;
					const double r2 = dot( r, r );
					if ( r2 >= h2 )
						continue;
					const double w = poly6( r2 );
					const Vec3 relative = temp[j] - temp[i];
					viscosity = viscosity + relative * w;
					weightSum += w;
					omega = omega + cross( relative, spiky( r, std::sqrt( r2 ) ) * -1.0 );
				}
				velocities[i] = temp[i] + viscosity * (settings.viscosity / weightSum);
				omega = omega * (1.0 / restDensity);
				omegas[i] = omega;
				omegaLength[i] = length( omega );
			}
			for ( size_t i = 0; i < n; ++i ) {
				Vec3 eta;
				for ( size_t j : lists[i] ) {
					const Vec3 r = particles[i].predicted - particles[j].predicted;
					const double r2 = dot( r, r );
					if ( r2 >= h2 )
						continue;
					eta = eta + spiky( r, std::sqrt( r2 ) ) * (omegaLength[j] - omegaLength[i]);
				}
				Vec3 v = velocities[i];
				const double etaLength = length( eta );
				if ( etaLength > 1e-9 )
					v = v + cross( eta * (1.0 / etaLength), omegas[i] ) * (settings.vorticity * h);
				const double speed = length( v );
				if ( speed > settings.maxSpeed )
					v = v * (settings.maxSpeed / speed);
				particles[i].velocity = v;
				particles[i].position = particles[i].predicted;
			}
		}
	};

	FluidSettings smallSettings() {
		FluidSettings settings;
		settings.maxParticles    = 4096;
		settings.maxBodies       = 4;
		settings.particleSpacing = 0.02f;
		settings.domainMin       = { 0.0f, 0.0f, 0.0f };
		settings.domainMax       = { 0.3f, 0.4f, 0.2f };
		settings.substeps        = 1;
		settings.iterations      = 4;
		return settings;
	}

	std::map<int, FluidParticle> byId( const std::vector<FluidParticle>& particles ) {
		std::map<int, FluidParticle> result;
		for ( const FluidParticle& particle : particles )
			result[(int)particle.velocity[3]] = particle;
		return result;
	}

	void testAgainstReference( HeadlessGpu& gpu ) {
		std::printf( "GPU substep against the CPU reference\n" );
		FluidSettings settings = smallSettings();
		FluidSimulation simulation( gpu.context, settings );
		simulation.addBlock( { 0.02f, 0.0f, 0.0f }, { 0.22f, 0.24f, 0.2f }, { 0.3f, 0.0f, -0.1f }, { 0.2f, 0.4f, 1.0f } );
		simulation.addBlock( { 0.0f, 0.26f, 0.04f }, { 0.12f, 0.36f, 0.14f }, { 0.0f, -1.0f, 0.0f }, { 1.0f, 0.3f, 0.2f } );
		const std::map<int, FluidParticle> initial = byId( simulation.downloadParticles() );

		std::vector<ReferenceParticle> reference;
		std::vector<int> ids;
		for ( const auto& [id, particle] : initial ) {
			ReferenceParticle p;
			p.position = { particle.position[0], particle.position[1], particle.position[2] };
			p.velocity = { particle.velocity[0], particle.velocity[1], particle.velocity[2] };
			reference.push_back( p );
			ids.push_back( id );
		}
		const float frameTime = 1.0f / 120.0f;
		Reference( simulation.settings(), simulation.constants() ).substep( reference, frameTime );
		gpu.run( simulation, 1, frameTime );
		const std::map<int, FluidParticle> result = byId( simulation.downloadParticles() );
		check( result.size() == reference.size(), "particle count after a step" );

		double maxPositionError = 0.0, maxVelocityError = 0.0, maxDisplacement = 0.0;
		for ( size_t k = 0; k < ids.size(); ++k ) {
			const FluidParticle& gpuParticle = result.at( ids[k] );
			for ( int c = 0; c < 3; ++c ) {
				maxPositionError = std::max( maxPositionError, std::fabs( gpuParticle.position[c] - reference[k].position[c] ) );
				maxVelocityError = std::max( maxVelocityError, std::fabs( gpuParticle.velocity[c] - reference[k].velocity[c] ) );
				maxDisplacement  = std::max( maxDisplacement, std::fabs( initial.at( ids[k] ).position[c] - reference[k].position[c] ) );
			}
		}
		std::printf( "  %zu particles: max position error %.3g m, max velocity error %.3g m/s (max displacement %.3g m)\n",
					 ids.size(), maxPositionError, maxVelocityError, maxDisplacement );
		check( maxPositionError < 2e-5, "GPU positions match the reference" );
		check( maxVelocityError < 4e-3, "GPU velocities match the reference" );
	}

	struct Stats {
		double maxSpeed = 0.0, meanSpeed = 0.0, top = 0.0, kineticEnergy = 0.0;
		bool isInside = true, isFinite = true;
	};

	Stats statistics( const FluidSimulation& simulation, const std::vector<FluidParticle>& particles ) {
		Stats stats;
		const FluidSettings& s = simulation.settings();
		std::vector<double> heights;
		for ( const FluidParticle& p : particles ) {
			const double speed = std::sqrt( p.velocity[0] * p.velocity[0] + p.velocity[1] * p.velocity[1] + p.velocity[2] * p.velocity[2] );
			stats.isFinite = stats.isFinite && std::isfinite( speed ) && std::isfinite( p.position[0] + p.position[1] + p.position[2] );
			stats.maxSpeed = std::max( stats.maxSpeed, speed );
			stats.meanSpeed += speed / particles.size();
			stats.kineticEnergy += 0.5 * speed * speed * simulation.constants().particleMass;
			for ( int c = 0; c < 3; ++c )
				stats.isInside = stats.isInside && p.position[c] >= s.domainMin[c] - 1e-4f && p.position[c] <= s.domainMax[c] + 1e-4f;
			heights.push_back( p.position[1] );
		}
		std::sort( heights.begin(), heights.end() );
		if ( !heights.empty() )
			stats.top = heights[(size_t)(heights.size() * 0.98)];                   ///< Robust surface height
		return stats;
	}

	void testRestingPool( HeadlessGpu& gpu ) {
		std::printf( "Resting pool\n" );
		FluidSettings settings = smallSettings();
		settings.substeps = 2;
		settings.domainMax = { 0.4f, 0.5f, 0.3f };
		settings.maxParticles = 16384;
		FluidSimulation simulation( gpu.context, settings );
		const uint32_t count = simulation.addBlock( { 0.0f, 0.0f, 0.0f }, { 0.4f, 0.2f, 0.3f }, { 0.0f, 0.0f, 0.0f }, { 0.2f, 0.5f, 1.0f } );
		gpu.run( simulation, 180, 1.0f / 60.0f );
		const Stats stats = statistics( simulation, simulation.downloadParticles() );
		std::printf( "  %u particles after 3 s: max speed %.3f m/s, mean %.4f m/s, surface %.3f m (filled 0.200 m)\n",
					 count, stats.maxSpeed, stats.meanSpeed, stats.top );
		check( stats.isFinite && stats.isInside, "resting pool stays finite and inside" );
		check( stats.meanSpeed < 0.02, "resting pool is at rest" );
		check( std::fabs( stats.top - 0.2 ) < 0.03, "resting pool keeps its volume (surface height)" );
	}

	void testDamBreak( HeadlessGpu& gpu ) {
		std::printf( "Dam break\n" );
		FluidSettings settings = smallSettings();
		settings.substeps = 2;
		settings.domainMax = { 0.8f, 0.6f, 0.3f };
		settings.maxParticles = 32768;
		FluidSimulation simulation( gpu.context, settings );
		simulation.addBlock( { 0.0f, 0.0f, 0.0f }, { 0.25f, 0.4f, 0.3f }, { 0.0f, 0.0f, 0.0f }, { 0.2f, 0.5f, 1.0f } );
		double peakEnergy = 0.0;
		for ( int second = 0; second < 6; ++second ) {
			gpu.run( simulation, 60, 1.0f / 60.0f );
			const Stats stats = statistics( simulation, simulation.downloadParticles() );
			peakEnergy = std::max( peakEnergy, stats.kineticEnergy );
			std::printf( "  t=%d s: max speed %.2f m/s, kinetic energy %.4f J, surface %.3f m\n", second + 1, stats.maxSpeed, stats.kineticEnergy, stats.top );
			check( stats.isFinite && stats.isInside, "dam break stays finite and inside at t=" + std::to_string(second + 1) );
			check( stats.maxSpeed < 3.5, "dam break speed is physical (sqrt(2 g H) = 2.8 m/s)" );
			if ( second == 5 )
				check( stats.kineticEnergy < 0.15 * peakEnergy, "dam break settles" );
		}
	}

	void testBuoyancy( HeadlessGpu& gpu ) {
		std::printf( "Floating box\n" );
		FluidSettings settings = smallSettings();
		settings.substeps = 2;
		settings.domainMax = { 0.5f, 0.5f, 0.5f };
		settings.maxParticles = 32768;
		FluidSimulation simulation( gpu.context, settings );
		simulation.addBlock( { 0.0f, 0.0f, 0.0f }, { 0.5f, 0.2f, 0.5f }, { 0.0f, 0.0f, 0.0f }, { 0.2f, 0.5f, 1.0f } );
		FluidBody box;
		box.type = BodyType::BOX;
		box.position = { 0.25f, 0.3f, 0.25f };
		box.halfExtents = { 0.08f, 0.05f, 0.08f };
		box.density = 500.0f;
		simulation.setBody( 0, box );
		gpu.run( simulation, 300, 1.0f / 60.0f );
		const std::vector<GpuBody> bodies = simulation.downloadBodies();
		const Stats stats = statistics( simulation, simulation.downloadParticles() );
		const double bottom = bodies[0].positionType[1] - box.halfExtents[1];
		const double submerged = std::clamp( (stats.top - bottom) / (2.0 * box.halfExtents[1]), 0.0, 1.0 );
		std::printf( "  box density 500: center %.3f m, water surface %.3f m, submerged fraction %.2f (expected 0.50), speed %.3f m/s\n",
					 bodies[0].positionType[1], stats.top, submerged,
					 std::sqrt( bodies[0].velocityInverseMass[0] * bodies[0].velocityInverseMass[0] + bodies[0].velocityInverseMass[1] * bodies[0].velocityInverseMass[1] ) );
		check( std::isfinite( bodies[0].positionType[1] ), "floating box is finite" );
		check( submerged > 0.3 && submerged < 0.7, "box of half the water density floats half submerged" );
	}

	void testPump( HeadlessGpu& gpu ) {
		std::printf( "Pump\n" );
		FluidSettings settings = smallSettings();
		settings.substeps = 2;
		settings.domainMax = { 0.6f, 0.5f, 0.3f };
		settings.maxParticles = 32768;
		FluidSimulation simulation( gpu.context, settings );
		const uint32_t count = simulation.addBlock( { 0.0f, 0.0f, 0.0f }, { 0.6f, 0.12f, 0.3f }, { 0.0f, 0.0f, 0.0f }, { 0.2f, 0.5f, 1.0f } );
		FluidPump pump;
		pump.isEnabled = true;
		pump.drainMin = { 0.45f, 0.0f, 0.0f };
		pump.drainMax = { 0.6f, 0.05f, 0.3f };
		pump.nozzle = { 0.05f, 0.35f, 0.15f };
		pump.nozzleRadius = 0.03f;
		pump.velocity = { 1.5f, 0.5f, 0.0f };
		simulation.setPump( pump );
		gpu.run( simulation, 120, 1.0f / 60.0f );
		const std::vector<FluidParticle> particles = simulation.downloadParticles();
		size_t airborne = 0;
		for ( const FluidParticle& p : particles )
			airborne += p.position[1] > 0.2f ? 1 : 0;
		std::printf( "  %zu particles (%u at start), %zu in the jet above the pool\n", particles.size(), count, airborne );
		check( particles.size() == count, "the pump keeps the particle count" );
		check( airborne > 50, "the pump creates a jet" );
	}

	void benchmark( HeadlessGpu& gpu ) {
		std::printf( "Benchmark (frame of 2 substeps x 3 iterations)\n" );
		for ( uint32_t target : { 16384u, 32768u, 65536u, 131072u } ) {
			FluidSettings settings;
			settings.maxParticles = target;
			settings.particleSpacing = 0.02f;
			/// Domain 2a x a x a, water block 0.8a x 0.8a x a holds the target number of particles
			const float spacing = settings.particleSpacing;
			const float side = std::cbrt( (float)target * spacing * spacing * spacing / 0.64f );
			settings.domainMax = { 2.0f * side, side, side };
			FluidSimulation simulation( gpu.context, settings );
			const uint32_t count = simulation.addBlock( { 0.0f, 0.0f, 0.0f }, { 0.8f * side, 0.8f * side, side }, { 0.0f, 0.0f, 0.0f }, { 0.2f, 0.5f, 1.0f } );
			simulation.setProfiling( true );
			gpu.run( simulation, 30, 1.0f / 60.0f );
			const double milliseconds = gpu.run( simulation, 60, 1.0f / 60.0f );
			const FluidTimings timings = simulation.timings();
			std::printf( "  %7u particles: %.2f ms per frame (sort %.2f, neighbors %.2f, solver %.2f, velocity %.2f)\n", count, milliseconds,
						 timings.sort, timings.neighborSearch, timings.solver, timings.velocity );
		}
	}
}

int main( int argc, char** argv ) {
	std::string shaderDirectory = "VKshaders/fluid/";
	bool isBenchmark = false;
	for ( int i = 1; i < argc; ++i ) {
		const std::string argument = argv[i];
		if ( argument == "--shaders" && i + 1 < argc )
			shaderDirectory = std::string( argv[++i] ) + "/";
		else if ( argument == "--benchmark" )
			isBenchmark = true;
	}

	try {
		HeadlessGpu gpu( shaderDirectory );
		std::printf( "Device: %s\n", gpu.context.properties.deviceName );
		testAgainstReference( gpu );
		testRestingPool( gpu );
		testDamBreak( gpu );
		testBuoyancy( gpu );
		testPump( gpu );
		if ( isBenchmark )
			benchmark( gpu );
	} catch ( const std::exception& error ) {
		std::printf( "ERROR: %s\n", error.what() );
		return 2;
	}
	std::printf( "%d checks passed, %d failed\n", checksPassed, checksFailed );
	return checksFailed == 0 ? 0 : 1;
}
