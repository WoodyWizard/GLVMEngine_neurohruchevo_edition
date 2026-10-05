// This file is part of Game Loop Versatile Modules (GLVM)
// Copyright © 2024 Maksim Manokhin a.k.a. Yuriorkis_Scream. Contacts: <fellfrostqtw@gmail.com>
// Author: Maksim Manokhin a.k.a. Yuriorkis_Scream
// License: http://opensource.org/licenses/MIT

#ifndef VULKAN_RENDERER_HG
#define VULKAN_RENDERER_HG

#include <cstdint>
#include <iostream>
#include <fstream>
#include <new>
#include <stdexcept>
#include <algorithm>
#include <chrono>
#include <vector>
#include <cstring>
#include <cstdlib>
#include <array>
#include <optional>
#include <set>
#include <cmath>
#include <atomic>
#include <memory>
#include <vulkan/vulkan_core.h>

#include "Components/ItemComponent.hpp"
#include "Components/MaterialComponent.hpp"
#include "Components/TransformComponent.hpp"
#include "Components/TextureComponent.hpp"
#include "Components/HealthComponent.hpp"
#include "Texture.hpp"
#include "Vector.hpp"
#include "VertexMath.hpp"
#include "TextureManager.hpp"
#include "ComponentManager.hpp"
#include "WavefrontObjParser.hpp"
#include "MeshManager.hpp"
#include "Globals.hpp"
#include "VkStructs.hpp"
#include "ToString.hpp"
#include "JsonParser.hpp"
#include "ShaderStructs.hpp"
#include "Components/FontComponent.hpp"
#include "PGA.hpp"
#include "VkBuilders.hpp"
#include "VkDebugUtils.hpp"
#include "GraphicAPI/RenderConfig.hpp"
#include "ThreadPool.hpp"

#ifdef __linux__
//#define VK_USE_PLATFORM_XLIB_KHR
//#define VK_USE_PLATFORM_XCB_KHR
#define VK_USE_PLATFORM_WAYLAND_KHR
#endif

#ifdef _WIN32
#define VK_USE_PLATFORM_WIN32_KHR
#endif

#ifdef VK_USE_PLATFORM_WAYLAND_KHR
#include "vulkan/vulkan.h"
#include "vulkan/vulkan_wayland.h"
#include "UnixApi/WindowWaylandVulkan.hpp"
#endif

#ifdef VK_USE_PLATFORM_XCB_KHR
#include "vulkan/vulkan.h"
#include <xcb/xcb.h>
#include "vulkan/vulkan_xcb.h"
#include "vulkan/vulkan_core.h"
#include "UnixApi/WindowXCBVulkan.hpp"
#endif

#ifdef VK_USE_PLATFORM_XLIB_KHR
#include "vulkan/vulkan.h"
#include <X11/Xlib.h>
#include "vulkan/vulkan_xlib.h"
#include "vulkan/vulkan_core.h"
#include "UnixApi/WindowXVulkan.hpp"
#endif

#ifdef VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>
#include "WinApi/WindowWinVulkan.hpp"
#endif

namespace GLVM::fluid
{
	struct GpuContext;
	struct FluidTankDescription;
	class FluidTank;
}

namespace GLVM::core
{
    const uint32_t WIDTH = 800;
    const uint32_t HEIGHT = 600;

//#define NDEBUG
    const std::vector<const char*> validationLayers = {
//        "VK_LAYER_KHRONOS_validation"
    };

    const std::vector<const char*> deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME, "VK_KHR_shader_non_semantic_info"
    };

#ifdef NDEBUG
    const bool enableValidationLayers = false;
#else
    const bool enableValidationLayers = true;
#endif

    struct Texture {
        VkDeviceSize textureSize_;
        unsigned char* textureData_;
    };

    struct QueueFamilyIndices {
        std::optional<uint32_t> graphicsFamily;
        std::optional<uint32_t> presentFamily;

        bool isComplete() {
            return graphicsFamily.has_value() && presentFamily.has_value();
        }
    };

    struct SwapChainSupportDetails {
        VkSurfaceCapabilitiesKHR capabilities;
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> presentModes;
    };
    

	enum ShadowMapType : uint32_t {
		DIRECTIONAL_SHADOW_MAP,
		SPOT_SHADOW_MAP,
		POINT_SHADOW_MAP,
		SHADOW_MAP_TYPES_NUMBER
	};

    class CVulkanRenderer {
    public:
		bool print = true;
		Vector<int, 4> indirectTexture[INDIRECT_TEXTURE_WIDTH * INDIRECT_TEXTURE_HEIGHT / 4 + 1];
		core::vector<unsigned int> entitiesCollectionLinked__Trn_Mat_Mes_Act;
		core::vector<unsigned int> entitiesCollectionLinked__Trn_PoL_Mes_Act;
		Frustum mainCameraFrustum;

		char glyphs[128]  = { 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K',
			'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
			'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o',
			'p', 'q', 'r', 's', 't', 'u', 'v', 'y', 'x', 'y', 'z', '0', '1', '2', '3',
			'4', '5', '6', '7', '8', '9', '.', ',', '"', '"', '\'', '\'', '"', '"', '\'', '\'',
			'?', '!', '_', '$', '(', ')', '+', '-', '/', ':', ';', '<', '>', '=', '[', ']', '\\'};
		const std::vector<Vertex> padding[128];                                     ///< FIXME: Some gabage here that needs maybe for aligh
		const std::vector<uint32_t> symbol_g_indices = {
			0, 1, 2,
			2, 1, 3
		};
		std::chrono::steady_clock::time_point startTime;

        std::vector<ecs::Texture> initializeTextureData_;
        std::vector<const char*> pathsArray_;
		core::vector<const char*> pathsGLTF_;
        std::vector<core::vector<core::Vertex>> levelGeneratedVertices;
        std::vector<std::vector<uint32_t>> levelGeneratedIndices;
		
        std::vector<core::vector<core::Vertex>> aVertices_;
        std::vector<std::vector<uint32_t>> aIndices_;                          ///< wavefront.obj indices
		std::vector<std::vector<float>> aVertexesTemp_;                        ///< gltf indices
		std::vector<float> highest_gltf_Y;                                     /// highest gltf y
		MeshAxisLimitingValues meshAxisLimitingValues;                         /// keep axis liniting values for every exis per mesh in current iteration while initializing wavefrontobj and gltf
		core::vector<core::vector<core::vector<mat4>>> jointMatricesPerMesh;
		core::vector<core::vector<float>> frames;
		bool isInventoryOpened = false;
		bool isDebugCollisitionsActive = false;
		vec3 forward;
		float hud_screen_x = 0.0f;
		float hud_screen_y;

		unsigned int entities[32]; ///<
		core::vector<RenderActor> actors;
		core::vector<RenderDirectionalLight> directionalLights;
		core::vector<RenderSpotLight> spotLights;
		core::vector<RenderPointLight> pointLights;
		core::vector<RenderHealth> healthBars;
		core::vector<RenderFont> fonts;
		core::vector<RenderInventory> inventories;
		core::vector<RenderItem> items;
		core::vector<RenderCrosshair> crosshairs;
		core::vector<RenderPlayer> players;
		core::vector<RenderMathObject> mathObjects;
		core::vector<RenderCollisionWireframe> collisionsWireframes;
		core::vector<VkBuffer> collisionsWireframesVKBuffers;
		core::vector<VkDeviceMemory> collisionsWireframesVKDeviceMemory;
		core::vector<VkBuffer> collisionsWireframesIndicesVKBuffers;
		core::vector<VkDeviceMemory> collisionsWireframesIndicesVKDeviceMemory;
		std::vector<uint32_t> collisionsWireframeIndices;
		bool isCollisionsWireframeBuffersInitialized = false;
		RenderPlayer player;
		bool isSpacialGridWireframeBuffersInitialized = false;
		RenderSpacialGrid renderSpacialGrid;
		core::vector<VkBuffer> spacialGridWireframesVKBuffers;
		core::vector<VkDeviceMemory> spacialGridWireframesVKDeviceMemory;

		float fYaw   = -90.0f;
        float fPitch = 0.0f;
		float prev_Y = 0.0f;
		float current_Y = 0.0f;
		float prev_X = 0.0f;
		float current_X = 0.0f;
		float aspectRate = 0.0f;                   ///< Multiplier of current aspect rate. For full hd this must be 1920 / 1080
		int   dragedItemEntity;

		
#ifdef VK_USE_PLATFORM_WAYLAND_KHR
		GLVM::core::WindowWaylandVulkan* Window;
#endif
		
#ifdef VK_USE_PLATFORM_XCB_KHR
		GLVM::core::WindowXCBVulkan* Window = nullptr;
#endif
		
#ifdef VK_USE_PLATFORM_XLIB_KHR
        GLVM::core::WindowXVulkan* Window;
#endif
    
#ifdef VK_USE_PLATFORM_WIN32_KHR
        GLVM::core::WindowWinVulkan* Window;
#endif
        
        CVulkanRenderer();
        ~CVulkanRenderer();

        void createTextureImage();
        void recreateSwapChain();
        void draw();
        void SetMeshData(std::vector<const char*> _pathsArray, core::vector<const char*> pathsGLTF);
        void SetProjectionMatrix(mat4 _projectionMatrix);
		Frustum extractFrustum( const mat4& vp );
		bool isFrustumIntersect(const AABB& aabb);
		void SetViewMatrix(mat4 _viewMatrix);
		void initializeGameLevelVertices();
        void run();
    
    public:
        VkInstance instance;
        VkDebugUtilsMessengerEXT debugMessenger;
		mat4 viewMatrix;
		mat4 projectionMatrix;
		ThreadPool* renderThreadPool = nullptr;

#ifdef VK_USE_PLATFORM_WAYLAND_KHR
		VkWaylandSurfaceCreateInfoKHR createWaylandSurfaceInfo;
#endif
		
#ifdef VK_USE_PLATFORM_XLIB_KHR
        VkXlibSurfaceCreateInfoKHR createXlibSurfaceInfo;
#endif

#ifdef VK_USE_PLATFORM_XCB_KHR
        VkXcbSurfaceCreateInfoKHR createXcbSurfaceInfo;
#endif		
		
#ifdef VK_USE_PLATFORM_WIN32_KHR
        VkWin32SurfaceCreateInfoKHR createWin32SurfaceInfo;
#endif
    
        VkSurfaceKHR surface = VK_NULL_HANDLE;

        VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
        VkDevice device;

        VkQueue graphicsQueue;
        VkQueue presentQueue;

        VkSwapchainKHR swapChain;
        std::vector<VkImage> swapChainImages;
        VkFormat swapChainImageFormat;
        VkExtent2D swapChainExtent{};
        std::vector<VkImageView> swapChainImageViews;
        std::vector<VkFramebuffer> swapChainFramebuffers;

		core::vector<VkDescriptorSet> virtualTexturesUBODesctiptorSets;
		core::vector<VkDescriptorSet> virtualTexturesSamplersDesctiptorSets;
		
		VkCommandPool mainRenderCommandPool = VK_NULL_HANDLE;
		std::vector<VkCommandPool> secondaryBuffersCommandPools;

		/// Main pipeline depth.
		VkImage     mainDepthPipelineImage;
		VkDeviceMemory mainDepthPipelineImageMemory;
        VkImageView mainDepthImageView;

		/// Depth varialbes for shadow map.
	public:
		unsigned int	directionalLightNumber = 0;
		std::vector<VkFramebuffer> directionalLightShadowMapFrameBuffers;
		core::vector<VK_Image> directionalLightTextureImages;

		/*
		===================================
		FOR TEST ONLY!!!
		===================================
		*/

		mat4 dirLightSpaceMatrix[DIRECTIONAL_LIGHTS_NUMBER];
		mat4 spotLightSpaceMatrix[SPOT_LIGHTS_NUMBER];
		
		unsigned int	pointLightNumber	   = 0;
		std::vector<std::vector<VkFramebuffer>> pointLightShadowMapFrameBuffers;
		core::vector<VK_Image> pointLightTextureImages;

		unsigned int	spotLightNumber		   = 0;
		std::vector<VkFramebuffer> spotLightShadowMapFrameBuffers;
		core::vector<VK_Image> spotLightTextureImages;

		/// Number of shadow map slots that hold a full size map, the remaining slots hold 1x1 placeholders.
		uint32_t allocatedShadowMapsNumber[SHADOW_MAP_TYPES_NUMBER] = {};

		std::vector<VK_Image> textureImages;
		VkSampler textureSampler = VK_NULL_HANDLE;
		VkSampler shadowMapSampler = VK_NULL_HANDLE;

        std::vector<VkBuffer> vertexBufferContainer;
        std::vector<VkDeviceMemory> vertexBufferMemoryContainer;
        std::vector<VkBuffer> indexBufferContainer;
        std::vector<VkDeviceMemory> indexBufferMemoryContaner;
		uint32_t wavefrontObjCounter = 0;
		uint32_t gltfCounter         = 0;

		core::vector<core::vector<core::Vertex>> mathObjectsVertices;
		std::vector<std::vector<uint32_t>> mathObjectsIndices;
        std::vector<VkBuffer> mathObjectsVertexBufferContainer;
        std::vector<VkDeviceMemory> mathObjectsVertexBufferMemoryContainer;
        std::vector<VkBuffer> mathObjectsIndexBufferContainer;
        std::vector<VkDeviceMemory> mathObjectsIndexBufferMemoryContaner;
		
		core::vector<core::vector<Vertex>> symbolGVerticesContainer;
		std::vector<unsigned int> fontIndicesContainer;
        std::vector<VkBuffer> fontVertexBufferContainer;
        std::vector<VkDeviceMemory> fontVertexBufferMemoryContainer;
        std::vector<VkBuffer> fontIndexBufferContainer;
        std::vector<VkDeviceMemory> fontIndexBufferMemoryContaner;
		

		// VkDescriptorImageInfo directionalLightsImageInfo[DIRECTIONAL_LIGHTS_NUMBER];
		// VkDescriptorImageInfo pointLightsImageInfo[POINT_LIGHTS_NUMBER];
		// VkDescriptorImageInfo spotLightsImageInfo[SPOT_LIGHTS_NUMBER];
		
        VkDescriptorPool descriptorPool;
//		const unsigned int uiIconsDescriptorsNumber = 64;
		[[maybe_unused]] const unsigned int virtualTexturesDescriptorsNumber= 64;
//		unsigned int viewPositionUboDescriptorsNumber = 0;
//		const unsigned int directionalLightUboDescriptorsNumber = matrixUboDescriptorsNumber * 4;        ///< 4 - maximum number of directional lights
//		const unsigned int pointLightUboDescriptorsNumber = matrixUboDescriptorsNumber * 32 * 6;         ///< 32 - maximum number of point lights, 6 - number of layers for cube shadow map
//		const unsigned int spotLightUboDescriptorsNumber = matrixUboDescriptorsNumber * 8;               ///< 8 - maximum number of spot lights

        std::vector<VkCommandBuffer> directionalLightCommandBuffers;
		std::vector<VkCommandBuffer> spotLightCommandBuffers;
		std::vector<VkCommandBuffer> pointLightCommandBuffers;
		std::vector<VkCommandBuffer> fontCommandBuffers;
		std::vector<VkCommandBuffer> hudCommandBuffers;
		std::vector<VkCommandBuffer> mainRenderCommandBuffers;
		std::vector<VkCommandBuffer> directionalLightSecondaryCommandBuffers;
		std::vector<VkCommandBuffer> spotLightSecondaryCommandBuffers;
		std::vector<VkCommandBuffer> pointLightSecondaryCommandBuffers;
		std::vector<VkCommandBuffer> virtualTexturesCommandBuffers;

		/// Main render pipe line sync objects
        std::vector<VkSemaphore> imageAvailableSemaphores;
        std::vector<VkSemaphore> renderFinishedSemaphores;
        std::vector<VkFence> inFlightFences;

		/// Hud render pipe line sync objects
        std::vector<VkSemaphore> hudImageAvailableSemaphores;
        std::vector<VkSemaphore> hudRenderFinishedSemaphores;
        std::vector<VkFence> hudInFlightFences;

		/// Font render pipe line sync objects
        std::vector<VkSemaphore> fontImageAvailableSemaphores;
        std::vector<VkSemaphore> fontRenderFinishedSemaphores;
        std::vector<VkFence> fontInFlightFences;
		
		/// Directional light shadow map sync objects
        std::vector<VkSemaphore> directionalLightShadowMapImageAvailableSemaphores;
        std::vector<VkSemaphore> directionalLightShadowMapRenderFinishedSemaphores;
        std::vector<VkFence> directionalLightShadowMapInFlightFences;

		/// Spot light shadow map sync objects
        std::vector<VkSemaphore> spotLightShadowMapImageAvailableSemaphores;
        std::vector<VkSemaphore> spotLightShadowMapRenderFinishedSemaphores;
        std::vector<VkFence> spotLightShadowMapInFlightFences;

		/// Point light shadow map sync objects
        std::vector<VkSemaphore> pointLightShadowMapImageAvailableSemaphores;
        std::vector<VkSemaphore> pointLightShadowMapRenderFinishedSemaphores;
        std::vector<VkFence> pointLightShadowMapInFlightFences;

		/// Virtual textures pipeline sync objects
        std::vector<VkSemaphore> virtualTexturesImageAvailableSemaphores;
        std::vector<VkSemaphore> virtualTexturesRenderFinishedSemaphores;
        std::vector<VkFence> virtualTexturesInFlightFences;
		
        uint32_t currentFrame = 0;
		uint32_t directionalLightCurrentFrame = 0;
		uint32_t spotLightCurrentFrame = 0;
		uint32_t pointLightCurrentFrame = 0;

		std::mutex mutex0; 
		std::mutex mutex1;
		std::mutex mutex2;
		std::mutex shadowMapPassesMutex;
		
        bool framebufferResized = false;
		bool swapChainExtentFollowsWindow = false;           ///< Surface has no fixed extent (Wayland): the window size defines the swapchain size
		VkExtent2D swapChainWindowSize{};                    ///< Window size the current swapchain was created for
		bool swapChainRecreatePending = false;               ///< Swapchain couldn't be recreated (zero sized window), retry on the next frame
		std::atomic<uint32_t> reportedUboOverflows{0};       ///< Bit per pipeline, uniform slot overflow is reported only once
		bool canCopySwapChainImages = false;                 ///< Swapchain images have TRANSFER_SRC usage (screenshots, fluid refraction)
		/// Development screenshot: environment variable GLVM_SCREENSHOT=<frame>:<file.png> saves the whole frame <frame> as a PNG.
		int64_t screenshotFrame = -1;
		std::string screenshotPath;
		uint64_t renderedFramesNumber = 0;
		void recordSwapChainCopy( VkCommandBuffer commandBuffer, uint32_t imageIndex, VkBuffer destination );
		void saveScreenshot( VkBuffer buffer, VkDeviceMemory memory );

		/*
		  Water tank of the level (fluid module, include/Fluid/FluidTank.hpp): simulated before the main pass, drawn over
		  it when it is in the view. Needs Vulkan 1.3 dynamic rendering and a graphics queue with compute; without them
		  the level has no tank. The engine places it (createFluidTank) and sets the frame time and the splashers.
		*/
		bool isDynamicRenderingEnabled = false;
		std::unique_ptr<fluid::GpuContext> fluidContext;
		std::unique_ptr<fluid::FluidTank>  fluidTank;
		float fluidFrameTime = 0.0f;
		bool isDiscreteGpu() const;
		bool createFluidTank( const fluid::FluidTankDescription& description );
		void destroyFluidTank();
		void recordFluidTank( VkCommandBuffer commandBuffer, uint32_t imageIndex, bool isSimulation );

        void initWindow();
        void initVulkan();
		void initializeVertexBuffersWithWavefrontData();
		void initializeVertexBuffersWithGLTFData();
		void initializeVertexBuffersWithFontData();
		void initializeVertexBuffersWithMathObjectsData();
		void initializeCollisionWireframesBuffers();
        void cleanupSwapChain();
        void cleanup();
        void createInstance();
        void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);
        void setupDebugMessenger();
        void createSurface();
        void pickPhysicalDevice();
        void createLogicalDevice();
        void createSwapChain();
        void createImageViews();
        void createMainRenderPass();
        void createDescriptorSetLayout();
        void createGraphicsPipeline();
        void createRenderPassFramebuffers(std::vector<VkImageView>& attachments, VkRenderPass& renderPass_,
										  VkFramebuffer& swapChainFramebuffer, uint32_t width,
										  uint32_t height);
		void createFramebuffers();
        void createCommandPool( VkCommandPool& commandPool );
        void createDepthResources();
		VK_Image createShadowMapImage( uint32_t size, bool isCube, const std::string& debugName );
		VK_Image* shadowMapSlot( ShadowMapType type, uint32_t index );
		void createShadowMapResources();
		void growShadowMaps( ShadowMapType type, uint32_t requiredNumber );
		void ensureShadowMaps();
		void destroyShadowMapFramebuffers();
		void createRenderFinishedSemaphores();
		void destroyRenderFinishedSemaphores();
		void reportUboOverflow( SpecificPipeline pipeline );
		u32 perFrameDescriptorNumber( DescriptorSetDataLink descriptorSetDataLink ) const;
		template<class T> T* mappedUBO( DescriptorSetDataLink descriptorSetDataLink, u32 index );
		VkDescriptorSet* textureDescriptorSet( const DescriptorSet& descriptorSet, u32 textureID );
        VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);
        VkFormat findDepthFormat();
        bool hasStencilComponent(VkFormat format);
        void createTextureImageView();
        void createTextureSampler();
        VkImageView createImageView(VK_Image image, uint32_t baseArrayLayers, uint32_t layerCount);
        void createImage(VK_Image& image);
        void transitionImageLayout(VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout);
        void copyBufferToImage(VkBuffer& buffer, VkImage image, uint32_t width, uint32_t height);
        void createVertexBuffer(VkBuffer& _vertexBuffer, VkDeviceMemory& _vertexBufferMemory, core::vector<Vertex>& _vertices);
        void createIndexBuffer(VkBuffer& _indexBuffer, VkDeviceMemory& _indexBufferMemory, const std::vector<uint32_t>& _indices);
        void createMainRenderUniformBuffers();
        void createMainRenderDescriptorPool();
		void allocateDescriptorSets( core::vector<VkDescriptorSet>& descriptorSets, VkDescriptorSetLayout setLayout,
									 const unsigned int descriptorSetsNumber, const unsigned int descriptorOffset );
		void updateDescriptorSetsUBO( VkBuffer ubo, const VkDeviceSize& uboStructSize, const unsigned int& uboDescriptorsNumber,
									  int uboBinding, core::vector<VkDescriptorSet>& uboDescriptorSets, const unsigned int offset);
		void updateLightDataDescriptorSets( const DescriptorSet& currentDescriptorSet1 );
		void updateDescriptorSetsCombinedImageSampler( const DescriptorSet& descriptorSet );
		void createDescriptorImageInfo( const unsigned int descriptorNumber, VkImageLayout imageLayout,
										core::vector<VK_Image>& textureImages, const unsigned int imageViewIndex,
										VkDescriptorImageInfo descriptorImageInfos[]);
		VkDescriptorBufferInfo createDescriptorBufferInfo( VkBuffer ubo, u32 offset, u32 range );
        void createMainRenderDescriptorSets();
        void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory);
        VkCommandBuffer beginSingleTimeCommands(VkCommandPool& commandPool);
        void endSingleTimeCommands(VkCommandPool& commandPool, VkCommandBuffer& commandBuffer);
        void copyBuffer(VkBuffer& srcBuffer, VkBuffer& dstBuffer, VkDeviceSize size);
        uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
        void createCommandBuffers(VkCommandPool& commandPool, std::vector<VkCommandBuffer>& commandBuffers,
								  uint32_t commandBuffersNumber, VkCommandBufferLevel commandBufferLevelFlag);
		void executeSecondaryCommandBuffer( VkRenderPass renderPass, VkFramebuffer frameBuffer, VkExtent2D extent,
											VkCommandBuffer primaryCommandBuffer, VkCommandBuffer secondaryCommandBuffer );
		void updateHudUBO( uint32_t offset, bool isHudExists, float highestY, uint32_t healthCounter );
		void updateHudScreenUBO( uint32_t offset, uint32_t crosshair );
		void updateSdfUBO(uint32_t offset, uint32_t crosshair);
		void updateMathObjectsDebugUBO(uint32_t offset, uint32_t crosshair);
		void updateCollisionsDebugUBO(uint32_t offset, mat4 model, DescriptorSetDataLink descriptorSetLink);
		void updateUBO_UI( const unsigned int currentInventoryRow, const unsigned int currentInventoryColumn, const unsigned int inventory, uint32_t offset );
		void updateUBO_IconsUI( uint32_t offset, uint32_t item );
		void hudRecordCommandBuffer(VkCommandBuffer& commandBuffer, uint32_t imageIndex);
		void uiRecordCommandBuffer(VkCommandBuffer& commandBuffer, uint32_t imageIndex);
		void uiIconsRecordCommandBuffer(VkCommandBuffer& commandBuffer, uint32_t imageIndex);
		void hudScreenRecordCommandBuffer(VkCommandBuffer& commandBuffer, uint32_t imageIndex);
		void sdfRecordCommandBuffer(VkCommandBuffer &commandBuffer, uint32_t imageIndex);
		void mathObjectsDebugRecordCommandBuffer(VkCommandBuffer& commandBuffer, uint32_t imageIndex);
		void collisionsDebugRecordCommandBuffer(VkCommandBuffer& commandBuffer, uint32_t imageIndex);
		void spacialGridDebugRecordCommandBuffer(VkCommandBuffer& commandBuffer, uint32_t imageIndex);
		void fontRecordCommandBuffer(VkCommandBuffer& commandBuffer, uint32_t imageIndex);
        void recordCommandBuffer(VkCommandBuffer& commandBuffer, uint32_t imageIndex);
        void createSyncObjects(std::vector<VkSemaphore> &imageAvailableSemaphores,
                          std::vector<VkSemaphore> &renderFinishedSemaphores,
                          std::vector<VkFence> &inFlightFences);
        void mapUniformBuffers();
		void updateDirectionalLightShadowMapMatrixUBO(uint32_t currentImage, uint32_t currentLight, unsigned int actor);
		void updateSpotLightShadowMapMatrixUBO(uint32_t currentImage, uint32_t currentLight, unsigned int actor);
		void updatePointLightShadowMapMatrixUBO(uint32_t currentImage, uint32_t currentLight, uint32_t layer, unsigned int actor);
        void updateMatrixUniformBuffer(uint32_t offset, unsigned int actor);
		void updateViewPositionUniformBuffer( uint32_t currentImage, uint32_t player );
        void mainRenderDrawFrame();
		void directionalLightShadowMapDrawFrame();
		void spotLightShadowMapDrawFrame();
		void pointLightShadowMapDrawFrame();
		void directionalLightRecordCoomandBuffer( std::vector<VkCommandBuffer>& commandBuffer, uint32_t currentFrame );
		void spotLightRecordCommandBuffer(std::vector<VkCommandBuffer>& commandBuffer, uint32_t currentFrame);
		void pointLightRecordCommandBuffer(std::vector<VkCommandBuffer>& commandBuffers, uint32_t currentFrame);
        VkShaderModule createShaderModule(const std::vector<char>& code);
        VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
        VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
        VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);
        SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);
        bool isDeviceSuitable(VkPhysicalDevice device);
        bool checkDeviceExtensionSupport(VkPhysicalDevice device);
        QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);
        std::vector<const char*> getRequiredExtensions();
        bool checkValidationLayerSupport();
	    VkDescriptorBufferInfo createDescriptorBufferInfo( VkBuffer ubo, const VkDeviceSize& uboStructSize, const VkDeviceSize& offsetStep );
		VkDescriptorImageInfo createDescriptorImageInfo( const VK_Image& textureImage, VkImageLayout layout, unsigned int textureIndex, VkSampler textureSampler );
        static std::vector<char> readFile(const std::string& filename);
        static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData);
		mat4 computeModelMatrix(ecs::components::transform* _transformComponent);
		void clearVK_Image( VK_Image* textureImages );
    };

};

#endif
