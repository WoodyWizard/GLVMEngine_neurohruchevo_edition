// This file is part of Game Loop Versatile Modules (GLVM)
// License: http://opensource.org/licenses/MIT

#include "DemoRenderer.hpp"
#include "Common/PngWriter.hpp"
#include "Fluid/FluidMeshes.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <stdexcept>

extern unsigned char fontAtlas_dat[];                                      ///< Engine font atlas, textures/fontAtlas.cpp (84 x 132 RGBA)

namespace GLVM::fluid::demo
{
	namespace
	{
		constexpr VkFormat HDR_FORMAT   = VK_FORMAT_R16G16B16A16_SFLOAT;
		constexpr VkFormat DEPTH_FORMAT = VK_FORMAT_D32_SFLOAT;
		const std::string  DEMO_SHADERS = "../fluidDemo/";                 ///< Relative to the fluid shader directory

		/// std140 mirror of SceneParams in scene_common.glsl.
		struct SceneParams {
			float viewProjection[16];
			float inverseViewProjection[16];
			float lightViewProjection[16];
			float cameraPosition[4];
			float sunDirection[4];
			float sunColor[4];
			float skyZenith[4];
			float skyHorizon[4];
			float groundColor[4];
			float tankMin[4];
			float tankMax[4];
			float waterAbsorption[4];
			float options[4];
		};

		struct ScenePush {
			float    model[16];
			float    color[4];
			uint32_t flags;
			uint32_t bodyType;
			uint32_t padding[2];
		};

		void setVec( float* destination, const Vec3& v, float w ) {
			destination[0] = v.x;
			destination[1] = v.y;
			destination[2] = v.z;
			destination[3] = w;
		}

		/// Glyph of the engine font atlas (order of CVulkanRenderer::glyphs), -1 if missing.
		int glyphIndex( char c ) {
			if ( c >= 'A' && c <= 'Z' ) return c - 'A';
			if ( c >= 'a' && c <= 'z' ) return 26 + (c - 'a');
			if ( c >= '0' && c <= '9' ) return 52 + (c - '0');
			const char* punctuation = ".,";
			for ( int i = 0; i < 2; ++i )
				if ( c == punctuation[i] ) return 62 + i;
			const char* symbols = "?!_$()+-/:;<>=[]\\";
			for ( int i = 0; symbols[i] != '\0'; ++i )
				if ( c == symbols[i] ) return 72 + i;
			return -1;
		}
	}

	DemoRenderer::DemoRenderer( DemoWindow& window, const RendererOptions& options ) : window_(window), options_(options) {
		createDevice();
		createSwapchain();
		createSceneResources();
		createTargets();
		createPipelines();
	}

	DemoRenderer::~DemoRenderer() {
		vkDeviceWaitIdle( context_.device );
		fluidRenderer_.reset();
		for ( Pipeline* pipeline : { &skyPipeline_, &opaquePipeline_, &shadowPipeline_, &glassBackPipeline_, &glassFrontPipeline_,
									 &tonemapPipeline_, &textPipeline_ } )
			destroyPipeline( context_, *pipeline );
		vkDestroyDescriptorPool( context_.device, descriptorPool_, nullptr );
		for ( VkDescriptorSetLayout layout : { sceneLayout_, tonemapLayout_, textLayout_ } )
			vkDestroyDescriptorSetLayout( context_.device, layout, nullptr );
		destroyTargets();
		destroyImage( context_, shadowMap_ );
		destroyImage( context_, fontAtlas_ );
		destroyBuffer( context_, vertices_ );
		destroyBuffer( context_, sceneParameters_ );
		vkDestroySampler( context_.device, linearSampler_, nullptr );
		vkDestroySampler( context_.device, nearestSampler_, nullptr );
		for ( FrameResources& frame : frames_ ) {
			destroyBuffer( context_, frame.glyphs );
			vkDestroySemaphore( context_.device, frame.imageAvailable, nullptr );
			vkDestroyFence( context_.device, frame.fence, nullptr );
		}
		vkDestroyQueryPool( context_.device, queryPool_, nullptr );
		destroySwapchain();
		vkDestroyCommandPool( context_.device, context_.commandPool, nullptr );
		vkDestroyDevice( context_.device, nullptr );
		vkDestroySurfaceKHR( instance_, surface_, nullptr );
		vkDestroyInstance( instance_, nullptr );
	}

	/*
	  ===================================================
	  Device and swapchain
	  ===================================================
	*/
	void DemoRenderer::createDevice() {
		VkApplicationInfo application{};
		application.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		application.pApplicationName = "GLVM fluid demo";
		application.apiVersion = VK_API_VERSION_1_3;
		const std::vector<const char*> extensions = DemoWindow::requiredInstanceExtensions();
		VkInstanceCreateInfo instanceInfo{};
		instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		instanceInfo.pApplicationInfo        = &application;
		instanceInfo.enabledExtensionCount   = (uint32_t)extensions.size();
		instanceInfo.ppEnabledExtensionNames = extensions.data();
		vkCheck( vkCreateInstance( &instanceInfo, nullptr, &instance_ ), "vkCreateInstance" );
		surface_ = window_.createSurface( instance_ );

		uint32_t count = 0;
		vkEnumeratePhysicalDevices( instance_, &count, nullptr );
		std::vector<VkPhysicalDevice> devices( count );
		vkEnumeratePhysicalDevices( instance_, &count, devices.data() );
		int bestScore = -1;
		for ( VkPhysicalDevice device : devices ) {
			VkPhysicalDeviceProperties properties;
			vkGetPhysicalDeviceProperties( device, &properties );
			if ( properties.apiVersion < VK_API_VERSION_1_3 )
				continue;
			uint32_t familyCount = 0;
			vkGetPhysicalDeviceQueueFamilyProperties( device, &familyCount, nullptr );
			std::vector<VkQueueFamilyProperties> families( familyCount );
			vkGetPhysicalDeviceQueueFamilyProperties( device, &familyCount, families.data() );
			for ( uint32_t family = 0; family < familyCount; ++family ) {
				VkBool32 canPresent = VK_FALSE;
				vkGetPhysicalDeviceSurfaceSupportKHR( device, family, surface_, &canPresent );
				const VkQueueFlags required = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT;
				if ( !canPresent || (families[family].queueFlags & required) != required )
					continue;
				const bool isDiscrete = properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
				const int score = (isDiscrete != options_.preferIntegrated) ? 2 : 1;
				if ( score > bestScore ) {
					bestScore = score;
					context_.physicalDevice = device;
					context_.queueFamily = family;
				}
				break;
			}
		}
		if ( bestScore < 0 )
			throw std::runtime_error( "no Vulkan 1.3 device can present to the window" );
		vkGetPhysicalDeviceProperties( context_.physicalDevice, &context_.properties );
		vkGetPhysicalDeviceMemoryProperties( context_.physicalDevice, &context_.memoryProperties );

		const float priority = 1.0f;
		VkDeviceQueueCreateInfo queueInfo{};
		queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueInfo.queueFamilyIndex = context_.queueFamily;
		queueInfo.queueCount       = 1;
		queueInfo.pQueuePriorities = &priority;
		VkPhysicalDeviceVulkan13Features features13{};
		features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
		features13.dynamicRendering = VK_TRUE;
		const char* deviceExtensions[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
		VkDeviceCreateInfo deviceInfo{};
		deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		deviceInfo.pNext                   = &features13;
		deviceInfo.queueCreateInfoCount    = 1;
		deviceInfo.pQueueCreateInfos       = &queueInfo;
		deviceInfo.enabledExtensionCount   = 1;
		deviceInfo.ppEnabledExtensionNames = deviceExtensions;
		vkCheck( vkCreateDevice( context_.physicalDevice, &deviceInfo, nullptr, &context_.device ), "vkCreateDevice" );
		vkGetDeviceQueue( context_.device, context_.queueFamily, 0, &context_.queue );

		VkCommandPoolCreateInfo poolInfo{};
		poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		poolInfo.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		poolInfo.queueFamilyIndex = context_.queueFamily;
		vkCheck( vkCreateCommandPool( context_.device, &poolInfo, nullptr, &context_.commandPool ), "vkCreateCommandPool" );
		context_.shaderDirectory = "../VKshaders/fluid/";

		for ( FrameResources& frame : frames_ ) {
			VkCommandBufferAllocateInfo allocateInfo{};
			allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
			allocateInfo.commandPool        = context_.commandPool;
			allocateInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
			allocateInfo.commandBufferCount = 1;
			vkCheck( vkAllocateCommandBuffers( context_.device, &allocateInfo, &frame.commandBuffer ), "vkAllocateCommandBuffers" );
			VkSemaphoreCreateInfo semaphoreInfo{};
			semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
			vkCheck( vkCreateSemaphore( context_.device, &semaphoreInfo, nullptr, &frame.imageAvailable ), "vkCreateSemaphore" );
			VkFenceCreateInfo fenceInfo{};
			fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
			fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
			vkCheck( vkCreateFence( context_.device, &fenceInfo, nullptr, &frame.fence ), "vkCreateFence" );
			frame.glyphs = createBuffer( context_, MAX_GLYPHS * 32, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, true );
		}
		VkQueryPoolCreateInfo queryInfo{};
		queryInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
		queryInfo.queryType  = VK_QUERY_TYPE_TIMESTAMP;
		queryInfo.queryCount = FRAMES_IN_FLIGHT * 8;
		vkCheck( vkCreateQueryPool( context_.device, &queryInfo, nullptr, &queryPool_ ), "vkCreateQueryPool" );
	}

	void DemoRenderer::createSwapchain() {
		VkSurfaceCapabilitiesKHR capabilities;
		vkGetPhysicalDeviceSurfaceCapabilitiesKHR( context_.physicalDevice, surface_, &capabilities );
		extent_ = capabilities.currentExtent;
		if ( extent_.width == UINT32_MAX ) {
			extent_.width  = std::clamp( window_.width(), capabilities.minImageExtent.width, capabilities.maxImageExtent.width );
			extent_.height = std::clamp( window_.height(), capabilities.minImageExtent.height, capabilities.maxImageExtent.height );
		}

		uint32_t formatCount = 0;
		vkGetPhysicalDeviceSurfaceFormatsKHR( context_.physicalDevice, surface_, &formatCount, nullptr );
		std::vector<VkSurfaceFormatKHR> formats( formatCount );
		vkGetPhysicalDeviceSurfaceFormatsKHR( context_.physicalDevice, surface_, &formatCount, formats.data() );
		VkSurfaceFormatKHR format = formats[0];
		for ( const VkSurfaceFormatKHR& candidate : formats ) {
			if ( (candidate.format == VK_FORMAT_B8G8R8A8_SRGB || candidate.format == VK_FORMAT_R8G8B8A8_SRGB) &&
				 candidate.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR ) {
				format = candidate;
				break;
			}
		}
		swapchainFormat_ = format.format;

		uint32_t modeCount = 0;
		vkGetPhysicalDeviceSurfacePresentModesKHR( context_.physicalDevice, surface_, &modeCount, nullptr );
		std::vector<VkPresentModeKHR> modes( modeCount );
		vkGetPhysicalDeviceSurfacePresentModesKHR( context_.physicalDevice, surface_, &modeCount, modes.data() );
		VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
		if ( !options_.isVsync ) {
			for ( VkPresentModeKHR preferred : { VK_PRESENT_MODE_MAILBOX_KHR, VK_PRESENT_MODE_IMMEDIATE_KHR } )
				if ( std::find( modes.begin(), modes.end(), preferred ) != modes.end() ) {
					presentMode = preferred;
					break;
				}
		}

		canScreenshot_ = (capabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) != 0;
		VkSwapchainCreateInfoKHR swapchainInfo{};
		swapchainInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		swapchainInfo.surface          = surface_;
		swapchainInfo.minImageCount    = std::min( capabilities.minImageCount + 1, capabilities.maxImageCount > 0 ? capabilities.maxImageCount : 8u );
		swapchainInfo.imageFormat      = format.format;
		swapchainInfo.imageColorSpace  = format.colorSpace;
		swapchainInfo.imageExtent      = extent_;
		swapchainInfo.imageArrayLayers = 1;
		swapchainInfo.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | (canScreenshot_ ? VK_IMAGE_USAGE_TRANSFER_SRC_BIT : 0u);
		swapchainInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		swapchainInfo.preTransform     = capabilities.currentTransform;
		swapchainInfo.compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		swapchainInfo.presentMode      = presentMode;
		swapchainInfo.clipped          = VK_TRUE;
		vkCheck( vkCreateSwapchainKHR( context_.device, &swapchainInfo, nullptr, &swapchain_ ), "vkCreateSwapchainKHR" );

		uint32_t imageCount = 0;
		vkGetSwapchainImagesKHR( context_.device, swapchain_, &imageCount, nullptr );
		swapchainImages_.resize( imageCount );
		vkGetSwapchainImagesKHR( context_.device, swapchain_, &imageCount, swapchainImages_.data() );
		for ( VkImage image : swapchainImages_ ) {
			VkImageViewCreateInfo viewInfo{};
			viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			viewInfo.image            = image;
			viewInfo.viewType         = VK_IMAGE_VIEW_TYPE_2D;
			viewInfo.format           = swapchainFormat_;
			viewInfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
			VkImageView view = VK_NULL_HANDLE;
			vkCheck( vkCreateImageView( context_.device, &viewInfo, nullptr, &view ), "vkCreateImageView" );
			swapchainViews_.push_back( view );
			VkSemaphoreCreateInfo semaphoreInfo{};
			semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
			VkSemaphore semaphore = VK_NULL_HANDLE;
			vkCheck( vkCreateSemaphore( context_.device, &semaphoreInfo, nullptr, &semaphore ), "vkCreateSemaphore" );
			renderFinished_.push_back( semaphore );
		}
	}

	void DemoRenderer::destroySwapchain() {
		for ( VkImageView view : swapchainViews_ )
			vkDestroyImageView( context_.device, view, nullptr );
		for ( VkSemaphore semaphore : renderFinished_ )
			vkDestroySemaphore( context_.device, semaphore, nullptr );
		swapchainViews_.clear();
		renderFinished_.clear();
		swapchainImages_.clear();
		if ( swapchain_ != VK_NULL_HANDLE )
			vkDestroySwapchainKHR( context_.device, swapchain_, nullptr );
		swapchain_ = VK_NULL_HANDLE;
	}

	void DemoRenderer::recreateSwapchain() {
		vkDeviceWaitIdle( context_.device );
		destroySwapchain();
		destroyTargets();
		createSwapchain();
		createTargets();
		isSwapchainStale_ = false;
	}

	void DemoRenderer::createTargets() {
		sceneColor_ = createImage( context_, extent_.width, extent_.height, HDR_FORMAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT );
		sceneDepth_ = createImage( context_, extent_.width, extent_.height, DEPTH_FORMAT,
								   VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT );
		hdr_        = createImage( context_, extent_.width, extent_.height, HDR_FORMAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT );
		if ( tonemapSet_ != VK_NULL_HANDLE )
			updateSet( context_, tonemapSet_, { imageWrite( 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, hdr_.view, linearSampler_,
															VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL ) } );
		if ( fluidRenderer_ )
			fluidRenderer_->setTargets( sceneColor_.view, sceneDepth_.view, extent_.width, extent_.height );
	}

	void DemoRenderer::destroyTargets() {
		destroyImage( context_, sceneColor_ );
		destroyImage( context_, sceneDepth_ );
		destroyImage( context_, hdr_ );
	}

	/*
	  ===================================================
	  Scene resources and pipelines
	  ===================================================
	*/
	void DemoRenderer::createSceneResources() {
		std::vector<float> vertices;
		const MeshKind kinds[4] = { MeshKind::CUBE, MeshKind::SPHERE, MeshKind::FLOOR, MeshKind::OPEN_BOX };
		for ( MeshKind kind : kinds ) {
			const size_t first = vertices.size() / 8;
			if ( kind == MeshKind::CUBE || kind == MeshKind::OPEN_BOX ) {
				appendBoxMesh( vertices, kind == MeshKind::OPEN_BOX );
			} else if ( kind == MeshKind::SPHERE ) {
				appendSphereMesh( vertices, 32, 20 );
			} else {
				appendFloorMesh( vertices );
			}
			meshFirst_[(int)kind] = (uint32_t)first;
			meshCount_[(int)kind] = (uint32_t)(vertices.size() / 8 - first);
		}
		vertices_ = createBuffer( context_, vertices.size() * 4, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, false );
		uploadToBuffer( context_, vertices_, vertices.data(), vertices.size() * 4 );
		sceneParameters_ = createBuffer( context_, sizeof(SceneParams), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, false );
		linearSampler_  = createSampler( context_, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE );
		nearestSampler_ = createSampler( context_, VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE );
		shadowMap_ = createImage( context_, SHADOW_MAP_SIZE, SHADOW_MAP_SIZE, DEPTH_FORMAT,
								  VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT );

		/// Font atlas of the engine.
		fontAtlas_ = createImage( context_, 84, 132, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT );
		GpuBuffer staging = createBuffer( context_, 84 * 132 * 4, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, true );
		std::memcpy( staging.mapped, fontAtlas_dat, 84 * 132 * 4 );
		context_.submitImmediate( [&]( VkCommandBuffer commandBuffer ) {
			imageBarrier( commandBuffer, fontAtlas_.image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
						  VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT );
			VkBufferImageCopy region{};
			region.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
			region.imageExtent      = { 84, 132, 1 };
			vkCmdCopyBufferToImage( commandBuffer, staging.buffer, fontAtlas_.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region );
			imageBarrier( commandBuffer, fontAtlas_.image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
						  VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT );
		});
		destroyBuffer( context_, staging );

		const VkShaderStageFlags graphics = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
		const VkDescriptorType sampled = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		sceneLayout_ = createSetLayout( context_, { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
													VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, sampled, sampled, sampled }, graphics );
		tonemapLayout_ = createSetLayout( context_, { sampled }, VK_SHADER_STAGE_FRAGMENT_BIT );
		textLayout_ = createSetLayout( context_, { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, sampled }, graphics );
		descriptorPool_ = createDescriptorPool( context_, 8 );
		sceneSet_   = allocateSet( context_, descriptorPool_, sceneLayout_ );
		tonemapSet_ = allocateSet( context_, descriptorPool_, tonemapLayout_ );
		for ( FrameResources& frame : frames_ ) {
			frame.textSet = allocateSet( context_, descriptorPool_, textLayout_ );
			updateSet( context_, frame.textSet, { bufferWrite( 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, frame.glyphs ),
												  imageWrite( 1, sampled, fontAtlas_.view, nearestSampler_, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL ) } );
		}
	}

	void DemoRenderer::createPipelines() {
		GraphicsPipelineDescription sky;
		sky.vertexShader   = DEMO_SHADERS + "fullscreen.vert.spv";
		sky.fragmentShader = DEMO_SHADERS + "sky.frag.spv";
		sky.setLayouts     = { sceneLayout_ };
		sky.pushConstantSize = sizeof(ScenePush);
		sky.colorFormats   = { HDR_FORMAT };
		sky.depthFormat    = DEPTH_FORMAT;
		skyPipeline_ = createGraphicsPipeline( context_, sky );

		GraphicsPipelineDescription opaque;
		opaque.vertexShader     = DEMO_SHADERS + "scene.vert.spv";
		opaque.fragmentShader   = DEMO_SHADERS + "scene.frag.spv";
		opaque.setLayouts       = { sceneLayout_ };
		opaque.pushConstantSize = sizeof(ScenePush);
		opaque.colorFormats     = { HDR_FORMAT };
		opaque.depthFormat      = DEPTH_FORMAT;
		opaque.depthTest        = true;
		opaque.depthWrite       = true;
		opaque.depthCompare     = VK_COMPARE_OP_LESS;
		opaque.cullMode         = VK_CULL_MODE_BACK_BIT;
		opaquePipeline_ = createGraphicsPipeline( context_, opaque );

		GraphicsPipelineDescription shadow = opaque;
		shadow.fragmentShader = "";
		shadow.colorFormats   = {};
		shadow.cullMode       = VK_CULL_MODE_NONE;
		shadow.depthBias      = true;
		shadowPipeline_ = createGraphicsPipeline( context_, shadow );

		GraphicsPipelineDescription glass = opaque;
		glass.fragmentShader = DEMO_SHADERS + "glass.frag.spv";
		glass.blendModes     = { BlendMode::ALPHA };
		glass.depthWrite     = false;
		glass.depthCompare   = VK_COMPARE_OP_LESS_OR_EQUAL;
		glass.cullMode       = VK_CULL_MODE_FRONT_BIT;
		glassBackPipeline_ = createGraphicsPipeline( context_, glass );
		glass.cullMode       = VK_CULL_MODE_BACK_BIT;
		glassFrontPipeline_ = createGraphicsPipeline( context_, glass );

		GraphicsPipelineDescription tonemap;
		tonemap.vertexShader     = DEMO_SHADERS + "fullscreen.vert.spv";
		tonemap.fragmentShader   = DEMO_SHADERS + "tonemap.frag.spv";
		tonemap.setLayouts       = { tonemapLayout_ };
		tonemap.pushConstantSize = 4;
		tonemap.colorFormats     = { swapchainFormat_ };
		tonemapPipeline_ = createGraphicsPipeline( context_, tonemap );

		GraphicsPipelineDescription text;
		text.vertexShader     = DEMO_SHADERS + "text.vert.spv";
		text.fragmentShader   = DEMO_SHADERS + "text.frag.spv";
		text.setLayouts       = { textLayout_ };
		text.pushConstantSize = 8;
		text.colorFormats     = { swapchainFormat_ };
		text.blendModes       = { BlendMode::ALPHA };
		textPipeline_ = createGraphicsPipeline( context_, text );

		updateSet( context_, tonemapSet_, { imageWrite( 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, hdr_.view, linearSampler_,
														VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL ) } );
	}

	void DemoRenderer::setSimulation( FluidSimulation* simulation ) {
		vkDeviceWaitIdle( context_.device );
		simulation_ = simulation;
		fluidRenderer_.reset();
		if ( simulation_ == nullptr )
			return;
		fluidRenderer_ = std::make_unique<FluidRenderer>( context_, *simulation_, HDR_FORMAT, 1024 );
		fluidRenderer_->setTargets( sceneColor_.view, sceneDepth_.view, extent_.width, extent_.height );
		updateSceneSet();
	}

	void DemoRenderer::updateSceneSet() {
		const VkDescriptorType sampled = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		updateSet( context_, sceneSet_, {
			bufferWrite( 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, sceneParameters_ ),
			bufferWrite( 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, vertices_ ),
			bufferWrite( 2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, simulation_->bodyBuffer() ),
			imageWrite( 3, sampled, shadowMap_.view, nearestSampler_, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL ),
			imageWrite( 4, sampled, fluidRenderer_->lightThicknessView(), linearSampler_, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL ),
			imageWrite( 5, sampled, fluidRenderer_->lightFrontDepthView(), nearestSampler_, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL ),
		});
	}

	/*
	  ===================================================
	  Frame
	  ===================================================
	*/
	void DemoRenderer::drawObjects( VkCommandBuffer commandBuffer, const DemoFrame& frame, const Pipeline& pipeline, bool isGlass, bool isShadow ) {
		vkCmdBindPipeline( commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.pipeline );
		vkCmdBindDescriptorSets( commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.layout, 0, 1, &sceneSet_, 0, nullptr );
		for ( const SceneObject& object : frame.objects ) {
			if ( object.isGlass != isGlass || (isShadow && !object.castsShadow) )
				continue;
			ScenePush push = {};
			std::memcpy( push.model, object.model.m, sizeof(push.model) );
			std::memcpy( push.color, object.color, sizeof(push.color) );
			push.flags = isShadow ? 2u : 0u;
			vkCmdPushConstants( commandBuffer, pipeline.layout, VK_SHADER_STAGE_ALL, 0, sizeof(push), &push );
			vkCmdDraw( commandBuffer, meshCount_[(int)object.mesh], 1, meshFirst_[(int)object.mesh], 0 );
		}
	}

	void DemoRenderer::drawBodies( VkCommandBuffer commandBuffer, const Pipeline& pipeline, uint32_t flags ) {
		if ( simulation_ == nullptr || simulation_->bodyCount() == 0 )
			return;
		vkCmdBindPipeline( commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.pipeline );
		vkCmdBindDescriptorSets( commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.layout, 0, 1, &sceneSet_, 0, nullptr );
		for ( uint32_t type = 1; type <= 2; ++type ) {
			ScenePush push = {};
			push.flags = 1u | flags;
			push.bodyType = type;
			vkCmdPushConstants( commandBuffer, pipeline.layout, VK_SHADER_STAGE_ALL, 0, sizeof(push), &push );
			const MeshKind mesh = type == 1 ? MeshKind::SPHERE : MeshKind::CUBE;
			vkCmdDraw( commandBuffer, meshCount_[(int)mesh], simulation_->bodyCount(), meshFirst_[(int)mesh], 0 );
		}
	}

	uint32_t DemoRenderer::writeText( FrameResources& frame, const std::vector<TextLine>& lines ) {
		float* glyphs = static_cast<float*>(frame.glyphs.mapped);
		uint32_t count = 0;
		/// A dark copy one font pixel down and right first: the text stays readable over the bright scene.
		for ( int layer = 0; layer < 2; ++layer )
			for ( const TextLine& line : lines ) {
				const float offset = layer == 0 ? line.scale : 0.0f;
				const float shadow[4] = { 0.0f, 0.0f, 0.0f, line.color[3] * 0.7f };
				float x = line.x + offset;
				for ( char c : line.text ) {
					const int glyph = glyphIndex( c );
					if ( glyph >= 0 && count < MAX_GLYPHS ) {
						float* g = glyphs + count * 8;
						g[0] = x;
						g[1] = line.y + offset;
						g[2] = line.scale;
						g[3] = (float)glyph;
						std::memcpy( g + 4, layer == 0 ? shadow : line.color, 16 );
						++count;
					}
					x += 7.0f * line.scale;
				}
			}
		return count;
	}

	void DemoRenderer::renderFrame( const DemoFrame& frame, const std::function<void(VkCommandBuffer)>& recordSimulation ) {
		/// A minimized window has no surface area (no frame), a new size needs a new swapchain. Wayland surfaces have no
		/// size of their own: the window size is used.
		VkSurfaceCapabilitiesKHR capabilities;
		vkGetPhysicalDeviceSurfaceCapabilitiesKHR( context_.physicalDevice, surface_, &capabilities );
		VkExtent2D surfaceExtent = capabilities.currentExtent;
		if ( surfaceExtent.width == UINT32_MAX )
			surfaceExtent = { window_.width(), window_.height() };
		if ( surfaceExtent.width == 0 || surfaceExtent.height == 0 )
			return;
		if ( isSwapchainStale_ || surfaceExtent.width != extent_.width || surfaceExtent.height != extent_.height )
			recreateSwapchain();

		FrameResources& resources = frames_[frameIndex_];
		vkCheck( vkWaitForFences( context_.device, 1, &resources.fence, VK_TRUE, UINT64_MAX ), "vkWaitForFences" );
		if ( resources.hasTimestamps ) {
			uint64_t stamps[5] = {};
			if ( vkGetQueryPoolResults( context_.device, queryPool_, frameIndex_ * 8, 5, sizeof(stamps), stamps, sizeof(uint64_t),
										VK_QUERY_RESULT_64_BIT ) == VK_SUCCESS ) {
				const double toMilliseconds = context_.properties.limits.timestampPeriod * 1e-6;
				timings_.simulation = (float)((stamps[1] - stamps[0]) * toMilliseconds);
				timings_.scene      = (float)((stamps[2] - stamps[1]) * toMilliseconds);
				timings_.fluid      = (float)((stamps[3] - stamps[2]) * toMilliseconds);
				timings_.total      = (float)((stamps[4] - stamps[0]) * toMilliseconds);
			}
		}

		uint32_t imageIndex = 0;
		VkResult acquired = vkAcquireNextImageKHR( context_.device, swapchain_, UINT64_MAX, resources.imageAvailable, VK_NULL_HANDLE, &imageIndex );
		if ( acquired == VK_ERROR_OUT_OF_DATE_KHR ) {
			isSwapchainStale_ = true;
			return;
		}
		if ( acquired != VK_SUCCESS && acquired != VK_SUBOPTIMAL_KHR )
			vkCheck( acquired, "vkAcquireNextImageKHR" );
		vkResetFences( context_.device, 1, &resources.fence );

		VkCommandBuffer commandBuffer = resources.commandBuffer;
		vkResetCommandBuffer( commandBuffer, 0 );
		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		vkCheck( vkBeginCommandBuffer( commandBuffer, &beginInfo ), "vkBeginCommandBuffer" );
		/// Frames share the render targets and the simulation buffers: the previous frame finishes before this one starts.
		memoryBarrier( commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT,
					   VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT );
		vkCmdResetQueryPool( commandBuffer, queryPool_, frameIndex_ * 8, 8 );
		vkCmdWriteTimestamp( commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, queryPool_, frameIndex_ * 8 + 0 );

		recordSimulation( commandBuffer );
		vkCmdWriteTimestamp( commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, queryPool_, frameIndex_ * 8 + 1 );

		/// Cameras: the viewer and the sun (orthographic box around the tank).
		const float aspect = (float)extent_.width / (float)extent_.height;
		const Mat4 view = Mat4::lookAt( frame.eye, frame.target, { 0.0f, 1.0f, 0.0f } );
		const Mat4 projection = Mat4::perspective( frame.verticalFov, aspect, 0.05f, 60.0f );
		const Mat4 viewProjection = projection * view;
		const Vec3 tankCenter = (frame.tankMin + frame.tankMax) * 0.5f;
		const Vec3 sun = normalize( frame.light.sunDirection );
		const Mat4 lightView = Mat4::lookAt( tankCenter + sun * 8.0f, tankCenter, std::fabs( sun.y ) > 0.99f ? Vec3{ 0, 0, 1 } : Vec3{ 0, 1, 0 } );
		const Mat4 lightProjection = Mat4::orthographic( -2.6f, 2.6f, -2.6f, 2.6f, 1.0f, 16.0f );

		SceneParams scene = {};
		std::memcpy( scene.viewProjection, viewProjection.m, 64 );
		std::memcpy( scene.inverseViewProjection, viewProjection.inverse().m, 64 );
		std::memcpy( scene.lightViewProjection, (lightProjection * lightView).m, 64 );
		setVec( scene.cameraPosition, frame.eye, frame.time );
		setVec( scene.sunDirection, sun, frame.light.sunIntensity );
		setVec( scene.sunColor, frame.light.sunColor, 0.0f );
		setVec( scene.skyZenith, frame.light.skyZenith, 0.0f );
		setVec( scene.skyHorizon, frame.light.skyHorizon, 0.0f );
		setVec( scene.groundColor, frame.light.groundColor, 0.0f );
		setVec( scene.tankMin, frame.tankMin, 0.0f );
		setVec( scene.tankMax, frame.tankMax, 0.0f );
		setVec( scene.waterAbsorption, frame.appearance.absorption * 3.0f, 3.0f );
		scene.options[0] = 1.0f / SHADOW_MAP_SIZE;
		scene.options[1] = frame.isWaterShadow ? 1.0f : 0.0f;
		scene.options[2] = frame.exposure;
		vkCmdUpdateBuffer( commandBuffer, sceneParameters_.buffer, 0, sizeof(scene), &scene );
		memoryBarrier( commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
					   VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_UNIFORM_READ_BIT );

		FluidFrame fluidFrame;
		fluidFrame.view = view;
		fluidFrame.projection = projection;
		fluidFrame.cameraPosition = frame.eye;
		fluidFrame.nearPlane = 0.05f;
		fluidFrame.farPlane = 60.0f;
		fluidFrame.verticalFov = frame.verticalFov;
		fluidFrame.light = frame.light;
		fluidFrame.appearance = frame.appearance;
		fluidFrame.mode = frame.mode;
		fluidFrame.lightView = lightView;
		fluidFrame.lightProjection = lightProjection;
		fluidFrame.time = frame.time;
		fluidRenderer_->update( commandBuffer, fluidFrame );
		fluidRenderer_->recordLightThickness( commandBuffer );

		/// Sun shadow map of the opaque objects.
		const VkPipelineStageFlags depthTests = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		const VkAccessFlags depthAccess = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		const VkPipelineStageFlags color = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		const VkPipelineStageFlags fragment = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		imageBarrier( commandBuffer, shadowMap_.image, VK_IMAGE_ASPECT_DEPTH_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
					  fragment, 0, depthTests, depthAccess );
		Attachment shadowDepth;
		shadowDepth.view = shadowMap_.view;
		shadowDepth.clearValue.depthStencil = { 1.0f, 0 };
		beginRendering( commandBuffer, shadowMap_.extent, {}, &shadowDepth );
		vkCmdSetDepthBias( commandBuffer, 1.5f, 0.0f, 2.0f );
		drawObjects( commandBuffer, frame, shadowPipeline_, false, true );
		drawBodies( commandBuffer, shadowPipeline_, 2u );
		vkCmdEndRendering( commandBuffer );
		imageBarrier( commandBuffer, shadowMap_.image, VK_IMAGE_ASPECT_DEPTH_BIT, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
					  depthTests, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, fragment, VK_ACCESS_SHADER_READ_BIT );

		/// Scene: sky, opaque objects, bodies, back faces of the glass (they are seen through the fluid).
		imageBarrier( commandBuffer, sceneColor_.image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
					  fragment, 0, color, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT );
		imageBarrier( commandBuffer, sceneDepth_.image, VK_IMAGE_ASPECT_DEPTH_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
					  fragment | depthTests, 0, depthTests, depthAccess );
		Attachment sceneColor, sceneDepth;
		sceneColor.view = sceneColor_.view;
		sceneDepth.view = sceneDepth_.view;
		sceneDepth.clearValue.depthStencil = { 1.0f, 0 };
		beginRendering( commandBuffer, extent_, { sceneColor }, &sceneDepth );
		vkCmdBindPipeline( commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, skyPipeline_.pipeline );
		vkCmdBindDescriptorSets( commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, skyPipeline_.layout, 0, 1, &sceneSet_, 0, nullptr );
		vkCmdDraw( commandBuffer, 3, 1, 0, 0 );
		drawObjects( commandBuffer, frame, opaquePipeline_, false, false );
		drawBodies( commandBuffer, opaquePipeline_, 0u );
		drawObjects( commandBuffer, frame, glassBackPipeline_, true, false );
		vkCmdEndRendering( commandBuffer );
		imageBarrier( commandBuffer, sceneColor_.image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
					  color, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, fragment, VK_ACCESS_SHADER_READ_BIT );
		imageBarrier( commandBuffer, sceneDepth_.image, VK_IMAGE_ASPECT_DEPTH_BIT, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
					  depthTests, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, fragment | depthTests,
					  VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT );
		vkCmdWriteTimestamp( commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, queryPool_, frameIndex_ * 8 + 2 );

		/// Fluid over the scene into the HDR target, then the front faces of the glass.
		imageBarrier( commandBuffer, hdr_.image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
					  fragment, 0, color, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT );
		fluidRenderer_->recordFluid( commandBuffer, hdr_.view );
		memoryBarrier( commandBuffer, color, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, color, VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT );
		Attachment hdr;
		hdr.view = hdr_.view;
		hdr.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
		Attachment readOnlyDepth;
		readOnlyDepth.view = sceneDepth_.view;
		readOnlyDepth.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
		readOnlyDepth.storeOp = VK_ATTACHMENT_STORE_OP_NONE;
		readOnlyDepth.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
		beginRendering( commandBuffer, extent_, { hdr }, &readOnlyDepth );
		drawObjects( commandBuffer, frame, glassFrontPipeline_, true, false );
		vkCmdEndRendering( commandBuffer );
		imageBarrier( commandBuffer, hdr_.image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
					  color, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, fragment, VK_ACCESS_SHADER_READ_BIT );
		vkCmdWriteTimestamp( commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, queryPool_, frameIndex_ * 8 + 3 );

		/// Tone mapping and text into the swapchain image.
		const VkImage swapchainImage = swapchainImages_[imageIndex];
		imageBarrier( commandBuffer, swapchainImage, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
					  color, 0, color, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT );
		Attachment output;
		output.view = swapchainViews_[imageIndex];
		output.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		beginRendering( commandBuffer, extent_, { output }, nullptr );
		vkCmdBindPipeline( commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, tonemapPipeline_.pipeline );
		vkCmdBindDescriptorSets( commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, tonemapPipeline_.layout, 0, 1, &tonemapSet_, 0, nullptr );
		vkCmdPushConstants( commandBuffer, tonemapPipeline_.layout, VK_SHADER_STAGE_ALL, 0, 4, &frame.exposure );
		vkCmdDraw( commandBuffer, 3, 1, 0, 0 );
		const uint32_t glyphCount = writeText( resources, frame.text );
		if ( glyphCount > 0 ) {
			vkCmdBindPipeline( commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, textPipeline_.pipeline );
			vkCmdBindDescriptorSets( commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, textPipeline_.layout, 0, 1, &resources.textSet, 0, nullptr );
			const float screenSize[2] = { (float)extent_.width, (float)extent_.height };
			vkCmdPushConstants( commandBuffer, textPipeline_.layout, VK_SHADER_STAGE_ALL, 0, 8, screenSize );
			vkCmdDraw( commandBuffer, 6, glyphCount, 0, 0 );
		}
		vkCmdEndRendering( commandBuffer );
		vkCmdWriteTimestamp( commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, queryPool_, frameIndex_ * 8 + 4 );
		resources.hasTimestamps = true;

		const bool isScreenshot = !frame.screenshotPath.empty() && canScreenshot_;
		GpuBuffer readback;
		if ( isScreenshot ) {
			readback = createBuffer( context_, (VkDeviceSize)extent_.width * extent_.height * 4, VK_BUFFER_USAGE_TRANSFER_DST_BIT, true );
			imageBarrier( commandBuffer, swapchainImage, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
						  color, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_READ_BIT );
			VkBufferImageCopy region{};
			region.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
			region.imageExtent      = { extent_.width, extent_.height, 1 };
			vkCmdCopyImageToBuffer( commandBuffer, swapchainImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, readback.buffer, 1, &region );
			imageBarrier( commandBuffer, swapchainImage, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
						  VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0 );
		} else {
			imageBarrier( commandBuffer, swapchainImage, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
						  color, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0 );
		}
		vkCheck( vkEndCommandBuffer( commandBuffer ), "vkEndCommandBuffer" );

		const VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.waitSemaphoreCount   = 1;
		submitInfo.pWaitSemaphores      = &resources.imageAvailable;
		submitInfo.pWaitDstStageMask    = &waitStage;
		submitInfo.commandBufferCount   = 1;
		submitInfo.pCommandBuffers      = &commandBuffer;
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores    = &renderFinished_[imageIndex];
		vkCheck( vkQueueSubmit( context_.queue, 1, &submitInfo, resources.fence ), "vkQueueSubmit" );

		VkPresentInfoKHR presentInfo{};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores    = &renderFinished_[imageIndex];
		presentInfo.swapchainCount     = 1;
		presentInfo.pSwapchains        = &swapchain_;
		presentInfo.pImageIndices      = &imageIndex;
		const VkResult presented = vkQueuePresentKHR( context_.queue, &presentInfo );
		if ( presented == VK_ERROR_OUT_OF_DATE_KHR || presented == VK_SUBOPTIMAL_KHR ) {
			isSwapchainStale_ = true;                                        ///< Recreated at the next frame (if the window has a size)
		} else if ( presented != VK_SUCCESS ) {
			vkCheck( presented, "vkQueuePresentKHR" );
		}

		if ( isScreenshot ) {
			vkCheck( vkWaitForFences( context_.device, 1, &resources.fence, VK_TRUE, UINT64_MAX ), "vkWaitForFences" );
			const uint8_t* pixels = static_cast<const uint8_t*>(readback.mapped);
			const bool isBgra = swapchainFormat_ == VK_FORMAT_B8G8R8A8_SRGB || swapchainFormat_ == VK_FORMAT_B8G8R8A8_UNORM;
			std::vector<uint8_t> rgb( (size_t)extent_.width * extent_.height * 3 );
			for ( size_t i = 0; i < (size_t)extent_.width * extent_.height; ++i ) {
				rgb[i * 3 + 0] = pixels[i * 4 + (isBgra ? 2 : 0)];
				rgb[i * 3 + 1] = pixels[i * 4 + 1];
				rgb[i * 3 + 2] = pixels[i * 4 + (isBgra ? 0 : 2)];
			}
			if ( core::writePng( frame.screenshotPath, extent_.width, extent_.height, rgb ) )
				std::printf( "Screenshot: %s\n", frame.screenshotPath.c_str() );
			destroyBuffer( context_, readback );
		}
		frameIndex_ = (frameIndex_ + 1) % FRAMES_IN_FLIGHT;
	}
}
