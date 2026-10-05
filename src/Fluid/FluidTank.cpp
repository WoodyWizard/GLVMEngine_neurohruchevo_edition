// This file is part of Game Loop Versatile Modules (GLVM)
// License: http://opensource.org/licenses/MIT

#include "Fluid/FluidTank.hpp"
#include "Fluid/FluidMeshes.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace GLVM::fluid
{
	namespace
	{
		/// std140 mirror of TankParams in VKshaders/fluid/tank_common.glsl.
		struct TankParams {
			float viewProjection[16];
			float lightViewProjection[16];
			float cameraPosition[4];
			float lightDirection[4];
			float lightColor[4];
			float ambientColor[4];
			float environmentColor[4];
			float waterAbsorption[4];
			float options[4];
		};

		struct TankPush {
			float    model[16];
			float    color[4];
			uint32_t flags;
			uint32_t bodyType;
			uint32_t padding[2];
		};

		constexpr uint32_t FLAG_BODIES = 1u, FLAG_NEAR_GLASS = 2u;
		constexpr float MATERIAL_TILES = 0.0f, MATERIAL_STEEL = 1.0f, MATERIAL_PAINT = 2.0f, MATERIAL_WOOD = 3.0f, MATERIAL_BALL = 4.0f;
		constexpr uint32_t PISTON_BODY = 0, FIRST_SPLASHER_BODY = 5;
		constexpr uint32_t LIGHT_MAP_SIZE = 512;
		constexpr float PISTON_AMPLITUDE = 0.12f, PISTON_PERIOD = 2.0f, PISTON_HALF_THICKNESS = 0.05f;
		constexpr float FIXED_STEP = 1.0f / 60.0f;                       ///< Simulation step (FluidSettings::substeps substeps each)
		constexpr float MAX_STEPS = 2.0f;                                ///< Per host frame
		constexpr std::array<float, 3> WATER_DYE = { 0.45f, 0.8f, 1.0f };

		void setVec( float* destination, const Vec3& v, float w ) {
			destination[0] = v.x;
			destination[1] = v.y;
			destination[2] = v.z;
			destination[3] = w;
		}

		std::array<float, 3> toArray( const Vec3& v ) { return { v.x, v.y, v.z }; }
	}

	FluidTank::FluidTank( const GpuContext& context, const FluidTankDescription& description, VkFormat colorFormat, VkFormat depthFormat )
		: context_(context), description_(description), colorFormat_(colorFormat) {
		const Vec3 size = description_.innerSize;
		const float baseHeight = 0.1f;
		innerMin_ = description_.floorCenter + Vec3{ -0.5f * size.x, baseHeight, -0.5f * size.z };
		innerMax_ = innerMin_ + size;

		/// Simulation in world space: the domain is the inside of the glass, with room for splashes above it.
		FluidSettings settings = description_.isHighQuality ? FluidSettings() : FluidSettings::fast();
		const float spacing = description_.particleSpacing;
		settings.particleSpacing = spacing;
		settings.domainMin = toArray( innerMin_ );
		settings.domainMax = toArray( innerMax_ + Vec3{ 0.0f, 0.6f, 0.0f } );
		const float waterVolume = size.x * size.z * description_.waterDepth;
		settings.maxParticles = (uint32_t)(waterVolume / (spacing * spacing * spacing) * 1.1f) + 1024u;
		settings.maxBodies = FIRST_SPLASHER_BODY + MAX_SPLASHERS;
		simulation_ = std::make_unique<FluidSimulation>( context_, settings );
		renderer_ = std::make_unique<FluidRenderer>( context_, *simulation_, colorFormat_, LIGHT_MAP_SIZE );

		createScene();
		createGpuResources( depthFormat );
	}

	FluidTank::~FluidTank() {
		vkDeviceWaitIdle( context_.device );
		renderer_.reset();
		simulation_.reset();
		destroyPipeline( context_, objectPipeline_ );
		destroyPipeline( context_, glassPipeline_ );
		vkDestroyDescriptorPool( context_.device, descriptorPool_, nullptr );
		vkDestroyDescriptorSetLayout( context_.device, setLayout_, nullptr );
		vkDestroySampler( context_.device, linearSampler_, nullptr );
		vkDestroySampler( context_.device, nearestSampler_, nullptr );
		destroyImage( context_, sceneCopy_ );
		destroyBuffer( context_, vertices_ );
		destroyBuffer( context_, parameters_ );
	}

	/*
	  ===================================================
	  Scene: water, bodies, frame, actuator
	  ===================================================
	*/
	void FluidTank::createScene() {
		const Vec3 size = description_.innerSize;
		const Vec3 center = (innerMin_ + innerMax_) * 0.5f;
		const float floorY = description_.floorCenter.y;
		const float post = 0.03f;
		const float outerX = 0.5f * size.x + glassThickness_ + post, outerZ = 0.5f * size.z + glassThickness_ + post;
		const float top = innerMax_.y;

		/// Piston: a kinematic plate across the tank at the -x end, it stands on the bottom and out of the water.
		/// Particles are pushed out of a box through its nearest face: the simulated box reaches far below the bottom and
		/// into the side walls, so particles leave it only along x and never get around the plate. It is not drawn,
		/// movingObjects() draws the visible plate.
		pistonHalfHeight_ = 0.5f * std::min( description_.waterDepth + 0.35f, size.y - 0.1f );
		pistonRest_ = innerMin_.x + PISTON_HALF_THICKNESS + PISTON_AMPLITUDE + 0.06f;
		const float overlap = 0.3f;
		piston_.type = BodyType::BOX;
		piston_.position = { pistonRest_, innerMin_.y + pistonHalfHeight_ - 0.5f * overlap, center.z };
		piston_.halfExtents = { PISTON_HALF_THICKNESS, pistonHalfHeight_ + 0.5f * overlap, 0.5f * size.z + overlap };
		piston_.density = 0.0f;
		piston_.color = { 0.0f, 0.0f, 0.0f, -1.0f };                     ///< Material < 0: not drawn
		simulation_->setBody( PISTON_BODY, piston_ );

		/// Water in front of the piston.
		const float waterStart = pistonRest_ + PISTON_HALF_THICKNESS + 0.03f;
		simulation_->addBlock( { waterStart, innerMin_.y, innerMin_.z }, { innerMax_.x, innerMin_.y + description_.waterDepth, innerMax_.z },
							   { 0.0f, 0.0f, 0.0f }, WATER_DYE );

		/// Beach balls and a crate on the water.
		const float surface = innerMin_.y + description_.waterDepth;
		const float ballPlaces[3][2] = { { 0.4f, -0.25f }, { 0.62f, 0.2f }, { 0.85f, -0.05f } };     ///< Fractions of the length, depth
		for ( uint32_t i = 0; i < 3; ++i ) {
			FluidBody ball;
			ball.type = BodyType::SPHERE;
			const float radius = 0.12f + 0.02f * (float)i;
			ball.position = { innerMin_.x + ballPlaces[i][0] * size.x, surface + radius, center.z + ballPlaces[i][1] * size.z };
			ball.halfExtents = { radius, radius, radius };
			ball.density = 120.0f;
			ball.color = { 1.0f, 1.0f, 1.0f, MATERIAL_BALL };
			simulation_->setBody( 1 + i, ball );
		}
		FluidBody crate;
		crate.type = BodyType::BOX;
		crate.position = { innerMin_.x + 0.73f * size.x, surface + 0.15f, center.z + 0.3f * size.z };
		crate.rotation = { 0.0f, 0.38f, 0.0f, 0.925f };
		crate.halfExtents = { 0.12f, 0.12f, 0.12f };
		crate.density = 550.0f;
		crate.color = { 0.72f, 0.5f, 0.3f, MATERIAL_WOOD };
		simulation_->setBody( 4, crate );
		FluidBody inactive;
		simulation_->setBody( FIRST_SPLASHER_BODY + MAX_SPLASHERS - 1, inactive );    ///< Reserves the splasher slots

		/// Static parts: base, tiled bottom, posts and top rails of the frame, the actuator bridge.
		auto add = [&]( Mesh mesh, const Vec3& boxCenter, const Vec3& half, std::array<float, 4> color ) {
			Object object;
			object.mesh = mesh;
			object.model = boxModel( boxCenter, half );
			std::memcpy( object.color, color.data(), sizeof(object.color) );
			staticObjects_.push_back( object );
		};
		const std::array<float, 4> steel = { 0.45f, 0.47f, 0.5f, MATERIAL_STEEL };
		const std::array<float, 4> darkSteel = { 0.2f, 0.21f, 0.23f, MATERIAL_STEEL };
		const float baseTop = innerMin_.y - 0.01f;
		add( CUBE, { center.x, 0.5f * (floorY + baseTop), center.z }, { outerX + post + 0.03f, 0.5f * (baseTop - floorY), outerZ + post + 0.03f }, darkSteel );
		add( CUBE, { center.x, innerMin_.y - 0.005f, center.z }, { 0.5f * size.x + glassThickness_, 0.005f, 0.5f * size.z + glassThickness_ },
			 { 1.0f, 1.0f, 1.0f, MATERIAL_TILES } );
		for ( float sx : { -1.0f, 1.0f } )
			for ( float sz : { -1.0f, 1.0f } )
				add( CUBE, { center.x + sx * outerX, 0.5f * (baseTop + top + 0.024f), center.z + sz * outerZ },
					 { post, 0.5f * (top + 0.024f - baseTop), post }, steel );
		for ( float sz : { -1.0f, 1.0f } )
			add( CUBE, { center.x, top + 0.012f, center.z + sz * outerZ }, { outerX + post, 0.012f, post }, steel );
		for ( float sx : { -1.0f, 1.0f } )
			add( CUBE, { center.x + sx * outerX, top + 0.012f, center.z }, { post, 0.012f, outerZ + post }, steel );

		/// Actuator: two cross beams on the top rails, two guide rails along x, a motor at the end.
		const float driveStart = innerMin_.x + 0.02f, driveEnd = pistonRest_ + PISTON_AMPLITUDE + 0.2f;
		for ( float x : { driveStart, driveEnd } )
			add( CUBE, { x, top + 0.044f, center.z }, { 0.03f, 0.02f, outerZ + post }, steel );
		for ( float sz : { -1.0f, 1.0f } )
			add( CUBE, { 0.5f * (driveStart + driveEnd), top + 0.079f, center.z + sz * 0.09f }, { 0.5f * (driveEnd - driveStart) + 0.03f, 0.015f, 0.015f }, steel );
		add( CUBE, { driveStart - 0.02f, top + 0.16f, center.z }, { 0.09f, 0.075f, 0.12f }, { 0.12f, 0.12f, 0.14f, MATERIAL_PAINT } );

		glass_.mesh = OPEN_BOX;
		glass_.model = boxModel( { center.x, innerMin_.y + 0.5f * size.y, center.z }, { 0.5f * size.x + glassThickness_, 0.5f * size.y, 0.5f * size.z + glassThickness_ } );
		glass_.color[3] = 0.0f;

		boundsMin_ = { center.x - outerX - post - 0.03f, floorY, center.z - outerZ - post - 0.03f };
		boundsMax_ = { center.x + outerX + post + 0.03f, top + 0.3f, center.z + outerZ + post + 0.03f };
	}

	float FluidTank::pistonX() const {
		const float ramp = std::min( time_ / 2.0f, 1.0f );
		return pistonRest_ + PISTON_AMPLITUDE * ramp * std::sin( 6.2831853f * time_ / PISTON_PERIOD );
	}

	/// Plate, rod and carriage of the actuator follow the piston.
	std::vector<FluidTank::Object> FluidTank::movingObjects() const {
		const float x = pistonX();
		const float top = innerMax_.y;
		const float plateTop = innerMin_.y + 2.0f * pistonHalfHeight_;
		const float rodBottom = plateTop - 0.02f, rodTop = top + 0.12f;
		const float centerZ = 0.5f * (innerMin_.z + innerMax_.z);
		const float paint[4] = { 0.85f, 0.6f, 0.05f, MATERIAL_PAINT };
		const float steel[4] = { 0.6f, 0.62f, 0.65f, MATERIAL_STEEL };
		std::vector<Object> objects( 3 );
		objects[0].model = boxModel( { x, innerMin_.y + pistonHalfHeight_, centerZ },
									 { PISTON_HALF_THICKNESS, pistonHalfHeight_, 0.5f * description_.innerSize.z } );
		std::memcpy( objects[0].color, paint, sizeof(paint) );
		objects[1].model = boxModel( { x, 0.5f * (rodBottom + rodTop), centerZ }, { 0.022f, 0.5f * (rodTop - rodBottom), 0.022f } );
		std::memcpy( objects[1].color, steel, sizeof(steel) );
		objects[2].model = boxModel( { x, top + 0.15f, centerZ }, { 0.1f, 0.055f, 0.13f } );
		std::memcpy( objects[2].color, paint, sizeof(paint) );
		return objects;
	}

	/*
	  ===================================================
	  GPU resources
	  ===================================================
	*/
	void FluidTank::createGpuResources( VkFormat depthFormat ) {
		std::vector<float> vertices;
		for ( Mesh mesh : { CUBE, OPEN_BOX, SPHERE } ) {
			const size_t first = vertices.size() / 8;
			if ( mesh == SPHERE )
				appendSphereMesh( vertices, 28, 18 );
			else
				appendBoxMesh( vertices, mesh == OPEN_BOX );
			meshFirst_[mesh] = (uint32_t)first;
			meshCount_[mesh] = (uint32_t)(vertices.size() / 8 - first);
		}
		vertices_ = createBuffer( context_, vertices.size() * sizeof(float), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, false );
		uploadToBuffer( context_, vertices_, vertices.data(), vertices.size() * sizeof(float) );
		parameters_ = createBuffer( context_, sizeof(TankParams), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, false );
		linearSampler_  = createSampler( context_, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE );
		nearestSampler_ = createSampler( context_, VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE );

		const VkDescriptorType sampled = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		setLayout_ = createSetLayout( context_, { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
												  sampled, sampled }, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT );
		descriptorPool_ = createDescriptorPool( context_, 1 );
		set_ = allocateSet( context_, descriptorPool_, setLayout_ );
		updateSet( context_, set_, {
			bufferWrite( 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, parameters_ ),
			bufferWrite( 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, vertices_ ),
			bufferWrite( 2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, simulation_->bodyBuffer() ),
			imageWrite( 3, sampled, renderer_->lightThicknessView(), linearSampler_, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL ),
			imageWrite( 4, sampled, renderer_->lightFrontDepthView(), nearestSampler_, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL ),
		});

		/// No face culling: the host projection may mirror the view; the shaders orient the normals towards the camera.
		GraphicsPipelineDescription object;
		object.vertexShader     = "tank_object.vert.spv";
		object.fragmentShader   = "tank_object.frag.spv";
		object.setLayouts       = { setLayout_ };
		object.pushConstantSize = sizeof(TankPush);
		object.colorFormats     = { colorFormat_ };
		object.depthFormat      = depthFormat;
		object.depthTest        = true;
		object.depthWrite       = true;
		object.depthCompare     = VK_COMPARE_OP_LESS;
		objectPipeline_ = createGraphicsPipeline( context_, object );

		GraphicsPipelineDescription glass = object;
		glass.fragmentShader = "tank_glass.frag.spv";
		glass.blendModes     = { BlendMode::ALPHA };
		glass.depthWrite     = false;
		glass.depthCompare   = VK_COMPARE_OP_LESS_OR_EQUAL;
		glassPipeline_ = createGraphicsPipeline( context_, glass );
	}

	void FluidTank::setTargets( uint32_t width, uint32_t height, VkImageView depthView ) {
		vkDeviceWaitIdle( context_.device );
		destroyImage( context_, sceneCopy_ );
		sceneCopy_ = createImage( context_, width, height, colorFormat_, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT );
		depthView_ = depthView;
		renderer_->setTargets( sceneCopy_.view, depthView, width, height );
	}

	void FluidTank::setSplashers( const std::vector<FluidSplasher>& splashers ) {
		splashers_.assign( splashers.begin(), splashers.begin() + std::min<size_t>( splashers.size(), MAX_SPLASHERS ) );
	}

	bool FluidTank::isInsideWater( const Vec3& p ) const {
		return p.x > innerMin_.x && p.x < innerMax_.x && p.y > innerMin_.y && p.y < innerMax_.y + 0.6f && p.z > innerMin_.z && p.z < innerMax_.z;
	}

	bool FluidTank::pushOut( Vec3& position, float radius ) const {
		if ( position.y > boundsMax_.y + radius || position.y < boundsMin_.y - radius )
			return false;
		const float left = position.x - (boundsMin_.x - radius), right = (boundsMax_.x + radius) - position.x;
		const float back = position.z - (boundsMin_.z - radius), front = (boundsMax_.z + radius) - position.z;
		if ( left <= 0.0f || right <= 0.0f || back <= 0.0f || front <= 0.0f )
			return false;
		const float smallest = std::min( std::min( left, right ), std::min( back, front ) );
		if ( smallest == left )        position.x -= left;
		else if ( smallest == right )  position.x += right;
		else if ( smallest == back )   position.z -= back;
		else                           position.z += front;
		return true;
	}

	/*
	  ===================================================
	  Frame
	  ===================================================
	*/
	/*
	  Fixed time steps: position based fluids turn the position corrections of a step into velocities (correction / dt),
	  a short host frame would turn the small remaining corrections into large velocities and the water would jump.
	  The host frame time is accumulated and simulated in steps of FIXED_STEP, at most MAX_STEPS per frame (slower
	  frames slow the water down instead of making larger steps).
	*/
	void FluidTank::recordSimulation( VkCommandBuffer commandBuffer, float frameTime ) {
		accumulatedTime_ = std::min( accumulatedTime_ + std::max( frameTime, 0.0f ), MAX_STEPS * FIXED_STEP );
		if ( accumulatedTime_ < FIXED_STEP )
			return;
		/// Frames in flight share the simulation buffers and the render targets: the previous frame finishes first.
		memoryBarrier( commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT,
					   VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT );

		for ( ; accumulatedTime_ >= FIXED_STEP; accumulatedTime_ -= FIXED_STEP ) {
			time_ += FIXED_STEP;
			const float ramp = std::min( time_ / 2.0f, 1.0f );
			piston_.position[0] = pistonX();
			piston_.velocity[0] = PISTON_AMPLITUDE * ramp * 6.2831853f / PISTON_PERIOD * std::cos( 6.2831853f * time_ / PISTON_PERIOD );
			simulation_->recordBodyUpdate( commandBuffer, PISTON_BODY, piston_ );

			/// Splashers are kinematic spheres; slots of splashers that left are switched off once.
			const uint32_t count = (uint32_t)splashers_.size();
			for ( uint32_t i = 0; i < std::max( count, activeSplashers_ ); ++i ) {
				FluidBody body;
				if ( i < count ) {
					body.type = BodyType::SPHERE;
					body.position = toArray( splashers_[i].position );
					body.velocity = toArray( splashers_[i].velocity );
					body.halfExtents = { splashers_[i].radius, splashers_[i].radius, splashers_[i].radius };
					body.density = 0.0f;
					body.color = { 0.9f, 0.9f, 0.9f, MATERIAL_STEEL };
				}
				simulation_->recordBodyUpdate( commandBuffer, FIRST_SPLASHER_BODY + i, body );
			}
			activeSplashers_ = count;
			simulation_->recordStep( commandBuffer, FIXED_STEP );
		}
	}

	void FluidTank::drawObject( VkCommandBuffer commandBuffer, const Pipeline& pipeline, const Object& object, uint32_t flags ) const {
		TankPush push = {};
		std::memcpy( push.model, object.model.m, sizeof(push.model) );
		std::memcpy( push.color, object.color, sizeof(push.color) );
		push.flags = flags;
		vkCmdPushConstants( commandBuffer, pipeline.layout, VK_SHADER_STAGE_ALL, 0, sizeof(push), &push );
		vkCmdDraw( commandBuffer, meshCount_[object.mesh], 1, meshFirst_[object.mesh], 0 );
	}

	/// Bodies are drawn from the simulation body buffer: spheres and boxes (inactive splashers are degenerate).
	void FluidTank::drawBodies( VkCommandBuffer commandBuffer ) const {
		for ( uint32_t type = 1; type <= 2; ++type ) {
			TankPush push = {};
			push.flags = FLAG_BODIES;
			push.bodyType = type;
			vkCmdPushConstants( commandBuffer, objectPipeline_.layout, VK_SHADER_STAGE_ALL, 0, sizeof(push), &push );
			const Mesh mesh = type == 1 ? SPHERE : CUBE;
			vkCmdDraw( commandBuffer, meshCount_[mesh], simulation_->bodyCount(), meshFirst_[mesh], 0 );
		}
	}

	void FluidTank::recordRendering( VkCommandBuffer commandBuffer, const FluidTankTarget& target, const FluidTankCamera& camera ) {
		const VkExtent2D extent = sceneCopy_.extent;
		memoryBarrier( commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT,
					   VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT );

		/// Light view of the fluid light maps: orthographic box around the tank.
		const Vec3 center = (innerMin_ + innerMax_) * 0.5f;
		const Vec3 light = normalize( camera.lightDirection );
		const float reach = 0.5f * std::max( description_.innerSize.x, description_.innerSize.z ) + 0.6f;
		const Mat4 lightView = Mat4::lookAt( center + light * 8.0f, center, std::fabs( light.y ) > 0.99f ? Vec3{ 0, 0, 1 } : Vec3{ 0, 1, 0 } );
		const Mat4 lightProjection = Mat4::orthographic( -reach, reach, -reach, reach, 1.0f, 16.0f );
		const Mat4 inverseView = camera.view.inverse();
		const Vec3 eye = { inverseView.at( 0, 3 ), inverseView.at( 1, 3 ), inverseView.at( 2, 3 ) };

		FluidFrame frame;
		frame.view = camera.view;
		frame.projection = camera.projection;
		frame.cameraPosition = eye;
		frame.nearPlane = camera.nearPlane;
		frame.farPlane = camera.farPlane;
		frame.verticalFov = camera.verticalFov;
		frame.light.sunDirection = light;
		frame.light.sunIntensity = camera.lightIntensity;
		frame.light.sunColor = camera.lightColor;
		frame.light.skyZenith = camera.environmentColor;
		frame.light.skyHorizon = camera.environmentColor * 1.1f;
		frame.light.groundColor = camera.environmentColor * 0.6f;
		frame.appearance.dyeAbsorption = 7.0f;
		frame.lightView = lightView;
		frame.lightProjection = lightProjection;
		frame.time = time_;
		renderer_->update( commandBuffer, frame );

		TankParams params = {};
		std::memcpy( params.viewProjection, (camera.projection * camera.view).m, 64 );
		std::memcpy( params.lightViewProjection, (lightProjection * lightView).m, 64 );
		setVec( params.cameraPosition, eye, time_ );
		setVec( params.lightDirection, light, camera.lightIntensity );
		setVec( params.lightColor, camera.lightColor, 0.0f );
		setVec( params.ambientColor, camera.ambientColor, 0.0f );
		setVec( params.environmentColor, camera.environmentColor, 0.0f );
		setVec( params.waterAbsorption, frame.appearance.absorption * 3.0f, 3.0f );
		params.options[0] = 1.0f / LIGHT_MAP_SIZE;
		params.options[1] = 1.0f;
		vkCmdUpdateBuffer( commandBuffer, parameters_.buffer, 0, sizeof(params), &params );
		memoryBarrier( commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
					   VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_UNIFORM_READ_BIT );
		renderer_->recordLightThickness( commandBuffer );

		const VkPipelineStageFlags color = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		const VkPipelineStageFlags fragment = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		const VkPipelineStageFlags transfer = VK_PIPELINE_STAGE_TRANSFER_BIT;
		const VkPipelineStageFlags depthTests = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		const VkAccessFlags colorAccess = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		const VkAccessFlags depthAccess = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

		/// 1. Opaque parts and the far glass into the host frame.
		imageBarrier( commandBuffer, target.colorImage, VK_IMAGE_ASPECT_COLOR_BIT, target.colorLayout, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
					  color, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, color, colorAccess );
		imageBarrier( commandBuffer, target.depthImage, target.depthAspect, target.depthLayout, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
					  depthTests, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, depthTests, depthAccess );
		Attachment hostColor;
		hostColor.view   = target.colorView;
		hostColor.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
		Attachment hostDepth;
		hostDepth.view   = depthView_;
		hostDepth.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
		beginRendering( commandBuffer, extent, { hostColor }, &hostDepth );
		vkCmdBindPipeline( commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, objectPipeline_.pipeline );
		vkCmdBindDescriptorSets( commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, objectPipeline_.layout, 0, 1, &set_, 0, nullptr );
		for ( const Object& object : staticObjects_ )
			drawObject( commandBuffer, objectPipeline_, object, 0u );
		for ( const Object& object : movingObjects() )
			drawObject( commandBuffer, objectPipeline_, object, 0u );
		drawBodies( commandBuffer );
		vkCmdBindPipeline( commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, glassPipeline_.pipeline );
		drawObject( commandBuffer, glassPipeline_, glass_, 0u );
		vkCmdEndRendering( commandBuffer );

		/// 2. The scene behind the water (refraction), then the fluid over the frame.
		imageBarrier( commandBuffer, target.colorImage, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
					  VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, color, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, transfer, VK_ACCESS_TRANSFER_READ_BIT );
		imageBarrier( commandBuffer, sceneCopy_.image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
					  fragment, 0, transfer, VK_ACCESS_TRANSFER_WRITE_BIT );
		VkImageCopy region{};
		region.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
		region.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
		region.extent         = { extent.width, extent.height, 1 };
		vkCmdCopyImage( commandBuffer, target.colorImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, sceneCopy_.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
						1, &region );
		imageBarrier( commandBuffer, sceneCopy_.image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
					  transfer, VK_ACCESS_TRANSFER_WRITE_BIT, fragment, VK_ACCESS_SHADER_READ_BIT );
		imageBarrier( commandBuffer, target.colorImage, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
					  transfer, VK_ACCESS_TRANSFER_READ_BIT, color, colorAccess );
		imageBarrier( commandBuffer, target.depthImage, target.depthAspect, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
					  VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL, depthTests, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
					  fragment | depthTests, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT );
		renderer_->recordFluid( commandBuffer, target.colorView );

		/// 3. The near glass over the water.
		memoryBarrier( commandBuffer, color, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, color, colorAccess );
		Attachment readOnlyDepth;
		readOnlyDepth.view    = depthView_;
		readOnlyDepth.loadOp  = VK_ATTACHMENT_LOAD_OP_LOAD;
		readOnlyDepth.storeOp = VK_ATTACHMENT_STORE_OP_NONE;
		readOnlyDepth.layout  = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
		beginRendering( commandBuffer, extent, { hostColor }, &readOnlyDepth );
		vkCmdBindPipeline( commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, glassPipeline_.pipeline );
		vkCmdBindDescriptorSets( commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, glassPipeline_.layout, 0, 1, &set_, 0, nullptr );
		drawObject( commandBuffer, glassPipeline_, glass_, FLAG_NEAR_GLASS );
		vkCmdEndRendering( commandBuffer );

		/// Back to the layouts of the host.
		imageBarrier( commandBuffer, target.colorImage, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, target.colorLayout,
					  color, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, color | fragment, colorAccess | VK_ACCESS_SHADER_READ_BIT );
		imageBarrier( commandBuffer, target.depthImage, target.depthAspect, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL, target.depthLayout,
					  fragment | depthTests, 0, depthTests, depthAccess );
	}
}
