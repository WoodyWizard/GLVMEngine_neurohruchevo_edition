// This file is part of Game Loop Versatile Modules (GLVM)
// License: http://opensource.org/licenses/MIT

/*
  Screen space fluid rendering of a FluidSimulation over a host rendered scene:
    1. particle spheres -> fluid depth (linear distance), occluded by the scene depth;
    2. additive thickness, dye and foam at half resolution;
    3. narrow range filter of the depth (separable, world space radius);
    4. composite: normals from the smoothed depth, refraction of the scene color, Beer-Lambert absorption,
       Fresnel reflection of the sky, sun highlight, foam, soft edges.
  Also renders a fluid thickness map from the sun, the host can attenuate sunlight with it (water shadows).

  Frame order: update() -> recordLightThickness() -> host scene -> recordFluid().
*/

#ifndef GLVM_FLUID_RENDERER_HPP
#define GLVM_FLUID_RENDERER_HPP

#include "Fluid/FluidMath.hpp"
#include "Fluid/FluidSimulation.hpp"

namespace GLVM::fluid
{
	enum class FluidRenderMode : uint32_t { FINAL, PARTICLES, DEPTH, THICKNESS, NORMALS, COUNT };

	struct FluidLight {
		Vec3  sunDirection = { 0.4f, 0.8f, 0.3f };                         ///< Towards the sun
		float sunIntensity = 1.0f;
		Vec3  sunColor     = { 1.0f, 0.95f, 0.85f };
		Vec3  skyZenith    = { 0.25f, 0.45f, 0.85f };
		Vec3  skyHorizon   = { 0.75f, 0.85f, 0.95f };
		Vec3  groundColor  = { 0.25f, 0.23f, 0.2f };
	};

	struct FluidAppearance {
		Vec3  absorption       = { 0.45f, 0.09f, 0.05f };                  ///< Per meter, pure water absorbs red most
		float dyeAbsorption    = 6.0f;                                     ///< Per meter, absorption of the colors the dye is not
		Vec3  scattering       = { 0.08f, 0.1f, 0.12f };                   ///< In-scattering of sky light by the dyed fluid (clear water: low)
		float refraction       = 0.25f;                                    ///< Bending of the view ray by the surface, about 1 - 1 / 1.33 for water
		float fresnelF0        = 0.02f;                                    ///< Water: (1.33 - 1)^2 / (1.33 + 1)^2
		float shininess        = 900.0f;
		float foamStrength     = 1.0f;
		float smoothingRadius  = 2.2f;                                     ///< In particle spacings
		float renderRadius     = 0.65f;                                    ///< Sphere radius in particle spacings
		float edgeThickness    = 0.015f;                                   ///< Meters, thinner fluid fades into the scene
		float speedColorRamp   = 2.5f;                                     ///< m/s, particle debug view
	};

	struct FluidFrame {
		Mat4            view;
		Mat4            projection;
		Vec3            cameraPosition;
		float           nearPlane = 0.05f;
		float           farPlane  = 100.0f;
		float           verticalFov = 1.0f;                                ///< Radians
		FluidLight      light;
		FluidAppearance appearance;
		FluidRenderMode mode = FluidRenderMode::FINAL;
		Mat4            lightView;                                         ///< Sun view of the light thickness map
		Mat4            lightProjection;                                   ///< Orthographic
		float           time = 0.0f;
	};

	class FluidRenderer {
	public:
		/// outputFormat: format of the composite target (the HDR scene format).
		FluidRenderer( const GpuContext& context, const FluidSimulation& simulation, VkFormat outputFormat, uint32_t lightMapSize = 1024 );
		~FluidRenderer();
		FluidRenderer( const FluidRenderer& ) = delete;
		FluidRenderer& operator=( const FluidRenderer& ) = delete;

		/// Host targets (scene HDR color and D32 depth, both sampled). Call at creation and after every resize.
		void setTargets( VkImageView sceneColor, VkImageView sceneDepth, uint32_t width, uint32_t height );

		/// Writes the frame parameters (outside of rendering).
		void update( VkCommandBuffer commandBuffer, const FluidFrame& frame );
		/// Renders the fluid thickness from the sun; afterwards lightThicknessView() is in SHADER_READ_ONLY_OPTIMAL.
		void recordLightThickness( VkCommandBuffer commandBuffer );
		VkImageView lightThicknessView() const { return lightThickness_.view; }
		/// Light space depth (0..1) of the fluid surface nearest to the sun, 1 where there is no fluid.
		VkImageView lightFrontDepthView() const { return lightFrontDepth_.view; }

		/// Renders the fluid over the scene into output (COLOR_ATTACHMENT_OPTIMAL, fully overwritten).
		/// sceneColor must be in SHADER_READ_ONLY_OPTIMAL, sceneDepth in DEPTH_STENCIL_READ_ONLY_OPTIMAL
		/// (the Vulkan 1.0 layouts: hosts need no separateDepthStencilLayouts).
		void recordFluid( VkCommandBuffer commandBuffer, VkImageView output );

	private:
		const GpuContext&      context_;
		const FluidSimulation& simulation_;
		VkFormat               outputFormat_;
		FluidRenderMode        mode_ = FluidRenderMode::FINAL;
		uint32_t               width_ = 0;
		uint32_t               height_ = 0;

		GpuBuffer              parameters_;
		GpuImage               depthA_;                                    ///< Linear fluid distance (R32F), smoothed in place
		GpuImage               depthB_;
		GpuImage               depthBuffer_;
		GpuImage               thickness_;                                 ///< Half resolution
		GpuImage               dyeFoam_;
		GpuImage               lightThickness_;
		GpuImage               lightFrontDepth_;
		VkSampler              nearestSampler_ = VK_NULL_HANDLE;
		VkSampler              linearSampler_ = VK_NULL_HANDLE;

		VkDescriptorSetLayout  particleLayout_ = VK_NULL_HANDLE;
		VkDescriptorSetLayout  compositeLayout_ = VK_NULL_HANDLE;
		VkDescriptorSetLayout  smoothLayout_ = VK_NULL_HANDLE;
		VkDescriptorPool       descriptorPool_ = VK_NULL_HANDLE;
		VkDescriptorSet        particleSets_[2] = {};                      ///< By simulation state parity
		VkDescriptorSet        compositeSet_ = VK_NULL_HANDLE;
		VkDescriptorSet        smoothSets_[2] = {};                        ///< A -> B, B -> A

		Pipeline               depthPipeline_;
		Pipeline               thicknessPipeline_;
		Pipeline               particlesPipeline_;
		Pipeline               lightPipeline_;
		Pipeline               compositePipeline_;
		Pipeline               smoothPipeline_;

		void destroyTargets();
		void drawParticles( VkCommandBuffer commandBuffer, const Pipeline& pipeline, uint32_t pass );
	};
}

#endif
