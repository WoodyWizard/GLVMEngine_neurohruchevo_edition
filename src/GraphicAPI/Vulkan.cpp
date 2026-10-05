// This file is part of Game Loop Versatile Modules (GLVM)
// Copyright © 2024 Maksim Manokhin a.k.a. Yuriorkis_Scream. Contacts: <fellfrostqtw@gmail.com>
// Author: Maksim Manokhin a.k.a. Yuriorkis_Scream
// License: http://opensource.org/licenses/MIT

#include "Archetypes/CrosshairArchetype.hpp"
#include "ComponentManager.hpp"
#include "GraphicAPI/Vulkan.hpp"
#include "Components/ActorComponent.hpp"
#include "Components/AnimationMoveComponent.hpp"
#include "Components/ColliderComponent.hpp"
#include "Components/ControllerComponent.hpp"
#include "Components/HealthComponent.hpp"
#include "Components/InventoryComponent.hpp"
#include "Components/InventorySlotComponent.hpp"
#include "Components/ItemComponent.hpp"
#include "Components/MaterialComponent.hpp"
#include "Components/InterfaceComponent.hpp"
#include "Components/PointLightComponent.hpp"
#include "Components/TransformComponent.hpp"
#include "Components/VertexComponent.hpp"
#include "Components/ViewComponent.hpp"
#include "Components/CrosshairComponent.hpp"
#include "EntityManager.hpp"
#include "GraphicAPI/RenderConfig.hpp"
#include "Common/PngWriter.hpp"
#include "Fluid/FluidTank.hpp"
#include "GraphicAPI/RenderData.hpp"
#include "PGA.hpp"
#include "ShaderStructs.hpp"
#include "Texture.hpp"
#include "ThreadPool.hpp"
#include "Vector.hpp"
#include "VertexMath.hpp"
#include "VkStructs.hpp"
#include "WavefrontObjParser.hpp"
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <algorithm>
#include <exception>
#include <string>
#include <thread>
#include <vulkan/vulkan_core.h>
#include <vulkan/vulkan_wayland.h>

namespace GLVM::core
{    
    CVulkanRenderer::CVulkanRenderer() {
    }
    
    CVulkanRenderer::~CVulkanRenderer() {
        cleanup();
    }
    
    void CVulkanRenderer::draw() {
		mainRenderDrawFrame();
    }

	u32 CVulkanRenderer::perFrameDescriptorNumber( DescriptorSetDataLink descriptorSetDataLink ) const {
		return descriptorSetsConfig[descriptorSetDataLink].hostDescriptorNumber / MAX_FRAMES_IN_FLIGHT;
	}

	/// Pointer to the uniform slot 'index' of the first (uniform buffer) binding of the descriptor set.
	/// Slots are uboChunkSize apart, the same stride that is used for descriptor buffer offsets.
	template<class T>
	T* CVulkanRenderer::mappedUBO( DescriptorSetDataLink descriptorSetDataLink, u32 index ) {
		const DescriptorBinding& binding = descriptorBindingsConfig[descriptorSetsConfig[descriptorSetDataLink].descriptorsBindingsIDs[0]];
		char* mappedData = static_cast<char*>(GPUDescriptors[binding.globalDescriptorOffset].GPUBuffer->mapedDataPtr);
		return reinterpret_cast<T*>(mappedData + index * binding.uboChunkSize);
	}

	/// Texture descriptor sets are stored per texture per frame in flight. Unknown texture ids fall back to
	/// texture 0 instead of indexing into a neighbouring descriptor set chunk.
	VkDescriptorSet* CVulkanRenderer::textureDescriptorSet( const DescriptorSet& descriptorSet, u32 textureID ) {
		if ( textureID >= initializeTextureData_.size() )
			textureID = 0;
		return descriptorSetsChunks.GetVectorContainer() + descriptorSet.descriptorSetOffset + MAX_FRAMES_IN_FLIGHT * textureID + currentFrame;
	}

	void CVulkanRenderer::reportUboOverflow( SpecificPipeline pipeline ) {
		const uint32_t pipelineBit = 1u << pipeline;
		if ( !(reportedUboOverflows.fetch_or(pipelineBit) & pipelineBit) )
			std::cerr << "Renderer: out of uniform slots for pipeline " << pipeline << ", extra draws are skipped" << std::endl;
	}

    void CVulkanRenderer::SetViewMatrix(mat4 _viewMatrix) {
        viewMatrix = _viewMatrix; // 
    }
    
    void CVulkanRenderer::SetProjectionMatrix(mat4 _projectionMatrix) {
        projectionMatrix = _projectionMatrix;
    }

	
    void CVulkanRenderer::createTextureImage() {
		uint32_t texWidth, texHeight;
		[[maybe_unused]] uint32_t texChannels;

		unsigned int readableTextureDescriptorBindingIndex = descriptorSetsConfig[DescriptorSetDataLink::RIDABLE_TEXTURES].descriptorsBindingsIDs[0];
		if ( initializeTextureData_.size() > MAX_TEXTURES ) {
			throw std::runtime_error("too many textures loaded, increase MAX_TEXTURES");
		}
        for(unsigned int i = 0; i < initializeTextureData_.size(); ++i)
        {
			VkDeviceSize imageSize{};
			unsigned char* pixels;
			[[maybe_unused]] const char* path_to_stb_image = nullptr;

			#ifndef STB_IMAGE_IMPLEMENTATION
            imageSize = initializeTextureData_[i].dat_length_;
            pixels = initializeTextureData_[i].u_iData_;
            texWidth = initializeTextureData_[i].iWidth_;
            texHeight = initializeTextureData_[i].iHeight_;
			#endif

			#ifdef STB_IMAGE_IMPLEMENTATION
			path_to_stb_image = initializeTextureData_[i].path_to_image;
			pixels = stbi_load(path_to_stb_image, reinterpret_cast<int*>(&texWidth), reinterpret_cast<int*>(&texHeight),
							   reinterpret_cast<int*>(&texChannels), STBI_rgb_alpha);
			imageSize = texWidth * texHeight * 4;
			#endif

			if (!pixels) {
                throw std::runtime_error("failed to load texture image!");
            }

            VkBuffer stagingBuffer;
            VkDeviceMemory stagingBufferMemory;
            createBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

            void* data;
            vkMapMemory(device, stagingBufferMemory, 0, imageSize, 0, &data);
            memcpy(data, pixels, static_cast<size_t>(imageSize));
            vkUnmapMemory(device, stagingBufferMemory);
			VK_Image textureImage = {
				.image = VkImage{},
				.deviceMemory = VkDeviceMemory{},
				.viewType = VK_IMAGE_VIEW_TYPE_2D,
				.createFlags  = 0,
				.memoryPropertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
				.usageFlags = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
				.aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT,
				.format = VK_FORMAT_R8G8B8A8_SRGB,
//				.format = VK_FORMAT_BC7_SRGB_BLOCK,
				.tiling = VK_IMAGE_TILING_OPTIMAL,
				.arrayLayers = 1,
				.width = texWidth,
				.height = texHeight
			};

            createImage(textureImage);

            transitionImageLayout(textureImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
            copyBufferToImage(stagingBuffer, textureImage.image, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));
            transitionImageLayout(textureImage.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

//			GPUDescriptors[descriptorBindingsConfig[readableTextureDescriptorBindingIndex].globalDescriptorOffset + i].GPUImage = new VK_Image;
			*GPUDescriptors[descriptorBindingsConfig[readableTextureDescriptorBindingIndex].globalDescriptorOffset + i].GPUImage = textureImage;
			
            vkDestroyBuffer(device, stagingBuffer, nullptr);
            vkFreeMemory(device, stagingBufferMemory, nullptr);
        }
    }

    void CVulkanRenderer::recreateSwapChain() {
#ifdef VK_USE_PLATFORM_XCB_KHR
		Window->configureWindow();
#endif

#ifdef VK_USE_PLATFORM_WIN32_KHR
		Window->configureWindow();
#endif
		/// A zero sized surface (minimized window) can't have a swapchain, retry on the next frame.
		const SwapChainSupportDetails swapChainSupport = querySwapChainSupport(physicalDevice);
		const VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities);
		if ( extent.width == 0 || extent.height == 0 ) {
			swapChainRecreatePending = true;
			return;
		}
		swapChainRecreatePending = false;

        vkDeviceWaitIdle(device);
        cleanupSwapChain();

		/// Only window size dependent resources are recreated. Shadow maps and descriptor sets don't depend on the swapchain.
        createSwapChain();
        createImageViews();
        createDepthResources();
		createFramebuffers();
		aspectRate = (float)swapChainExtent.width / (float)swapChainExtent.height;
		if ( fluidTank )
			fluidTank->setTargets( swapChainExtent.width, swapChainExtent.height, mainDepthImageView );

		/// Present semaphores are indexed by swapchain image, their number has to follow the image count.
		if ( renderFinishedSemaphores.size() != swapChainImages.size() ) {
			destroyRenderFinishedSemaphores();
			createRenderFinishedSemaphores();
		}
    }

    void CVulkanRenderer::SetMeshData(std::vector<const char*> _pathsArray, core::vector<const char*> pathsGLTF) {
        for (unsigned int i = 0; i < _pathsArray.size(); ++i)
            pathsArray_.push_back(_pathsArray[i]);

		for (unsigned int i = 0; i < pathsGLTF.GetSize(); ++i)
			pathsGLTF_.Push(pathsGLTF[i]);
    }
    
    void CVulkanRenderer::run() {
		VkConfigInitializer();
		descriptorSetBuilder();
		pipelineBuilder();
		renderPassesBuilder();

		renderThreadPool = new ThreadPool(3);
		startTime = std::chrono::steady_clock::now();      ///< For SDF pipeline

		if ( const char* screenshot = std::getenv("GLVM_SCREENSHOT") ) {
			const char* colon = std::strchr(screenshot, ':');
			if ( colon != nullptr && colon[1] != '\0' ) {
				screenshotFrame = std::atoll(screenshot);
				screenshotPath  = colon + 1;
			}
		}

        initWindow();
        initVulkan();
		mapUniformBuffers();
    }
    
    void CVulkanRenderer::initWindow() {
#ifdef VK_USE_PLATFORM_WAYLAND_KHR
		Window = initializeWaylandWindow();
			
		createWaylandSurfaceInfo.display = Window->display;
		createWaylandSurfaceInfo.surface = Window->wl_surface;
		aspectRate = (float)Window->width / (float)Window->height;

		if ( createWaylandSurfaceInfo.display == NULL )
			std::cout << "DISPLAY NULL" << std::endl;
		else if ( createWaylandSurfaceInfo.surface == NULL )
			std::cout << "SURFACE NULL" << std::endl;
		
		createWaylandSurfaceInfo.sType   = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR;
		createWaylandSurfaceInfo.pNext   = nullptr;
		createWaylandSurfaceInfo.flags   = 0;
#endif
		
#ifdef VK_USE_PLATFORM_XLIB_KHR
		Window = new GLVM::core::WindowXVulkan();
        createXlibSurfaceInfo.dpy = Window->GetDisplay();
        createXlibSurfaceInfo.window = Window->GetWindow();
		aspectRate = (float)Window->width / (float)Window->height;

        createXlibSurfaceInfo.sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR;
        createXlibSurfaceInfo.pNext = nullptr;
        createXlibSurfaceInfo.flags = 0;
#endif

#ifdef VK_USE_PLATFORM_XCB_KHR
		Window = new GLVM::core::WindowXCBVulkan();
		createXcbSurfaceInfo.window = Window->GetWindow();
		createXcbSurfaceInfo.connection = Window->GetConnection();
		aspectRate = (float)Window->width / (float)Window->height;

		createXcbSurfaceInfo.sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR;
		createXcbSurfaceInfo.pNext = nullptr;
		createXcbSurfaceInfo.flags = 0;
#endif
		
#ifdef VK_USE_PLATFORM_WIN32_KHR
		Window = new GLVM::core::WindowWinVulkan();
        createWin32SurfaceInfo.hwnd = Window->GetModernWindowHWND();
		aspectRate = (float)Window->width / (float)Window->height;
        
        createWin32SurfaceInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
        createWin32SurfaceInfo.pNext = nullptr;
        createWin32SurfaceInfo.flags = 0;
#endif
    }

	void CVulkanRenderer::initializeGameLevelVertices() {
		for ( unsigned int m = 0; m < levelGeneratedVertices.size(); ++m ) {
			aVertices_.push_back(levelGeneratedVertices[m]);
			aIndices_.push_back(levelGeneratedIndices[m]);
			jointMatricesPerMesh.Push({});
			frames.Push({});
			for( int i = 0; i < 64; ++i ) {
				frames[frames.GetSize() - 1].Push(0.0f);
			}
			int maximumJoints     = 64;
			core::vector<core::vector<mat4>> jointMatrices;
			for ( int i = 0; i < maximumJoints; ++i) {
				core::vector<mat4>  globalAllFrameNodeMatrix;
				int numberOfFrames = 64;
				for ( int j = 0; j < numberOfFrames; ++j ) {
					mat4 unitMatrix(1.0f);
					globalAllFrameNodeMatrix.Push(unitMatrix);
				}

				jointMatrices.Push(globalAllFrameNodeMatrix);
			}
			jointMatricesPerMesh[jointMatricesPerMesh.GetSize() - 1] = jointMatrices;
			
			uint32_t nextIndexGLTF = wavefrontObjCounter + gltfCounter + m;
		
			vertexBufferContainer.emplace_back();
			vertexBufferMemoryContainer.emplace_back();
			createVertexBuffer(vertexBufferContainer[nextIndexGLTF], vertexBufferMemoryContainer[nextIndexGLTF], aVertices_[nextIndexGLTF]);

			indexBufferContainer.emplace_back();
			indexBufferMemoryContaner.emplace_back();
			createIndexBuffer(indexBufferContainer[nextIndexGLTF], indexBufferMemoryContaner[nextIndexGLTF], aIndices_[nextIndexGLTF]);
		}
	}
	
    void CVulkanRenderer::initVulkan() {
        createInstance();
        setupDebugMessenger();
        createSurface();
        pickPhysicalDevice();
        createLogicalDevice();
        createSwapChain();
        createImageViews();
        createMainRenderPass();
		createDescriptorSetLayout();
		createGraphicsPipeline();
		createCommandPool(mainRenderCommandPool);
		const uint32_t secondaryBuffersCommandPoolsNumber = 3;
		secondaryBuffersCommandPools.resize(secondaryBuffersCommandPoolsNumber);
		for( uint32_t i = 0; i < secondaryBuffersCommandPools.size(); ++i ) {
			createCommandPool(secondaryBuffersCommandPools[i]);
		}
        createDepthResources();
		createShadowMapResources();
        createFramebuffers();
        createTextureImage();
        createTextureImageView();
        createTextureSampler();
		initializeVertexBuffersWithWavefrontData();
		initializeVertexBuffersWithGLTFData();
		initializeVertexBuffersWithFontData();
        initializeVertexBuffersWithMathObjectsData();
		
        createMainRenderUniformBuffers();
        createMainRenderDescriptorPool();
        createMainRenderDescriptorSets();
		vkDebugUtils::setDebugObjectNames( device, vertexBufferContainer, indexBufferContainer, GPUDescriptors,
											fontIndicesContainer, fontVertexBufferContainer, fontIndexBufferContainer);
        // createCommandBuffers(mainRenderCommandPool, directionalLightCommandBuffers);
		// createCommandBuffers(mainRenderCommandPool, spotLightCommandBuffers);
		// createCommandBuffers(mainRenderCommandPool, pointLightCommandBuffers);
		const uint32_t mainRenderCommandBuffersNumber = 1;
		createCommandBuffers(mainRenderCommandPool, mainRenderCommandBuffers,
							 mainRenderCommandBuffersNumber, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

		/// Secondary buffers are allocated for the maximum number of lights, the number of lights may change at runtime.
		createCommandBuffers(secondaryBuffersCommandPools[0], directionalLightSecondaryCommandBuffers,
							 DIRECTIONAL_LIGHTS_NUMBER, VK_COMMAND_BUFFER_LEVEL_SECONDARY);
		createCommandBuffers(secondaryBuffersCommandPools[1], spotLightSecondaryCommandBuffers,
							 SPOT_LIGHTS_NUMBER, VK_COMMAND_BUFFER_LEVEL_SECONDARY);
		createCommandBuffers(secondaryBuffersCommandPools[2], pointLightSecondaryCommandBuffers,
							 POINT_LIGHTS_NUMBER * CUBE_MAP_LAYER_NUMBER, VK_COMMAND_BUFFER_LEVEL_SECONDARY);
		// createSyncObjects(directionalLightShadowMapImageAvailableSemaphores,
		// 				  directionalLightShadowMapRenderFinishedSemaphores,
		// 				  directionalLightShadowMapInFlightFences);
		// createSyncObjects(spotLightShadowMapImageAvailableSemaphores,
		// 				  spotLightShadowMapRenderFinishedSemaphores,
		// 				  spotLightShadowMapInFlightFences);
		// createSyncObjects(pointLightShadowMapImageAvailableSemaphores,
		// 				  pointLightShadowMapRenderFinishedSemaphores,
		// 				  pointLightShadowMapInFlightFences);
        createSyncObjects(imageAvailableSemaphores, renderFinishedSemaphores, inFlightFences);
    }

	void CVulkanRenderer::initializeVertexBuffersWithWavefrontData() {
        for (unsigned int m = 0; m < pathsArray_.size(); ++m) {		
			vertexBufferContainer.emplace_back();
			vertexBufferMemoryContainer.emplace_back();
			createVertexBuffer(vertexBufferContainer[m], vertexBufferMemoryContainer[m], aVertices_[m]);

			indexBufferContainer.emplace_back();
			indexBufferMemoryContaner.emplace_back();
			createIndexBuffer(indexBufferContainer[m], indexBufferMemoryContaner[m], aIndices_[m]);
			++wavefrontObjCounter;
		}
	}

	void CVulkanRenderer::initializeVertexBuffersWithGLTFData() {
		for (unsigned int m = 0; m < pathsGLTF_.GetSize(); ++m) {
			uint32_t nextIndexGLTF = wavefrontObjCounter + m;
			vertexBufferContainer.emplace_back();
			vertexBufferMemoryContainer.emplace_back();
			createVertexBuffer(vertexBufferContainer[nextIndexGLTF], vertexBufferMemoryContainer[nextIndexGLTF], aVertices_[nextIndexGLTF]);

			indexBufferContainer.emplace_back();
			indexBufferMemoryContaner.emplace_back();
			createIndexBuffer(indexBufferContainer[nextIndexGLTF], indexBufferMemoryContaner[nextIndexGLTF], aIndices_[nextIndexGLTF]);
			++gltfCounter;
		}
	}

	void CVulkanRenderer::initializeVertexBuffersWithFontData() {
		for ( unsigned int i = 0; i < symbolGVerticesContainer.GetSize(); ++i ) {
				const unsigned int nextBufferIndex = fontIndicesContainer[i];
				core::vector<Vertex> symbol_g_vertices = symbolGVerticesContainer[i];
				
				createVertexBuffer(fontVertexBufferContainer[nextBufferIndex], fontVertexBufferMemoryContainer[nextBufferIndex], symbol_g_vertices);
				createIndexBuffer(fontIndexBufferContainer[nextBufferIndex], fontIndexBufferMemoryContaner[nextBufferIndex], symbol_g_indices);
		}
	}

	void CVulkanRenderer::initializeVertexBuffersWithMathObjectsData() {
		for (unsigned int m = 0; m < mathObjectsVertices.GetSize(); ++m) {
			mathObjectsVertexBufferContainer.emplace_back();
			mathObjectsVertexBufferMemoryContainer.emplace_back();
			createVertexBuffer(mathObjectsVertexBufferContainer[m], mathObjectsVertexBufferMemoryContainer[m], mathObjectsVertices[m]);

			mathObjectsIndexBufferContainer.emplace_back();
			mathObjectsIndexBufferMemoryContaner.emplace_back();
			createIndexBuffer(mathObjectsIndexBufferContainer[m], mathObjectsIndexBufferMemoryContaner[m], mathObjectsIndices[m]);
		}
	}

	void CVulkanRenderer::initializeCollisionWireframesBuffers() {
		// for( size_t i = 0; i < collisionsWireframesVKBuffers.GetSize(); ++i ) {
		// 	vkDestroyBuffer(device, collisionsWireframesVKBuffers[i], nullptr);
		// 	vkFreeMemory(device, collisionsWireframesVKDeviceMemory[i], nullptr);
		// 	vkDestroyBuffer(device, collisionsWireframesIndicesVKBuffers[i], nullptr);
		// 	vkFreeMemory(device, collisionsWireframesIndicesVKDeviceMemory[i], nullptr);
		// }
		// vkDeviceWaitIdle(device);
		// collisionsWireframesVKBuffers.clear();
		// collisionsWireframesVKDeviceMemory.clear();
		// collisionsWireframesIndicesVKBuffers.clear();
		// collisionsWireframesIndicesVKDeviceMemory.clear();
		// collisionsWireframeIndices.clear();

		constexpr int boxIndicesForIndexBuffer[36] =
			{ 0, 1, 2, 3, 0, 2,
			  4, 0, 3, 7, 4, 3,
			  4, 5, 1, 0, 4, 1,
			  1, 5, 6, 2, 1, 6,
			  5, 4, 7, 6, 5, 7,
			  3, 2, 6, 7, 3, 6 };

		/// The function is called again whenever the engine resets isCollisionsWireframeBuffersInitialized, so it only
		/// creates buffers for meshes that don't have them yet. Recreating all of them used to leak the old buffers.
		if ( collisionsWireframeIndices.empty() ) {
			for ( unsigned int i = 0; i < 36; ++i )
				collisionsWireframeIndices.push_back(boxIndicesForIndexBuffer[i]);
		}

		for ( unsigned int i = collisionsWireframesVKBuffers.GetSize(); i < allMeshMaxAbsoluteValues.GetSize(); ++i ) {
			core::vector<core::Vertex> vertices;
			unsigned int cube_vertices = 8;

			const GLVM::core::MeshAxisMaxAbsoluteValues meshAxisMaxAbsoluteValues = allMeshMaxAbsoluteValues[i];
//			const float scale   = collisionWireframe.scale;
			const float half_x = meshAxisMaxAbsoluteValues.origin_offset_x + meshAxisMaxAbsoluteValues.absolute_x;
			const float half_y = meshAxisMaxAbsoluteValues.origin_offset_y + meshAxisMaxAbsoluteValues.absolute_y;
			const float half_z = meshAxisMaxAbsoluteValues.origin_offset_z + meshAxisMaxAbsoluteValues.absolute_z;

			const float bottom_half_x = meshAxisMaxAbsoluteValues.origin_offset_x - meshAxisMaxAbsoluteValues.absolute_x;
			const float bottom_half_y = meshAxisMaxAbsoluteValues.origin_offset_y - meshAxisMaxAbsoluteValues.absolute_y;
			const float bottom_half_z = meshAxisMaxAbsoluteValues.origin_offset_z - meshAxisMaxAbsoluteValues.absolute_z;
			
			for ( unsigned int i = 0; i < cube_vertices; ++i ) {
				SVertex vertex;
				switch( i ) {
				case 0:
					vertex[0] = half_x;
					vertex[1] = half_y;
					vertex[2] = half_z;
					break;
				case 1:
					vertex[0] = (float)bottom_half_x;
					vertex[1] = half_y;
					vertex[2] = half_z;
					break;			
				case 2:
					vertex[0] = (float)bottom_half_x;
					vertex[1] = (float)bottom_half_y;
					vertex[2] = half_z;
					break;			
				case 3:
					vertex[0] = half_x;
					vertex[1] = (float)bottom_half_y;
					vertex[2] = half_z;
					break;
				case 4:
					vertex[0] = half_x;
					vertex[1] = half_y;
					vertex[2] = (float)bottom_half_z;
					break;
				case 5:
					vertex[0] = (float)bottom_half_x;
					vertex[1] = half_y;
					vertex[2] = (float)bottom_half_z;
					break;			
				case 6:
					vertex[0] = (float)bottom_half_x;
					vertex[1] = (float)bottom_half_y;
					vertex[2] = (float)bottom_half_z;
					break;			
				case 7:
					vertex[0] = half_x;
					vertex[1] = (float)bottom_half_y;
					vertex[2] = (float)bottom_half_z;
					break;			
				}

				SVertex normal;
				normal[0] = 0;
				normal[1] = 1;
				normal[2] = 0;
				SVertex texture;
				texture[0] = 0;
				texture[1] = 1;

				vertices.Push({{vertex[0], vertex[1], vertex[2]},
							   {normal[0], normal[1], normal[2]},
							   {texture[0], texture[1]},
							   { -1, -1, -1, -1 },
							   { 1, 1, 1, 1 }});
			}			

			collisionsWireframesVKBuffers.Push({});;
			collisionsWireframesVKDeviceMemory.Push({});
			createVertexBuffer(collisionsWireframesVKBuffers[i], collisionsWireframesVKDeviceMemory[i], vertices);

			collisionsWireframesIndicesVKBuffers.Push({});
			collisionsWireframesIndicesVKDeviceMemory.Push({});
			createIndexBuffer(collisionsWireframesIndicesVKBuffers[i], collisionsWireframesIndicesVKDeviceMemory[i], collisionsWireframeIndices);
		}
		isCollisionsWireframeBuffersInitialized = true;
	}
	
	void CVulkanRenderer::clearVK_Image( VK_Image* textureImages ) {
		vkDestroySampler(device, textureImages->sampler, nullptr);
		for ( unsigned int j = 0; j < textureImages->views.size(); ++j )
			vkDestroyImageView(device, textureImages->views[j], nullptr);

		textureImages->views.clear();
					
		vkDestroyImage(device, textureImages->image, nullptr);
		vkFreeMemory(device, textureImages->deviceMemory, nullptr);
	}
	
    void CVulkanRenderer::cleanupSwapChain() {
		vkDeviceWaitIdle(device);
        vkDestroyImageView(device, mainDepthImageView, nullptr);
		vkDestroyImage(device, mainDepthPipelineImage, nullptr);
		vkFreeMemory(device, mainDepthPipelineImageMemory, nullptr);
		
        for (VkFramebuffer& framebuffer : swapChainFramebuffers) {
            vkDestroyFramebuffer(device, framebuffer, nullptr);
        }

        for (VkImageView& imageView : swapChainImageViews) {
            vkDestroyImageView(device, imageView, nullptr);
        }

        vkDestroySwapchainKHR(device, swapChain, nullptr);
    }

	void CVulkanRenderer::destroyShadowMapFramebuffers() {
        for (VkFramebuffer& framebuffer : directionalLightShadowMapFrameBuffers) {
            vkDestroyFramebuffer(device, framebuffer, nullptr);
			framebuffer = VK_NULL_HANDLE;
        }

		for (VkFramebuffer& framebuffer : spotLightShadowMapFrameBuffers) {
            vkDestroyFramebuffer(device, framebuffer, nullptr);
			framebuffer = VK_NULL_HANDLE;
        }

		for (std::vector<VkFramebuffer>& inner_vector : pointLightShadowMapFrameBuffers) {
			for (VkFramebuffer& framebuffer : inner_vector) {
				vkDestroyFramebuffer(device, framebuffer, nullptr);
				framebuffer = VK_NULL_HANDLE;
			}
        }
	}

    void CVulkanRenderer::cleanup() {
		destroyFluidTank();
        cleanupSwapChain();

		/// Worker threads only record command buffers, they have to be stopped before the device is destroyed.
		delete renderThreadPool;
		renderThreadPool = nullptr;

		destroyShadowMapFramebuffers();

		u32 descriptorIndex = 0;
		for( u32 i = 0; i < actualDescriptorBindingsConfigNumber; ++i ) {
			std::cout << "i: " << i << std::endl;
			if( descriptorBindingsConfig[i].vkType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER ) {
				for( unsigned int n = 0; n < descriptorBindingsConfig[i].shaderDescriptorsNumber; ++n ) {
					vkDestroyBuffer(device, GPUDescriptors[descriptorIndex].GPUBuffer->buffer, nullptr);
					vkFreeMemory(device, GPUDescriptors[descriptorIndex].GPUBuffer->deviceMemory, nullptr);
					delete GPUDescriptors[descriptorIndex].GPUBuffer;
					GPUDescriptors[descriptorIndex].GPUBuffer = nullptr;
					++descriptorIndex;
				}
			} else if( descriptorBindingsConfig[i].vkType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER ) {
				for( unsigned int n = 0; n < descriptorBindingsConfig[i].shaderDescriptorsNumber; ++n ) {
					if( GPUDescriptors[descriptorIndex].GPUImage->views.size() )
						clearVK_Image( GPUDescriptors[descriptorIndex].GPUImage );

					delete GPUDescriptors[descriptorIndex].GPUImage;
					GPUDescriptors[descriptorIndex].GPUImage = nullptr;
					++descriptorIndex;
				}
			}
		}

		for( size_t i = 0; i < collisionsWireframesVKBuffers.GetSize(); ++i ) {
			vkDestroyBuffer(device, collisionsWireframesVKBuffers[i], nullptr);
			vkFreeMemory(device, collisionsWireframesVKDeviceMemory[i], nullptr);
			vkDestroyBuffer(device, collisionsWireframesIndicesVKBuffers[i], nullptr);
			vkFreeMemory(device, collisionsWireframesIndicesVKDeviceMemory[i], nullptr);
		}

		for( size_t i = 0; i < spacialGridWireframesVKBuffers.GetSize(); ++i ) {
			vkDestroyBuffer(device, spacialGridWireframesVKBuffers[i], nullptr);
			vkFreeMemory(device, spacialGridWireframesVKDeviceMemory[i], nullptr);
		}

		for ( size_t j = 0; j < mathObjectsVertexBufferContainer.size(); ++j ) {
			vkDestroyBuffer(device, mathObjectsVertexBufferContainer[j], nullptr);
			vkFreeMemory(device, mathObjectsVertexBufferMemoryContainer[j], nullptr);
		}
		for ( size_t j = 0; j < mathObjectsIndexBufferContainer.size(); ++j ) {
			vkDestroyBuffer(device, mathObjectsIndexBufferContainer[j], nullptr);
			vkFreeMemory(device, mathObjectsIndexBufferMemoryContaner[j], nullptr);
		}

		for ( size_t j = 0; j < vertexBufferContainer.size(); ++j ) {
			vkDestroyBuffer(device, vertexBufferContainer[j], nullptr);
			vkFreeMemory(device, vertexBufferMemoryContainer[j], nullptr);
		}
		for ( size_t j = 0; j < indexBufferContainer.size(); ++j ) {
			vkDestroyBuffer(device, indexBufferContainer[j], nullptr);
			vkFreeMemory(device, indexBufferMemoryContaner[j], nullptr);
		}
		for ( size_t j = 0; j < fontIndicesContainer.size(); ++j ) {
			vkDestroyBuffer(device, fontVertexBufferContainer[fontIndicesContainer[j]], nullptr);
			vkFreeMemory(device, fontVertexBufferMemoryContainer[fontIndicesContainer[j]], nullptr);
		}
		for ( size_t j = 0; j < fontIndicesContainer.size(); ++j ) {
			vkDestroyBuffer(device, fontIndexBufferContainer[fontIndicesContainer[j]], nullptr);
			vkFreeMemory(device, fontIndexBufferMemoryContaner[fontIndicesContainer[j]], nullptr);
		}

		vkDeviceWaitIdle(device);

		for( int i = 0; i < SpecificPipeline::PIPELINES_NUMBER; ++i ) {
			vkDestroyRenderPass( device, renderPasses[i], nullptr );
		}
		
		for ( unsigned int i = 0; i < DescriptorSetDataLink::DESCRIPTOR_CHUNKS_NUMBER; ++i ) {
			vkDestroyDescriptorSetLayout(device, descriptorSetsConfig[i].setLayout, nullptr);
		}
		for ( unsigned int i = 0; i < SpecificPipeline::PIPELINES_NUMBER; ++i ) {
			vkDestroyPipeline(device, pipelineConfigs[i].pipeline, nullptr);
			vkDestroyPipelineLayout(device, pipelineConfigs[i].pipelineLayout, nullptr);
		}
		
		vkDestroySampler(device, textureSampler, nullptr);
		vkDestroySampler(device, shadowMapSampler, nullptr);
        for(unsigned int i = 0; i < textureImages.size(); ++i)
        {
            vkDestroySampler(device, textureImages[i].sampler, nullptr);
			for ( unsigned int j = 0; j < textureImages[i].views.size(); ++j )
				vkDestroyImageView(device, textureImages[i].views[j], nullptr);
			
			vkDestroyImage(device, textureImages[i].image, nullptr);
            vkFreeMemory(device, textureImages[i].deviceMemory, nullptr);
        }

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
//            vkDestroySemaphore(device, renderFinishedSemaphores[i], nullptr);
            vkDestroySemaphore(device, imageAvailableSemaphores[i], nullptr);
            vkDestroyFence(device, inFlightFences[i], nullptr);
        }

		destroyRenderFinishedSemaphores();

		vkDestroyCommandPool(device, mainRenderCommandPool, nullptr);
		for( uint32_t i = 0; i < secondaryBuffersCommandPools.size(); ++i ) {
			vkDestroyCommandPool(device, secondaryBuffersCommandPools[i], nullptr);
		}
		vkDestroyDescriptorPool(device, descriptorPool, nullptr);

		vkDeviceWaitIdle(device);
        vkDestroyDevice(device, nullptr);

        if (enableValidationLayers) {
			vkDebugUtils::DestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
        }

        vkDestroySurfaceKHR(instance, surface, nullptr);
        vkDestroyInstance(instance, nullptr);
        Window->Close();
    }

    void CVulkanRenderer::createInstance() {
        if (enableValidationLayers && !checkValidationLayerSupport()) {
            throw std::runtime_error("validation layers requested, but not available!");
        }

        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "Hello Triangle";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName = "Grey Lane Vertex Machine";
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion = VK_API_VERSION_1_0;
		/// Vulkan 1.3 when the loader has it: the water tank (fluid module) needs dynamic rendering.
		uint32_t instanceVersion = VK_API_VERSION_1_0;
		const auto enumerateInstanceVersion = reinterpret_cast<PFN_vkEnumerateInstanceVersion>(vkGetInstanceProcAddr(nullptr, "vkEnumerateInstanceVersion"));
		if ( enumerateInstanceVersion != nullptr && enumerateInstanceVersion(&instanceVersion) == VK_SUCCESS && instanceVersion >= VK_API_VERSION_1_3 )
			appInfo.apiVersion = VK_API_VERSION_1_3;

        VkInstanceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;

        std::vector<const char*> extensions = getRequiredExtensions();
		
        createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
        createInfo.ppEnabledExtensionNames = extensions.data();

        VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
        if (enableValidationLayers) {
            createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
            createInfo.ppEnabledLayerNames = validationLayers.data();
//			createInfo.enabledLayerCount = 0;
			
            populateDebugMessengerCreateInfo(debugCreateInfo);
            createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*) &debugCreateInfo;
        } else {
            createInfo.enabledLayerCount = 0;

            createInfo.pNext = nullptr;
        }

        if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
            throw std::runtime_error("failed to create instance!");
        }
    }

    void CVulkanRenderer::populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo) {
        createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        createInfo.pfnUserCallback = debugCallback;
    }

    void CVulkanRenderer::setupDebugMessenger() {
        if (!enableValidationLayers) return;

        VkDebugUtilsMessengerCreateInfoEXT createInfo;
        populateDebugMessengerCreateInfo(createInfo);

        if (vkDebugUtils::CreateDebugUtilsMessengerEXT(instance, &createInfo, nullptr, &debugMessenger) != VK_SUCCESS) {
            throw std::runtime_error("failed to set up debug messenger!");
        }
    }

    void CVulkanRenderer::createSurface() {
#ifdef VK_USE_PLATFORM_WAYLAND_KHR
        if (vkCreateWaylandSurfaceKHR(instance, &createWaylandSurfaceInfo, nullptr, &surface) != VK_SUCCESS) {
            throw std::runtime_error("failed to create window surface!");
        }
#endif

#ifdef VK_USE_PLATFORM_XLIB_KHR
        if (vkCreateXlibSurfaceKHR(instance, &createXlibSurfaceInfo, nullptr, &surface) != VK_SUCCESS) {
            throw std::runtime_error("failed to create window surface!");
        }
#endif

#ifdef VK_USE_PLATFORM_XCB_KHR
        if (vkCreateXcbSurfaceKHR(instance, &createXcbSurfaceInfo, nullptr, &surface) != VK_SUCCESS) {
            throw std::runtime_error("failed to create window surface!");
        }
#endif
		
#ifdef VK_USE_PLATFORM_WIN32_KHR
        if (vkCreateWin32SurfaceKHR(instance, &createWin32SurfaceInfo, nullptr, &surface) != VK_SUCCESS) {
            throw std::runtime_error("failed to create window surface!");
        }
#endif
    }

    void CVulkanRenderer::pickPhysicalDevice() {
        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

        if (deviceCount == 0) {
            throw std::runtime_error("failed to find GPUs with Vulkan support!");
        }

        std::vector<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

        for (const VkPhysicalDevice& device : devices) {
			VkPhysicalDeviceProperties prop;
			vkGetPhysicalDeviceProperties(device, &prop);
			
            if (isDeviceSuitable(device)) {
				// std::cout << prop.deviceType << std::endl;
				// std::cout << prop.deviceName << std::endl;
                physicalDevice = device;
                break;
            }
        }

        if (physicalDevice == VK_NULL_HANDLE) {
            throw std::runtime_error("failed to find a suitable GPU!");
        }
    }

    void CVulkanRenderer::createLogicalDevice() {
        QueueFamilyIndices indices = findQueueFamilies(physicalDevice);

        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
        std::set<uint32_t> uniqueQueueFamilies = {indices.graphicsFamily.value(), indices.presentFamily.value()};

        float queuePriority = 1.0f;
        for (uint32_t queueFamily : uniqueQueueFamilies) {
            VkDeviceQueueCreateInfo queueCreateInfo{};
            queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueCreateInfo.queueFamilyIndex = queueFamily;
            queueCreateInfo.queueCount = 1;
            queueCreateInfo.pQueuePriorities = &queuePriority;
            queueCreateInfos.push_back(queueCreateInfo);
        }

        VkPhysicalDeviceFeatures deviceFeatures{};
        deviceFeatures.samplerAnisotropy = VK_TRUE;
		deviceFeatures.fillModeNonSolid  = VK_TRUE;
		deviceFeatures.shaderSampledImageArrayDynamicIndexing = VK_TRUE;    ///< Shadow map arrays are indexed by the light loop counter

        VkDeviceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

        createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
        createInfo.pQueueCreateInfos = queueCreateInfos.data();

        createInfo.pEnabledFeatures = &deviceFeatures;

		/// Dynamic rendering (Vulkan 1.3) for the water tank, if the device has it and its graphics queue has compute.
		VkPhysicalDeviceProperties deviceProperties;
		vkGetPhysicalDeviceProperties(physicalDevice, &deviceProperties);
		uint32_t queueFamilyNumber = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyNumber, nullptr);
		std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyNumber);
		vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyNumber, queueFamilies.data());
		const bool hasCompute = (queueFamilies[indices.graphicsFamily.value()].queueFlags & VK_QUEUE_COMPUTE_BIT) != 0;
		if ( deviceProperties.apiVersion >= VK_API_VERSION_1_3 && hasCompute ) {
			VkPhysicalDeviceVulkan13Features supportedVulkan13Features{};
			supportedVulkan13Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
			VkPhysicalDeviceFeatures2 supportedFeatures{};
			supportedFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
			supportedFeatures.pNext = &supportedVulkan13Features;
			vkGetPhysicalDeviceFeatures2(physicalDevice, &supportedFeatures);
			isDynamicRenderingEnabled = supportedVulkan13Features.dynamicRendering == VK_TRUE;
		}
		VkPhysicalDeviceVulkan13Features vulkan13Features{};
		vulkan13Features.sType            = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
		vulkan13Features.dynamicRendering = VK_TRUE;
		if ( isDynamicRenderingEnabled )
			createInfo.pNext = &vulkan13Features;

        createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
        createInfo.ppEnabledExtensionNames = deviceExtensions.data();

        if (enableValidationLayers) {
//            createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
//            createInfo.ppEnabledLayerNames = validationLayers.data();
			createInfo.enabledLayerCount = 0;
        } else {
            createInfo.enabledLayerCount = 0;
        }

        if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) != VK_SUCCESS) {
            throw std::runtime_error("failed to create logical device!");
        }

        vkGetDeviceQueue(device, indices.graphicsFamily.value(), 0, &graphicsQueue);
        vkGetDeviceQueue(device, indices.presentFamily.value(), 0, &presentQueue);
    }

    void CVulkanRenderer::createSwapChain() {
        SwapChainSupportDetails swapChainSupport = querySwapChainSupport(physicalDevice);

        VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
        [[maybe_unused]] VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
        VkExtent2D  extent = chooseSwapExtent(swapChainSupport.capabilities);

        uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
        if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
            imageCount = swapChainSupport.capabilities.maxImageCount;
        }

        VkSwapchainCreateInfoKHR createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.surface = surface;

        createInfo.minImageCount = imageCount;
        createInfo.imageFormat = surfaceFormat.format;
        createInfo.imageColorSpace = surfaceFormat.colorSpace;
        createInfo.imageExtent = extent;
        createInfo.imageArrayLayers = 1;
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		/// Copies of the frame: screenshots and the scene behind the fluid (refraction)
		canCopySwapChainImages = (swapChainSupport.capabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) != 0;
		if ( canCopySwapChainImages )
			createInfo.imageUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

        QueueFamilyIndices indices = findQueueFamilies(physicalDevice);
        uint32_t queueFamilyIndices[] = {indices.graphicsFamily.value(), indices.presentFamily.value()};

        if (indices.graphicsFamily != indices.presentFamily) {
            createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            createInfo.queueFamilyIndexCount = 2;
            createInfo.pQueueFamilyIndices = queueFamilyIndices;
        } else {
            createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        }

        createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        createInfo.presentMode = presentMode;
        createInfo.clipped = VK_TRUE;

        if (vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapChain) != VK_SUCCESS) {
            throw std::runtime_error("failed to create swap chain!");
        }

        vkGetSwapchainImagesKHR(device, swapChain, &imageCount, nullptr);
        swapChainImages.resize(imageCount);
        vkGetSwapchainImagesKHR(device, swapChain, &imageCount, swapChainImages.data());

        swapChainImageFormat = surfaceFormat.format;
        swapChainExtent = extent;
		/// Without a fixed surface extent (Wayland) nothing reports a resize, the renderer has to compare sizes itself.
		swapChainExtentFollowsWindow = swapChainSupport.capabilities.currentExtent.width == (std::numeric_limits<uint32_t>::max)();
		swapChainWindowSize = { Window->width, Window->height };
    }

    void CVulkanRenderer::createImageViews() {
        swapChainImageViews.resize(swapChainImages.size());

        for (uint32_t i = 0; i < swapChainImages.size(); i++) {
			VK_Image swapChainImage		 = {
				.image				 = swapChainImages[i],
				.viewType			 = VK_IMAGE_VIEW_TYPE_2D,
				.aspectFlags         = VK_IMAGE_ASPECT_COLOR_BIT,
				.format				 = swapChainImageFormat,
				.red                 = VK_COMPONENT_SWIZZLE_IDENTITY,
				.green               = VK_COMPONENT_SWIZZLE_IDENTITY,
				.blue                = VK_COMPONENT_SWIZZLE_IDENTITY,
				.alpha               = VK_COMPONENT_SWIZZLE_IDENTITY,
				.arrayLayers         = 1,
				.width               = swapChainExtent.width,
				.height              = swapChainExtent.height
			};

            swapChainImageViews[i] = createImageView(swapChainImage, 0, 1);
        }
    }

    void CVulkanRenderer::createMainRenderPass() {
		for( unsigned int j = 0; j < SpecificPipeline::PIPELINES_NUMBER; ++j ) {
			for( unsigned int i = 0; i < renderPassConfigs[j].actualAttachmentDescriptionNumber; ++i ) {
				if( renderPassConfigs[j].attachmentDescriptions[i].finalLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL ||
					renderPassConfigs[j].attachmentDescriptions[i].finalLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL ) {
					renderPassConfigs[j].attachmentDescriptions[i].format = findDepthFormat();
				} else {
					renderPassConfigs[j].attachmentDescriptions[i].format = swapChainImageFormat;
				}
			}
		
			VkSubpassDescription subpass{};
			subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
			for( unsigned int i = 0; i < renderPassConfigs[j].actualAttachmentReferenceNumber; ++i ) {
				if( renderPassConfigs[j].attachmentReferences[i].layout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL ) {
					subpass.colorAttachmentCount = 1;
					subpass.pColorAttachments = &renderPassConfigs[j].attachmentReferences[i];
				} else if( renderPassConfigs[j].attachmentReferences[i].layout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL ) {
					subpass.pDepthStencilAttachment = &renderPassConfigs[j].attachmentReferences[i];
				}
			}

			VkRenderPassCreateInfo renderPassInfo{};
			renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
			renderPassInfo.attachmentCount = static_cast<uint32_t>(renderPassConfigs[j].actualAttachmentDescriptionNumber);
			renderPassInfo.pAttachments = renderPassConfigs[j].attachmentDescriptions;
			renderPassInfo.subpassCount = 1;
			renderPassInfo.pSubpasses = &subpass;
			renderPassInfo.dependencyCount = renderPassConfigs[j].actualSubpassDependencyNumber;
			renderPassInfo.pDependencies = renderPassConfigs[j].subpassDependencies;
//			std::cout << "PIPELINE NUMBER: " << j << std::endl;
			if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPasses[j]) != VK_SUCCESS) {
				throw std::runtime_error("failed to create render pass!");
			}
		}
    }

	
    void CVulkanRenderer::createDescriptorSetLayout() {
		for ( int descriptorSetCounter = 0; descriptorSetCounter < DescriptorSetDataLink::DESCRIPTOR_CHUNKS_NUMBER; ++descriptorSetCounter ) {
			DescriptorSet& descriptorSet = descriptorSetsConfig[descriptorSetCounter];
			std::vector<VkDescriptorSetLayoutBinding> bindings;
			// std::cout << "NEXT DS" << std::endl;
			// std::cout << "binding count: " << descriptorSet.actualLinkedDescriptorBindingsNumber << std::endl;
			for ( u32 j = 0; j < descriptorSet.actualLinkedDescriptorBindingsNumber; ++j ) {
				u32 currentDescriptorBindingID = descriptorSet.descriptorsBindingsIDs[j];
//			u32 currentDescriptorBindingID = j;
//				std::cout << "DS ID: " << currentDescriptorBindingID << std::endl;
				VkDescriptorSetLayoutBinding modelMatrixUboLayout{};
				modelMatrixUboLayout.binding = descriptorBindingsConfig[currentDescriptorBindingID].binding;
				modelMatrixUboLayout.descriptorCount = descriptorBindingsConfig[currentDescriptorBindingID].shaderDescriptorsNumber;
				modelMatrixUboLayout.descriptorType = descriptorBindingsConfig[currentDescriptorBindingID].vkType;
				modelMatrixUboLayout.pImmutableSamplers = nullptr;
				modelMatrixUboLayout.stageFlags = descriptorBindingsConfig[currentDescriptorBindingID].shaderStageFlag;

				bindings.push_back(modelMatrixUboLayout);
			}

			VkDescriptorSetLayoutCreateInfo layoutInfo{};
			layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
			layoutInfo.flags = 0;
			layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
			layoutInfo.pBindings = bindings.data();
//			std::cout << "NUMBER OF BINDINGS: " << static_cast<uint32_t>(bindings.size()) << std::endl;
			if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSet.setLayout) != VK_SUCCESS) {
				throw std::runtime_error("failed to create descriptor set layout!");
			}
		}
    }

    void CVulkanRenderer::createGraphicsPipeline() {
		for ( int graphicsPipelineCounter = 0; graphicsPipelineCounter < SpecificPipeline::PIPELINES_NUMBER; ++graphicsPipelineCounter ) {
			Pipeline& pipeline = pipelineConfigs[graphicsPipelineCounter];
			VkRenderPass renderPass = renderPasses[graphicsPipelineCounter];
			std::vector<VkPipelineShaderStageCreateInfo> shaderStages;

			VkShaderModule vertShaderModule;
			VkShaderModule fragShaderModule;
//			std::cout << "shader: " << pipeline.vertShader << std::endl;
			if (pipeline.vertShader != nullptr) {
				std::vector<char> vertShaderCode = readFile(pipeline.vertShader);
				vertShaderModule = createShaderModule(vertShaderCode);

				VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
				vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
				vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
				vertShaderStageInfo.module = vertShaderModule;
				vertShaderStageInfo.pName = "main";

				shaderStages.push_back(vertShaderStageInfo);
			}

			if (pipeline.fragShader != nullptr) {
				std::vector<char> fragShaderCode = readFile(pipeline.fragShader);
				fragShaderModule = createShaderModule(fragShaderCode);

				VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
				fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
				fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
				fragShaderStageInfo.module = fragShaderModule;
				fragShaderStageInfo.pName = "main";

				shaderStages.push_back(fragShaderStageInfo);
			}

			VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
			vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

			vertexInputInfo.vertexBindingDescriptionCount = 1;
			vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(pipeline.attributeDescriptions.size());
			vertexInputInfo.pVertexBindingDescriptions = &pipeline.bindingDescription;
			vertexInputInfo.pVertexAttributeDescriptions = pipeline.attributeDescriptions.data();

			VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
			inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
			inputAssembly.topology = pipeline.topology;
			inputAssembly.primitiveRestartEnable = VK_FALSE;

			VkPipelineViewportStateCreateInfo viewportState{};
			viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
			viewportState.viewportCount = 1;
			viewportState.scissorCount = 1;

			VkPipelineRasterizationStateCreateInfo rasterizer{};
			rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
			rasterizer.depthClampEnable = VK_FALSE;
			rasterizer.rasterizerDiscardEnable = VK_FALSE;
			rasterizer.polygonMode = pipeline.polygonMode;
			rasterizer.lineWidth = 1.0f;
			rasterizer.cullMode = pipeline.cullMode;
			rasterizer.frontFace = pipeline.windingOrder;
			rasterizer.depthBiasEnable = VK_FALSE;

			VkPipelineMultisampleStateCreateInfo multisampling{};
			multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
			multisampling.sampleShadingEnable = VK_FALSE;
			multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

			VkPipelineDepthStencilStateCreateInfo depthStencil{};
			depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
			depthStencil.depthTestEnable = VK_TRUE;
			depthStencil.depthWriteEnable = VK_TRUE;
			depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
			depthStencil.depthBoundsTestEnable = VK_FALSE;
			depthStencil.stencilTestEnable = VK_FALSE;

			VkPipelineColorBlendAttachmentState colorBlendAttachment{};
			colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
			colorBlendAttachment.blendEnable = VK_FALSE;

			VkPipelineColorBlendStateCreateInfo colorBlending{};
			colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
			colorBlending.logicOpEnable = VK_FALSE;
			colorBlending.logicOp = VK_LOGIC_OP_COPY;
			colorBlending.attachmentCount = 1;
			colorBlending.pAttachments = &colorBlendAttachment;
			colorBlending.blendConstants[0] = 0.0f;
			colorBlending.blendConstants[1] = 0.0f;
			colorBlending.blendConstants[2] = 0.0f;
			colorBlending.blendConstants[3] = 0.0f;

			std::vector<VkDynamicState> dynamicStates = {
				VK_DYNAMIC_STATE_VIEWPORT,
				VK_DYNAMIC_STATE_SCISSOR
			};
			VkPipelineDynamicStateCreateInfo dynamicState{};
			dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
			dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
			dynamicState.pDynamicStates = dynamicStates.data();

			/*
			  Need to access inside pipeline and take ID for specific descriptor set,
			  then with that ID we got descriptor set and take it's layout
			*/
			unsigned int descriptorLayoutsNumber = pipeline.actualLinkedDescriptorSetsNumber;
			core::vector<VkDescriptorSetLayout> descriptorSetLayouts;
			for ( unsigned i = 0; i < descriptorLayoutsNumber; ++i ) {
//				std::cout << "ds inside pipeline: " << pipeline.linkedDescriptorSetIDs[i] << std::endl;
				descriptorSetLayouts.Push( descriptorSetsConfig[pipeline.linkedDescriptorSetIDs[i]].setLayout );
			}
		
			VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
			pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
			pipelineLayoutInfo.setLayoutCount = descriptorLayoutsNumber;
			pipelineLayoutInfo.pSetLayouts = descriptorSetLayouts.GetVectorContainer();

			if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipeline.pipelineLayout) != VK_SUCCESS) {
				throw std::runtime_error("failed to create pipeline layout!");
			}

			VkGraphicsPipelineCreateInfo pipelineInfo{};
			pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
			pipelineInfo.stageCount = shaderStages.size();
			pipelineInfo.pStages = shaderStages.data();
			pipelineInfo.pVertexInputState = &vertexInputInfo;
			pipelineInfo.pInputAssemblyState = &inputAssembly;
			pipelineInfo.pViewportState = &viewportState;
			pipelineInfo.pRasterizationState = &rasterizer;
			pipelineInfo.pMultisampleState = &multisampling;
			pipelineInfo.pDepthStencilState = &depthStencil;
			pipelineInfo.pColorBlendState = &colorBlending;
			pipelineInfo.pDynamicState = &dynamicState;
			pipelineInfo.layout = pipeline.pipelineLayout;
			pipelineInfo.renderPass = renderPass;
			pipelineInfo.subpass = 0;
			pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

			if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline.pipeline) != VK_SUCCESS) {
				throw std::runtime_error("failed to create graphics pipeline!");
			}

			if (pipeline.vertShader != nullptr)
				vkDestroyShaderModule(device, vertShaderModule, nullptr);

			if (pipeline.fragShader != nullptr)
				vkDestroyShaderModule(device, fragShaderModule, nullptr);
		}
    }

    void CVulkanRenderer::createFramebuffers() {
		/// Main renderer frame buffers initialization
		swapChainFramebuffers.resize(swapChainImageViews.size());
        for (size_t i = 0; i < swapChainImageViews.size(); ++i) {
			std::vector<VkImageView> mainRenderAttachments;
			mainRenderAttachments.push_back(swapChainImageViews[i]);
			mainRenderAttachments.push_back(mainDepthImageView);

			createRenderPassFramebuffers(mainRenderAttachments, renderPasses[SpecificPipeline::MAIN_RENDER_PIPELINE], swapChainFramebuffers[i],
										 swapChainExtent.width, swapChainExtent.height);
		}
		/// Shadow map framebuffers don't depend on the swapchain, they are created by growShadowMaps().
    }

    void CVulkanRenderer::createRenderPassFramebuffers(std::vector<VkImageView>& attachments, VkRenderPass& renderPass_, VkFramebuffer& swapChainFramebuffer, uint32_t width, uint32_t height) {
            VkFramebufferCreateInfo framebufferInfo{};
            framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferInfo.renderPass = renderPass_;
            framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
            framebufferInfo.pAttachments = attachments.data();
            framebufferInfo.width = width;
            framebufferInfo.height = height;
            framebufferInfo.layers = 1;
			
            if (vkCreateFramebuffer(device, &framebufferInfo, nullptr, &swapChainFramebuffer) != VK_SUCCESS) {
                throw std::runtime_error("failed to create framebuffer!");
            }
    }
	
    void CVulkanRenderer::createCommandPool( VkCommandPool& commandPools ) {
		QueueFamilyIndices queueFamilyIndices = findQueueFamilies(physicalDevice);

		VkCommandPoolCreateInfo poolInfo{};
		poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();

		if (vkCreateCommandPool(device, &poolInfo, nullptr, &commandPools) != VK_SUCCESS) {
			throw std::runtime_error("failed to create graphics command pool!");
		}
    }

    void CVulkanRenderer::createDepthResources() {
        VkFormat depthFormat = findDepthFormat();

		VK_Image depthImage		 = {
			.image				 = VkImage{},
			.deviceMemory		 = VkDeviceMemory{},
			.viewType			 = VK_IMAGE_VIEW_TYPE_2D,
			.createFlags		 = 0,
			.memoryPropertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			.usageFlags			 = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
			.aspectFlags         = VK_IMAGE_ASPECT_DEPTH_BIT,
			.format				 = depthFormat,
			.tiling				 = VK_IMAGE_TILING_OPTIMAL,
			.arrayLayers		 = 1,
			.width				 = swapChainExtent.width,
			.height				 = swapChainExtent.height,
		};

		createImage(depthImage);
		mainDepthPipelineImage = depthImage.image;
		mainDepthPipelineImageMemory = depthImage.deviceMemory;
        mainDepthImageView = createImageView(depthImage, 0, 1);
    }

	namespace {
		constexpr uint32_t shadowMapsMaxNumber[SHADOW_MAP_TYPES_NUMBER] = { DIRECTIONAL_LIGHTS_NUMBER, SPOT_LIGHTS_NUMBER, POINT_LIGHTS_NUMBER };
		const char* const  shadowMapNames[SHADOW_MAP_TYPES_NUMBER]      = { "directional light", "spot light", "point light cube" };
	}

	/// Creates a depth image used as a shadow map (6 layers for a cube map). The image is cleared to the far plane
	/// and left in the layout it is sampled in, so a placeholder is a valid "no shadow" map. The last view is the one
	/// bound for sampling, cube maps also get one 2D view per face for rendering.
	VK_Image CVulkanRenderer::createShadowMapImage( uint32_t size, bool isCube, const std::string& debugName ) {
		VK_Image depthImage		 = {
			.image				 = VkImage{},
			.deviceMemory		 = VkDeviceMemory{},
			.viewType			 = VK_IMAGE_VIEW_TYPE_2D,
			.createFlags		 = isCube ? static_cast<VkImageCreateFlags>(VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT) : 0u,
			.memoryPropertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			.usageFlags			 = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
			.aspectFlags         = VK_IMAGE_ASPECT_DEPTH_BIT,
			.format				 = findDepthFormat(),
			.tiling				 = VK_IMAGE_TILING_OPTIMAL,
			.arrayLayers		 = isCube ? CUBE_MAP_LAYER_NUMBER : 1u,
			.width				 = size,
			.height				 = size
		};

		createImage(depthImage);

		VkCommandBuffer commandBuffer = beginSingleTimeCommands(mainRenderCommandPool);

		VkImageSubresourceRange subresourceRange{};
		subresourceRange.aspectMask     = VK_IMAGE_ASPECT_DEPTH_BIT;
		subresourceRange.baseMipLevel   = 0;
		subresourceRange.levelCount     = 1;
		subresourceRange.baseArrayLayer = 0;
		subresourceRange.layerCount     = depthImage.arrayLayers;

		VkImageMemoryBarrier barrier{};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.image = depthImage.image;
		barrier.subresourceRange = subresourceRange;
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
							 0, 0, nullptr, 0, nullptr, 1, &barrier);

		const VkClearDepthStencilValue farPlaneDepth = { 1.0f, 0 };
		vkCmdClearDepthStencilImage(commandBuffer, depthImage.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &farPlaneDepth, 1, &subresourceRange);

		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
							 0, 0, nullptr, 0, nullptr, 1, &barrier);

		endSingleTimeCommands(mainRenderCommandPool, commandBuffer);

		if ( isCube ) {
			for ( uint32_t face = 0; face < CUBE_MAP_LAYER_NUMBER; ++face )
				depthImage.views.push_back(createImageView(depthImage, face, 1));

			depthImage.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
			depthImage.views.push_back(createImageView(depthImage, 0, CUBE_MAP_LAYER_NUMBER));
		} else {
			depthImage.views.push_back(createImageView(depthImage, 0, 1));
		}

		vkDebugUtils::setImageDebugObjectName(device, depthImage, debugName);
		return depthImage;
	}

	VK_Image* CVulkanRenderer::shadowMapSlot( ShadowMapType type, uint32_t index ) {
		/// Shadow map bindings of the light data descriptor set: 1 - directional, 2 - point (cube), 3 - spot.
		const unsigned int lightDataBinding = type == DIRECTIONAL_SHADOW_MAP ? 1 : ( type == POINT_SHADOW_MAP ? 2 : 3 );
		const unsigned int bindingID = descriptorSetsConfig[DescriptorSetDataLink::MAIN_RENDER_LIGHT_DATA_UBO].descriptorsBindingsIDs[lightDataBinding];
		return GPUDescriptors[descriptorBindingsConfig[bindingID].globalDescriptorOffset + index].GPUImage;
	}

	/// Every slot of the shadow map descriptor arrays needs a valid image. Slots get a 1x1 placeholder here and a full
	/// size map only when a light uses them (see ensureShadowMaps): 32 full size cube maps would take 2.7 GiB.
	/// Shadow maps don't depend on the window size, they are not recreated with the swapchain.
	void CVulkanRenderer::createShadowMapResources() {
		directionalLightShadowMapFrameBuffers.assign(DIRECTIONAL_LIGHTS_NUMBER, VK_NULL_HANDLE);
		spotLightShadowMapFrameBuffers.assign(SPOT_LIGHTS_NUMBER, VK_NULL_HANDLE);
		pointLightShadowMapFrameBuffers.assign(POINT_LIGHTS_NUMBER, std::vector<VkFramebuffer>(CUBE_MAP_LAYER_NUMBER, VK_NULL_HANDLE));

		for ( uint32_t type = 0; type < SHADOW_MAP_TYPES_NUMBER; ++type ) {
			for ( uint32_t i = 0; i < shadowMapsMaxNumber[type]; ++i ) {
				*shadowMapSlot(static_cast<ShadowMapType>(type), i) =
					createShadowMapImage(1, type == POINT_SHADOW_MAP, std::string(shadowMapNames[type]) + " placeholder");
			}
			allocatedShadowMapsNumber[type] = 0;
		}
	}

	/// Replaces placeholders of slots [allocated, requiredNumber) with full size shadow maps and their framebuffers.
	/// The caller must make sure the placeholders are not used by the GPU anymore.
	void CVulkanRenderer::growShadowMaps( ShadowMapType type, uint32_t requiredNumber ) {
		const bool isCube = type == POINT_SHADOW_MAP;
		for ( uint32_t i = allocatedShadowMapsNumber[type]; i < requiredNumber; ++i ) {
			VK_Image* shadowMap = shadowMapSlot(type, i);
			clearVK_Image(shadowMap);
			*shadowMap = createShadowMapImage(SHADOW_MAP_SIZE, isCube, shadowMapNames[type]);

			if ( type == DIRECTIONAL_SHADOW_MAP ) {
				std::vector<VkImageView> attachments = { shadowMap->views[0] };
				createRenderPassFramebuffers(attachments, renderPasses[SpecificPipeline::DIRECTIONAL_LIGHT_PIPELINE],
											 directionalLightShadowMapFrameBuffers[i], SHADOW_MAP_SIZE, SHADOW_MAP_SIZE);
			} else if ( type == SPOT_SHADOW_MAP ) {
				std::vector<VkImageView> attachments = { shadowMap->views[0] };
				createRenderPassFramebuffers(attachments, renderPasses[SpecificPipeline::SPOT_LIGHT_PIPELINE],
											 spotLightShadowMapFrameBuffers[i], SHADOW_MAP_SIZE, SHADOW_MAP_SIZE);
			} else {
				for ( uint32_t face = 0; face < CUBE_MAP_LAYER_NUMBER; ++face ) {
					std::vector<VkImageView> attachments = { shadowMap->views[face] };
					createRenderPassFramebuffers(attachments, renderPasses[SpecificPipeline::POINT_LIGHT_PIPELINE],
												 pointLightShadowMapFrameBuffers[i][face], SHADOW_MAP_SIZE, SHADOW_MAP_SIZE);
				}
			}
		}

		if ( requiredNumber > allocatedShadowMapsNumber[type] )
			allocatedShadowMapsNumber[type] = requiredNumber;
	}

	/// Called before a frame is recorded: lights without a full size shadow map get one. Replaced placeholders may be
	/// referenced by frames in flight, so the device is idled first. This only happens when the number of lights grows.
	void CVulkanRenderer::ensureShadowMaps() {
		const uint32_t requiredNumber[SHADOW_MAP_TYPES_NUMBER] = { directionalLightNumber, spotLightNumber, pointLightNumber };
		bool isGrowNeeded = false;
		for ( uint32_t type = 0; type < SHADOW_MAP_TYPES_NUMBER; ++type ) {
			if ( requiredNumber[type] > allocatedShadowMapsNumber[type] )
				isGrowNeeded = true;
		}

		if ( !isGrowNeeded )
			return;

		vkDeviceWaitIdle(device);
		for ( uint32_t type = 0; type < SHADOW_MAP_TYPES_NUMBER; ++type )
			growShadowMaps(static_cast<ShadowMapType>(type), requiredNumber[type]);

		updateLightDataDescriptorSets(descriptorSetsConfig[DescriptorSetDataLink::MAIN_RENDER_LIGHT_DATA_UBO]);
	}

    VkFormat CVulkanRenderer::findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features) {
        for (VkFormat format : candidates) {
            VkFormatProperties props;
            vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &props);

            if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
                return format;
            } else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
                return format;
            }
        }

        throw std::runtime_error("failed to find supported format!");
    }

    VkFormat CVulkanRenderer::findDepthFormat() {
        return findSupportedFormat(
            {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
            VK_IMAGE_TILING_OPTIMAL,
            VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
            );
    }

    bool CVulkanRenderer::hasStencilComponent(VkFormat format) {
        return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
    }

    void CVulkanRenderer::createTextureImageView() {
		unsigned int readableTextureDescriptorBindingIndex = descriptorSetsConfig[DescriptorSetDataLink::RIDABLE_TEXTURES].descriptorsBindingsIDs[0];		
        for(unsigned int i = 0; i < initializeTextureData_.size(); ++i) {
			VK_Image* image = GPUDescriptors[descriptorBindingsConfig[readableTextureDescriptorBindingIndex].globalDescriptorOffset + i].GPUImage;
			image->views.push_back(createImageView(*image, 0, 1));
		}
    }

    void CVulkanRenderer::createTextureSampler() {
		VkPhysicalDeviceProperties properties{};
		vkGetPhysicalDeviceProperties(physicalDevice, &properties);

		VkSamplerCreateInfo samplerInfo{};
		samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		samplerInfo.magFilter = VK_FILTER_NEAREST;
		samplerInfo.minFilter = VK_FILTER_NEAREST;
		samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerInfo.anisotropyEnable = VK_TRUE;
		samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
		samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
		samplerInfo.unnormalizedCoordinates = VK_FALSE;
		samplerInfo.compareEnable = VK_FALSE;
		samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
		samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

		textureSampler = {};              /// TODO: Is it realy need here?
		if (vkCreateSampler(device, &samplerInfo, nullptr, &textureSampler) != VK_SUCCESS) {
			throw std::runtime_error("failed to create texture sampler!");
		}

		/// Shadow maps must not wrap around: outside of the map the depth reads as the far plane (no shadow).
		VkSamplerCreateInfo shadowSamplerInfo = samplerInfo;
		shadowSamplerInfo.addressModeU     = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
		shadowSamplerInfo.addressModeV     = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
		shadowSamplerInfo.addressModeW     = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
		shadowSamplerInfo.borderColor      = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
		shadowSamplerInfo.anisotropyEnable = VK_FALSE;
		shadowSamplerInfo.maxAnisotropy    = 1.0f;
		shadowSamplerInfo.mipmapMode       = VK_SAMPLER_MIPMAP_MODE_NEAREST;
		if (vkCreateSampler(device, &shadowSamplerInfo, nullptr, &shadowMapSampler) != VK_SUCCESS) {
			throw std::runtime_error("failed to create shadow map sampler!");
		}
    }

    VkImageView CVulkanRenderer::createImageView(VK_Image image, uint32_t baseArrayLayers, uint32_t layerCount) {
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = image.image;
        viewInfo.viewType = image.viewType;
        viewInfo.format = image.format;
		viewInfo.components.r = image.red;
		viewInfo.components.g = image.green;
		viewInfo.components.b = image.blue;
		viewInfo.components.a = image.alpha;
        viewInfo.subresourceRange.aspectMask = image.aspectFlags;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = baseArrayLayers;
        viewInfo.subresourceRange.layerCount = layerCount;

        VkImageView imageView;
        if (vkCreateImageView(device, &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
            throw std::runtime_error("failed to create texture image view!");
        }

        return imageView;
    }

    void CVulkanRenderer::createImage(VK_Image& image) {
        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width = image.width;
        imageInfo.extent.height = image.height;
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = image.arrayLayers;
        imageInfo.format = image.format;
        imageInfo.tiling = image.tiling;
        imageInfo.usage = image.usageFlags;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageInfo.flags = image.createFlags;

        if (vkCreateImage(device, &imageInfo, nullptr, &image.image) != VK_SUCCESS) {
            throw std::runtime_error("failed to create image!");
        }

        VkMemoryRequirements memRequirements;
        vkGetImageMemoryRequirements(device, image.image, &memRequirements);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, image.memoryPropertyFlags);

        if (vkAllocateMemory(device, &allocInfo, nullptr, &image.deviceMemory) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate image memory!");
        }

        vkBindImageMemory(device, image.image, image.deviceMemory, 0);
    }

    void CVulkanRenderer::transitionImageLayout(VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout) {
        VkCommandBuffer commandBuffer = beginSingleTimeCommands(mainRenderCommandPool);

        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = oldLayout;
        barrier.newLayout = newLayout;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;

        VkPipelineStageFlags sourceStage;
        VkPipelineStageFlags destinationStage;

        if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

            sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

            sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        } else {
            throw std::invalid_argument("unsupported layout transition!");
        }

        vkCmdPipelineBarrier(
            commandBuffer,
            sourceStage, destinationStage,
            0,
            0, nullptr,
            0, nullptr,
            1, &barrier
            );

        endSingleTimeCommands(mainRenderCommandPool, commandBuffer);
    }

    void CVulkanRenderer::copyBufferToImage(VkBuffer& buffer, VkImage image, uint32_t width, uint32_t height) {
        VkCommandBuffer commandBuffer = beginSingleTimeCommands(mainRenderCommandPool);

        VkBufferImageCopy region{};
        region.bufferOffset = 0;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;
        region.imageOffset = {0, 0, 0};
        region.imageExtent = {
            width,
            height,
            1
        };

        vkCmdCopyBufferToImage(commandBuffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

        endSingleTimeCommands(mainRenderCommandPool, commandBuffer);
    }

    void CVulkanRenderer::createVertexBuffer(VkBuffer& _vertexBuffer, VkDeviceMemory& _vertexBufferMemory, core::vector<Vertex>& _vertices) {
        VkDeviceSize bufferSize = sizeof(_vertices[0]) * _vertices.GetSize();

        VkBuffer stagingBuffer;
        VkDeviceMemory stagingBufferMemory;
        createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

        void* data;
        vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &data);
        memcpy(data, _vertices.GetVectorContainer(), (size_t) bufferSize);
        vkUnmapMemory(device, stagingBufferMemory);

        createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, _vertexBuffer, _vertexBufferMemory);

        copyBuffer(stagingBuffer, _vertexBuffer, bufferSize);

        vkDestroyBuffer(device, stagingBuffer, nullptr);
        vkFreeMemory(device, stagingBufferMemory, nullptr);
    }

    void CVulkanRenderer::createIndexBuffer(VkBuffer& _indexBuffer, VkDeviceMemory& _indexBufferMemory, const std::vector<uint32_t>& _indices) {
        VkDeviceSize bufferSize = sizeof(_indices[0]) * _indices.size();

        VkBuffer stagingBuffer;
        VkDeviceMemory stagingBufferMemory;
        createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

        void* data;
        vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &data);
        memcpy(data, _indices.data(), (size_t) bufferSize);
        vkUnmapMemory(device, stagingBufferMemory);

        createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, _indexBuffer, _indexBufferMemory);

        copyBuffer(stagingBuffer, _indexBuffer, bufferSize);

        vkDestroyBuffer(device, stagingBuffer, nullptr);
        vkFreeMemory(device, stagingBufferMemory, nullptr);
    }

	// void CVulkanRenderer::createMemoryArenaBuffers(VkBuffer buffer, VkDeviceMemory deviceMemory, VkDeviceSize,
	// 	) {
		
	// }
	
    void CVulkanRenderer::createMainRenderUniformBuffers() {
		/// Uniform slots are addressed by descriptor offsets, so the slot stride has to respect the device alignment.
		VkPhysicalDeviceProperties properties{};
		vkGetPhysicalDeviceProperties(physicalDevice, &properties);
		const VkDeviceSize uboOffsetAlignment = properties.limits.minUniformBufferOffsetAlignment;

		for( unsigned int descriptorSetConfigCounter = 0; descriptorSetConfigCounter <
				 DescriptorSetDataLink::DESCRIPTOR_CHUNKS_NUMBER; ++descriptorSetConfigCounter ) {
			for( unsigned int j = 0; j < descriptorSetsConfig[descriptorSetConfigCounter].actualLinkedDescriptorBindingsNumber; ++j ) {
				unsigned int descriptorBindingIndex = descriptorSetsConfig[descriptorSetConfigCounter].descriptorsBindingsIDs[j];
				VkDescriptorType descriptorType = descriptorBindingsConfig[descriptorBindingIndex].vkType;
				if( descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER ) {
					VkDeviceSize& uboChunkSize = descriptorBindingsConfig[descriptorBindingIndex].uboChunkSize;
					uboChunkSize = (uboChunkSize + uboOffsetAlignment - 1) / uboOffsetAlignment * uboOffsetAlignment;
					VkDeviceSize memory = uboChunkSize * descriptorSetsConfig[descriptorSetConfigCounter].hostDescriptorNumber;
					// std::cout << "ds binding index: " << descriptorBindingIndex << std::endl;
					// std::cout << "host ds number: " << descriptorSetsConfig[descriptorBindingIndex].hostDescriptorNumber << std::endl;
					// std::cout << "chunk size: " << descriptorBindingsConfig[descriptorBindingIndex].uboChunkSize << std::endl;
					// std::cout << "MEMORY: " << memory << std::endl;
					createBuffer(memory, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
								 GPUDescriptors[descriptorBindingsConfig[descriptorBindingIndex].globalDescriptorOffset].GPUBuffer->buffer,
								 GPUDescriptors[descriptorBindingsConfig[descriptorBindingIndex].globalDescriptorOffset].GPUBuffer->deviceMemory);
				} else {
					continue;
				}
			}
		}
    }

    void CVulkanRenderer::createMainRenderDescriptorPool() {
        std::array<VkDescriptorPoolSize, 2> poolSizes{};

		uint32_t descriptorCount = 65536;
        poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        poolSizes[0].descriptorCount = static_cast<uint32_t>(descriptorCount);
		poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        poolSizes[1].descriptorCount = static_cast<uint32_t>(descriptorCount);

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
        poolInfo.pPoolSizes = poolSizes.data();
        poolInfo.maxSets = static_cast<uint32_t>(descriptorCount);

        if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
            throw std::runtime_error("failed to create descriptor pool!");
        }
    }

	void CVulkanRenderer::allocateDescriptorSets( core::vector<VkDescriptorSet>& descriptorSets, VkDescriptorSetLayout setLayout,
												  const unsigned int descriptorSetsNumber, const unsigned int descriptorOffset ) {
		if( descriptorSetsNumber == 0 ) {
			return;
		}
		
		std::vector<VkDescriptorSetLayout> matrixUboLayouts(descriptorSetsNumber, setLayout);
		VkDescriptorSetAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool = descriptorPool;
		allocInfo.descriptorSetCount = static_cast<uint32_t>(descriptorSetsNumber);
		allocInfo.pSetLayouts = matrixUboLayouts.data();

//		descriptorSets.Resize(descriptorSetsNumber);
		if (vkAllocateDescriptorSets(device, &allocInfo, descriptorSets.GetVectorContainer() + descriptorOffset) != VK_SUCCESS) {
			throw std::runtime_error("failed to allocate descriptor sets!");
		}
	}

	void CVulkanRenderer::updateDescriptorSetsUBO( VkBuffer ubo, const VkDeviceSize& uboStructSize, const unsigned int& uboDescriptorsNumber,
												   int uboBinding, [[maybe_unused]] core::vector<VkDescriptorSet>& uboDescriptorSets, const unsigned int offset ) {
		for (size_t i = 0; i < uboDescriptorsNumber; ++i) {
			VkDescriptorBufferInfo modelMatrixBufferInfo = createDescriptorBufferInfo( ubo, uboStructSize, i );
			std::array<VkWriteDescriptorSet, 1> descriptorWrites{};
			
			descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			descriptorWrites[0].dstSet = *(descriptorSetsChunks.GetVectorContainer() + offset + i);
			descriptorWrites[0].dstBinding = uboBinding;
			descriptorWrites[0].dstArrayElement = 0;
			descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			descriptorWrites[0].descriptorCount = 1;
			descriptorWrites[0].pBufferInfo = &modelMatrixBufferInfo;

			vkUpdateDescriptorSets(device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
		}
	}

	void CVulkanRenderer::updateLightDataDescriptorSets( const DescriptorSet& currentDescriptorSet1 ) {
		const unsigned int bindingsNumber = currentDescriptorSet1.actualLinkedDescriptorBindingsNumber;

		/// All info storage is sized up front, so the pointers kept in descriptorWrites stay valid until vkUpdateDescriptorSets.
		std::vector<VkWriteDescriptorSet> descriptorWrites(bindingsNumber);
		std::vector<VkDescriptorBufferInfo> descriptorBufferInfos(bindingsNumber);
		std::vector<std::vector<VkDescriptorImageInfo>> descriptorImageInfos(bindingsNumber);
		for ( size_t i = 0; i < currentDescriptorSet1.hostDescriptorNumber; ++i ) {
			for( size_t j = 0; j < bindingsNumber; ++j ) {
				const DescriptorBinding& binding = descriptorBindingsConfig[currentDescriptorSet1.descriptorsBindingsIDs[j]];
				descriptorWrites[j] = {};
				if( binding.vkType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER ) {
					/// Uniform buffer bindings hold one descriptor, set i points to uniform slot i.
					descriptorBufferInfos[j] = createDescriptorBufferInfo( GPUDescriptors[binding.globalDescriptorOffset].GPUBuffer->buffer,
																		   binding.uboChunkSize, i );
					descriptorWrites[j].pBufferInfo = &descriptorBufferInfos[j];
				} else if ( binding.vkType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER ) {
					/// Image arrays of non texture descriptor sets are shadow maps, the last view is the one to sample.
					descriptorImageInfos[j].clear();
					for( size_t m = 0; m < binding.shaderDescriptorsNumber; ++m ) {
						const VK_Image& shadowMap = *GPUDescriptors[binding.globalDescriptorOffset + m].GPUImage;
						descriptorImageInfos[j].push_back( createDescriptorImageInfo( shadowMap, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
																					  shadowMap.views.size() - 1, shadowMapSampler ) );
					}
					descriptorWrites[j].pImageInfo = descriptorImageInfos[j].data();
				}
				descriptorWrites[j].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				descriptorWrites[j].dstSet = *(descriptorSetsChunks.GetVectorContainer() + currentDescriptorSet1.descriptorSetOffset + i);
				descriptorWrites[j].dstBinding = binding.binding;
				descriptorWrites[j].dstArrayElement = 0;
				descriptorWrites[j].descriptorType = binding.vkType;
				descriptorWrites[j].descriptorCount = binding.vkType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER ? 1 : binding.shaderDescriptorsNumber;
			}

			vkUpdateDescriptorSets(device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
		}
	}
	
	void CVulkanRenderer::updateDescriptorSetsCombinedImageSampler( const DescriptorSet& descriptorSet ) {
		core::vector<u32> bindingsIDs;
		for ( size_t j = 0; j < descriptorSet.actualLinkedDescriptorBindingsNumber; ++j ) {
			bindingsIDs.Push( descriptorSet.descriptorsBindingsIDs[j] );
		}

		unsigned int readableTextureDescriptorBindingIndex = descriptorSetsConfig[DescriptorSetDataLink::RIDABLE_TEXTURES].descriptorsBindingsIDs[0];
		if ( initializeTextureData_.empty() ) {
			return;
		}
		for (size_t i = 0; i < descriptorSet.hostDescriptorNumber; ++i) {
			/// One set per texture per frame in flight. Sets of texture ids that are not loaded point to texture 0.
			unsigned int textureIndex = i / MAX_FRAMES_IN_FLIGHT;
			if ( textureIndex >= initializeTextureData_.size() )
				textureIndex = 0;
			constexpr unsigned int textureViewIndex = 0;
			VkDescriptorImageInfo imageInfo = createDescriptorImageInfo( *GPUDescriptors[descriptorBindingsConfig[readableTextureDescriptorBindingIndex].globalDescriptorOffset + textureIndex].GPUImage, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, textureViewIndex, textureSampler );
			core::vector<VkWriteDescriptorSet> descriptorWrites{};

			for ( unsigned int j = 0; j < bindingsIDs.GetSize(); ++j ) {
				descriptorWrites.Push({});
				const unsigned int lastElement = descriptorWrites.GetSize() - 1;
				descriptorWrites[lastElement].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				descriptorWrites[lastElement].dstSet = *(descriptorSetsChunks.GetVectorContainer() + descriptorSet.descriptorSetOffset + i);
				descriptorWrites[lastElement].dstBinding = descriptorBindingsConfig[bindingsIDs[j]].binding;;
				descriptorWrites[lastElement].dstArrayElement = 0;
				descriptorWrites[lastElement].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
				descriptorWrites[lastElement].descriptorCount = 1;
				descriptorWrites[lastElement].pImageInfo = &imageInfo;
			}
			vkUpdateDescriptorSets(device, static_cast<uint32_t>(descriptorWrites.GetSize()), descriptorWrites.GetVectorContainer(), 0, nullptr);
		}
	}
	
	void CVulkanRenderer::createDescriptorImageInfo( const unsigned int descriptorNumber, VkImageLayout imageLayout,
													 core::vector<VK_Image>& textureImages, const unsigned int imageViewIndex,
													 VkDescriptorImageInfo descriptorImageInfos[] ) {
		for (size_t i = 0; i < descriptorNumber; ++i) {
			descriptorImageInfos[i] = {};
			descriptorImageInfos[i].imageLayout = imageLayout;
			descriptorImageInfos[i].imageView = textureImages[i].views[imageViewIndex];
			descriptorImageInfos[i].sampler = textureSampler;
		}
	}
	
    void CVulkanRenderer::createMainRenderDescriptorSets() {
		for( unsigned int pipelineCounter = 0; pipelineCounter < SpecificPipeline::PIPELINES_NUMBER; ++pipelineCounter ) {
			for( unsigned int descriptorSetCounter = 0; descriptorSetCounter < pipelineConfigs[pipelineCounter].actualLinkedDescriptorSetsNumber; ++descriptorSetCounter ) {
				const unsigned int linkedDescriptorSetMatrixUboID = pipelineConfigs[pipelineCounter].linkedDescriptorSetIDs[descriptorSetCounter];
				const DescriptorSet& currentDescriptorSet0 = descriptorSetsConfig[linkedDescriptorSetMatrixUboID];
				allocateDescriptorSets( descriptorSetsChunks, currentDescriptorSet0.setLayout,
										currentDescriptorSet0.hostDescriptorNumber, currentDescriptorSet0.descriptorSetOffset );
				if( currentDescriptorSet0.isTexture ) {
					updateDescriptorSetsCombinedImageSampler( currentDescriptorSet0);
				} else {
					updateLightDataDescriptorSets( currentDescriptorSet0 );
				}
			}
		}
	}

	void CVulkanRenderer::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory) {
		VkBufferCreateInfo bufferInfo{};
		bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = size;
        bufferInfo.usage = usage;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
            throw std::runtime_error("failed to create buffer!");
        }

        VkMemoryRequirements memRequirements;
        vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

		i32 result = vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory);
        if (result != VK_SUCCESS) {
			std::cout << "result" << result << std::endl;
            throw std::runtime_error("failed to allocate buffer memory!");
        }

        vkBindBufferMemory(device, buffer, bufferMemory, 0);
    }

    VkCommandBuffer CVulkanRenderer::beginSingleTimeCommands(VkCommandPool& commandPool) {
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandPool = commandPool;
        allocInfo.commandBufferCount = 1;

        VkCommandBuffer commandBuffer;
        vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        vkBeginCommandBuffer(commandBuffer, &beginInfo);

        return commandBuffer;
    }

    void CVulkanRenderer::endSingleTimeCommands(VkCommandPool& commandPool, VkCommandBuffer& commandBuffer) {
        vkEndCommandBuffer(commandBuffer);

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;

        vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(graphicsQueue);

        vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
    }

    void CVulkanRenderer::copyBuffer(VkBuffer& srcBuffer, VkBuffer& dstBuffer, VkDeviceSize size) {
        VkCommandBuffer commandBuffer = beginSingleTimeCommands(mainRenderCommandPool);

        VkBufferCopy copyRegion{};
        copyRegion.size = size;
        vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

        endSingleTimeCommands(mainRenderCommandPool, commandBuffer);
    }

    uint32_t CVulkanRenderer::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
        VkPhysicalDeviceMemoryProperties memProperties;
        vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

        for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
//            if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties && memProperties.memoryTypes[i].heapIndex == 0) {
			if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
                return i;
            }
        }

        throw std::runtime_error("failed to find suitable memory type!");
    }

    void CVulkanRenderer::createCommandBuffers(VkCommandPool& commandPool, std::vector<VkCommandBuffer>& commandBuffers,
											   uint32_t commandBuffersNumber, VkCommandBufferLevel commandBufferLevelFlag) {
        commandBuffers.resize(commandBuffersNumber * MAX_FRAMES_IN_FLIGHT);

        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = commandPool;
        allocInfo.level = commandBufferLevelFlag;
        allocInfo.commandBufferCount = (uint32_t) commandBuffers.size();

        if (vkAllocateCommandBuffers(device, &allocInfo, commandBuffers.data()) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate command buffers!");
        }
    }

	void CVulkanRenderer::executeSecondaryCommandBuffer( VkRenderPass renderPass, VkFramebuffer frameBuffer, VkExtent2D extent,
														 VkCommandBuffer primaryCommandBuffer, VkCommandBuffer secondaryCommandBuffer ) {
		VkClearValue shadowMapClearValues[1];
		shadowMapClearValues[0].depthStencil.depth = 1.0f;
		shadowMapClearValues[0].depthStencil.stencil = 0;

		VkRenderPassBeginInfo shadowMapRenderPassInfo{};
		shadowMapRenderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		shadowMapRenderPassInfo.pNext = NULL;
		shadowMapRenderPassInfo.renderPass = renderPass;
		shadowMapRenderPassInfo.framebuffer = frameBuffer;
		shadowMapRenderPassInfo.renderArea.offset.x = 0;
		shadowMapRenderPassInfo.renderArea.offset.y = 0;
		shadowMapRenderPassInfo.renderArea.extent.width = extent.width;
		shadowMapRenderPassInfo.renderArea.extent.height = extent.height;
		shadowMapRenderPassInfo.clearValueCount = 1;
		shadowMapRenderPassInfo.pClearValues = shadowMapClearValues;

		vkCmdBeginRenderPass(primaryCommandBuffer, &shadowMapRenderPassInfo, 
							 VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);
		vkCmdExecuteCommands(primaryCommandBuffer, 1, 
							 &secondaryCommandBuffer);
		vkCmdEndRenderPass(primaryCommandBuffer);
	}
	
	void CVulkanRenderer::updateHudUBO(uint32_t offset, bool isHudExists, float highestY, uint32_t healthCounter) {
		HUD_UBO hudUBO{};

		hudUBO.view = viewMatrix;
		hudUBO.proj = projectionMatrix;
		
		hudUBO.isHudExists = isHudExists;
		hudUBO.currentHP   = healthBars[healthCounter].currentHealth;
		hudUBO.maxHP       = healthBars[healthCounter].maxHealth;
		hudUBO.entityPosition = healthBars[healthCounter].position;
		hudUBO.highestY    = highestY;

		HUD_UBO* hudMatrixData = mappedUBO<HUD_UBO>(DescriptorSetDataLink::HUD, offset);
        memcpy(hudMatrixData, &hudUBO, sizeof(HUD_UBO));
	}

	void CVulkanRenderer::updateHudScreenUBO(uint32_t offset, uint32_t crosshair) {
		HUD_SCREEN_UBO hudUBO{};
		hudUBO.model = crosshairs[crosshair].model;
		
		HUD_SCREEN_UBO* hudMatrixData = mappedUBO<HUD_SCREEN_UBO>(DescriptorSetDataLink::HUD_SCREEN, offset);
        memcpy(hudMatrixData, &hudUBO, sizeof(HUD_SCREEN_UBO));
	}

	void CVulkanRenderer::updateSdfUBO(uint32_t offset, uint32_t crosshair) {
		SDF_UBO hudUBO{};
		hudUBO.model = crosshairs[crosshair].model;

		float currentTime = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - startTime).count() * 0.001;
		hudUBO.iTime = currentTime;
		
		SDF_UBO* hudMatrixData = mappedUBO<SDF_UBO>(DescriptorSetDataLink::SDF_DATA, offset);
        memcpy(hudMatrixData, &hudUBO, sizeof(SDF_UBO));
	}

	void CVulkanRenderer::updateMathObjectsDebugUBO(uint32_t offset, uint32_t mathObject) {
		COLLISIONS_DEBUG_UBO hudUBO{};
		hudUBO.model      = mathObjects[mathObject].modelMatrix;
		hudUBO.view       = viewMatrix;
		hudUBO.projection = projectionMatrix;
		
		COLLISIONS_DEBUG_UBO* hudMatrixData = mappedUBO<COLLISIONS_DEBUG_UBO>(DescriptorSetDataLink::MATH_OBJECTS_DEBUG_DATA, offset);
        memcpy(hudMatrixData, &hudUBO, sizeof(COLLISIONS_DEBUG_UBO));
	}
	
	void CVulkanRenderer::updateCollisionsDebugUBO(uint32_t offset, mat4 model, DescriptorSetDataLink descriptorSetLink) {
		COLLISIONS_DEBUG_UBO collisionsDebugUBO{};
		collisionsDebugUBO.model      = model;
		collisionsDebugUBO.view       = viewMatrix;
		collisionsDebugUBO.projection = projectionMatrix;

		COLLISIONS_DEBUG_UBO* collisionsDebugData = mappedUBO<COLLISIONS_DEBUG_UBO>(descriptorSetLink, offset);
        memcpy(collisionsDebugData, &collisionsDebugUBO, sizeof(COLLISIONS_DEBUG_UBO));
	}
	
	void CVulkanRenderer::updateUBO_UI( const unsigned int currentInventoryRow, const unsigned int currentInventoryColumn, const unsigned int inventory, uint32_t offset ) {
		UI_UBO hudUBO{};

		const unsigned int colSize = inventories[inventory].col;
		hudUBO.model = inventories[inventory].slotData[colSize * currentInventoryRow + currentInventoryColumn].model;
		hudUBO.color = inventories[inventory].slotData[colSize * currentInventoryRow + currentInventoryColumn].color;

		UI_UBO* hudMatrixData = mappedUBO<UI_UBO>(DescriptorSetDataLink::UI, offset);
        memcpy(hudMatrixData, &hudUBO, sizeof(UI_UBO));
	}

	void CVulkanRenderer::updateUBO_IconsUI( uint32_t offset, uint32_t item ) {
		UI_UBO hudUBO{};
		hudUBO.model = items[item].model;
		
		UI_UBO* hudMatrixData = mappedUBO<UI_UBO>(DescriptorSetDataLink::UI_ICONS, offset);
        memcpy(hudMatrixData, &hudUBO, sizeof(UI_UBO));
	}
	
    void CVulkanRenderer::hudRecordCommandBuffer(VkCommandBuffer& commandBuffer, uint32_t imageIndex) {
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

        // if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
        //     throw std::runtime_error("failed to begin recording command buffer!");
        // }

		
//		CreateEndDebugUtilsLabelEXT(instance, commandBuffer);
		
        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = renderPasses[SpecificPipeline::HUD_PIPELINE];
        renderPassInfo.framebuffer = swapChainFramebuffers[imageIndex];
        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent.height = swapChainExtent.height;
		renderPassInfo.renderArea.extent.width = swapChainExtent.width;

        std::array<VkClearValue, 2> clearValues{};
        clearValues[0].color = {{0.5f, 0.2f, 0.2f, 1.0f}};
        clearValues[1].depthStencil = {1.0f, 0};

        renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
        renderPassInfo.pClearValues = clearValues.data();

        vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineConfigs[SpecificPipeline::HUD_PIPELINE].pipeline);

        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = (float) swapChainExtent.width;
        viewport.height = (float) swapChainExtent.height;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = swapChainExtent;
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

		const u32 hudUboPerFrameNumber = perFrameDescriptorNumber(DescriptorSetDataLink::HUD);
		for ( unsigned int i = 0; i < healthBars.GetSize(); ++i ) {
			if ( i >= hudUboPerFrameNumber ) {
				reportUboOverflow(SpecificPipeline::HUD_PIPELINE);
				break;
			}
			unsigned int uiVertexId = healthBars[i].meshID;
			unsigned int uboIndex = currentFrame * hudUboPerFrameNumber + i;
			updateHudUBO(uboIndex, true, highest_gltf_Y[uiVertexId], i);
			const unsigned int linkedDescriptorSetID = pipelineConfigs[SpecificPipeline::HUD_PIPELINE].linkedDescriptorSetIDs[0];
  			const DescriptorSet& currentDescriptorSet = descriptorSetsConfig[linkedDescriptorSetID];
			/// Descriptor set k reads uniform slot k, so the set has to be the one of the slot written above.
			vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineConfigs[SpecificPipeline::HUD_PIPELINE].pipelineLayout,
									0, 1, &(*(descriptorSetsChunks.GetVectorContainer() + currentDescriptorSet.descriptorSetOffset + uboIndex)), 0, nullptr);

			VkBuffer vertexBuffers[] = {vertexBufferContainer[uiVertexId]};
			VkDeviceSize offsets[] = {0};
			vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

			vkCmdBindIndexBuffer(commandBuffer, indexBufferContainer[uiVertexId], 0, VK_INDEX_TYPE_UINT32);

			unsigned int indicesContainerSize = aIndices_[uiVertexId].size();

			vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(indicesContainerSize), 1, 0, 0, 0);
		}

        vkCmdEndRenderPass(commandBuffer);

        // if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        //     throw std::runtime_error("failed to record command buffer!");
        // }
    }

    void CVulkanRenderer::uiRecordCommandBuffer(VkCommandBuffer& commandBuffer, uint32_t imageIndex) {
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

        // if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
        //     throw std::runtime_error("failed to begin recording command buffer!");
        // }

        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = renderPasses[SpecificPipeline::UI_PIPELINE];
        renderPassInfo.framebuffer = swapChainFramebuffers[imageIndex];
        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent.height = swapChainExtent.height;
		renderPassInfo.renderArea.extent.width = swapChainExtent.width;

        std::array<VkClearValue, 2> clearValues{};
        clearValues[0].color = {{0.5f, 0.2f, 0.2f, 1.0f}};
        clearValues[1].depthStencil = {1.0f, 0};

        renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
        renderPassInfo.pClearValues = clearValues.data();

        vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineConfigs[SpecificPipeline::UI_PIPELINE].pipeline);

        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = (float) swapChainExtent.width;
        viewport.height = (float) swapChainExtent.height;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = swapChainExtent;
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

		const u32 uiUboPerFrameNumber = perFrameDescriptorNumber(DescriptorSetDataLink::UI);
		u32 uiSlot = 0;                                      ///< Slots are shared by all inventories of the frame
		for ( unsigned int i = 0; i < inventories.GetSize(); ++i ) {
			const RenderInventory& inventory = inventories[i];
			unsigned int inventoryTextureID   = inventory.inventoryTextureID;
			unsigned int uiVertexId           = inventory.meshID;
			for ( unsigned int j = 0; j < inventory.row; ++j ) {
				for ( unsigned int m = 0; m < inventory.col; ++m ) {
					if ( uiSlot >= uiUboPerFrameNumber ) {
						reportUboOverflow(SpecificPipeline::UI_PIPELINE);
						break;
					}
					unsigned int uboIndex = currentFrame * uiUboPerFrameNumber + uiSlot++;
					updateUBO_UI(j, m, i, uboIndex);
					const unsigned int linkedDescriptorSetID = pipelineConfigs[SpecificPipeline::UI_PIPELINE].linkedDescriptorSetIDs[0];
					const DescriptorSet& currentDescriptorSet = descriptorSetsConfig[linkedDescriptorSetID];
					vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineConfigs[SpecificPipeline::UI_PIPELINE].pipelineLayout,
											0, 1, &(*(descriptorSetsChunks.GetVectorContainer() + currentDescriptorSet.descriptorSetOffset + uboIndex)), 0, nullptr);

					const unsigned int linkedDescriptorSetID1 = pipelineConfigs[SpecificPipeline::UI_PIPELINE].linkedDescriptorSetIDs[1];
					const DescriptorSet& currentDescriptorSet1 = descriptorSetsConfig[linkedDescriptorSetID1];
					vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineConfigs[SpecificPipeline::UI_PIPELINE].pipelineLayout,
											1, 1, textureDescriptorSet(currentDescriptorSet1, inventoryTextureID), 0, nullptr);

					VkBuffer vertexBuffers[] = {vertexBufferContainer[uiVertexId]};
					VkDeviceSize offsets[] = {0};
					vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

					vkCmdBindIndexBuffer(commandBuffer, indexBufferContainer[uiVertexId], 0, VK_INDEX_TYPE_UINT32);

					unsigned int indicesContainerSize = aIndices_[uiVertexId].size();

					vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(indicesContainerSize), 1, 0, 0, 0);
				}
			}
		}

        vkCmdEndRenderPass(commandBuffer);

        // if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        //     throw std::runtime_error("failed to record command buffer!");
        // }
    }

    void CVulkanRenderer::uiIconsRecordCommandBuffer(VkCommandBuffer& commandBuffer, uint32_t imageIndex) {
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

        // if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
        //     throw std::runtime_error("failed to begin recording command buffer!");
        // }

//		CreateEndDebugUtilsLabelEXT(instance, commandBuffer);
		
        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = renderPasses[SpecificPipeline::UI_ICONS_PIPELINE];
        renderPassInfo.framebuffer = swapChainFramebuffers[imageIndex];
        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent.height = swapChainExtent.height;
		renderPassInfo.renderArea.extent.width = swapChainExtent.width;

        std::array<VkClearValue, 2> clearValues{};
        clearValues[0].color = {{0.5f, 0.2f, 0.2f, 1.0f}};
        clearValues[1].depthStencil = {1.0f, 0};

        renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
        renderPassInfo.pClearValues = clearValues.data();

        vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineConfigs[SpecificPipeline::UI_ICONS_PIPELINE].pipeline);

        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = (float) swapChainExtent.width;
        viewport.height = (float) swapChainExtent.height;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = swapChainExtent;
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

		/// Each frame in flight owns a fixed range of slots, so a changing number of items can't overlap the other frame.
		const u32 uiIconsUboPerFrameNumber = perFrameDescriptorNumber(DescriptorSetDataLink::UI_ICONS);
		for ( unsigned int i = 0; i < items.GetSize(); ++i ) {
			if ( i >= uiIconsUboPerFrameNumber ) {
				reportUboOverflow(SpecificPipeline::UI_ICONS_PIPELINE);
				break;
			}
			const RenderItem& item = items[i];
			unsigned int uiVertexId = item.meshID;
			unsigned int diffuseTexureID = item.diffuseTexureID;
			unsigned int uboIndex = currentFrame * uiIconsUboPerFrameNumber + i;

			updateUBO_IconsUI(uboIndex, i);
			const unsigned int linkedDescriptorSetID = pipelineConfigs[SpecificPipeline::UI_ICONS_PIPELINE].linkedDescriptorSetIDs[0];
  			const DescriptorSet& currentDescriptorSet = descriptorSetsConfig[linkedDescriptorSetID];
			vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineConfigs[SpecificPipeline::UI_ICONS_PIPELINE].pipelineLayout,
									0, 1, &(*(descriptorSetsChunks.GetVectorContainer() + currentDescriptorSet.descriptorSetOffset + uboIndex)), 0, nullptr);

			const unsigned int linkedDescriptorSetID1 = pipelineConfigs[SpecificPipeline::UI_ICONS_PIPELINE].linkedDescriptorSetIDs[1];
  			const DescriptorSet& currentDescriptorSet1 = descriptorSetsConfig[linkedDescriptorSetID1];
			vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineConfigs[SpecificPipeline::UI_ICONS_PIPELINE].pipelineLayout,
									1, 1, textureDescriptorSet(currentDescriptorSet1, diffuseTexureID), 0, nullptr);

			VkBuffer vertexBuffers[] = {vertexBufferContainer[uiVertexId]};
			VkDeviceSize offsets[] = {0};
			vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

			vkCmdBindIndexBuffer(commandBuffer, indexBufferContainer[uiVertexId], 0, VK_INDEX_TYPE_UINT32);

			unsigned int indicesContainerSize = aIndices_[uiVertexId].size();

			vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(indicesContainerSize), 1, 0, 0, 0);
		}

        vkCmdEndRenderPass(commandBuffer);

        // if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        //     throw std::runtime_error("failed to record command buffer!");
        // }
    }
	
    void CVulkanRenderer::hudScreenRecordCommandBuffer(VkCommandBuffer& commandBuffer, uint32_t imageIndex) {
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

        // if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
        //     throw std::runtime_error("failed to begin recording command buffer!");
        // }

//		CreateEndDebugUtilsLabelEXT(instance, commandBuffer);
		
        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = renderPasses[SpecificPipeline::HUD_SCREEN_PIPELINE];
        renderPassInfo.framebuffer = swapChainFramebuffers[imageIndex];
        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent.height = swapChainExtent.height;
		renderPassInfo.renderArea.extent.width = swapChainExtent.width;

        std::array<VkClearValue, 2> clearValues{};
        clearValues[0].color = {{0.5f, 0.2f, 0.2f, 1.0f}};
        clearValues[1].depthStencil = {1.0f, 0};

        renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
        renderPassInfo.pClearValues = clearValues.data();

        vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineConfigs[SpecificPipeline::HUD_SCREEN_PIPELINE].pipeline);

        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = (float) swapChainExtent.width;
        viewport.height = (float) swapChainExtent.height;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = swapChainExtent;
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

		const u32 hudScreenUboPerFrameNumber = perFrameDescriptorNumber(DescriptorSetDataLink::HUD_SCREEN);
		for ( unsigned int i = 0; i < crosshairs.GetSize(); ++i ) {
			if ( i >= hudScreenUboPerFrameNumber ) {
				reportUboOverflow(SpecificPipeline::HUD_SCREEN_PIPELINE);
				break;
			}
			const RenderCrosshair& crosshair = crosshairs[i];
			unsigned int uiVertexId = crosshair.meshID;

			unsigned int uboIndex = currentFrame * hudScreenUboPerFrameNumber + i;
			updateHudScreenUBO(uboIndex, i);
			const unsigned int linkedDescriptorSetID = pipelineConfigs[SpecificPipeline::HUD_SCREEN_PIPELINE].linkedDescriptorSetIDs[0];
  			const DescriptorSet& currentDescriptorSet = descriptorSetsConfig[linkedDescriptorSetID];
			vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineConfigs[SpecificPipeline::HUD_SCREEN_PIPELINE].pipelineLayout,
									0, 1, &(*(descriptorSetsChunks.GetVectorContainer() + currentDescriptorSet.descriptorSetOffset + uboIndex)), 0, nullptr);

			VkBuffer vertexBuffers[] = {vertexBufferContainer[uiVertexId]};
			VkDeviceSize offsets[] = {0};
			vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

			vkCmdBindIndexBuffer(commandBuffer, indexBufferContainer[uiVertexId], 0, VK_INDEX_TYPE_UINT32);

			unsigned int indicesContainerSize = aIndices_[uiVertexId].size();

			vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(indicesContainerSize), 1, 0, 0, 0);
		}

        vkCmdEndRenderPass(commandBuffer);

        // if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        //     throw std::runtime_error("failed to record command buffer!");
        // }
    }

    void CVulkanRenderer::sdfRecordCommandBuffer(VkCommandBuffer& commandBuffer, uint32_t imageIndex) {
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

        // if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
        //     throw std::runtime_error("failed to begin recording command buffer!");
        // }

//		CreateEndDebugUtilsLabelEXT(instance, commandBuffer);
		
        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = renderPasses[SpecificPipeline::SDF_PIPELINE];
        renderPassInfo.framebuffer = swapChainFramebuffers[imageIndex];
        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent.height = swapChainExtent.height;
		renderPassInfo.renderArea.extent.width = swapChainExtent.width;

        std::array<VkClearValue, 2> clearValues{};
        clearValues[0].color = {{0.5f, 0.2f, 0.2f, 1.0f}};
        clearValues[1].depthStencil = {1.0f, 0};

        renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
        renderPassInfo.pClearValues = clearValues.data();

        vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineConfigs[SpecificPipeline::SDF_PIPELINE].pipeline);

        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = (float) swapChainExtent.width;
        viewport.height = (float) swapChainExtent.height;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = swapChainExtent;
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

		const u32 sdfUboPerFrameNumber = perFrameDescriptorNumber(DescriptorSetDataLink::SDF_DATA);
		for ( unsigned int i = 0; i < crosshairs.GetSize(); ++i ) {
			if ( i >= sdfUboPerFrameNumber ) {
				reportUboOverflow(SpecificPipeline::SDF_PIPELINE);
				break;
			}
			const RenderCrosshair& crosshair = crosshairs[i];
			unsigned int uiVertexId = crosshair.meshID;

			unsigned int uboIndex = currentFrame * sdfUboPerFrameNumber + i;
			updateSdfUBO(uboIndex, i);
			const unsigned int linkedDescriptorSetID = pipelineConfigs[SpecificPipeline::SDF_PIPELINE].linkedDescriptorSetIDs[0];
  			const DescriptorSet& currentDescriptorSet = descriptorSetsConfig[linkedDescriptorSetID];
			vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineConfigs[SpecificPipeline::SDF_PIPELINE].pipelineLayout,
									0, 1, &(*(descriptorSetsChunks.GetVectorContainer() + currentDescriptorSet.descriptorSetOffset + uboIndex)), 0, nullptr);

			VkBuffer vertexBuffers[] = {vertexBufferContainer[uiVertexId]};
			VkDeviceSize offsets[] = {0};
			vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

			vkCmdBindIndexBuffer(commandBuffer, indexBufferContainer[uiVertexId], 0, VK_INDEX_TYPE_UINT32);

//			unsigned int indicesContainerSize = aIndices_[uiVertexId].size();

//			vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(indicesContainerSize), 1, 0, 0, 0);
			vkCmdDrawIndexed(commandBuffer, 3, 1, 0, 0, 0);
		}

        vkCmdEndRenderPass(commandBuffer);
		/// The main command buffer is ended in mainRenderDrawFrame.
    }

    void CVulkanRenderer::collisionsDebugRecordCommandBuffer(VkCommandBuffer& commandBuffer, uint32_t imageIndex) {
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

        // if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
        //     throw std::runtime_error("failed to begin recording command buffer!");
        // }

//		CreateEndDebugUtilsLabelEXT(instance, commandBuffer);
		
        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = renderPasses[SpecificPipeline::COLLISIONS_DEBUG_PIPELINE];
        renderPassInfo.framebuffer = swapChainFramebuffers[imageIndex];
        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent.height = swapChainExtent.height;
		renderPassInfo.renderArea.extent.width = swapChainExtent.width;

        std::array<VkClearValue, 2> clearValues{};
        clearValues[0].color = {{0.5f, 0.2f, 0.2f, 1.0f}};
        clearValues[1].depthStencil = {1.0f, 0};

        renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
        renderPassInfo.pClearValues = clearValues.data();

        vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineConfigs[SpecificPipeline::COLLISIONS_DEBUG_PIPELINE].pipeline);

        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = (float) swapChainExtent.width;
        viewport.height = (float) swapChainExtent.height;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = swapChainExtent;
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
//		std::cout << "FRAME" << std::endl;

		const u32 collisionsUboPerFrameNumber = perFrameDescriptorNumber(DescriptorSetDataLink::COLLISIONS_DEBUG_DATA);
		for ( unsigned int i = 0; i < collisionsWireframes.GetSize(); ++i ) {
			if ( i >= collisionsUboPerFrameNumber ) {
				reportUboOverflow(SpecificPipeline::COLLISIONS_DEBUG_PIPELINE);
				break;
			}
			const RenderCollisionWireframe& collisionWireframe = collisionsWireframes[i];
//			RenderCrosshair crosshair = crosshairs[i];
//			unsigned int uiVertexId = crosshair.meshID;

			const u32 meshID = collisionWireframe.meshID;
			if ( meshID >= collisionsWireframesVKBuffers.GetSize() )
				continue;                                    ///< Wireframe buffers of this mesh are not created yet
			unsigned int uboIndex = currentFrame * collisionsUboPerFrameNumber + i;
			updateCollisionsDebugUBO(uboIndex, collisionWireframe.model, DescriptorSetDataLink::COLLISIONS_DEBUG_DATA);
			const unsigned int linkedDescriptorSetID = pipelineConfigs[SpecificPipeline::COLLISIONS_DEBUG_PIPELINE].linkedDescriptorSetIDs[0];
  			const DescriptorSet& currentDescriptorSet = descriptorSetsConfig[linkedDescriptorSetID];
			vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineConfigs[SpecificPipeline::COLLISIONS_DEBUG_PIPELINE].pipelineLayout,
									0, 1, &(*(descriptorSetsChunks.GetVectorContainer() + currentDescriptorSet.descriptorSetOffset + uboIndex)), 0, nullptr);

			VkBuffer vertexBuffers[] = {collisionsWireframesVKBuffers[meshID]};
			VkDeviceSize offsets[] = {0};
			vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

			vkCmdBindIndexBuffer(commandBuffer, collisionsWireframesIndicesVKBuffers[meshID], 0, VK_INDEX_TYPE_UINT32);

			unsigned int indicesContainerSize = collisionsWireframeIndices.size();

			vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(indicesContainerSize), 1, 0, 0, 0);
//			vkCmdDrawIndexed(commandBuffer, 3, 1, 0, 0, 0);
		}

        vkCmdEndRenderPass(commandBuffer);

        // if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        //     throw std::runtime_error("failed to record command buffer!");
        // }
    }

    void CVulkanRenderer::mathObjectsDebugRecordCommandBuffer(VkCommandBuffer& commandBuffer, uint32_t imageIndex) {
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

        // if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
        //     throw std::runtime_error("failed to begin recording command buffer!");
        // }

//		CreateEndDebugUtilsLabelEXT(instance, commandBuffer);
		
        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = renderPasses[SpecificPipeline::MATH_OBJECTS_DEBUG_PIPELINE];
        renderPassInfo.framebuffer = swapChainFramebuffers[imageIndex];
        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent.height = swapChainExtent.height;
		renderPassInfo.renderArea.extent.width = swapChainExtent.width;

        std::array<VkClearValue, 2> clearValues{};
        clearValues[0].color = {{0.5f, 0.2f, 0.2f, 1.0f}};
        clearValues[1].depthStencil = {1.0f, 0};

        renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
        renderPassInfo.pClearValues = clearValues.data();

        vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineConfigs[SpecificPipeline::MATH_OBJECTS_DEBUG_PIPELINE].pipeline);

        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = (float) swapChainExtent.width;
        viewport.height = (float) swapChainExtent.height;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = swapChainExtent;
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

		const u32 mathObjectsUboPerFrameNumber = perFrameDescriptorNumber(DescriptorSetDataLink::MATH_OBJECTS_DEBUG_DATA);
		for ( unsigned int i = 0; i < mathObjects.GetSize(); ++i ) {
			if ( i >= mathObjectsUboPerFrameNumber ) {
				reportUboOverflow(SpecificPipeline::MATH_OBJECTS_DEBUG_PIPELINE);
				break;
			}
			const RenderMathObject& mathObject = mathObjects[i];
			unsigned int uiVertexId = mathObject.meshID;

			/// Each frame in flight writes its own slots, the frame the GPU may still read is not touched.
			unsigned int uboIndex = currentFrame * mathObjectsUboPerFrameNumber + i;
			updateMathObjectsDebugUBO(uboIndex, i);
			const unsigned int linkedDescriptorSetID = pipelineConfigs[SpecificPipeline::MATH_OBJECTS_DEBUG_PIPELINE].linkedDescriptorSetIDs[0];
  			const DescriptorSet& currentDescriptorSet = descriptorSetsConfig[linkedDescriptorSetID];
			vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineConfigs[SpecificPipeline::MATH_OBJECTS_DEBUG_PIPELINE].pipelineLayout,
									0, 1, &(*(descriptorSetsChunks.GetVectorContainer() + currentDescriptorSet.descriptorSetOffset + uboIndex)), 0, nullptr);

			VkBuffer vertexBuffers[] = {mathObjectsVertexBufferContainer[uiVertexId]};
			VkDeviceSize offsets[] = {0};
			vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
			vkCmdBindIndexBuffer(commandBuffer, mathObjectsIndexBufferContainer[uiVertexId], 0, VK_INDEX_TYPE_UINT32);
			unsigned int indicesContainerSize = mathObjectsIndices[uiVertexId].size();
			vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(indicesContainerSize), 1, 0, 0, 0);
		}

        vkCmdEndRenderPass(commandBuffer);
		/// The main command buffer is ended in mainRenderDrawFrame.
    }

    void CVulkanRenderer::spacialGridDebugRecordCommandBuffer(VkCommandBuffer& commandBuffer, uint32_t imageIndex) {
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

        // if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
        //     throw std::runtime_error("failed to begin recording command buffer!");
        // }

//		CreateEndDebugUtilsLabelEXT(instance, commandBuffer);
		
        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = renderPasses[SpecificPipeline::SPACIAL_GRID_DEBUG_PIPELINE];
        renderPassInfo.framebuffer = swapChainFramebuffers[imageIndex];
        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent.height = swapChainExtent.height;
		renderPassInfo.renderArea.extent.width = swapChainExtent.width;

        std::array<VkClearValue, 2> clearValues{};
        clearValues[0].color = {{0.5f, 0.2f, 0.2f, 1.0f}};
        clearValues[1].depthStencil = {1.0f, 0};

        renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
        renderPassInfo.pClearValues = clearValues.data();

        vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineConfigs[SpecificPipeline::SPACIAL_GRID_DEBUG_PIPELINE].pipeline);

        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = (float) swapChainExtent.width;
        viewport.height = (float) swapChainExtent.height;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = swapChainExtent;
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
//		std::cout << "FRAME" << std::endl;

		const float chunkSize = renderSpacialGrid.chunkSize;
		
		const u32 spacialGridDepth  = renderSpacialGrid.halfDepth;
		const u32 spacialGridHeight = renderSpacialGrid.halfHeight;
		const u32 spacialGridWidth  = renderSpacialGrid.halfWidth;
		
		if( !isSpacialGridWireframeBuffersInitialized ) {
			// vkDeviceWaitIdle(device);
			// for( size_t i = 0; i < spacialGridWireframesVKBuffers.GetSize(); ++i ) {
			// 	vkDestroyBuffer(device, spacialGridWireframesVKBuffers[i], nullptr);
			// 	vkFreeMemory(device, spacialGridWireframesVKDeviceMemory[i], nullptr);
			// }
			// vkDeviceWaitIdle(device);
			// spacialGridWireframesVKBuffers.clear();
			// spacialGridWireframesVKDeviceMemory.clear();

			for( u32 i2 = 0; i2 < spacialGridDepth; ++i2 ) {
				for( u32 i3 = 0; i3 < spacialGridHeight; ++i3 ) {
					for( u32 i4 = 0; i4 < spacialGridWidth; ++i4 ) {
						core::vector<core::Vertex> vertices;
						unsigned int cube_vertices = 8;

						const float chunkHalfSize = chunkSize * 0.5f;
						
						const float half_x = chunkHalfSize;
						const float half_y = chunkHalfSize;
						const float half_z = chunkHalfSize;

						for ( unsigned int i = 0; i < cube_vertices; ++i ) {
							SVertex vertex;
							switch( i ) {
							case 0:
								vertex[0] = half_x;
								vertex[1] = half_y;
								vertex[2] = half_z;
								break;
							case 1:
								vertex[0] = -(float)half_x;
								vertex[1] = half_y;
								vertex[2] = half_z;
								break;			
							case 2:
								vertex[0] = -(float)half_x;
								vertex[1] = -(float)half_y;
								vertex[2] = half_z;
								break;			
							case 3:
								vertex[0] = half_x;
								vertex[1] = -(float)half_y;
								vertex[2] = half_z;
								break;
							case 4:
								vertex[0] = half_x;
								vertex[1] = half_y;
								vertex[2] = -(float)half_z;
								break;
							case 5:
								vertex[0] = -(float)half_x;
								vertex[1] = half_y;
								vertex[2] = -(float)half_z;
								break;			
							case 6:
								vertex[0] = -(float)half_x;
								vertex[1] = -(float)half_y;
								vertex[2] = -(float)half_z;
								break;			
							case 7:
								vertex[0] = half_x;
								vertex[1] = -(float)half_y;
								vertex[2] = -(float)half_z;
								break;			
							}

							SVertex normal;
							normal[0] = 0;
							normal[1] = 1;
							normal[2] = 0;
							SVertex texture;
							texture[0] = 0;
							texture[1] = 1;

							vertices.Push({{vertex[0], vertex[1], vertex[2]},
										   {normal[0], normal[1], normal[2]},
										   {texture[0], texture[1]},
										   { -1, -1, -1, -1 },
										   { 1, 1, 1, 1 }});
						}

						const u32 index = i2 * spacialGridHeight * spacialGridWidth + i3 * spacialGridWidth + i4;
						
						spacialGridWireframesVKBuffers.Push({});;
						spacialGridWireframesVKDeviceMemory.Push({});
						createVertexBuffer(spacialGridWireframesVKBuffers[index], spacialGridWireframesVKDeviceMemory[index], vertices);
					}
				}
			}
			isSpacialGridWireframeBuffersInitialized = true;
		}
		
		for( u32 i2 = 0; i2 < spacialGridDepth; ++i2 ) {
			for( u32 i3 = 0; i3 < spacialGridHeight; ++i3 ) {
				for( u32 i4 = 0; i4 < spacialGridWidth; ++i4 ) {
					const u32 index = i2 * spacialGridHeight * spacialGridWidth + i3 * spacialGridWidth + i4;

					mat4 scale(1.0f);
					mat4 translation(1.0f);

					const float halfSize = chunkSize * 0.5f;
					
					translation[3][0] = i4 * chunkSize + halfSize - 4.0 + 1.0;
					translation[3][1] = i3 * chunkSize + halfSize + 1.0;
					translation[3][2] = i2 * chunkSize + halfSize - 4.0 + 1.0;
					translation[3][3] = 1.0f;

					const u32 spacialGridUboPerFrameNumber = perFrameDescriptorNumber(DescriptorSetDataLink::SPACIAL_GRID_DEBUG_DATA);
					if ( index >= spacialGridUboPerFrameNumber ) {
						reportUboOverflow(SpecificPipeline::SPACIAL_GRID_DEBUG_PIPELINE);
						continue;
					}
					unsigned int uboIndex = currentFrame * spacialGridUboPerFrameNumber + index;
					updateCollisionsDebugUBO(uboIndex, scale * translation, DescriptorSetDataLink::SPACIAL_GRID_DEBUG_DATA);
					const unsigned int linkedDescriptorSetID = pipelineConfigs[SpecificPipeline::SPACIAL_GRID_DEBUG_PIPELINE].linkedDescriptorSetIDs[0];
					const DescriptorSet& currentDescriptorSet = descriptorSetsConfig[linkedDescriptorSetID];
					vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineConfigs[SpecificPipeline::SPACIAL_GRID_DEBUG_PIPELINE].pipelineLayout,
											0, 1, &(*(descriptorSetsChunks.GetVectorContainer() + currentDescriptorSet.descriptorSetOffset + uboIndex)), 0, nullptr);

			
					VkBuffer vertexBuffers[] = {spacialGridWireframesVKBuffers[index]};
					VkDeviceSize offsets[] = {0};
					vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

					vkCmdBindIndexBuffer(commandBuffer, collisionsWireframesIndicesVKBuffers[0], 0, VK_INDEX_TYPE_UINT32);

					unsigned int indicesContainerSize = collisionsWireframeIndices.size();

					vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(indicesContainerSize), 1, 0, 0, 0);
//			vkCmdDrawIndexed(commandBuffer, 3, 1, 0, 0, 0);
				}
			}
		}

        vkCmdEndRenderPass(commandBuffer);
		/// The main command buffer is ended in mainRenderDrawFrame.
    }
	
    void CVulkanRenderer::fontRecordCommandBuffer(VkCommandBuffer& commandBuffer, uint32_t imageIndex) {
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

        // if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
        //     throw std::runtime_error("failed to begin recording command buffer!");
        // }

//		CreateEndDebugUtilsLabelEXT(instance, commandBuffer);
		
        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = renderPasses[SpecificPipeline::FONT_PIPELINE];
        renderPassInfo.framebuffer = swapChainFramebuffers[imageIndex];
        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent.height = swapChainExtent.height;
		renderPassInfo.renderArea.extent.width = swapChainExtent.width;

        std::array<VkClearValue, 2> clearValues{};
        clearValues[0].color = {{0.5f, 0.2f, 0.2f, 1.0f}};
        clearValues[1].depthStencil = {1.0f, 0};

        renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
        renderPassInfo.pClearValues = clearValues.data();

        vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineConfigs[SpecificPipeline::FONT_PIPELINE].pipeline);

        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = (float) swapChainExtent.width;
        viewport.height = (float) swapChainExtent.height;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = swapChainExtent;
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

//		RenderPlayer player = {};
		for( unsigned int playerCounter = 0; playerCounter < players.GetSize(); ++playerCounter ) {
			player = players[playerCounter];
		}
			/// Every glyph gets its own uniform slot inside the range of the current frame in flight.
			const u32 fontUboPerFrameNumber = perFrameDescriptorNumber(DescriptorSetDataLink::FONT_RENDER_UBO);
			u32 glyphSlot = 0;
			for ( unsigned int i = 0; i < fonts.GetSize(); ++i ) {
				RenderFont& font = fonts[i];
				vec3 playerTragetDirection = font.position - player.position;
				float dotProduct = Dot(playerTragetDirection, player.forward);
				if ( dotProduct <= 0 )
					continue;

				for ( unsigned int j = 0; j < font.font_string.GetSize(); ++j ) {
					if ( glyphSlot >= fontUboPerFrameNumber ) {
						reportUboOverflow(SpecificPipeline::FONT_PIPELINE);
						break;
					}
					const unsigned int ascii_code = static_cast<unsigned char>(font.font_string[j]);
					if ( ascii_code >= fontVertexBufferContainer.size() || fontVertexBufferContainer[ascii_code] == VK_NULL_HANDLE )
						continue;                                ///< No glyph mesh for this character
					const u32 uboIndex = currentFrame * fontUboPerFrameNumber + glyphSlot++;
					VkBuffer vertexBuffers[] = { fontVertexBufferContainer[ascii_code] };
					VkDeviceSize offsets[] = {0};
				
					vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
					vkCmdBindIndexBuffer(commandBuffer, fontIndexBufferContainer[ascii_code], 0, VK_INDEX_TYPE_UINT32);

					unsigned int indicesContainerSize = symbol_g_indices.size();
					FONT_UBO fontUBO{};
					vec3 result;
					vec4 pos = vec4(font.position[0],
									font.position[1],
									font.position[2], 1.0f);

					vec4 clipSpacePosition =  pos * viewMatrix * projectionMatrix;
					vec3 ndcPosition = vec3(clipSpacePosition[0] / clipSpacePosition[3],
											clipSpacePosition[1] / clipSpacePosition[3],
											clipSpacePosition[2] / clipSpacePosition[3]);

					fontUBO.view = viewMatrix;
					fontUBO.proj = projectionMatrix;

					fontUBO.scale    = 0.3f;
					ndcPosition[0] += (float)j * 0.17f * fontUBO.scale;
					ndcPosition[1] -= font.lifeTime / 5.0f;
					fontUBO.position = ndcPosition;

					/// The glyph data and the bound descriptor set must use the same slot.
					FONT_UBO* modelMatrixData = mappedUBO<FONT_UBO>(DescriptorSetDataLink::FONT_RENDER_UBO, uboIndex);
					memcpy(modelMatrixData, &fontUBO, sizeof(fontUBO));

					const unsigned int linkedDescriptorSetID = pipelineConfigs[SpecificPipeline::FONT_PIPELINE].linkedDescriptorSetIDs[0];
					const DescriptorSet& currentDescriptorSet = descriptorSetsConfig[linkedDescriptorSetID];
					vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineConfigs[SpecificPipeline::FONT_PIPELINE].pipelineLayout, 0, 1,
											&(*(descriptorSetsChunks.GetVectorContainer() + currentDescriptorSet.descriptorSetOffset + uboIndex)), 0, nullptr);
					const unsigned int linkedDescriptorSetID1 = pipelineConfigs[SpecificPipeline::FONT_PIPELINE].linkedDescriptorSetIDs[1];
					const unsigned int fontAtlasTextureID = 6;
					const DescriptorSet& currentDescriptorSet1 = descriptorSetsConfig[linkedDescriptorSetID1];
					vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineConfigs[SpecificPipeline::FONT_PIPELINE].pipelineLayout, 1, 1,
											textureDescriptorSet(currentDescriptorSet1, fontAtlasTextureID), 0, nullptr);

					vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(indicesContainerSize), 1, 0, 0, 0);
				}
			}
//		}

        vkCmdEndRenderPass(commandBuffer);

        // if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        //     throw std::runtime_error("failed to record command buffer!");
        // }
    }
	
    void CVulkanRenderer::recordCommandBuffer(VkCommandBuffer& commandBuffer, uint32_t imageIndex) {
        // VkCommandBufferBeginInfo beginInfo{};
        // beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

        // if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
        //     throw std::runtime_error("failed to begin recording command buffer!");
        // }

		namespace cm = GLVM::ecs::components;
//		vkDebugUtils::CreateEndDebugUtilsLabelEXT(instance, commandBuffer);
		
        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = renderPasses[SpecificPipeline::MAIN_RENDER_PIPELINE];
        renderPassInfo.framebuffer = swapChainFramebuffers[imageIndex];
        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent.height = swapChainExtent.height;
		renderPassInfo.renderArea.extent.width = swapChainExtent.width;

		for( unsigned int player = 0; player < players.GetSize(); ++player ) {
			updateViewPositionUniformBuffer(currentFrame, player);
		}
		
        std::array<VkClearValue, 2> clearValues{};
		if( players.GetSize() == 0 ) {
			clearValues[0].color = {{0.7f, 0.2f, 0.2f, 1.0f}};   ///< Player death screen
		} else {
			clearValues[0].color = {{0.2f, 0.2f, 0.2f, 1.0f}};
		}
        clearValues[1].depthStencil = {1.0f, 0};

        renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
        renderPassInfo.pClearValues = clearValues.data();

        vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineConfigs[SpecificPipeline::MAIN_RENDER_PIPELINE].pipeline);

        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = (float) swapChainExtent.width;
        viewport.height = (float) swapChainExtent.height;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = swapChainExtent;
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
		
		const u32 matrixUboPerFrameNumber = perFrameDescriptorNumber(DescriptorSetDataLink::MAIN_RENDER_MATRIX_UBO);
		for ( unsigned int i = 0; i < actors.GetSize(); ++i ) {
			if ( i >= matrixUboPerFrameNumber ) {
				reportUboOverflow(SpecificPipeline::MAIN_RENDER_PIPELINE);
				break;
			}
			const RenderActor& actor = actors[i];
			unsigned int uiVertexId = actor.meshID;
			unsigned int diffuseTextureIndex = actor.diffuseTextureIndex;
			unsigned int specularTextureIndex = actor.specularTextureIndex;

			unsigned int uboIndex = currentFrame * matrixUboPerFrameNumber + i;
			updateMatrixUniformBuffer(uboIndex, i);
			const unsigned int linkedDescriptorSetID = pipelineConfigs[SpecificPipeline::MAIN_RENDER_PIPELINE].linkedDescriptorSetIDs[0];
			const DescriptorSet& currentDescriptorSet = descriptorSetsConfig[linkedDescriptorSetID];
			vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineConfigs[SpecificPipeline::MAIN_RENDER_PIPELINE].pipelineLayout,
									0, 1, &(*(descriptorSetsChunks.GetVectorContainer() + currentDescriptorSet.descriptorSetOffset + uboIndex)), 0, nullptr);

			const unsigned int linkedDescriptorSetID1 = pipelineConfigs[SpecificPipeline::MAIN_RENDER_PIPELINE].linkedDescriptorSetIDs[1];
			const DescriptorSet& currentDescriptorSet1 = descriptorSetsConfig[linkedDescriptorSetID1];
			vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineConfigs[SpecificPipeline::MAIN_RENDER_PIPELINE].pipelineLayout,
									1, 1, &(*(descriptorSetsChunks.GetVectorContainer() + currentDescriptorSet1.descriptorSetOffset + currentFrame)), 0, nullptr);

			VkBuffer vertexBuffers[] = {vertexBufferContainer[uiVertexId]};
			VkDeviceSize offsets[] = {0};
			vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

			vkCmdBindIndexBuffer(commandBuffer, indexBufferContainer[uiVertexId], 0, VK_INDEX_TYPE_UINT32);

			unsigned int indicesContainerSize = aIndices_[uiVertexId].size();

			const unsigned int linkedDescriptorSetID2 = pipelineConfigs[SpecificPipeline::MAIN_RENDER_PIPELINE].linkedDescriptorSetIDs[2];
			const DescriptorSet& currentDescriptorSet2 = descriptorSetsConfig[linkedDescriptorSetID2];
			vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineConfigs[SpecificPipeline::MAIN_RENDER_PIPELINE].pipelineLayout, 2, 1,
									textureDescriptorSet(currentDescriptorSet2, specularTextureIndex), 0, nullptr);
			const unsigned int linkedDescriptorSetID3 = pipelineConfigs[SpecificPipeline::MAIN_RENDER_PIPELINE].linkedDescriptorSetIDs[3];
			const DescriptorSet& currentDescriptorSet3 = descriptorSetsConfig[linkedDescriptorSetID3];
			vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineConfigs[SpecificPipeline::MAIN_RENDER_PIPELINE].pipelineLayout, 3, 1,
									textureDescriptorSet(currentDescriptorSet3, diffuseTextureIndex), 0, nullptr);
			
			vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(indicesContainerSize), 1, 0, 0, 0);
		}
//		}

        vkCmdEndRenderPass(commandBuffer);

        // if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        //     throw std::runtime_error("failed to record command buffer!");
        // }
    }

    void CVulkanRenderer::createSyncObjects(std::vector<VkSemaphore>& imageAvailableSemaphores,
											[[maybe_unused]] std::vector<VkSemaphore>& renderFinishedSemaphores,   ///< Created by createRenderFinishedSemaphores()
											std::vector<VkFence>& inFlightFences) {
        imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
		
        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		
        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
		
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
            if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
                vkCreateFence(device, &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS) {
                throw std::runtime_error("failed to create synchronization objects for a frame!");
            }
        }

		createRenderFinishedSemaphores();
    }

	/// Present semaphores are indexed by swapchain image, one per image.
	void CVulkanRenderer::createRenderFinishedSemaphores() {
        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        renderFinishedSemaphores.resize(swapChainImages.size());
		for (size_t i = 0; i < renderFinishedSemaphores.size(); ++i) {
            if ( vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS ) {
                throw std::runtime_error("failed to create synchronization objects for a frame!");
            }
        }
	}

	void CVulkanRenderer::destroyRenderFinishedSemaphores() {
		for (VkSemaphore& semaphore : renderFinishedSemaphores) {
			vkDestroySemaphore(device, semaphore, nullptr);
		}
		renderFinishedSemaphores.clear();
	}

	/// Uniform buffers are host coherent and stay mapped for the lifetime of the renderer.
    void CVulkanRenderer::mapUniformBuffers() {
		for ( unsigned int descriptorSetCounter = 0; descriptorSetCounter < DescriptorSetDataLink::DESCRIPTOR_CHUNKS_NUMBER; ++descriptorSetCounter ) {
			for ( unsigned int j = 0; j < descriptorSetsConfig[descriptorSetCounter].actualLinkedDescriptorBindingsNumber; ++j ) {
				const DescriptorBinding& binding = descriptorBindingsConfig[descriptorSetsConfig[descriptorSetCounter].descriptorsBindingsIDs[j]];
				if ( binding.vkType != VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER )
					continue;

				GPUBuffer* uniformBuffer = GPUDescriptors[binding.globalDescriptorOffset].GPUBuffer;
				if ( vkMapMemory(device, uniformBuffer->deviceMemory, 0, VK_WHOLE_SIZE, 0, &uniformBuffer->mapedDataPtr) != VK_SUCCESS ) {
					throw std::runtime_error("failed to map uniform buffer memory!");
				}
			}
		}
	}

	/// Number of joint matrices that fit into the uniform block, skins with more joints are truncated.
	static uint32_t clampedJointMatricesNumber( const RenderActor& actor ) {
		return std::min<uint32_t>(actor.jointMatrices.GetSize(), MAX_JOINTS_NUMBER);
	}

    void CVulkanRenderer::updateDirectionalLightShadowMapMatrixUBO(uint32_t currentImage, uint32_t currentLight, unsigned int actor) {
		ShadowMapMatrixUBO *modelMatrixUBO = mappedUBO<ShadowMapMatrixUBO>(DescriptorSetDataLink::SHADOW_MAP_DIRECTIONAL_LIGHT, currentImage);

		modelMatrixUBO->model            = actors[actor].modelMatrix;
		modelMatrixUBO->lightSpaceMatrix = dirLightSpaceMatrix[currentLight];

		memcpy(modelMatrixUBO->jointMatrices, actors[actor].jointMatrices.GetVectorContainer(), clampedJointMatricesNumber(actors[actor]) * sizeof(mat4));
    }

    void CVulkanRenderer::updateSpotLightShadowMapMatrixUBO(uint32_t currentImage, uint32_t currentLight, unsigned int actor) {
		ShadowMapMatrixUBO *modelMatrixUBO = mappedUBO<ShadowMapMatrixUBO>(DescriptorSetDataLink::SHADOW_MAP_SPOT_LIGHT, currentImage);

		modelMatrixUBO->model            = actors[actor].modelMatrix;
		modelMatrixUBO->lightSpaceMatrix = spotLightSpaceMatrix[currentLight];

		memcpy(modelMatrixUBO->jointMatrices, actors[actor].jointMatrices.GetVectorContainer(), clampedJointMatricesNumber(actors[actor]) * sizeof(mat4));
    }

    void CVulkanRenderer::updatePointLightShadowMapMatrixUBO([[maybe_unused]] uint32_t currentImage, uint32_t currentLight, uint32_t layer, unsigned int actor) {
        PointLightShadowMapMatrixUBO *modelMatrixUBO = mappedUBO<PointLightShadowMapMatrixUBO>(DescriptorSetDataLink::SHADOW_MAP_POINT_LIGHT, currentImage);

		modelMatrixUBO->model = actors[actor].modelMatrix;
		
		modelMatrixUBO->lightSpaceMatrix = pointLights[currentLight].pointLightSpaceMatrix[layer];
		modelMatrixUBO->farPlane         = 100.0f;
		modelMatrixUBO->lightPosition    = pointLights[currentLight].position;

		memcpy(modelMatrixUBO->jointMatrices, actors[actor].jointMatrices.GetVectorContainer(), clampedJointMatricesNumber(actors[actor]) * sizeof(mat4));
    }

    void CVulkanRenderer::updateMatrixUniformBuffer(uint32_t offset, unsigned int actor) {
		ModelMatrixUBO* modelMatrixUBO = mappedUBO<ModelMatrixUBO>(DescriptorSetDataLink::MAIN_RENDER_MATRIX_UBO, offset);

		modelMatrixUBO->model = actors[actor].modelMatrix;
        modelMatrixUBO->view  = viewMatrix;
        modelMatrixUBO->proj  = projectionMatrix;

		memcpy(modelMatrixUBO->jointMatrices, actors[actor].jointMatrices.GetVectorContainer(), clampedJointMatricesNumber(actors[actor]) * sizeof(mat4));
		memcpy(modelMatrixUBO->dirSpaceMatrix, dirLightSpaceMatrix, DIRECTIONAL_LIGHTS_NUMBER * sizeof(mat4));
		memcpy(modelMatrixUBO->spotSpaceMatrix, spotLightSpaceMatrix, SPOT_LIGHTS_NUMBER * sizeof(mat4));
		
		modelMatrixUBO->ambient                 = actors[actor].ambient;
		modelMatrixUBO->shininess               = actors[actor].shininess;
		modelMatrixUBO->directionalLightsNumber = directionalLightNumber;
		modelMatrixUBO->spotLightsNumber        = spotLightNumber;
    }

	void CVulkanRenderer::updateViewPositionUniformBuffer( uint32_t currentImage, uint32_t player ) {
		LightData* lightDataUBO = mappedUBO<LightData>(DescriptorSetDataLink::MAIN_RENDER_LIGHT_DATA_UBO, currentImage);

		/// Light numbers are set (and clamped to the array sizes) at the start of mainRenderDrawFrame.
		lightDataUBO->viewPosition = players[player].position;
		for ( unsigned int i = 0; i < directionalLightNumber; ++i ) {
			lightDataUBO->directionalLights[i].position  = directionalLights[i].position;
			lightDataUBO->directionalLights[i].direction = directionalLights[i].direction;
			lightDataUBO->directionalLights[i].ambient   = directionalLights[i].ambient;
			lightDataUBO->directionalLights[i].diffuse   = directionalLights[i].diffuse;
			lightDataUBO->directionalLights[i].specular  = directionalLights[i].specular;
		}
		lightDataUBO->directionalLightsArraySize = directionalLightNumber;

		for ( unsigned int i = 0; i < pointLightNumber; ++i ) {
 			lightDataUBO->pointLights[i].position  = pointLights[i].position;
			lightDataUBO->pointLights[i].ambient   = pointLights[i].ambient;
			lightDataUBO->pointLights[i].diffuse   = pointLights[i].diffuse;
			lightDataUBO->pointLights[i].specular  = pointLights[i].specular;
			lightDataUBO->pointLights[i].constant  = pointLights[i].constant;
			lightDataUBO->pointLights[i].linear    = pointLights[i].linear;
			lightDataUBO->pointLights[i].quadratic = pointLights[i].quadratic;
		}
		lightDataUBO->pointLightsArraySize = pointLightNumber;
		lightDataUBO->farPlane = 100.0f;

		for ( unsigned int i = 0; i < spotLightNumber; ++i ) {
			lightDataUBO->spotLights[i].position    = spotLights[i].position;
			lightDataUBO->spotLights[i].direction   = spotLights[i].direction;
			lightDataUBO->spotLights[i].cutOff      = std::cos(Radians(spotLights[i].cutOff));
			lightDataUBO->spotLights[i].outerCutOff = std::cos(Radians(spotLights[i].outerCutOff));
			lightDataUBO->spotLights[i].ambient     = spotLights[i].ambient;
			lightDataUBO->spotLights[i].diffuse     = spotLights[i].diffuse;
			lightDataUBO->spotLights[i].specular    = spotLights[i].specular;
			lightDataUBO->spotLights[i].constant    = spotLights[i].constant;
			lightDataUBO->spotLights[i].linear      = spotLights[i].linear;
			lightDataUBO->spotLights[i].quadratic   = spotLights[i].quadratic; 
		}
		lightDataUBO->spotLightArraySize = spotLightNumber;

		if( print == true ) {
		/// The indirect texture is generated once, the random generator is only needed here.
		std::random_device rd;
		std::mt19937 mersenne(rd());
		std::uniform_int_distribution<int> distributionTileIndex(0, INDIRECT_TEXTURE_HEIGHT * INDIRECT_TEXTURE_WIDTH);

		for( int i = 0; i < INDIRECT_TEXTURE_HEIGHT * INDIRECT_TEXTURE_WIDTH / 4 + 1; ++i )
			for( int j = 0; j < 4; ++j ) {
				int randomTileIndex = distributionTileIndex(mersenne);
//				randomTileIndex = 20;
				indirectTexture[i][j] = randomTileIndex;
				// if( print )
				// 	std::cout << "element: " << i * 4 + j << " value: " << indirectTexture[i][j] << std::endl;
				
				// std::cout << "index: " << i * INDIRECT_TEXTURE_WIDTH * 4 + j * 4 + 3 << std::endl;
				// lightDataUBO.indirectTexture[i * INDIRECT_TEXTURE_WIDTH * 4 + j * 4 + 3] = randomTileIndex;
//				std::cout << "element: " << i * INDIRECT_TEXTURE_WIDTH + j << " equal: " << lightDataUBO.indirectTexture[i * INDIRECT_TEXTURE_WIDTH + j] << std::endl;
			}
		}
		print = false;
		// if( print == true )
		// 	print = false;

		lightDataUBO->tilesetTilesCount = vec2(TILESET_ROW, TILESET_COLUMN);
		lightDataUBO->tilesRaw = 8;
		lightDataUBO->tilesColumn = 8;
		
		memcpy(lightDataUBO->indirectTexture, indirectTexture, (INDIRECT_TEXTURE_HEIGHT * INDIRECT_TEXTURE_WIDTH / 4 + 1) * sizeof(Vector<int, 4>));
	}

    void CVulkanRenderer::mainRenderDrawFrame() {
		namespace cm = GLVM::ecs::components;
        vkWaitForFences(device, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);

		/// Without a fixed surface extent (Wayland) the window size change is not reported by the swapchain.
		if ( swapChainExtentFollowsWindow && Window->width != 0 && Window->height != 0 &&
			 ( static_cast<uint32_t>(Window->width) != swapChainWindowSize.width || static_cast<uint32_t>(Window->height) != swapChainWindowSize.height ) ) {
			framebufferResized = true;
		}
		if ( framebufferResized || swapChainRecreatePending ) {
			framebufferResized = false;
			recreateSwapChain();
			if ( swapChainRecreatePending )
				return;                                      ///< Zero sized window, nothing to render into
		}

		/// Light numbers are clamped to the sizes of the shadow map and light arrays, extra lights are ignored.
		directionalLightNumber = std::min<unsigned int>(directionalLights.GetSize(), DIRECTIONAL_LIGHTS_NUMBER);
		spotLightNumber        = std::min<unsigned int>(spotLights.GetSize(), SPOT_LIGHTS_NUMBER);
		pointLightNumber       = std::min<unsigned int>(pointLights.GetSize(), POINT_LIGHTS_NUMBER);
		ensureShadowMaps();

        uint32_t imageIndex;
		/* vkAcquireNextImageKHR give index of image that WILL BE SOON available for rendering and signal imageAvailablesemaphore when its so.
		   GraphicsQueue waint for this semaphore bacause we pass it in submitInfo.
		 */
        VkResult result = vkAcquireNextImageKHR(device, swapChain, UINT64_MAX, imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);

        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            recreateSwapChain();
            return;
        } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
            throw std::runtime_error("failed to acquire swap chain image!");
        }

        vkResetFences(device, 1, &inFlightFences[currentFrame]);
        vkResetCommandBuffer(mainRenderCommandBuffers[currentFrame], /*VkCommandBufferResetFlagBits*/ 0);
		// directionalLightRecordCoomandBuffer(directionalLightSecondaryCommandBuffers, currentFrame);
		// spotLightRecordCommandBuffer(spotLightSecondaryCommandBuffers, currentFrame);
		// pointLightRecordCommandBuffer(pointLightSecondaryCommandBuffers, currentFrame);

		auto future1 = renderThreadPool->enqueue([this]() {
			directionalLightRecordCoomandBuffer(directionalLightSecondaryCommandBuffers, this->currentFrame);
		});
    
		auto future2 = renderThreadPool->enqueue([this]() {
			spotLightRecordCommandBuffer(spotLightSecondaryCommandBuffers, this->currentFrame);
		});
    
		auto future3 = renderThreadPool->enqueue([this]() {
			pointLightRecordCommandBuffer(pointLightSecondaryCommandBuffers, this->currentFrame);
		});
    
		/// get() rethrows exceptions of the recording threads.
		future1.get();
		future2.get();
		future3.get();

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

        if (vkBeginCommandBuffer(mainRenderCommandBuffers[currentFrame], &beginInfo) != VK_SUCCESS) {
            throw std::runtime_error("failed to begin recording command buffer!");
        }
		const VkExtent2D shadowMapExtent = { SHADOW_MAP_SIZE, SHADOW_MAP_SIZE };
		for ( uint32_t directionalLightCounter = 0; directionalLightCounter < directionalLightNumber; ++ directionalLightCounter ) {
			executeSecondaryCommandBuffer( renderPasses[SpecificPipeline::DIRECTIONAL_LIGHT_PIPELINE], directionalLightShadowMapFrameBuffers[directionalLightCounter],
										   shadowMapExtent, mainRenderCommandBuffers[currentFrame], directionalLightSecondaryCommandBuffers[currentFrame * DIRECTIONAL_LIGHTS_NUMBER + directionalLightCounter] );
		}
		for ( uint32_t spotLightCounter = 0; spotLightCounter < spotLightNumber; ++ spotLightCounter ) {
			executeSecondaryCommandBuffer( renderPasses[SpecificPipeline::SPOT_LIGHT_PIPELINE], spotLightShadowMapFrameBuffers[spotLightCounter],
										   shadowMapExtent, mainRenderCommandBuffers[currentFrame], spotLightSecondaryCommandBuffers[currentFrame * SPOT_LIGHTS_NUMBER + spotLightCounter] );
		}
		for ( uint32_t pointLightCounter = 0; pointLightCounter < pointLightNumber; ++pointLightCounter ) {
			for ( uint32_t cubeMapLayerCounter = 0; cubeMapLayerCounter < CUBE_MAP_LAYER_NUMBER; ++cubeMapLayerCounter ) {
				executeSecondaryCommandBuffer( renderPasses[SpecificPipeline::POINT_LIGHT_PIPELINE], pointLightShadowMapFrameBuffers[pointLightCounter][cubeMapLayerCounter],
											   shadowMapExtent, mainRenderCommandBuffers[currentFrame],
											   pointLightSecondaryCommandBuffers[(currentFrame * POINT_LIGHTS_NUMBER + pointLightCounter) * CUBE_MAP_LAYER_NUMBER + cubeMapLayerCounter] );
			}
		}
		
		if ( fluidTank )
			recordFluidTank(mainRenderCommandBuffers[currentFrame], imageIndex, true);
        recordCommandBuffer(mainRenderCommandBuffers[currentFrame], imageIndex);
		if ( fluidTank )
			recordFluidTank(mainRenderCommandBuffers[currentFrame], imageIndex, false);
		hudRecordCommandBuffer(mainRenderCommandBuffers[currentFrame], imageIndex);
		fontRecordCommandBuffer(mainRenderCommandBuffers[currentFrame], imageIndex);
		if ( isInventoryOpened ) {
			uiRecordCommandBuffer(mainRenderCommandBuffers[currentFrame], imageIndex);
			uiIconsRecordCommandBuffer(mainRenderCommandBuffers[currentFrame], imageIndex);
		}

		if( isDebugCollisitionsActive ) {
			collisionsDebugRecordCommandBuffer(mainRenderCommandBuffers[currentFrame], imageIndex);
		}

		hudScreenRecordCommandBuffer(mainRenderCommandBuffers[currentFrame], imageIndex);
		mathObjectsDebugRecordCommandBuffer(mainRenderCommandBuffers[currentFrame], imageIndex);
//		sdfRecordCommandBuffer(mainRenderCommandBuffers[currentFrame], imageIndex);

//		spacialGridDebugRecordCommandBuffer(mainRenderCommandBuffers[currentFrame], imageIndex);

		VkBuffer       screenshotBuffer = VK_NULL_HANDLE;
		VkDeviceMemory screenshotMemory = VK_NULL_HANDLE;
		if ( canCopySwapChainImages && screenshotFrame >= 0 && renderedFramesNumber == static_cast<uint64_t>(screenshotFrame) ) {
			createBuffer( static_cast<VkDeviceSize>(swapChainExtent.width) * swapChainExtent.height * 4, VK_BUFFER_USAGE_TRANSFER_DST_BIT,
						  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, screenshotBuffer, screenshotMemory );
			recordSwapChainCopy( mainRenderCommandBuffers[currentFrame], imageIndex, screenshotBuffer );
		}
		++renderedFramesNumber;

        if (vkEndCommandBuffer(mainRenderCommandBuffers[currentFrame]) != VK_SUCCESS) {
            throw std::runtime_error("failed to record command buffer!");
        }

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

		/// GraphicsQueue wait for swapchain image when its become available.
        VkSemaphore waitSemaphores[] = {imageAvailableSemaphores[currentFrame]};
        VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores;
        submitInfo.pWaitDstStageMask = waitStages;

        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &mainRenderCommandBuffers[currentFrame];

        VkSemaphore signalSemaphores[] = {renderFinishedSemaphores[imageIndex]};
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSemaphores;

        if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFences[currentFrame]) != VK_SUCCESS) {
            throw std::runtime_error("failed to submit draw command buffer!");
        }
		if ( screenshotBuffer != VK_NULL_HANDLE ) {
			vkWaitForFences(device, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);
			saveScreenshot( screenshotBuffer, screenshotMemory );
		}

        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = signalSemaphores;

        VkSwapchainKHR swapChains[] = {swapChain};
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = swapChains;

        presentInfo.pImageIndices = &imageIndex;

        result = vkQueuePresentKHR(presentQueue, &presentInfo);

        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || framebufferResized) {
            framebufferResized = false;
            recreateSwapChain();
        } else if (result != VK_SUCCESS) {
            throw std::runtime_error("failed to present swap chain image!");
        }

        currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
//		currentFrame = 0;
    }

	bool CVulkanRenderer::isDiscreteGpu() const {
		VkPhysicalDeviceProperties properties;
		vkGetPhysicalDeviceProperties(physicalDevice, &properties);
		return properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
	}

	/// Creates the water tank after the renderer is initialized (run()). Returns false when the GPU can't run it.
	bool CVulkanRenderer::createFluidTank( const fluid::FluidTankDescription& description ) {
		if ( !isDynamicRenderingEnabled || !canCopySwapChainImages ) {
			std::cout << "Water tank skipped: it needs Vulkan 1.3 dynamic rendering and copyable swapchain images" << std::endl;
			return false;
		}
		try {
			fluidContext = std::make_unique<fluid::GpuContext>();
			fluidContext->physicalDevice  = physicalDevice;
			fluidContext->device          = device;
			fluidContext->queue           = graphicsQueue;
			fluidContext->queueFamily     = findQueueFamilies(physicalDevice).graphicsFamily.value();
			fluidContext->shaderDirectory = "../VKshaders/fluid/";
			vkGetPhysicalDeviceProperties(physicalDevice, &fluidContext->properties);
			vkGetPhysicalDeviceMemoryProperties(physicalDevice, &fluidContext->memoryProperties);
			createCommandPool(fluidContext->commandPool);

			fluidTank = std::make_unique<fluid::FluidTank>( *fluidContext, description, swapChainImageFormat, findDepthFormat() );
			fluidTank->setTargets( swapChainExtent.width, swapChainExtent.height, mainDepthImageView );
			std::cout << "Water tank: " << fluidTank->particleCount() << " particles" << std::endl;
			return true;
		} catch ( const std::exception& error ) {
			std::cerr << "Water tank disabled: " << error.what() << std::endl;
			destroyFluidTank();
			return false;
		}
	}

	void CVulkanRenderer::destroyFluidTank() {
		fluidTank.reset();
		if ( fluidContext ) {
			if ( fluidContext->commandPool != VK_NULL_HANDLE )
				vkDestroyCommandPool(device, fluidContext->commandPool, nullptr);
			fluidContext.reset();
		}
	}

	/*
	  Water tank commands: the simulation step before the main pass (isSimulation), the tank over the main pass after it.
	  Both only when the tank is in the view: out of the view the water waits.
	*/
	void CVulkanRenderer::recordFluidTank( VkCommandBuffer commandBuffer, uint32_t imageIndex, bool isSimulation ) {
		const fluid::Vec3 low = fluidTank->boundsMin(), high = fluidTank->boundsMax();
		const AABB bounds = { .origin  = vec3( 0.5f * (low.x + high.x), 0.5f * (low.y + high.y), 0.5f * (low.z + high.z) ),
							  .extents = vec3( 0.5f * (high.x - low.x), 0.5f * (high.y - low.y), 0.5f * (high.z - low.z) ) };
		if ( !::isFrustumIntersect( mainCameraFrustum, bounds ) )
			return;
		if ( isSimulation ) {
			fluidTank->recordSimulation( commandBuffer, fluidFrameTime );
			return;
		}

		fluid::FluidTankCamera camera;
		for ( int column = 0; column < 4; ++column )
			for ( int row = 0; row < 4; ++row ) {
				camera.view.at( row, column )       = viewMatrix[column][row];
				camera.projection.at( row, column ) = projectionMatrix[column][row];
			}
		/// Perspective parameters from the matrix: P22 = f / (n - f), P32 = f n / (n - f), P11 = 1 / tan(fov / 2).
		const float p22 = camera.projection.at( 2, 2 ), p32 = camera.projection.at( 2, 3 );
		camera.nearPlane   = p32 / p22;
		camera.farPlane    = p32 / (p22 + 1.0f);
		camera.verticalFov = 2.0f * std::atan( 1.0f / std::fabs( camera.projection.at( 1, 1 ) ) );
		if ( directionalLights.GetSize() > 0 ) {
			const vec4& direction = directionalLights[0].direction;          ///< Direction the light shines in
			camera.lightDirection = fluid::normalize( { -direction[0], -direction[1], -direction[2] } );
		}
		camera.lightColor       = { 1.0f, 0.97f, 0.92f };
		camera.lightIntensity   = 0.9f;
		camera.ambientColor     = { 0.32f, 0.32f, 0.34f };
		camera.environmentColor = { 0.3f, 0.3f, 0.32f };                  ///< The gray level around

		fluid::FluidTankTarget target;
		target.colorImage  = swapChainImages[imageIndex];
		target.colorView   = swapChainImageViews[imageIndex];
		target.colorLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;                  ///< Final layout of the main pass, initial of the HUD pass
		target.depthImage  = mainDepthPipelineImage;
		target.depthAspect = VK_IMAGE_ASPECT_DEPTH_BIT | (hasStencilComponent(findDepthFormat()) ? VK_IMAGE_ASPECT_STENCIL_BIT : 0u);
		target.depthLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		fluidTank->recordRendering( commandBuffer, target, camera );
	}

	/// Copies the finished swapchain image (PRESENT_SRC_KHR before and after) into a buffer.
	void CVulkanRenderer::recordSwapChainCopy( VkCommandBuffer commandBuffer, uint32_t imageIndex, VkBuffer destination ) {
		VkImageMemoryBarrier barrier{};
		barrier.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.image               = swapChainImages[imageIndex];
		barrier.subresourceRange    = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
		barrier.oldLayout           = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		barrier.newLayout           = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		barrier.srcAccessMask       = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		barrier.dstAccessMask       = VK_ACCESS_TRANSFER_READ_BIT;
		vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
							 0, 0, nullptr, 0, nullptr, 1, &barrier);

		VkBufferImageCopy region{};
		region.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
		region.imageExtent      = { swapChainExtent.width, swapChainExtent.height, 1 };
		vkCmdCopyImageToBuffer(commandBuffer, swapChainImages[imageIndex], VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, destination, 1, &region);

		barrier.oldLayout     = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		barrier.newLayout     = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		barrier.dstAccessMask = 0;
		vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
							 0, 0, nullptr, 0, nullptr, 1, &barrier);
	}

	/// Writes the copied frame (the GPU work is finished) as a PNG and frees the buffer.
	void CVulkanRenderer::saveScreenshot( VkBuffer buffer, VkDeviceMemory memory ) {
		const size_t pixelsNumber = static_cast<size_t>(swapChainExtent.width) * swapChainExtent.height;
		void* mapped = nullptr;
		vkMapMemory(device, memory, 0, VK_WHOLE_SIZE, 0, &mapped);
		const uint8_t* pixels = static_cast<const uint8_t*>(mapped);
		const bool isBgra = swapChainImageFormat == VK_FORMAT_B8G8R8A8_SRGB || swapChainImageFormat == VK_FORMAT_B8G8R8A8_UNORM;
		std::vector<uint8_t> rgb( pixelsNumber * 3 );
		for ( size_t i = 0; i < pixelsNumber; ++i ) {
			rgb[i * 3 + 0] = pixels[i * 4 + (isBgra ? 2 : 0)];
			rgb[i * 3 + 1] = pixels[i * 4 + 1];
			rgb[i * 3 + 2] = pixels[i * 4 + (isBgra ? 0 : 2)];
		}
		vkUnmapMemory(device, memory);
		vkDestroyBuffer(device, buffer, nullptr);
		vkFreeMemory(device, memory, nullptr);
		if ( writePng( screenshotPath, swapChainExtent.width, swapChainExtent.height, rgb ) )
			std::cout << "Screenshot: " << screenshotPath << std::endl;
		else
			std::cerr << "Can't write the screenshot " << screenshotPath << std::endl;
	}

    void CVulkanRenderer::directionalLightShadowMapDrawFrame() {
		namespace cm = GLVM::ecs::components;
        vkWaitForFences(device, 1, &directionalLightShadowMapInFlightFences[directionalLightCurrentFrame], VK_TRUE, UINT64_MAX);

        [[maybe_unused]] uint32_t imageIndex = 0;

        vkResetFences(device, 1, &directionalLightShadowMapInFlightFences[directionalLightCurrentFrame]);
        vkResetCommandBuffer(directionalLightCommandBuffers[directionalLightCurrentFrame], /*VkCommandBufferResetFlagBits*/ 0);
//        directionalLightRecordCoomandBuffer(directionalLightCommandBuffers[directionalLightCurrentFrame], imageIndex);

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &directionalLightCommandBuffers[directionalLightCurrentFrame];

        if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, directionalLightShadowMapInFlightFences[directionalLightCurrentFrame]) != VK_SUCCESS) {
            throw std::runtime_error("failed to submit draw command buffer!");
        }

        directionalLightCurrentFrame = (directionalLightCurrentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    }

	void CVulkanRenderer::spotLightShadowMapDrawFrame() {
		namespace cm = GLVM::ecs::components;
        vkWaitForFences(device, 1, &spotLightShadowMapInFlightFences[spotLightCurrentFrame], VK_TRUE, UINT64_MAX);

        [[maybe_unused]] uint32_t imageIndex = 0;

        vkResetFences(device, 1, &spotLightShadowMapInFlightFences[spotLightCurrentFrame]);
        vkResetCommandBuffer(spotLightCommandBuffers[spotLightCurrentFrame], /*VkCommandBufferResetFlagBits*/ 0);
//        spotLightRecordCommandBuffer(spotLightCommandBuffers[spotLightCurrentFrame], imageIndex);

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &spotLightCommandBuffers[spotLightCurrentFrame];

        if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, spotLightShadowMapInFlightFences[spotLightCurrentFrame]) != VK_SUCCESS) {
            throw std::runtime_error("failed to submit draw command buffer!");
        }

        spotLightCurrentFrame = (spotLightCurrentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    }

	    void CVulkanRenderer::pointLightShadowMapDrawFrame() {
		namespace cm = GLVM::ecs::components;
        vkWaitForFences(device, 1, &pointLightShadowMapInFlightFences[pointLightCurrentFrame], VK_TRUE, UINT64_MAX);

        [[maybe_unused]] uint32_t imageIndex = 0;

        vkResetFences(device, 1, &pointLightShadowMapInFlightFences[pointLightCurrentFrame]);
        vkResetCommandBuffer(pointLightCommandBuffers[pointLightCurrentFrame], /*VkCommandBufferResetFlagBits*/ 0);
//        pointLightRecordCommandBuffer(pointLightCommandBuffers[pointLightCurrentFrame], imageIndex);

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &pointLightCommandBuffers[pointLightCurrentFrame];

        if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, pointLightShadowMapInFlightFences[pointLightCurrentFrame]) != VK_SUCCESS) {
            throw std::runtime_error("failed to submit draw command buffer!");
        }

        pointLightCurrentFrame = (pointLightCurrentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    }
	
		void CVulkanRenderer::directionalLightRecordCoomandBuffer(std::vector<VkCommandBuffer>& commandBuffers, [[maybe_unused]] uint32_t currentFrame) {
		const u32 perFrameUboNumber = perFrameDescriptorNumber(DescriptorSetDataLink::SHADOW_MAP_DIRECTIONAL_LIGHT);
		for ( uint32_t directionalLightCounter = 0; directionalLightCounter < directionalLightNumber; ++ directionalLightCounter ) {
			VkCommandBufferInheritanceInfo inheritanceInfo{};
			inheritanceInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
			inheritanceInfo.renderPass = renderPasses[SpecificPipeline::DIRECTIONAL_LIGHT_PIPELINE];
			inheritanceInfo.framebuffer = directionalLightShadowMapFrameBuffers[directionalLightCounter];
			inheritanceInfo.subpass = 0;

			VkCommandBufferBeginInfo beginInfo{};
			beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			beginInfo.flags = VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT |
				VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
			beginInfo.pInheritanceInfo = &inheritanceInfo;

			VkCommandBuffer commandBuffer = commandBuffers[currentFrame * DIRECTIONAL_LIGHTS_NUMBER + directionalLightCounter];
			if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
				throw std::runtime_error("failed to begin recording command buffer!");
			}

			// VkClearValue shadowMapClearValues[1];
			// shadowMapClearValues[0].depthStencil.depth = 1.0f;
			// shadowMapClearValues[0].depthStencil.stencil = 0;

			// VkRenderPassBeginInfo shadowMapRenderPassInfo{};
			// shadowMapRenderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			// shadowMapRenderPassInfo.pNext = NULL;
			// shadowMapRenderPassInfo.renderPass = renderPasses[SpecificPipeline::DIRECTIONAL_LIGHT_PIPELINE];
			// shadowMapRenderPassInfo.framebuffer = directionalLightShadowMapFrameBuffers[directionalLightCounter];
			// shadowMapRenderPassInfo.renderArea.offset.x = 0;
			// shadowMapRenderPassInfo.renderArea.offset.y = 0;
			// shadowMapRenderPassInfo.renderArea.extent.width = swapChainExtent.width;
			// shadowMapRenderPassInfo.renderArea.extent.height = swapChainExtent.height;
			// shadowMapRenderPassInfo.clearValueCount = 1;
			// shadowMapRenderPassInfo.pClearValues = shadowMapClearValues;

			// vkCmdBeginRenderPass(commandBuffer, &shadowMapRenderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

			VkViewport shadowMapViewPort;
			shadowMapViewPort.height = SHADOW_MAP_SIZE;
			shadowMapViewPort.width = SHADOW_MAP_SIZE;
			shadowMapViewPort.minDepth = 0.0f;
			shadowMapViewPort.maxDepth = 1.0f;
			shadowMapViewPort.x = 0;
			shadowMapViewPort.y = 0;
			vkCmdSetViewport(commandBuffer, 0, 1, &shadowMapViewPort);

			VkRect2D shadowMapScissor;
			shadowMapScissor.extent.width = SHADOW_MAP_SIZE;
			shadowMapScissor.extent.height = SHADOW_MAP_SIZE;
			shadowMapScissor.offset.x = 0;
			shadowMapScissor.offset.y = 0;
			vkCmdSetScissor(commandBuffer, 0, 1, &shadowMapScissor);

			vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineConfigs[SpecificPipeline::DIRECTIONAL_LIGHT_PIPELINE].pipeline);
			dirLightSpaceMatrix[directionalLightCounter] = directionalLights[directionalLightCounter].DirectionalLightSpaceMatrix;
			
			uint32_t actorsNumber = actors.GetSize();
			for ( unsigned int actorCounter = 0; actorCounter < actorsNumber; ++actorCounter ) {
				const RenderActor& actor = actors[actorCounter];
				unsigned int meshId = actor.meshID;

				/// Slots [currentFrame * perFrameUboNumber, (currentFrame + 1) * perFrameUboNumber) belong to this frame in flight.
				const u32 localUboIndex = actorsNumber * directionalLightCounter + actorCounter;
				if ( localUboIndex >= perFrameUboNumber ) {
					reportUboOverflow(SpecificPipeline::DIRECTIONAL_LIGHT_PIPELINE);
					break;
				}
				unsigned int uboDirectionalLightIndex = perFrameUboNumber * currentFrame + localUboIndex;

				updateDirectionalLightShadowMapMatrixUBO(uboDirectionalLightIndex, directionalLightCounter, actorCounter);
				const unsigned int linkedDescriptorSetID = pipelineConfigs[SpecificPipeline::DIRECTIONAL_LIGHT_PIPELINE].linkedDescriptorSetIDs[0];
				const DescriptorSet& currentDescriptorSet = descriptorSetsConfig[linkedDescriptorSetID];
				vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineConfigs[SpecificPipeline::DIRECTIONAL_LIGHT_PIPELINE].pipelineLayout, 0, 1,
										&(*(descriptorSetsChunks.GetVectorContainer() + currentDescriptorSet.descriptorSetOffset + uboDirectionalLightIndex)), 0, nullptr);
				
				VkBuffer vertexBuffers[] = {vertexBufferContainer[meshId]};
				VkDeviceSize offsets[] = {0};
				vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

				vkCmdBindIndexBuffer(commandBuffer, indexBufferContainer[meshId], 0, VK_INDEX_TYPE_UINT32);

				unsigned int indicesContainerSize = aIndices_[meshId].size();
				vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(indicesContainerSize), 1, 0, 0, 0);
			}

			if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
				throw std::runtime_error("failed to record command buffer!");
			}
//			vkCmdEndRenderPass(commandBuffer);
		}
	}

		void CVulkanRenderer::spotLightRecordCommandBuffer(std::vector<VkCommandBuffer>& commandBuffers, [[maybe_unused]] uint32_t currentFrame) {
		const u32 perFrameUboNumber = perFrameDescriptorNumber(DescriptorSetDataLink::SHADOW_MAP_SPOT_LIGHT);
		for ( uint32_t spotLightCounter = 0; spotLightCounter < spotLightNumber; ++ spotLightCounter ) {
			VkCommandBufferInheritanceInfo inheritanceInfo{};
			inheritanceInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
			inheritanceInfo.renderPass = renderPasses[SpecificPipeline::SPOT_LIGHT_PIPELINE];
			inheritanceInfo.framebuffer = spotLightShadowMapFrameBuffers[spotLightCounter];
			inheritanceInfo.subpass = 0;
			
			VkCommandBufferBeginInfo beginInfo{};
			beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			beginInfo.flags = VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT |
				VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
			beginInfo.pInheritanceInfo = &inheritanceInfo;

			VkCommandBuffer commandBuffer = commandBuffers[currentFrame * SPOT_LIGHTS_NUMBER + spotLightCounter];
			if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
				throw std::runtime_error("failed to begin recording command buffer!");
			}

			// VkClearValue spotLightShadowMapClearValues[1];
			// spotLightShadowMapClearValues[0].depthStencil.depth = 1.0f;
			// spotLightShadowMapClearValues[0].depthStencil.stencil = 0;

			// VkRenderPassBeginInfo spotLightShadowMapRenderPassInfo{};
			// spotLightShadowMapRenderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			// spotLightShadowMapRenderPassInfo.pNext = NULL;
			// spotLightShadowMapRenderPassInfo.renderPass = renderPasses[SpecificPipeline::SPOT_LIGHT_PIPELINE];
			// spotLightShadowMapRenderPassInfo.framebuffer = spotLightShadowMapFrameBuffers[spotLightCounter];
			// spotLightShadowMapRenderPassInfo.renderArea.offset.x = 0;
			// spotLightShadowMapRenderPassInfo.renderArea.offset.y = 0;
			// spotLightShadowMapRenderPassInfo.renderArea.extent.width = swapChainExtent.width;
			// spotLightShadowMapRenderPassInfo.renderArea.extent.height = swapChainExtent.height;
			// spotLightShadowMapRenderPassInfo.clearValueCount = 1;
			// spotLightShadowMapRenderPassInfo.pClearValues = spotLightShadowMapClearValues;

			// vkCmdBeginRenderPass(commandBuffer, &spotLightShadowMapRenderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

			VkViewport spotLightShadowMapViewPort;
			spotLightShadowMapViewPort.height = SHADOW_MAP_SIZE;
			spotLightShadowMapViewPort.width = SHADOW_MAP_SIZE;
			spotLightShadowMapViewPort.minDepth = 0.0f;
			spotLightShadowMapViewPort.maxDepth = 1.0f;
			spotLightShadowMapViewPort.x = 0;
			spotLightShadowMapViewPort.y = 0;
			vkCmdSetViewport(commandBuffer, 0, 1, &spotLightShadowMapViewPort);

			VkRect2D spotLightShadowMapScissor;
			spotLightShadowMapScissor.extent.width = SHADOW_MAP_SIZE;
			spotLightShadowMapScissor.extent.height = SHADOW_MAP_SIZE;
			spotLightShadowMapScissor.offset.x = 0;
			spotLightShadowMapScissor.offset.y = 0;
			vkCmdSetScissor(commandBuffer, 0, 1, &spotLightShadowMapScissor);

			vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineConfigs[SpecificPipeline::SPOT_LIGHT_PIPELINE].pipeline);

			spotLightSpaceMatrix[spotLightCounter] = spotLights[spotLightCounter].SpotLigthSpaceMatrix; 
			uint32_t actorsNumber = actors.GetSize();
			for ( unsigned int actorsCounter = 0; actorsCounter < actorsNumber; ++actorsCounter ) {
				const RenderActor& actor = actors[actorsCounter];
				unsigned int meshID = actor.meshID;
				/// Slots [currentFrame * perFrameUboNumber, (currentFrame + 1) * perFrameUboNumber) belong to this frame in flight.
				const u32 localUboIndex = actorsNumber * spotLightCounter + actorsCounter;
				if ( localUboIndex >= perFrameUboNumber ) {
					reportUboOverflow(SpecificPipeline::SPOT_LIGHT_PIPELINE);
					break;
				}
				unsigned int uboSpotLightIndex = perFrameUboNumber * currentFrame + localUboIndex;

				updateSpotLightShadowMapMatrixUBO(uboSpotLightIndex, spotLightCounter, actorsCounter);
				const unsigned int linkedDescriptorSetID = pipelineConfigs[SpecificPipeline::SPOT_LIGHT_PIPELINE].linkedDescriptorSetIDs[0];
				const DescriptorSet& currentDescriptorSet = descriptorSetsConfig[linkedDescriptorSetID];
				vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineConfigs[SpecificPipeline::SPOT_LIGHT_PIPELINE].pipelineLayout, 0, 1,
										&(*(descriptorSetsChunks.GetVectorContainer() + currentDescriptorSet.descriptorSetOffset + uboSpotLightIndex)), 0, nullptr);
				VkBuffer vertexBuffers[] = {vertexBufferContainer[meshID]};
				VkDeviceSize offsets[] = {0};
				vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

				vkCmdBindIndexBuffer(commandBuffer, indexBufferContainer[meshID], 0, VK_INDEX_TYPE_UINT32);

				unsigned int indicesContainerSize = aIndices_[meshID].size();
				vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(indicesContainerSize), 1, 0, 0, 0);
			}

			if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
				throw std::runtime_error("failed to record command buffer!");
			}
//			vkCmdEndRenderPass(commandBuffer);
		}
	}

		void CVulkanRenderer::pointLightRecordCommandBuffer(std::vector<VkCommandBuffer>& commandBuffers, [[maybe_unused]] uint32_t currentFrame) {

// 		if ( entityManager->isEntitiesCollectionChanged && componentManager->isComponentsCollectionChanged ) {

// 			core::vector<unsigned int> linkedEntities;
// 			for ( unsigned int i = 0; i < linkedEntitiesTemp.GetSize(); ++i ) {
// 				unsigned int entity = linkedEntitiesTemp[i];
// 				for ( unsigned int j = 0; j < pointLightEntities.GetSize(); ++j ) {
// 					if ( entity == pointLightEntities[j] ) {
// 						break;
// 					} else if ( entity != pointLightEntities[j] && j == pointLightEntities.GetSize() - 1 ) {
// 						linkedEntities.Push(entity);
// 					}
// 				}
// 			}
// //			std::cout << "number of actors: " << linkedEntities.GetSize() << std::endl;
// 			entitiesCollectionLinked__Trn_Mat_Mes_Act.clear();
// 			for ( unsigned int i = 0; i < linkedEntities.GetSize(); ++i )
// 				entitiesCollectionLinked__Trn_Mat_Mes_Act.Push(linkedEntities[i]);
				
// 			entitiesCollectionLinked__Trn_PoL_Mes_Act.clear();
// 			for ( unsigned int i = 0; i < pointLightEntities.GetSize(); ++i )
// 				entitiesCollectionLinked__Trn_PoL_Mes_Act.Push(pointLightEntities[i]);

// 			entityManager->isEntitiesCollectionChanged = false;
// 			componentManager->isComponentsCollectionChanged = false;
// 		}

		// VkDebugUtilsLabelEXT label;
		// label.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
		// label.color[0] = 0.1;
		// label.color[0] = 0.7;
		// label.color[0] = 0.2;
		// label.color[0] = 1.0;
		// label.pLabelName = "pointLightShadowMap";
		// label.pNext = NULL;
		
		// vkDebugUtils::CreateBeginDebugUtilsLabelEXT(instance, commandBuffers[0], &label);
		const u32 perFrameUboNumber = perFrameDescriptorNumber(DescriptorSetDataLink::SHADOW_MAP_POINT_LIGHT);
		for ( uint32_t pointLightCounter = 0; pointLightCounter < pointLightNumber; ++pointLightCounter ) {
			uint32_t maxCubeMapLayers = CUBE_MAP_LAYER_NUMBER;
			for ( uint32_t cubeMapLayerCounter = 0; cubeMapLayerCounter < maxCubeMapLayers; ++cubeMapLayerCounter ) {                      ///< 6 is a number of cube map layers.
				VkCommandBufferInheritanceInfo inheritanceInfo{};
				inheritanceInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
				inheritanceInfo.renderPass = renderPasses[SpecificPipeline::POINT_LIGHT_PIPELINE];
				inheritanceInfo.framebuffer = pointLightShadowMapFrameBuffers[pointLightCounter][cubeMapLayerCounter];
				inheritanceInfo.subpass = 0;
				
				VkCommandBufferBeginInfo beginInfo{};
				beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
				beginInfo.flags = VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT |
					VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
				beginInfo.pInheritanceInfo = &inheritanceInfo;

				VkCommandBuffer commandBuffer = commandBuffers[(currentFrame * POINT_LIGHTS_NUMBER + pointLightCounter) * maxCubeMapLayers + cubeMapLayerCounter];
				if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
					throw std::runtime_error("failed to begin recording command buffer!");
				}
				
				// VkClearValue pointLightShadowMapClearValues[2];
				// pointLightShadowMapClearValues[0].depthStencil.depth = 1.0f;
				// pointLightShadowMapClearValues[0].depthStencil.stencil = 0;
				// pointLightShadowMapClearValues[1].color = {{0.5f, 0.5f, 0.5f, 1.0f}};

				// VkRenderPassBeginInfo pointLightShadowMapRenderPassInfo{};
				// pointLightShadowMapRenderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
				// pointLightShadowMapRenderPassInfo.pNext = NULL;
				// pointLightShadowMapRenderPassInfo.renderPass = renderPasses[SpecificPipeline::POINT_LIGHT_PIPELINE];
				// pointLightShadowMapRenderPassInfo.framebuffer = pointLightShadowMapFrameBuffers[pointLightCounter][cubeMapLayerCounter];
				// pointLightShadowMapRenderPassInfo.renderArea.offset.x = 0;
				// pointLightShadowMapRenderPassInfo.renderArea.offset.y = 0;
				// pointLightShadowMapRenderPassInfo.renderArea.extent.width = SHADOW_MAP_SIZE;
				// pointLightShadowMapRenderPassInfo.renderArea.extent.height = SHADOW_MAP_SIZE;
				// pointLightShadowMapRenderPassInfo.clearValueCount = 2;
				// pointLightShadowMapRenderPassInfo.pClearValues = pointLightShadowMapClearValues;

				// vkCmdBeginRenderPass(commandBuffer, &pointLightShadowMapRenderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

				VkViewport pointLightShadowMapViewPort;
				pointLightShadowMapViewPort.height = SHADOW_MAP_SIZE;
				pointLightShadowMapViewPort.width = SHADOW_MAP_SIZE;
				pointLightShadowMapViewPort.minDepth = 0.0f;
				pointLightShadowMapViewPort.maxDepth = 1.0f;
				pointLightShadowMapViewPort.x = 0;
				pointLightShadowMapViewPort.y = 0;
				vkCmdSetViewport(commandBuffer, 0, 1, &pointLightShadowMapViewPort);

				VkRect2D pointLightShadowMapScissor;
				pointLightShadowMapScissor.extent.width = SHADOW_MAP_SIZE;
				pointLightShadowMapScissor.extent.height = SHADOW_MAP_SIZE;
				pointLightShadowMapScissor.offset.x = 0;
				pointLightShadowMapScissor.offset.y = 0;
				vkCmdSetScissor(commandBuffer, 0, 1, &pointLightShadowMapScissor);

				vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineConfigs[SpecificPipeline::POINT_LIGHT_PIPELINE].pipeline);

//				unsigned int pointLightEntity = entitiesCollectionLinked__Trn_PoL_Mes_Act[pointLightCounter];
				
				uint32_t actorsNumber = actors.GetSize();
				for ( unsigned int actorCounter = 0; actorCounter < actorsNumber; ++actorCounter ) {
//					unsigned int meshOwnerEntity = entitiesCollectionLinked__Trn_Mat_Mes_Act[actorCounter];
					const RenderActor& actor = actors[actorCounter];
					unsigned int meshID = actor.meshID;

					const u32 localUboIndex =
						actorsNumber * maxCubeMapLayers * pointLightCounter +                      ///< Choose point light (i)
						maxCubeMapLayers * actorCounter + cubeMapLayerCounter;                     ///< Choose actor (m) and layer (j)
					if ( localUboIndex >= perFrameUboNumber ) {
						reportUboOverflow(SpecificPipeline::POINT_LIGHT_PIPELINE);
						break;
					}
					unsigned int uboIndex = perFrameUboNumber * currentFrame + localUboIndex;      ///< Choose frame

					updatePointLightShadowMapMatrixUBO(uboIndex, pointLightCounter, cubeMapLayerCounter, actorCounter);
					const unsigned int linkedDescriptorSetID = pipelineConfigs[SpecificPipeline::POINT_LIGHT_PIPELINE].linkedDescriptorSetIDs[0];
					const DescriptorSet& currentDescriptorSet = descriptorSetsConfig[linkedDescriptorSetID];
					vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineConfigs[SpecificPipeline::POINT_LIGHT_PIPELINE].pipelineLayout, 0, 1,
											&(*(descriptorSetsChunks.GetVectorContainer() + currentDescriptorSet.descriptorSetOffset + uboIndex)), 0, nullptr);

					VkBuffer vertexBuffers[] = {vertexBufferContainer[meshID]};
					VkDeviceSize offsets[] = {0};
					vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
					
					vkCmdBindIndexBuffer(commandBuffer, indexBufferContainer[meshID], 0, VK_INDEX_TYPE_UINT32);

					unsigned int indicesContainerSize = aIndices_[meshID].size();
					vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(indicesContainerSize), 1, 0, 0, 0);
				}

				if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
					throw std::runtime_error("failed to record command buffer!");
				}
//				vkCmdEndRenderPass(commandBuffer);
			}
		}
	}
	
    VkShaderModule CVulkanRenderer::createShaderModule(const std::vector<char>& code) {
        VkShaderModuleCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize = code.size();
        createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

        VkShaderModule shaderModule;
        if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
            throw std::runtime_error("failed to create shader module!");
        }

        return shaderModule;
    }

    VkSurfaceFormatKHR CVulkanRenderer::chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
        for (const auto& availableFormat : availableFormats) {
            if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                return availableFormat;
            }
        }

        return availableFormats[0];
    }

    VkPresentModeKHR CVulkanRenderer::chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) {
        for (const auto& availablePresentMode : availablePresentModes) {
            if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
//			if (availablePresentMode == VK_PRESENT_MODE_FIFO_KHR) {
//				std::cout << "present mode found!" << std::endl;
                return availablePresentMode;
            }
        }

        return VK_PRESENT_MODE_FIFO_KHR;
    }

    VkExtent2D CVulkanRenderer::chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) {
        if (capabilities.currentExtent.width != (std::numeric_limits<uint32_t>::max)()) {
            return capabilities.currentExtent;
        } else {

			VkExtent2D actualExtent{
				.width  = Window->width,
				.height = Window->height
			};
			/// A configure event may leave one dimension unset, keep the current size for it.
			if ( actualExtent.width == 0 )
				actualExtent.width = swapChainExtent.width;
			if ( actualExtent.height == 0 )
				actualExtent.height = swapChainExtent.height;
			if ( actualExtent.width == 0 || actualExtent.height == 0 )
				return actualExtent;
            actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
            actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

            return actualExtent;
        }
    }

    SwapChainSupportDetails CVulkanRenderer::querySwapChainSupport(VkPhysicalDevice device) {
        SwapChainSupportDetails details;

        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

        uint32_t formatCount;
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);

        if (formatCount != 0) {
            details.formats.resize(formatCount);
            vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
        }

        uint32_t presentModeCount;
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);

        if (presentModeCount != 0) {
            details.presentModes.resize(presentModeCount);
            vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());
        }
        
        return details;
    }

    bool CVulkanRenderer::isDeviceSuitable(VkPhysicalDevice device) {
        QueueFamilyIndices indices = findQueueFamilies(device);

        bool extensionsSupported = checkDeviceExtensionSupport(device);

        bool swapChainAdequate = false;
        if (extensionsSupported) {
            SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device);
            swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
        }

        VkPhysicalDeviceFeatures supportedFeatures;
        vkGetPhysicalDeviceFeatures(device, &supportedFeatures);

        return indices.isComplete() && extensionsSupported && swapChainAdequate && supportedFeatures.samplerAnisotropy && supportedFeatures.fillModeNonSolid &&
			supportedFeatures.shaderSampledImageArrayDynamicIndexing;
    }

    bool CVulkanRenderer::checkDeviceExtensionSupport(VkPhysicalDevice device) {
        uint32_t extensionCount;
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

        std::vector<VkExtensionProperties> availableExtensions(extensionCount);
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

        std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

        for (const auto& extension : availableExtensions) {
            requiredExtensions.erase(extension.extensionName);
        }

        return requiredExtensions.empty();
    }

    QueueFamilyIndices CVulkanRenderer::findQueueFamilies(VkPhysicalDevice device) {
        QueueFamilyIndices indices;

        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

        int i = 0;
        for (const auto& queueFamily : queueFamilies) {
            if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                indices.graphicsFamily = i;
            }

            VkBool32 presentSupport = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);

            if (presentSupport) {
                indices.presentFamily = i;
            }

            if (indices.isComplete()) {
                break;
            }

            i++;
        }

        return indices;
    }

    std::vector<const char*> CVulkanRenderer::getRequiredExtensions() {
#ifdef VK_USE_PLATFORM_XLIB_KHR
        std::vector<const char*> pRequiredExtentions = {"VK_KHR_xlib_surface",
            "VK_EXT_acquire_xlib_display", "VK_KHR_display", "VK_KHR_surface",
            "VK_EXT_direct_mode_display", "VK_LAYER_KHRONOS_validation"};
#endif

#ifdef VK_USE_PLATFORM_XCB_KHR
		std::vector<const char*> pRequiredExtentions = {"VK_KHR_xcb_surface",
            "VK_KHR_display", "VK_KHR_surface",
            "VK_EXT_direct_mode_display"};
#endif
		
#ifdef VK_USE_PLATFORM_WIN32_KHR
        std::vector<const char*> pRequiredExtentions = {"VK_KHR_win32_surface",
            "VK_KHR_surface"};
#endif

#ifdef VK_USE_PLATFORM_WAYLAND_KHR
        std::vector<const char*> pRequiredExtentions = {"VK_KHR_wayland_surface",
			"VK_KHR_display", "VK_EXT_direct_mode_display",
            "VK_KHR_surface"};
#endif

        if (enableValidationLayers) {
            pRequiredExtentions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }

        return pRequiredExtentions;
    }

    bool CVulkanRenderer::checkValidationLayerSupport() {
        uint32_t layerCount;
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

        std::vector<VkLayerProperties> availableLayers(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

        for (const char* layerName : validationLayers) {
            bool layerFound = false;

            for (const auto& layerProperties : availableLayers) {
                if (strcmp(layerName, layerProperties.layerName) == 0) {
                    layerFound = true;
                    break;
                }
            }

            if (!layerFound) {
                return false;
            }
        }

        return true;
    }

	VkDescriptorBufferInfo CVulkanRenderer::createDescriptorBufferInfo( VkBuffer ubo, const VkDeviceSize& uboStructSize, const VkDeviceSize& offsetStep ) {
		VkDescriptorBufferInfo uboBufferInfo{};
		uboBufferInfo.buffer = ubo;
		uboBufferInfo.offset = offsetStep * uboStructSize;
		uboBufferInfo.range = uboStructSize;

		return uboBufferInfo;
	}

	VkDescriptorImageInfo CVulkanRenderer::createDescriptorImageInfo( const VK_Image& textureImage, VkImageLayout layout, unsigned int textureViewIndex, VkSampler textureSampler ) {
		VkDescriptorImageInfo imageInfo{};
//		imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		imageInfo.imageLayout = layout;
		imageInfo.imageView = textureImage.views[textureViewIndex];
		imageInfo.sampler = textureSampler;

		return imageInfo;
	}
	
    std::vector<char> CVulkanRenderer::readFile(const std::string& filename) {
        std::ifstream file(filename, std::ios::ate | std::ios::binary);

        if (!file.is_open()) {
            throw std::runtime_error("failed to open file!");
        }

        size_t fileSize = (size_t) file.tellg();
        std::vector<char> buffer(fileSize);

        file.seekg(0);
        file.read(buffer.data(), fileSize);

        file.close();

        return buffer;
    }

    VKAPI_ATTR VkBool32 VKAPI_CALL CVulkanRenderer::debugCallback([[maybe_unused]] VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, [[maybe_unused]] VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, [[maybe_unused]] void* pUserData) {
		// (void) messageSeverity;
		// (void) messageType;
		// (void) pUserData;
		// if ( pCallbackData->messageIdNumber == 941228658 ) {
		// 	[[maybe_unused]] int i = 0;
		// }


		
		// std::cout << "Error code: " << pCallbackData->messageIdNumber << std::endl;
		// std::cout << "Message name:: " << pCallbackData->pMessageIdName << std::endl;
        std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;

        return VK_FALSE;
    }
}

