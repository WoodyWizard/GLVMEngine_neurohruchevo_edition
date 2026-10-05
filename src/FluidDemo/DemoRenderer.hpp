// This file is part of Game Loop Versatile Modules (GLVM)
// License: http://opensource.org/licenses/MIT

/*
  Renderer of the fluid demo: Vulkan device and swapchain, the scene (sky, floor, tank, rigid bodies) with sun
  shadows and water shadows, the FluidRenderer over it, glass, tone mapping and a text overlay.

  Frame: simulation -> fluid light maps -> shadow map -> scene (HDR) -> fluid composite -> glass -> tone map + text.
*/

#ifndef GLVM_FLUID_DEMO_RENDERER_HPP
#define GLVM_FLUID_DEMO_RENDERER_HPP

#include "DemoPlatform.hpp"
#include "Fluid/FluidRenderer.hpp"

#include <functional>
#include <memory>
#include <string>

namespace GLVM::fluid::demo
{
	enum class MeshKind { CUBE, SPHERE, FLOOR, OPEN_BOX };                  ///< OPEN_BOX: a cube without top and bottom (tank glass)

	struct SceneObject {
		MeshKind mesh = MeshKind::CUBE;
		Mat4     model;
		float    color[4] = { 1.0f, 1.0f, 1.0f, 0.0f };                    ///< rgb albedo, w material (see scene_common.glsl)
		bool     isGlass = false;
		bool     castsShadow = true;
	};

	struct TextLine {
		std::string text;
		float       x = 0.0f, y = 0.0f;                                     ///< Pixels from the top left corner
		float       scale = 2.0f;
		float       color[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
	};

	struct DemoFrame {
		Vec3                     eye;
		Vec3                     target;
		float                    verticalFov = 0.85f;
		FluidRenderMode          mode = FluidRenderMode::FINAL;
		FluidLight               light;
		FluidAppearance          appearance;
		Vec3                     tankMin, tankMax;
		std::vector<SceneObject> objects;
		std::vector<TextLine>    text;
		bool                     isWaterShadow = true;
		float                    exposure = 1.0f;
		float                    time = 0.0f;
		std::string              screenshotPath;                            ///< Saved after this frame if not empty
	};

	struct GpuTimings {
		float simulation = 0.0f, scene = 0.0f, fluid = 0.0f, total = 0.0f;   ///< Milliseconds
	};

	struct RendererOptions {
		bool isVsync = true;
		bool preferIntegrated = false;
	};

	class DemoRenderer {
	public:
		DemoRenderer( DemoWindow& window, const RendererOptions& options );
		~DemoRenderer();
		DemoRenderer( const DemoRenderer& ) = delete;
		DemoRenderer& operator=( const DemoRenderer& ) = delete;

		const GpuContext& context() const { return context_; }
		bool isDiscreteGpu() const { return context_.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU; }
		uint32_t width() const { return extent_.width; }
		uint32_t height() const { return extent_.height; }
		/// Waits until the GPU is idle (before immediate uploads into buffers that frames in flight use).
		void waitIdle() const { vkDeviceWaitIdle( context_.device ); }

		/// The simulation to render, created with context(). Must be called before the first frame and when it changes.
		void setSimulation( FluidSimulation* simulation );
		/// Renders and presents a frame; recordSimulation records the simulation step at the beginning of the frame.
		void renderFrame( const DemoFrame& frame, const std::function<void(VkCommandBuffer)>& recordSimulation );
		/// GPU times of the latest finished frame.
		const GpuTimings& timings() const { return timings_; }

	private:
		struct FrameResources {
			VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
			VkSemaphore     imageAvailable = VK_NULL_HANDLE;
			VkFence         fence = VK_NULL_HANDLE;
			GpuBuffer       glyphs;
			VkDescriptorSet textSet = VK_NULL_HANDLE;
			bool            hasTimestamps = false;
		};
		static constexpr uint32_t FRAMES_IN_FLIGHT = 2;
		static constexpr uint32_t MAX_GLYPHS = 4096;
		static constexpr uint32_t SHADOW_MAP_SIZE = 2048;

		DemoWindow&                    window_;
		RendererOptions                options_;
		VkInstance                     instance_ = VK_NULL_HANDLE;
		VkSurfaceKHR                   surface_ = VK_NULL_HANDLE;
		GpuContext                     context_;
		VkSwapchainKHR                 swapchain_ = VK_NULL_HANDLE;
		VkFormat                       swapchainFormat_ = VK_FORMAT_UNDEFINED;
		VkExtent2D                     extent_{};
		bool                           canScreenshot_ = false;
		bool                           isSwapchainStale_ = false;          ///< Out of date or suboptimal: recreated before the next frame
		std::vector<VkImage>           swapchainImages_;
		std::vector<VkImageView>       swapchainViews_;
		std::vector<VkSemaphore>       renderFinished_;
		FrameResources                 frames_[FRAMES_IN_FLIGHT];
		uint32_t                       frameIndex_ = 0;
		VkQueryPool                    queryPool_ = VK_NULL_HANDLE;
		GpuTimings                     timings_;

		GpuImage                       sceneColor_;
		GpuImage                       sceneDepth_;
		GpuImage                       hdr_;
		GpuImage                       shadowMap_;
		GpuImage                       fontAtlas_;
		GpuBuffer                      vertices_;
		GpuBuffer                      sceneParameters_;
		VkSampler                      linearSampler_ = VK_NULL_HANDLE;
		VkSampler                      nearestSampler_ = VK_NULL_HANDLE;
		uint32_t                       meshFirst_[4] = {};
		uint32_t                       meshCount_[4] = {};

		VkDescriptorSetLayout          sceneLayout_ = VK_NULL_HANDLE;
		VkDescriptorSetLayout          tonemapLayout_ = VK_NULL_HANDLE;
		VkDescriptorSetLayout          textLayout_ = VK_NULL_HANDLE;
		VkDescriptorPool               descriptorPool_ = VK_NULL_HANDLE;
		VkDescriptorSet                sceneSet_ = VK_NULL_HANDLE;
		VkDescriptorSet                tonemapSet_ = VK_NULL_HANDLE;

		Pipeline                       skyPipeline_;
		Pipeline                       opaquePipeline_;
		Pipeline                       shadowPipeline_;
		Pipeline                       glassBackPipeline_;
		Pipeline                       glassFrontPipeline_;
		Pipeline                       tonemapPipeline_;
		Pipeline                       textPipeline_;

		FluidSimulation*               simulation_ = nullptr;
		std::unique_ptr<FluidRenderer> fluidRenderer_;

		void createDevice();
		void createSwapchain();
		void destroySwapchain();
		void recreateSwapchain();
		void createTargets();
		void destroyTargets();
		void createSceneResources();
		void createPipelines();
		void updateSceneSet();
		void drawObjects( VkCommandBuffer commandBuffer, const DemoFrame& frame, const Pipeline& pipeline, bool isGlass, bool isShadow );
		void drawBodies( VkCommandBuffer commandBuffer, const Pipeline& pipeline, uint32_t flags );
		uint32_t writeText( FrameResources& frame, const std::vector<TextLine>& lines );
	};
}

#endif
