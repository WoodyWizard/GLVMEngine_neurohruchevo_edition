// This file is part of Game Loop Versatile Modules (GLVM)
// License: http://opensource.org/licenses/MIT

#include "Fluid/GpuContext.hpp"

#include <cstring>
#include <fstream>
#include <stdexcept>

namespace GLVM::fluid
{
	void vkCheck( VkResult result, const char* what ) {
		if ( result != VK_SUCCESS )
			throw std::runtime_error( std::string(what) + " failed: VkResult " + std::to_string( (int)result ) );
	}

	uint32_t GpuContext::findMemoryType( uint32_t typeBits, VkMemoryPropertyFlags required ) const {
		for ( uint32_t i = 0; i < memoryProperties.memoryTypeCount; ++i ) {
			if ( (typeBits & (1u << i)) && (memoryProperties.memoryTypes[i].propertyFlags & required) == required )
				return i;
		}
		throw std::runtime_error( "no suitable memory type" );
	}

	void GpuContext::submitImmediate( const std::function<void(VkCommandBuffer)>& record ) const {
		VkCommandBufferAllocateInfo allocateInfo{};
		allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocateInfo.commandPool        = commandPool;
		allocateInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocateInfo.commandBufferCount = 1;
		VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
		vkCheck( vkAllocateCommandBuffers( device, &allocateInfo, &commandBuffer ), "vkAllocateCommandBuffers" );

		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		vkCheck( vkBeginCommandBuffer( commandBuffer, &beginInfo ), "vkBeginCommandBuffer" );
		record( commandBuffer );
		vkCheck( vkEndCommandBuffer( commandBuffer ), "vkEndCommandBuffer" );

		VkFenceCreateInfo fenceInfo{};
		fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		VkFence fence = VK_NULL_HANDLE;
		vkCheck( vkCreateFence( device, &fenceInfo, nullptr, &fence ), "vkCreateFence" );
		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers    = &commandBuffer;
		vkCheck( vkQueueSubmit( queue, 1, &submitInfo, fence ), "vkQueueSubmit" );
		vkCheck( vkWaitForFences( device, 1, &fence, VK_TRUE, UINT64_MAX ), "vkWaitForFences" );
		vkDestroyFence( device, fence, nullptr );
		vkFreeCommandBuffers( device, commandPool, 1, &commandBuffer );
	}

	/*
	  ===================================================
	  Buffers
	  ===================================================
	*/
	GpuBuffer createBuffer( const GpuContext& context, VkDeviceSize size, VkBufferUsageFlags usage, bool isHostVisible ) {
		GpuBuffer result;
		result.size = size;
		VkBufferCreateInfo bufferInfo{};
		bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferInfo.size        = size;
		bufferInfo.usage       = usage | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
		bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		vkCheck( vkCreateBuffer( context.device, &bufferInfo, nullptr, &result.buffer ), "vkCreateBuffer" );

		VkMemoryRequirements requirements;
		vkGetBufferMemoryRequirements( context.device, result.buffer, &requirements );
		VkMemoryAllocateInfo allocateInfo{};
		allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocateInfo.allocationSize  = requirements.size;
		allocateInfo.memoryTypeIndex = context.findMemoryType( requirements.memoryTypeBits, isHostVisible
			? VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT : VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
		vkCheck( vkAllocateMemory( context.device, &allocateInfo, nullptr, &result.memory ), "vkAllocateMemory" );
		vkCheck( vkBindBufferMemory( context.device, result.buffer, result.memory, 0 ), "vkBindBufferMemory" );
		if ( isHostVisible )
			vkCheck( vkMapMemory( context.device, result.memory, 0, VK_WHOLE_SIZE, 0, &result.mapped ), "vkMapMemory" );
		return result;
	}

	void destroyBuffer( const GpuContext& context, GpuBuffer& buffer ) {
		if ( buffer.buffer != VK_NULL_HANDLE )
			vkDestroyBuffer( context.device, buffer.buffer, nullptr );
		if ( buffer.memory != VK_NULL_HANDLE )
			vkFreeMemory( context.device, buffer.memory, nullptr );
		buffer = GpuBuffer();
	}

	void uploadToBuffer( const GpuContext& context, const GpuBuffer& destination, const void* data, VkDeviceSize size, VkDeviceSize offset ) {
		if ( size == 0 )
			return;
		GpuBuffer staging = createBuffer( context, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, true );
		std::memcpy( staging.mapped, data, (size_t)size );
		context.submitImmediate( [&]( VkCommandBuffer commandBuffer ) {
			VkBufferCopy region{ 0, offset, size };
			vkCmdCopyBuffer( commandBuffer, staging.buffer, destination.buffer, 1, &region );
		});
		destroyBuffer( context, staging );
	}

	void downloadFromBuffer( const GpuContext& context, const GpuBuffer& source, void* data, VkDeviceSize size, VkDeviceSize offset ) {
		if ( size == 0 )
			return;
		GpuBuffer staging = createBuffer( context, size, VK_BUFFER_USAGE_TRANSFER_DST_BIT, true );
		context.submitImmediate( [&]( VkCommandBuffer commandBuffer ) {
			memoryBarrier( commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_MEMORY_WRITE_BIT,
						   VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_READ_BIT );
			VkBufferCopy region{ offset, 0, size };
			vkCmdCopyBuffer( commandBuffer, source.buffer, staging.buffer, 1, &region );
		});
		std::memcpy( data, staging.mapped, (size_t)size );
		destroyBuffer( context, staging );
	}

	/*
	  ===================================================
	  Images
	  ===================================================
	*/
	bool isDepthFormat( VkFormat format ) {
		return format == VK_FORMAT_D32_SFLOAT || format == VK_FORMAT_D16_UNORM || format == VK_FORMAT_D24_UNORM_S8_UINT ||
			format == VK_FORMAT_D32_SFLOAT_S8_UINT;
	}

	GpuImage createImage( const GpuContext& context, uint32_t width, uint32_t height, VkFormat format, VkImageUsageFlags usage ) {
		GpuImage result;
		result.format = format;
		result.extent = { width, height };
		result.aspect = isDepthFormat(format) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;

		VkImageCreateInfo imageInfo{};
		imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageInfo.imageType     = VK_IMAGE_TYPE_2D;
		imageInfo.format        = format;
		imageInfo.extent        = { width, height, 1 };
		imageInfo.mipLevels     = 1;
		imageInfo.arrayLayers   = 1;
		imageInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
		imageInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
		imageInfo.usage         = usage;
		imageInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
		imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		vkCheck( vkCreateImage( context.device, &imageInfo, nullptr, &result.image ), "vkCreateImage" );

		VkMemoryRequirements requirements;
		vkGetImageMemoryRequirements( context.device, result.image, &requirements );
		VkMemoryAllocateInfo allocateInfo{};
		allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocateInfo.allocationSize  = requirements.size;
		allocateInfo.memoryTypeIndex = context.findMemoryType( requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
		vkCheck( vkAllocateMemory( context.device, &allocateInfo, nullptr, &result.memory ), "vkAllocateMemory" );
		vkCheck( vkBindImageMemory( context.device, result.image, result.memory, 0 ), "vkBindImageMemory" );

		VkImageViewCreateInfo viewInfo{};
		viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewInfo.image            = result.image;
		viewInfo.viewType         = VK_IMAGE_VIEW_TYPE_2D;
		viewInfo.format           = format;
		viewInfo.subresourceRange = { result.aspect, 0, 1, 0, 1 };
		vkCheck( vkCreateImageView( context.device, &viewInfo, nullptr, &result.view ), "vkCreateImageView" );
		return result;
	}

	void destroyImage( const GpuContext& context, GpuImage& image ) {
		if ( image.view != VK_NULL_HANDLE )
			vkDestroyImageView( context.device, image.view, nullptr );
		if ( image.image != VK_NULL_HANDLE )
			vkDestroyImage( context.device, image.image, nullptr );
		if ( image.memory != VK_NULL_HANDLE )
			vkFreeMemory( context.device, image.memory, nullptr );
		image = GpuImage();
	}

	void imageBarrier( VkCommandBuffer commandBuffer, VkImage image, VkImageAspectFlags aspect, VkImageLayout oldLayout, VkImageLayout newLayout,
					   VkPipelineStageFlags sourceStage, VkAccessFlags sourceAccess, VkPipelineStageFlags destinationStage, VkAccessFlags destinationAccess ) {
		VkImageMemoryBarrier barrier{};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.srcAccessMask       = sourceAccess;
		barrier.dstAccessMask       = destinationAccess;
		barrier.oldLayout           = oldLayout;
		barrier.newLayout           = newLayout;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.image               = image;
		barrier.subresourceRange    = { aspect, 0, 1, 0, 1 };
		vkCmdPipelineBarrier( commandBuffer, sourceStage, destinationStage, 0, 0, nullptr, 0, nullptr, 1, &barrier );
	}

	void memoryBarrier( VkCommandBuffer commandBuffer, VkPipelineStageFlags sourceStage, VkAccessFlags sourceAccess,
						VkPipelineStageFlags destinationStage, VkAccessFlags destinationAccess ) {
		VkMemoryBarrier barrier{};
		barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
		barrier.srcAccessMask = sourceAccess;
		barrier.dstAccessMask = destinationAccess;
		vkCmdPipelineBarrier( commandBuffer, sourceStage, destinationStage, 0, 1, &barrier, 0, nullptr, 0, nullptr );
	}

	void computeBarrier( VkCommandBuffer commandBuffer ) {
		memoryBarrier( commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT,
					   VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT );
	}

	VkShaderModule loadShader( const GpuContext& context, const std::string& fileName ) {
		const std::string path = context.shaderDirectory + fileName;
		std::ifstream file( path, std::ios::binary | std::ios::ate );
		if ( !file.is_open() )
			throw std::runtime_error( "can't open shader " + path );
		const std::streamsize size = file.tellg();
		if ( size <= 0 || size % 4 != 0 )
			throw std::runtime_error( "shader " + path + " is not SPIR-V" );
		std::vector<uint32_t> code( (size_t)size / 4 );
		file.seekg( 0 );
		file.read( reinterpret_cast<char*>(code.data()), size );

		VkShaderModuleCreateInfo moduleInfo{};
		moduleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		moduleInfo.codeSize = (size_t)size;
		moduleInfo.pCode    = code.data();
		VkShaderModule module = VK_NULL_HANDLE;
		vkCheck( vkCreateShaderModule( context.device, &moduleInfo, nullptr, &module ), ("vkCreateShaderModule " + fileName).c_str() );
		return module;
	}

	VkSampler createSampler( const GpuContext& context, VkFilter filter, VkSamplerAddressMode addressMode ) {
		VkSamplerCreateInfo samplerInfo{};
		samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		samplerInfo.magFilter    = filter;
		samplerInfo.minFilter    = filter;
		samplerInfo.mipmapMode   = VK_SAMPLER_MIPMAP_MODE_NEAREST;
		samplerInfo.addressModeU = addressMode;
		samplerInfo.addressModeV = addressMode;
		samplerInfo.addressModeW = addressMode;
		samplerInfo.borderColor  = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
		samplerInfo.maxLod       = 0.0f;
		VkSampler sampler = VK_NULL_HANDLE;
		vkCheck( vkCreateSampler( context.device, &samplerInfo, nullptr, &sampler ), "vkCreateSampler" );
		return sampler;
	}

	/*
	  ===================================================
	  Descriptor sets
	  ===================================================
	*/
	VkDescriptorSetLayout createSetLayout( const GpuContext& context, const std::vector<VkDescriptorType>& types, VkShaderStageFlags stages ) {
		std::vector<VkDescriptorSetLayoutBinding> bindings( types.size() );
		for ( uint32_t i = 0; i < types.size(); ++i ) {
			bindings[i].binding         = i;
			bindings[i].descriptorType  = types[i];
			bindings[i].descriptorCount = 1;
			bindings[i].stageFlags      = stages;
		}
		VkDescriptorSetLayoutCreateInfo layoutInfo{};
		layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layoutInfo.bindingCount = (uint32_t)bindings.size();
		layoutInfo.pBindings    = bindings.data();
		VkDescriptorSetLayout layout = VK_NULL_HANDLE;
		vkCheck( vkCreateDescriptorSetLayout( context.device, &layoutInfo, nullptr, &layout ), "vkCreateDescriptorSetLayout" );
		return layout;
	}

	DescriptorWrite bufferWrite( uint32_t binding, VkDescriptorType type, const GpuBuffer& buffer ) {
		DescriptorWrite write;
		write.binding = binding;
		write.type    = type;
		write.buffer  = buffer.buffer;
		write.range   = VK_WHOLE_SIZE;
		return write;
	}

	DescriptorWrite imageWrite( uint32_t binding, VkDescriptorType type, VkImageView view, VkSampler sampler, VkImageLayout layout ) {
		DescriptorWrite write;
		write.binding = binding;
		write.type    = type;
		write.view    = view;
		write.sampler = sampler;
		write.layout  = layout;
		return write;
	}

	void updateSet( const GpuContext& context, VkDescriptorSet set, const std::vector<DescriptorWrite>& writes ) {
		std::vector<VkDescriptorBufferInfo> bufferInfos( writes.size() );
		std::vector<VkDescriptorImageInfo>  imageInfos( writes.size() );
		std::vector<VkWriteDescriptorSet>   descriptorWrites( writes.size() );
		for ( size_t i = 0; i < writes.size(); ++i ) {
			const DescriptorWrite& write = writes[i];
			VkWriteDescriptorSet& descriptorWrite = descriptorWrites[i];
			descriptorWrite = {};
			descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			descriptorWrite.dstSet          = set;
			descriptorWrite.dstBinding      = write.binding;
			descriptorWrite.descriptorCount = 1;
			descriptorWrite.descriptorType  = write.type;
			if ( write.type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER || write.type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER ) {
				bufferInfos[i] = { write.buffer, 0, write.range };
				descriptorWrite.pBufferInfo = &bufferInfos[i];
			} else {
				imageInfos[i] = { write.sampler, write.view, write.layout };
				descriptorWrite.pImageInfo = &imageInfos[i];
			}
		}
		vkUpdateDescriptorSets( context.device, (uint32_t)descriptorWrites.size(), descriptorWrites.data(), 0, nullptr );
	}

	VkDescriptorPool createDescriptorPool( const GpuContext& context, uint32_t maxSets ) {
		const VkDescriptorPoolSize sizes[] = {
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,         maxSets * 24 },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         maxSets * 4 },
			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, maxSets * 12 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,          maxSets * 4 },
		};
		VkDescriptorPoolCreateInfo poolInfo{};
		poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolInfo.maxSets       = maxSets;
		poolInfo.poolSizeCount = (uint32_t)(sizeof(sizes) / sizeof(sizes[0]));
		poolInfo.pPoolSizes    = sizes;
		VkDescriptorPool pool = VK_NULL_HANDLE;
		vkCheck( vkCreateDescriptorPool( context.device, &poolInfo, nullptr, &pool ), "vkCreateDescriptorPool" );
		return pool;
	}

	VkDescriptorSet allocateSet( const GpuContext& context, VkDescriptorPool pool, VkDescriptorSetLayout layout ) {
		VkDescriptorSetAllocateInfo allocateInfo{};
		allocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocateInfo.descriptorPool     = pool;
		allocateInfo.descriptorSetCount = 1;
		allocateInfo.pSetLayouts        = &layout;
		VkDescriptorSet set = VK_NULL_HANDLE;
		vkCheck( vkAllocateDescriptorSets( context.device, &allocateInfo, &set ), "vkAllocateDescriptorSets" );
		return set;
	}

	/*
	  ===================================================
	  Pipelines
	  ===================================================
	*/
	namespace
	{
		VkPipelineLayout createPipelineLayout( const GpuContext& context, const std::vector<VkDescriptorSetLayout>& setLayouts, uint32_t pushConstantSize ) {
			VkPushConstantRange pushRange{ VK_SHADER_STAGE_ALL, 0, pushConstantSize };
			VkPipelineLayoutCreateInfo layoutInfo{};
			layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
			layoutInfo.setLayoutCount         = (uint32_t)setLayouts.size();
			layoutInfo.pSetLayouts            = setLayouts.data();
			layoutInfo.pushConstantRangeCount = pushConstantSize > 0 ? 1 : 0;
			layoutInfo.pPushConstantRanges    = pushConstantSize > 0 ? &pushRange : nullptr;
			VkPipelineLayout layout = VK_NULL_HANDLE;
			vkCheck( vkCreatePipelineLayout( context.device, &layoutInfo, nullptr, &layout ), "vkCreatePipelineLayout" );
			return layout;
		}
	}

	Pipeline createComputePipeline( const GpuContext& context, const std::string& shaderFile, const std::vector<VkDescriptorSetLayout>& setLayouts,
									uint32_t pushConstantSize ) {
		Pipeline result;
		result.layout = createPipelineLayout( context, setLayouts, pushConstantSize );
		VkShaderModule module = loadShader( context, shaderFile );

		VkComputePipelineCreateInfo pipelineInfo{};
		pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
		pipelineInfo.stage.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		pipelineInfo.stage.stage  = VK_SHADER_STAGE_COMPUTE_BIT;
		pipelineInfo.stage.module = module;
		pipelineInfo.stage.pName  = "main";
		pipelineInfo.layout       = result.layout;
		const VkResult status = vkCreateComputePipelines( context.device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &result.pipeline );
		vkDestroyShaderModule( context.device, module, nullptr );
		vkCheck( status, ("vkCreateComputePipelines " + shaderFile).c_str() );
		return result;
	}

	Pipeline createGraphicsPipeline( const GpuContext& context, const GraphicsPipelineDescription& description ) {
		Pipeline result;
		result.layout = createPipelineLayout( context, description.setLayouts, description.pushConstantSize );

		VkShaderModule vertexModule = loadShader( context, description.vertexShader );
		VkShaderModule fragmentModule = description.fragmentShader.empty() ? VK_NULL_HANDLE : loadShader( context, description.fragmentShader );
		VkPipelineShaderStageCreateInfo stages[2] = {};
		stages[0] = {};
		stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		stages[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
		stages[0].module = vertexModule;
		stages[0].pName  = "main";
		stages[1] = {};
		stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		stages[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
		stages[1].module = fragmentModule;
		stages[1].pName  = "main";

		VkPipelineVertexInputStateCreateInfo vertexInput{};
		vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
		inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		inputAssembly.topology = description.topology;
		VkPipelineViewportStateCreateInfo viewportState{};
		viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewportState.viewportCount = 1;
		viewportState.scissorCount  = 1;

		VkPipelineRasterizationStateCreateInfo rasterization{};
		rasterization.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rasterization.polygonMode     = VK_POLYGON_MODE_FILL;
		rasterization.cullMode        = description.cullMode;
		rasterization.frontFace       = VK_FRONT_FACE_COUNTER_CLOCKWISE;
		rasterization.lineWidth       = 1.0f;
		rasterization.depthBiasEnable = description.depthBias ? VK_TRUE : VK_FALSE;

		VkPipelineMultisampleStateCreateInfo multisample{};
		multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

		VkPipelineDepthStencilStateCreateInfo depthStencil{};
		depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		depthStencil.depthTestEnable  = description.depthTest ? VK_TRUE : VK_FALSE;
		depthStencil.depthWriteEnable = description.depthWrite ? VK_TRUE : VK_FALSE;
		depthStencil.depthCompareOp   = description.depthCompare;

		std::vector<VkPipelineColorBlendAttachmentState> blendStates( description.colorFormats.size() );
		for ( size_t i = 0; i < blendStates.size(); ++i ) {
			const BlendMode mode = i < description.blendModes.size() ? description.blendModes[i] : BlendMode::NONE;
			VkPipelineColorBlendAttachmentState& state = blendStates[i];
			state = {};
			state.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
			if ( mode == BlendMode::NONE )
				continue;
			state.blendEnable  = VK_TRUE;
			state.colorBlendOp = VK_BLEND_OP_ADD;
			state.alphaBlendOp = VK_BLEND_OP_ADD;
			switch ( mode ) {
			case BlendMode::ALPHA:
				state.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
				state.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
				state.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
				state.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
				break;
			case BlendMode::PREMULTIPLIED:
				state.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
				state.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
				state.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
				state.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
				break;
			case BlendMode::ADDITIVE:
				state.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
				state.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
				state.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
				state.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
				break;
			case BlendMode::MINIMUM:
				state.colorBlendOp = VK_BLEND_OP_MIN;
				state.alphaBlendOp = VK_BLEND_OP_MIN;
				state.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
				state.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
				state.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
				state.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
				break;
			case BlendMode::NONE:
				break;
			}
		}
		VkPipelineColorBlendStateCreateInfo colorBlend{};
		colorBlend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		colorBlend.attachmentCount = (uint32_t)blendStates.size();
		colorBlend.pAttachments    = blendStates.data();

		std::vector<VkDynamicState> dynamicStates = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
		if ( description.depthBias )
			dynamicStates.push_back( VK_DYNAMIC_STATE_DEPTH_BIAS );
		VkPipelineDynamicStateCreateInfo dynamicState{};
		dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		dynamicState.dynamicStateCount = (uint32_t)dynamicStates.size();
		dynamicState.pDynamicStates    = dynamicStates.data();

		VkPipelineRenderingCreateInfo renderingInfo{};
		renderingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
		renderingInfo.colorAttachmentCount    = (uint32_t)description.colorFormats.size();
		renderingInfo.pColorAttachmentFormats = description.colorFormats.data();
		renderingInfo.depthAttachmentFormat   = description.depthFormat;

		VkGraphicsPipelineCreateInfo pipelineInfo{};
		pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipelineInfo.pNext               = &renderingInfo;
		pipelineInfo.stageCount          = fragmentModule != VK_NULL_HANDLE ? 2 : 1;
		pipelineInfo.pStages             = stages;
		pipelineInfo.pVertexInputState   = &vertexInput;
		pipelineInfo.pInputAssemblyState = &inputAssembly;
		pipelineInfo.pViewportState      = &viewportState;
		pipelineInfo.pRasterizationState = &rasterization;
		pipelineInfo.pMultisampleState   = &multisample;
		pipelineInfo.pDepthStencilState  = &depthStencil;
		pipelineInfo.pColorBlendState    = &colorBlend;
		pipelineInfo.pDynamicState       = &dynamicState;
		pipelineInfo.layout              = result.layout;
		const VkResult status = vkCreateGraphicsPipelines( context.device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &result.pipeline );
		vkDestroyShaderModule( context.device, vertexModule, nullptr );
		if ( fragmentModule != VK_NULL_HANDLE )
			vkDestroyShaderModule( context.device, fragmentModule, nullptr );
		vkCheck( status, ("vkCreateGraphicsPipelines " + description.vertexShader + " " + description.fragmentShader).c_str() );
		return result;
	}

	void destroyPipeline( const GpuContext& context, Pipeline& pipeline ) {
		if ( pipeline.pipeline != VK_NULL_HANDLE )
			vkDestroyPipeline( context.device, pipeline.pipeline, nullptr );
		if ( pipeline.layout != VK_NULL_HANDLE )
			vkDestroyPipelineLayout( context.device, pipeline.layout, nullptr );
		pipeline = Pipeline();
	}

	void beginRendering( VkCommandBuffer commandBuffer, VkExtent2D extent, const std::vector<Attachment>& colorAttachments,
						 const Attachment* depthAttachment ) {
		std::vector<VkRenderingAttachmentInfo> colors( colorAttachments.size() );
		for ( size_t i = 0; i < colors.size(); ++i ) {
			colors[i] = {};
			colors[i].sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
			colors[i].imageView   = colorAttachments[i].view;
			colors[i].imageLayout = colorAttachments[i].layout != VK_IMAGE_LAYOUT_UNDEFINED ? colorAttachments[i].layout : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			colors[i].loadOp      = colorAttachments[i].loadOp;
			colors[i].storeOp     = colorAttachments[i].storeOp;
			colors[i].clearValue  = colorAttachments[i].clearValue;
		}
		VkRenderingAttachmentInfo depth{};
		depth.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
		if ( depthAttachment != nullptr ) {
			depth.imageView   = depthAttachment->view;
			depth.imageLayout = depthAttachment->layout != VK_IMAGE_LAYOUT_UNDEFINED ? depthAttachment->layout : VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
			depth.loadOp      = depthAttachment->loadOp;
			depth.storeOp     = depthAttachment->storeOp;
			depth.clearValue  = depthAttachment->clearValue;
		}

		VkRenderingInfo renderingInfo{};
		renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
		renderingInfo.renderArea           = { { 0, 0 }, extent };
		renderingInfo.layerCount           = 1;
		renderingInfo.colorAttachmentCount = (uint32_t)colors.size();
		renderingInfo.pColorAttachments    = colors.data();
		renderingInfo.pDepthAttachment     = depthAttachment != nullptr ? &depth : nullptr;
		vkCmdBeginRendering( commandBuffer, &renderingInfo );

		const VkViewport viewport{ 0.0f, 0.0f, (float)extent.width, (float)extent.height, 0.0f, 1.0f };
		const VkRect2D scissor{ { 0, 0 }, extent };
		vkCmdSetViewport( commandBuffer, 0, 1, &viewport );
		vkCmdSetScissor( commandBuffer, 0, 1, &scissor );
	}
}
