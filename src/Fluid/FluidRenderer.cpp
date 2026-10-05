// This file is part of Game Loop Versatile Modules (GLVM)
// License: http://opensource.org/licenses/MIT

#include "Fluid/FluidRenderer.hpp"

#include <cmath>
#include <algorithm>
#include <cstring>

namespace GLVM::fluid
{
	namespace
	{
		constexpr float NO_FLUID = 1e6f;                                   ///< Must match fluid_render_common.glsl
		constexpr VkFormat DEPTH_FORMAT = VK_FORMAT_D32_SFLOAT;

		/// std140 mirror of RenderParams in fluid_render_common.glsl.
		struct RenderParams {
			float view[16];
			float projection[16];
			float inverseView[16];
			float inverseProjection[16];
			float lightView[16];
			float lightProjection[16];
			float cameraPosition[4];
			float projectionParams[4];
			float viewport[4];
			float sunDirection[4];
			float sunColor[4];
			float skyZenith[4];
			float skyHorizon[4];
			float groundColor[4];
			float absorption[4];
			float scattering[4];
			float material[4];
			float particleParams[4];
			float fluidParams[4];
		};

		void copyMatrix( float* destination, const Mat4& matrix ) {
			std::memcpy( destination, matrix.m, sizeof(matrix.m) );
		}

		void copyVec( float* destination, const Vec3& v, float w ) {
			destination[0] = v.x;
			destination[1] = v.y;
			destination[2] = v.z;
			destination[3] = w;
		}

		uint32_t groupsFor( uint32_t count, uint32_t groupSize ) {
			return (count + groupSize - 1) / groupSize;
		}
	}

	FluidRenderer::FluidRenderer( const GpuContext& context, const FluidSimulation& simulation, VkFormat outputFormat, uint32_t lightMapSize )
		: context_(context), simulation_(simulation), outputFormat_(outputFormat) {
		parameters_ = createBuffer( context_, sizeof(RenderParams), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, false );
		nearestSampler_ = createSampler( context_, VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE );
		linearSampler_  = createSampler( context_, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE );
		lightThickness_ = createImage( context_, lightMapSize, lightMapSize, VK_FORMAT_R16_SFLOAT,
									   VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT );
		lightFrontDepth_ = createImage( context_, lightMapSize, lightMapSize, VK_FORMAT_R16_SFLOAT,
										VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT );

		const VkShaderStageFlags allStages = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT;
		particleLayout_ = createSetLayout( context_, { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
													   VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
													   VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER }, allStages );
		compositeLayout_ = createSetLayout( context_, std::vector<VkDescriptorType>( 4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER ),
											VK_SHADER_STAGE_FRAGMENT_BIT );
		smoothLayout_ = createSetLayout( context_, { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE },
										 VK_SHADER_STAGE_COMPUTE_BIT );
		descriptorPool_ = createDescriptorPool( context_, 8 );
		for ( uint32_t parity = 0; parity < 2; ++parity )
			particleSets_[parity] = allocateSet( context_, descriptorPool_, particleLayout_ );
		compositeSet_  = allocateSet( context_, descriptorPool_, compositeLayout_ );
		smoothSets_[0] = allocateSet( context_, descriptorPool_, smoothLayout_ );
		smoothSets_[1] = allocateSet( context_, descriptorPool_, smoothLayout_ );

		GraphicsPipelineDescription particles;
		particles.vertexShader     = "fluid_particle.vert.spv";
		particles.setLayouts       = { particleLayout_ };
		particles.pushConstantSize = 4;

		GraphicsPipelineDescription depth = particles;
		depth.fragmentShader = "fluid_depth.frag.spv";
		depth.colorFormats   = { VK_FORMAT_R32_SFLOAT };
		depth.depthFormat    = DEPTH_FORMAT;
		depth.depthTest      = true;
		depth.depthWrite     = true;
		depth.depthCompare   = VK_COMPARE_OP_LESS;
		depthPipeline_ = createGraphicsPipeline( context_, depth );

		GraphicsPipelineDescription thickness = particles;
		thickness.fragmentShader = "fluid_thickness.frag.spv";
		thickness.colorFormats   = { VK_FORMAT_R16_SFLOAT, VK_FORMAT_R16G16B16A16_SFLOAT };
		thickness.blendModes     = { BlendMode::ADDITIVE, BlendMode::ADDITIVE };
		thicknessPipeline_ = createGraphicsPipeline( context_, thickness );

		GraphicsPipelineDescription light = particles;
		light.fragmentShader = "fluid_light_thickness.frag.spv";
		light.colorFormats   = { VK_FORMAT_R16_SFLOAT, VK_FORMAT_R16_SFLOAT };
		light.blendModes     = { BlendMode::ADDITIVE, BlendMode::MINIMUM };
		lightPipeline_ = createGraphicsPipeline( context_, light );

		GraphicsPipelineDescription debugParticles = particles;
		debugParticles.fragmentShader = "fluid_particles.frag.spv";
		debugParticles.colorFormats   = { outputFormat_ };
		debugParticles.depthFormat    = DEPTH_FORMAT;
		debugParticles.depthTest      = true;
		debugParticles.depthWrite     = true;
		debugParticles.depthCompare   = VK_COMPARE_OP_LESS;
		particlesPipeline_ = createGraphicsPipeline( context_, debugParticles );

		GraphicsPipelineDescription composite;
		composite.vertexShader   = "fluid_fullscreen.vert.spv";
		composite.fragmentShader = "fluid_composite.frag.spv";
		composite.setLayouts     = { particleLayout_, compositeLayout_ };
		composite.colorFormats   = { outputFormat_ };
		compositePipeline_ = createGraphicsPipeline( context_, composite );

		smoothPipeline_ = createComputePipeline( context_, "fluid_smooth.comp.spv", { particleLayout_, smoothLayout_ }, 8 );
	}

	FluidRenderer::~FluidRenderer() {
		vkDeviceWaitIdle( context_.device );
		for ( Pipeline* pipeline : { &depthPipeline_, &thicknessPipeline_, &particlesPipeline_, &lightPipeline_, &compositePipeline_, &smoothPipeline_ } )
			destroyPipeline( context_, *pipeline );
		vkDestroyDescriptorPool( context_.device, descriptorPool_, nullptr );
		for ( VkDescriptorSetLayout layout : { particleLayout_, compositeLayout_, smoothLayout_ } )
			vkDestroyDescriptorSetLayout( context_.device, layout, nullptr );
		destroyTargets();
		destroyImage( context_, lightThickness_ );
		destroyImage( context_, lightFrontDepth_ );
		vkDestroySampler( context_.device, nearestSampler_, nullptr );
		vkDestroySampler( context_.device, linearSampler_, nullptr );
		destroyBuffer( context_, parameters_ );
	}

	void FluidRenderer::destroyTargets() {
		for ( GpuImage* image : { &depthA_, &depthB_, &depthBuffer_, &thickness_, &dyeFoam_ } )
			destroyImage( context_, *image );
	}

	void FluidRenderer::setTargets( VkImageView sceneColor, VkImageView sceneDepth, uint32_t width, uint32_t height ) {
		vkDeviceWaitIdle( context_.device );
		destroyTargets();
		width_  = width;
		height_ = height;
		const uint32_t halfWidth = std::max( width / 2, 1u ), halfHeight = std::max( height / 2, 1u );
		depthA_      = createImage( context_, width, height, VK_FORMAT_R32_SFLOAT,
									VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT );
		depthB_      = createImage( context_, width, height, VK_FORMAT_R32_SFLOAT, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT );
		depthBuffer_ = createImage( context_, width, height, DEPTH_FORMAT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT );
		thickness_   = createImage( context_, halfWidth, halfHeight, VK_FORMAT_R16_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT );
		dyeFoam_     = createImage( context_, halfWidth, halfHeight, VK_FORMAT_R16G16B16A16_SFLOAT,
									VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT );

		const VkDescriptorType storage = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		const VkDescriptorType sampled = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		for ( uint32_t parity = 0; parity < 2; ++parity ) {
			updateSet( context_, particleSets_[parity], {
				bufferWrite( 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, parameters_ ),
				bufferWrite( 1, storage, simulation_.positionBuffer( parity ) ),
				bufferWrite( 2, storage, simulation_.velocityBuffer( parity ) ),
				bufferWrite( 3, storage, simulation_.colorBuffer( parity ) ),
				imageWrite( 4, sampled, sceneDepth, nearestSampler_, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL ),
			});
		}
		updateSet( context_, compositeSet_, {
			imageWrite( 0, sampled, sceneColor, linearSampler_, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL ),
			imageWrite( 1, sampled, depthA_.view, nearestSampler_, VK_IMAGE_LAYOUT_GENERAL ),
			imageWrite( 2, sampled, thickness_.view, linearSampler_, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL ),
			imageWrite( 3, sampled, dyeFoam_.view, linearSampler_, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL ),
		});
		const VkDescriptorType image = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		updateSet( context_, smoothSets_[0], { imageWrite( 0, image, depthA_.view, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL ),
											   imageWrite( 1, image, depthB_.view, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL ) } );
		updateSet( context_, smoothSets_[1], { imageWrite( 0, image, depthB_.view, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL ),
											   imageWrite( 1, image, depthA_.view, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL ) } );
	}

	void FluidRenderer::update( VkCommandBuffer commandBuffer, const FluidFrame& frame ) {
		mode_ = frame.mode;
		const FluidAppearance& look = frame.appearance;
		const float spacing = simulation_.settings().particleSpacing;
		const float renderRadius = look.renderRadius * spacing;

		RenderParams p = {};
		copyMatrix( p.view, frame.view );
		copyMatrix( p.projection, frame.projection );
		copyMatrix( p.inverseView, frame.view.inverse() );
		copyMatrix( p.inverseProjection, frame.projection.inverse() );
		copyMatrix( p.lightView, frame.lightView );
		copyMatrix( p.lightProjection, frame.lightProjection );
		copyVec( p.cameraPosition, frame.cameraPosition, renderRadius );
		/// Depth d = -A - B / z for view z, so the linear distance is B / (A + d) (see FluidMath perspective()).
		p.projectionParams[0] = frame.farPlane / (frame.nearPlane - frame.farPlane);
		p.projectionParams[1] = frame.farPlane * frame.nearPlane / (frame.nearPlane - frame.farPlane);
		p.projectionParams[2] = (float)height_ / (2.0f * std::tan( 0.5f * frame.verticalFov ));
		p.projectionParams[3] = frame.farPlane;
		p.viewport[0] = (float)width_;
		p.viewport[1] = (float)height_;
		p.viewport[2] = 1.0f / (float)std::max( width_, 1u );
		p.viewport[3] = 1.0f / (float)std::max( height_, 1u );
		copyVec( p.sunDirection, normalize( frame.light.sunDirection ), frame.light.sunIntensity );
		copyVec( p.sunColor, frame.light.sunColor, 0.0f );
		copyVec( p.skyZenith, frame.light.skyZenith, 0.0f );
		copyVec( p.skyHorizon, frame.light.skyHorizon, 0.0f );
		copyVec( p.groundColor, frame.light.groundColor, 0.0f );
		copyVec( p.absorption, look.absorption, look.dyeAbsorption );
		copyVec( p.scattering, look.scattering, look.refraction );
		p.material[0] = look.fresnelF0;
		p.material[1] = look.shininess;
		p.material[2] = look.foamStrength;
		p.material[3] = look.smoothingRadius * spacing;
		p.particleParams[0] = spacing;
		p.particleParams[1] = look.speedColorRamp;
		p.particleParams[2] = (float)frame.mode;
		p.particleParams[3] = frame.time;
		/// Summed chords of the rendered spheres are their volume per pixel area; scaled to the water volume per particle.
		p.fluidParams[0] = spacing * spacing * spacing / (4.0f / 3.0f * 3.14159265f * renderRadius * renderRadius * renderRadius);
		p.fluidParams[1] = look.edgeThickness;

		memoryBarrier( commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_UNIFORM_READ_BIT,
					   VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT );
		vkCmdUpdateBuffer( commandBuffer, parameters_.buffer, 0, sizeof(p), &p );
		memoryBarrier( commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
					   VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
					   VK_ACCESS_UNIFORM_READ_BIT );
	}

	void FluidRenderer::drawParticles( VkCommandBuffer commandBuffer, const Pipeline& pipeline, uint32_t pass ) {
		const uint32_t count = simulation_.particleCount();
		if ( count == 0 )
			return;
		vkCmdBindPipeline( commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.pipeline );
		vkCmdBindDescriptorSets( commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.layout, 0, 1,
								 &particleSets_[simulation_.stateParity()], 0, nullptr );
		vkCmdPushConstants( commandBuffer, pipeline.layout, VK_SHADER_STAGE_ALL, 0, 4, &pass );
		vkCmdDraw( commandBuffer, 6, count, 0, 0 );
	}

	void FluidRenderer::recordLightThickness( VkCommandBuffer commandBuffer ) {
		for ( const GpuImage* image : { &lightThickness_, &lightFrontDepth_ } )
			imageBarrier( commandBuffer, image->image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
						  VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
						  VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT );
		Attachment thickness, frontDepth;
		thickness.view  = lightThickness_.view;
		frontDepth.view = lightFrontDepth_.view;
		frontDepth.clearValue.color = { { 1.0f, 1.0f, 1.0f, 1.0f } };
		beginRendering( commandBuffer, lightThickness_.extent, { thickness, frontDepth }, nullptr );
		drawParticles( commandBuffer, lightPipeline_, 1 );
		vkCmdEndRendering( commandBuffer );
		for ( const GpuImage* image : { &lightThickness_, &lightFrontDepth_ } )
			imageBarrier( commandBuffer, image->image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
						  VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
						  VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT );
	}

	void FluidRenderer::recordFluid( VkCommandBuffer commandBuffer, VkImageView output ) {
		const VkPipelineStageFlags fragment = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		const VkPipelineStageFlags compute  = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
		const VkPipelineStageFlags color    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		const VkPipelineStageFlags depthTests = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;

		/// 1. Fluid depth: nearest sphere surface per pixel.
		imageBarrier( commandBuffer, depthA_.image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
					  fragment | compute, 0, color, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT );
		imageBarrier( commandBuffer, depthBuffer_.image, VK_IMAGE_ASPECT_DEPTH_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
					  depthTests, 0, depthTests, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT );
		const VkExtent2D extent = { width_, height_ };
		Attachment fluidDepth;
		fluidDepth.view = depthA_.view;
		fluidDepth.clearValue.color = { { NO_FLUID, 0.0f, 0.0f, 0.0f } };
		Attachment depthBuffer;
		depthBuffer.view = depthBuffer_.view;
		depthBuffer.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		depthBuffer.clearValue.depthStencil = { 1.0f, 0 };
		beginRendering( commandBuffer, extent, { fluidDepth }, &depthBuffer );
		drawParticles( commandBuffer, depthPipeline_, 0 );
		vkCmdEndRendering( commandBuffer );

		/// 2. Thickness, dye and foam at half resolution.
		for ( const GpuImage* image : { &thickness_, &dyeFoam_ } )
			imageBarrier( commandBuffer, image->image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
						  fragment, 0, color, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT );
		Attachment thickness, dyeFoam;
		thickness.view = thickness_.view;
		dyeFoam.view   = dyeFoam_.view;
		beginRendering( commandBuffer, thickness_.extent, { thickness, dyeFoam }, nullptr );
		drawParticles( commandBuffer, thicknessPipeline_, 0 );
		vkCmdEndRendering( commandBuffer );
		for ( const GpuImage* image : { &thickness_, &dyeFoam_ } )
			imageBarrier( commandBuffer, image->image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
						  VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, color, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, fragment, VK_ACCESS_SHADER_READ_BIT );

		/// 3. Narrow range filter: horizontal, vertical, then the two diagonals; A -> B -> A -> B -> A.
		imageBarrier( commandBuffer, depthA_.image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
					  color, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, compute, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT );
		imageBarrier( commandBuffer, depthB_.image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
					  fragment | compute, 0, compute, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT );
		vkCmdBindPipeline( commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, smoothPipeline_.pipeline );
		vkCmdBindDescriptorSets( commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, smoothPipeline_.layout, 0, 1,
								 &particleSets_[simulation_.stateParity()], 0, nullptr );
		for ( int iteration = 0; iteration < 2; ++iteration ) {
			for ( int axis = 0; axis < 2; ++axis ) {
				const int32_t direction[2] = { iteration == 0 ? (axis == 0 ? 1 : 0) : 1, iteration == 0 ? (axis == 0 ? 0 : 1) : (axis == 0 ? 1 : -1) };
				vkCmdBindDescriptorSets( commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, smoothPipeline_.layout, 1, 1, &smoothSets_[axis], 0, nullptr );
				vkCmdPushConstants( commandBuffer, smoothPipeline_.layout, VK_SHADER_STAGE_ALL, 0, 8, direction );
				vkCmdDispatch( commandBuffer, groupsFor( width_, 16 ), groupsFor( height_, 16 ), 1 );
				computeBarrier( commandBuffer );
			}
		}
		memoryBarrier( commandBuffer, compute, VK_ACCESS_SHADER_WRITE_BIT, fragment, VK_ACCESS_SHADER_READ_BIT );

		/// 4. Composite into the output.
		Attachment target;
		target.view   = output;
		target.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		beginRendering( commandBuffer, extent, { target }, nullptr );
		vkCmdBindPipeline( commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, compositePipeline_.pipeline );
		const VkDescriptorSet sets[2] = { particleSets_[simulation_.stateParity()], compositeSet_ };
		vkCmdBindDescriptorSets( commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, compositePipeline_.layout, 0, 2, sets, 0, nullptr );
		vkCmdDraw( commandBuffer, 3, 1, 0, 0 );
		vkCmdEndRendering( commandBuffer );

		if ( mode_ == FluidRenderMode::PARTICLES ) {
			imageBarrier( commandBuffer, depthBuffer_.image, VK_IMAGE_ASPECT_DEPTH_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
						  depthTests, 0, depthTests, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT );
			memoryBarrier( commandBuffer, color, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, color, VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT );
			target.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
			beginRendering( commandBuffer, extent, { target }, &depthBuffer );
			drawParticles( commandBuffer, particlesPipeline_, 0 );
			vkCmdEndRendering( commandBuffer );
		}
	}
}
