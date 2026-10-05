// This file is part of Game Loop Versatile Modules (GLVM)
// License: http://opensource.org/licenses/MIT

/*
  Small Vulkan helper layer of the fluid module: buffers, images, shaders, descriptor sets, compute and
  graphics pipelines. Graphics pipelines use dynamic rendering (Vulkan 1.3), so the module needs no render
  passes or framebuffers of the host renderer.
*/

#ifndef GLVM_FLUID_GPU_CONTEXT_HPP
#define GLVM_FLUID_GPU_CONTEXT_HPP

#include <vulkan/vulkan_core.h>

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace GLVM::fluid
{
	/// Throws std::runtime_error("<what> failed: <VkResult>") if result is not VK_SUCCESS.
	void vkCheck( VkResult result, const char* what );

	struct GpuContext {
		VkPhysicalDevice                 physicalDevice = VK_NULL_HANDLE;
		VkDevice                         device         = VK_NULL_HANDLE;
		VkQueue                          queue          = VK_NULL_HANDLE;   ///< Graphics + compute queue
		uint32_t                         queueFamily    = 0;
		VkCommandPool                    commandPool    = VK_NULL_HANDLE;   ///< For immediate submissions, owned by the context creator
		VkPhysicalDeviceProperties       properties{};
		VkPhysicalDeviceMemoryProperties memoryProperties{};
		std::string                      shaderDirectory;                   ///< Directory of compiled SPIR-V files, ends with '/'

		uint32_t findMemoryType( uint32_t typeBits, VkMemoryPropertyFlags required ) const;
		/// Records commands into a one time command buffer, submits it and waits for completion.
		void submitImmediate( const std::function<void(VkCommandBuffer)>& record ) const;
	};

	struct GpuBuffer {
		VkBuffer       buffer = VK_NULL_HANDLE;
		VkDeviceMemory memory = VK_NULL_HANDLE;
		VkDeviceSize   size   = 0;
		void*          mapped = nullptr;                                      ///< Persistently mapped for host visible buffers
	};

	GpuBuffer createBuffer( const GpuContext& context, VkDeviceSize size, VkBufferUsageFlags usage, bool isHostVisible );
	void destroyBuffer( const GpuContext& context, GpuBuffer& buffer );
	/// Copies data into a device buffer through a staging buffer (waits for completion).
	void uploadToBuffer( const GpuContext& context, const GpuBuffer& destination, const void* data, VkDeviceSize size, VkDeviceSize offset = 0 );
	/// Copies a device buffer range to host memory (waits for completion).
	void downloadFromBuffer( const GpuContext& context, const GpuBuffer& source, void* data, VkDeviceSize size, VkDeviceSize offset = 0 );

	struct GpuImage {
		VkImage            image  = VK_NULL_HANDLE;
		VkDeviceMemory     memory = VK_NULL_HANDLE;
		VkImageView        view   = VK_NULL_HANDLE;
		VkFormat           format = VK_FORMAT_UNDEFINED;
		VkExtent2D         extent{};
		VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT;
	};

	GpuImage createImage( const GpuContext& context, uint32_t width, uint32_t height, VkFormat format, VkImageUsageFlags usage );
	void destroyImage( const GpuContext& context, GpuImage& image );
	bool isDepthFormat( VkFormat format );

	void imageBarrier( VkCommandBuffer commandBuffer, VkImage image, VkImageAspectFlags aspect, VkImageLayout oldLayout, VkImageLayout newLayout,
					   VkPipelineStageFlags sourceStage, VkAccessFlags sourceAccess, VkPipelineStageFlags destinationStage, VkAccessFlags destinationAccess );
	void memoryBarrier( VkCommandBuffer commandBuffer, VkPipelineStageFlags sourceStage, VkAccessFlags sourceAccess,
						VkPipelineStageFlags destinationStage, VkAccessFlags destinationAccess );
	/// Compute writes -> compute reads and writes.
	void computeBarrier( VkCommandBuffer commandBuffer );

	VkShaderModule loadShader( const GpuContext& context, const std::string& fileName );
	VkSampler createSampler( const GpuContext& context, VkFilter filter, VkSamplerAddressMode addressMode );

	/*
	  ===================================================
	  Descriptor sets
	  ===================================================
	*/
	/// Binding i of the layout has type types[i].
	VkDescriptorSetLayout createSetLayout( const GpuContext& context, const std::vector<VkDescriptorType>& types, VkShaderStageFlags stages );

	struct DescriptorWrite {
		uint32_t         binding = 0;
		VkDescriptorType type    = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		VkBuffer         buffer  = VK_NULL_HANDLE;
		VkDeviceSize     range   = VK_WHOLE_SIZE;
		VkImageView      view    = VK_NULL_HANDLE;
		VkSampler        sampler = VK_NULL_HANDLE;
		VkImageLayout    layout  = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	};

	DescriptorWrite bufferWrite( uint32_t binding, VkDescriptorType type, const GpuBuffer& buffer );
	DescriptorWrite imageWrite( uint32_t binding, VkDescriptorType type, VkImageView view, VkSampler sampler, VkImageLayout layout );
	void updateSet( const GpuContext& context, VkDescriptorSet set, const std::vector<DescriptorWrite>& writes );

	/// Descriptor pool for a fixed number of sets with generous per type capacity.
	VkDescriptorPool createDescriptorPool( const GpuContext& context, uint32_t maxSets );
	VkDescriptorSet allocateSet( const GpuContext& context, VkDescriptorPool pool, VkDescriptorSetLayout layout );

	/*
	  ===================================================
	  Pipelines
	  ===================================================
	*/
	struct Pipeline {
		VkPipeline       pipeline = VK_NULL_HANDLE;
		VkPipelineLayout layout   = VK_NULL_HANDLE;
	};

	Pipeline createComputePipeline( const GpuContext& context, const std::string& shaderFile, const std::vector<VkDescriptorSetLayout>& setLayouts,
									uint32_t pushConstantSize );

	enum class BlendMode { NONE, ALPHA, PREMULTIPLIED, ADDITIVE, MINIMUM };

	struct GraphicsPipelineDescription {
		std::string                        vertexShader;
		std::string                        fragmentShader;                     ///< Empty for depth only pipelines
		std::vector<VkDescriptorSetLayout> setLayouts;
		uint32_t                           pushConstantSize = 0;
		std::vector<VkFormat>              colorFormats;
		std::vector<BlendMode>             blendModes;                         ///< Per color attachment, NONE if missing
		VkFormat                           depthFormat = VK_FORMAT_UNDEFINED;
		bool                               depthTest   = false;
		bool                               depthWrite  = false;
		VkCompareOp                        depthCompare = VK_COMPARE_OP_LESS_OR_EQUAL;
		VkCullModeFlags                    cullMode    = VK_CULL_MODE_NONE;
		VkPrimitiveTopology                topology    = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		bool                               depthBias   = false;                ///< Dynamic depth bias (shadow maps)
	};

	/// Pipeline for dynamic rendering. Vertex data is fetched from storage buffers (no vertex input), viewport and scissor are dynamic.
	Pipeline createGraphicsPipeline( const GpuContext& context, const GraphicsPipelineDescription& description );
	void destroyPipeline( const GpuContext& context, Pipeline& pipeline );

	/*
	  ===================================================
	  Dynamic rendering helpers
	  ===================================================
	*/
	struct Attachment {
		VkImageView         view        = VK_NULL_HANDLE;
		VkAttachmentLoadOp  loadOp      = VK_ATTACHMENT_LOAD_OP_CLEAR;
		VkAttachmentStoreOp storeOp     = VK_ATTACHMENT_STORE_OP_STORE;
		VkClearValue        clearValue{};
		VkImageLayout       layout      = VK_IMAGE_LAYOUT_UNDEFINED;     ///< UNDEFINED: COLOR_ATTACHMENT_OPTIMAL or DEPTH_STENCIL_ATTACHMENT_OPTIMAL
	};

	/// Begins dynamic rendering (color attachments in COLOR_ATTACHMENT_OPTIMAL, depth in DEPTH_STENCIL_ATTACHMENT_OPTIMAL) and sets viewport and scissor.
	void beginRendering( VkCommandBuffer commandBuffer, VkExtent2D extent, const std::vector<Attachment>& colorAttachments,
						 const Attachment* depthAttachment );
}

#endif
