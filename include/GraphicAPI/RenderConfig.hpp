#ifndef RENDER_CONFIG
#define RENDER_CONFIG

#include "GraphicAPI/RenderData.hpp"
#include "ShaderStructs.hpp"
#include "VkStructs.hpp"
#include <cstdint>
#include <vulkan/vulkan_core.h>

namespace GLVM::core
{
	const u32 descriptorBindingsConfigMaxNumber = 32;
	inline DescriptorSet descriptorSetsConfig[32];
	inline DescriptorBinding descriptorBindingsConfig[descriptorBindingsConfigMaxNumber];
	inline Pipeline pipelineConfigs[32];
	inline RenderPass renderPassConfigs[32];
	constexpr int      MAX_FRAMES_IN_FLIGHT = 2;
	constexpr uint32_t MAX_TEXTURES = 18;                                                       ///< Maximum number of loaded textures
	constexpr uint32_t TEXTURE_DESCRIPTOR_SETS_NUMBER = MAX_TEXTURES * MAX_FRAMES_IN_FLIGHT;    ///< One texture descriptor set per texture per frame in flight

	inline void VkConfigInitializer() {
		/*
		=====================================================================================
		                         PIPELINES AND ITS RENDER PASSES
		put all meta data related to pipeline here. Also needed to add meta data of desriptor
		sets and it's bindings that will be related to specific pipeline
		=====================================================================================
		*/
		
		descriptorSetsConfig[SHADOW_MAP_DIRECTIONAL_LIGHT].actualLinkedDescriptorBindingsNumber = 1;
		descriptorSetsConfig[SHADOW_MAP_DIRECTIONAL_LIGHT].hostDescriptorNumber                 = 1024;
		descriptorSetsConfig[SHADOW_MAP_DIRECTIONAL_LIGHT].isTexture                            = false;

		descriptorBindingsConfig[0].vkType                  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		descriptorBindingsConfig[0].shaderStageFlag         = VK_SHADER_STAGE_VERTEX_BIT;
		descriptorBindingsConfig[0].binding                 = 0;
		descriptorBindingsConfig[0].shaderDescriptorsNumber = 1;
		descriptorBindingsConfig[0].uboChunkSize            = sizeof(ShadowMapMatrixUBO);
		
		pipelineConfigs[DIRECTIONAL_LIGHT_PIPELINE].vertShader                       = "../VKshaders/flatShadowMapShaders/vertFlatShadowMap.spv";
		pipelineConfigs[DIRECTIONAL_LIGHT_PIPELINE].bindingDescription               = Vertex::getBindingDescription();
		pipelineConfigs[DIRECTIONAL_LIGHT_PIPELINE].attributeDescriptions            = Vertex::getAttributeDescriptions();
		pipelineConfigs[DIRECTIONAL_LIGHT_PIPELINE].actualLinkedDescriptorSetsNumber = 1;
		pipelineConfigs[DIRECTIONAL_LIGHT_PIPELINE].cullMode                         = VK_CULL_MODE_BACK_BIT;
//		pipelineConfigs[DIRECTIONAL_LIGHT_PIPELINE].cullMode                         = VK_CULL_MODE_FRONT_BIT;

		renderPassConfigs[DIRECTIONAL_LIGHT_PIPELINE].actualAttachmentDescriptionNumber        = 1;
		renderPassConfigs[DIRECTIONAL_LIGHT_PIPELINE].attachmentDescriptions[0].flags          = 0;
		renderPassConfigs[DIRECTIONAL_LIGHT_PIPELINE].attachmentDescriptions[0].samples        = VK_SAMPLE_COUNT_1_BIT;
		renderPassConfigs[DIRECTIONAL_LIGHT_PIPELINE].attachmentDescriptions[0].loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
		renderPassConfigs[DIRECTIONAL_LIGHT_PIPELINE].attachmentDescriptions[0].storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
		renderPassConfigs[DIRECTIONAL_LIGHT_PIPELINE].attachmentDescriptions[0].stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		renderPassConfigs[DIRECTIONAL_LIGHT_PIPELINE].attachmentDescriptions[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		renderPassConfigs[DIRECTIONAL_LIGHT_PIPELINE].attachmentDescriptions[0].initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
		renderPassConfigs[DIRECTIONAL_LIGHT_PIPELINE].attachmentDescriptions[0].finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

		renderPassConfigs[DIRECTIONAL_LIGHT_PIPELINE].actualAttachmentReferenceNumber          = 1;
		renderPassConfigs[DIRECTIONAL_LIGHT_PIPELINE].attachmentReferences[0].attachment       = 0;
		renderPassConfigs[DIRECTIONAL_LIGHT_PIPELINE].attachmentReferences[0].layout           = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		renderPassConfigs[DIRECTIONAL_LIGHT_PIPELINE].actualSubpassDependencyNumber            = 2;
		renderPassConfigs[DIRECTIONAL_LIGHT_PIPELINE].subpassDependencies[0].srcSubpass        = VK_SUBPASS_EXTERNAL;
		renderPassConfigs[DIRECTIONAL_LIGHT_PIPELINE].subpassDependencies[0].dstSubpass        = 0;
		renderPassConfigs[DIRECTIONAL_LIGHT_PIPELINE].subpassDependencies[0].srcStageMask      = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		renderPassConfigs[DIRECTIONAL_LIGHT_PIPELINE].subpassDependencies[0].dstStageMask      = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
		renderPassConfigs[DIRECTIONAL_LIGHT_PIPELINE].subpassDependencies[0].srcAccessMask     = VK_ACCESS_SHADER_READ_BIT;
		renderPassConfigs[DIRECTIONAL_LIGHT_PIPELINE].subpassDependencies[0].dstAccessMask     = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		renderPassConfigs[DIRECTIONAL_LIGHT_PIPELINE].subpassDependencies[0].dependencyFlags   = {};          ///< Framebuffer-global: the main pass samples arbitrary shadow map texels
		
		renderPassConfigs[DIRECTIONAL_LIGHT_PIPELINE].subpassDependencies[1].srcSubpass        = 0;
		renderPassConfigs[DIRECTIONAL_LIGHT_PIPELINE].subpassDependencies[1].dstSubpass        = VK_SUBPASS_EXTERNAL;
		renderPassConfigs[DIRECTIONAL_LIGHT_PIPELINE].subpassDependencies[1].srcStageMask      = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		renderPassConfigs[DIRECTIONAL_LIGHT_PIPELINE].subpassDependencies[1].dstStageMask      = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		renderPassConfigs[DIRECTIONAL_LIGHT_PIPELINE].subpassDependencies[1].srcAccessMask     = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		renderPassConfigs[DIRECTIONAL_LIGHT_PIPELINE].subpassDependencies[1].dstAccessMask     = VK_ACCESS_SHADER_READ_BIT;
		renderPassConfigs[DIRECTIONAL_LIGHT_PIPELINE].subpassDependencies[1].dependencyFlags   = {};          ///< Framebuffer-global: the main pass samples arbitrary shadow map texels

		/// ===================================================================================== ///

		descriptorSetsConfig[SHADOW_MAP_SPOT_LIGHT].actualLinkedDescriptorBindingsNumber        = 1;
		descriptorSetsConfig[SHADOW_MAP_SPOT_LIGHT].hostDescriptorNumber                        = 1024;
		descriptorSetsConfig[SHADOW_MAP_SPOT_LIGHT].isTexture                                   = false;

		descriptorBindingsConfig[1].vkType                  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		descriptorBindingsConfig[1].shaderStageFlag         = VK_SHADER_STAGE_VERTEX_BIT;
		descriptorBindingsConfig[1].binding                 = 0;
		descriptorBindingsConfig[1].shaderDescriptorsNumber = 1;
		descriptorBindingsConfig[1].uboChunkSize            = sizeof(ShadowMapMatrixUBO);
		
		pipelineConfigs[SPOT_LIGHT_PIPELINE].vertShader                       = "../VKshaders/flatShadowMapShaders/vertFlatShadowMap.spv";
		pipelineConfigs[SPOT_LIGHT_PIPELINE].bindingDescription               = Vertex::getBindingDescription();
		pipelineConfigs[SPOT_LIGHT_PIPELINE].attributeDescriptions            = Vertex::getAttributeDescriptions();
		pipelineConfigs[SPOT_LIGHT_PIPELINE].actualLinkedDescriptorSetsNumber = 1;

		renderPassConfigs[SPOT_LIGHT_PIPELINE].actualAttachmentDescriptionNumber        = 1;
		renderPassConfigs[SPOT_LIGHT_PIPELINE].attachmentDescriptions[0].flags          = 0;
		renderPassConfigs[SPOT_LIGHT_PIPELINE].attachmentDescriptions[0].samples        = VK_SAMPLE_COUNT_1_BIT;
		renderPassConfigs[SPOT_LIGHT_PIPELINE].attachmentDescriptions[0].loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
		renderPassConfigs[SPOT_LIGHT_PIPELINE].attachmentDescriptions[0].storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
		renderPassConfigs[SPOT_LIGHT_PIPELINE].attachmentDescriptions[0].stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		renderPassConfigs[SPOT_LIGHT_PIPELINE].attachmentDescriptions[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		renderPassConfigs[SPOT_LIGHT_PIPELINE].attachmentDescriptions[0].initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
		renderPassConfigs[SPOT_LIGHT_PIPELINE].attachmentDescriptions[0].finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

		renderPassConfigs[SPOT_LIGHT_PIPELINE].actualAttachmentReferenceNumber          = 1;
		renderPassConfigs[SPOT_LIGHT_PIPELINE].attachmentReferences[0].attachment       = 0;
		renderPassConfigs[SPOT_LIGHT_PIPELINE].attachmentReferences[0].layout           = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		renderPassConfigs[SPOT_LIGHT_PIPELINE].actualSubpassDependencyNumber            = 2;
		renderPassConfigs[SPOT_LIGHT_PIPELINE].subpassDependencies[0].srcSubpass        = VK_SUBPASS_EXTERNAL;
		renderPassConfigs[SPOT_LIGHT_PIPELINE].subpassDependencies[0].dstSubpass        = 0;
		renderPassConfigs[SPOT_LIGHT_PIPELINE].subpassDependencies[0].srcStageMask      = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		renderPassConfigs[SPOT_LIGHT_PIPELINE].subpassDependencies[0].dstStageMask      = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
		renderPassConfigs[SPOT_LIGHT_PIPELINE].subpassDependencies[0].srcAccessMask     = VK_ACCESS_SHADER_READ_BIT;
		renderPassConfigs[SPOT_LIGHT_PIPELINE].subpassDependencies[0].dstAccessMask     = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		renderPassConfigs[SPOT_LIGHT_PIPELINE].subpassDependencies[0].dependencyFlags   = {};          ///< Framebuffer-global: the main pass samples arbitrary shadow map texels
		
		renderPassConfigs[SPOT_LIGHT_PIPELINE].subpassDependencies[1].srcSubpass        = 0;
		renderPassConfigs[SPOT_LIGHT_PIPELINE].subpassDependencies[1].dstSubpass        = VK_SUBPASS_EXTERNAL;
		renderPassConfigs[SPOT_LIGHT_PIPELINE].subpassDependencies[1].srcStageMask      = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		renderPassConfigs[SPOT_LIGHT_PIPELINE].subpassDependencies[1].dstStageMask      = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		renderPassConfigs[SPOT_LIGHT_PIPELINE].subpassDependencies[1].srcAccessMask     = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		renderPassConfigs[SPOT_LIGHT_PIPELINE].subpassDependencies[1].dstAccessMask     = VK_ACCESS_SHADER_READ_BIT;
		renderPassConfigs[SPOT_LIGHT_PIPELINE].subpassDependencies[1].dependencyFlags   = {};          ///< Framebuffer-global: the main pass samples arbitrary shadow map texels
		
		/// ===================================================================================== ///

		descriptorSetsConfig[SHADOW_MAP_POINT_LIGHT].actualLinkedDescriptorBindingsNumber       = 1;
		descriptorSetsConfig[SHADOW_MAP_POINT_LIGHT].hostDescriptorNumber                       = 2048;
		descriptorSetsConfig[SHADOW_MAP_POINT_LIGHT].isTexture                                  = false;

		descriptorBindingsConfig[2].vkType                  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		descriptorBindingsConfig[2].shaderStageFlag         = VK_SHADER_STAGE_VERTEX_BIT;
		descriptorBindingsConfig[2].binding                 = 0;
		descriptorBindingsConfig[2].shaderDescriptorsNumber = 1;
		descriptorBindingsConfig[2].uboChunkSize            = sizeof(PointLightShadowMapMatrixUBO);
		
		pipelineConfigs[POINT_LIGHT_PIPELINE].vertShader                       = "../VKshaders/cubeShadowMapShaders/vertCubeShadowMap.spv";
		pipelineConfigs[POINT_LIGHT_PIPELINE].fragShader                       = "../VKshaders/cubeShadowMapShaders/fragCubeShadowMap.spv";
		pipelineConfigs[POINT_LIGHT_PIPELINE].bindingDescription               = Vertex::getBindingDescription();
		pipelineConfigs[POINT_LIGHT_PIPELINE].attributeDescriptions            = Vertex::getAttributeDescriptions();
		pipelineConfigs[POINT_LIGHT_PIPELINE].actualLinkedDescriptorSetsNumber = 1;

		renderPassConfigs[POINT_LIGHT_PIPELINE].actualAttachmentDescriptionNumber        = 1;
		renderPassConfigs[POINT_LIGHT_PIPELINE].attachmentDescriptions[0].flags          = 0;
		renderPassConfigs[POINT_LIGHT_PIPELINE].attachmentDescriptions[0].samples        = VK_SAMPLE_COUNT_1_BIT;
		renderPassConfigs[POINT_LIGHT_PIPELINE].attachmentDescriptions[0].loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
		renderPassConfigs[POINT_LIGHT_PIPELINE].attachmentDescriptions[0].storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
		renderPassConfigs[POINT_LIGHT_PIPELINE].attachmentDescriptions[0].stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		renderPassConfigs[POINT_LIGHT_PIPELINE].attachmentDescriptions[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		renderPassConfigs[POINT_LIGHT_PIPELINE].attachmentDescriptions[0].initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
		renderPassConfigs[POINT_LIGHT_PIPELINE].attachmentDescriptions[0].finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
		
		renderPassConfigs[POINT_LIGHT_PIPELINE].actualAttachmentReferenceNumber          = 1;
		renderPassConfigs[POINT_LIGHT_PIPELINE].attachmentReferences[0].attachment       = 0;
		renderPassConfigs[POINT_LIGHT_PIPELINE].attachmentReferences[0].layout           = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		
		renderPassConfigs[POINT_LIGHT_PIPELINE].actualSubpassDependencyNumber            = 2;
		renderPassConfigs[POINT_LIGHT_PIPELINE].subpassDependencies[0].srcSubpass        = VK_SUBPASS_EXTERNAL;
		renderPassConfigs[POINT_LIGHT_PIPELINE].subpassDependencies[0].dstSubpass        = 0;
		renderPassConfigs[POINT_LIGHT_PIPELINE].subpassDependencies[0].srcStageMask      = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		renderPassConfigs[POINT_LIGHT_PIPELINE].subpassDependencies[0].dstStageMask      = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
		renderPassConfigs[POINT_LIGHT_PIPELINE].subpassDependencies[0].srcAccessMask     = VK_ACCESS_SHADER_READ_BIT;
		renderPassConfigs[POINT_LIGHT_PIPELINE].subpassDependencies[0].dstAccessMask     = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		renderPassConfigs[POINT_LIGHT_PIPELINE].subpassDependencies[0].dependencyFlags   = {};          ///< Framebuffer-global: the main pass samples arbitrary shadow map texels
		
		renderPassConfigs[POINT_LIGHT_PIPELINE].subpassDependencies[1].srcSubpass        = 0;
		renderPassConfigs[POINT_LIGHT_PIPELINE].subpassDependencies[1].dstSubpass        = VK_SUBPASS_EXTERNAL;
		renderPassConfigs[POINT_LIGHT_PIPELINE].subpassDependencies[1].srcStageMask      = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		renderPassConfigs[POINT_LIGHT_PIPELINE].subpassDependencies[1].dstStageMask      = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		renderPassConfigs[POINT_LIGHT_PIPELINE].subpassDependencies[1].srcAccessMask     = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		renderPassConfigs[POINT_LIGHT_PIPELINE].subpassDependencies[1].dstAccessMask     = VK_ACCESS_SHADER_READ_BIT;
		renderPassConfigs[POINT_LIGHT_PIPELINE].subpassDependencies[1].dependencyFlags   = {};          ///< Framebuffer-global: the main pass samples arbitrary shadow map texels

		/// ===================================================================================== ///

		descriptorSetsConfig[HUD].actualLinkedDescriptorBindingsNumber                          = 1;
		descriptorSetsConfig[HUD].hostDescriptorNumber                                          = 1024;
		descriptorSetsConfig[HUD].isTexture                                                     = false;

		descriptorBindingsConfig[3].vkType                  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		descriptorBindingsConfig[3].shaderStageFlag         = VK_SHADER_STAGE_VERTEX_BIT;
		descriptorBindingsConfig[3].binding                 = 0;
		descriptorBindingsConfig[3].shaderDescriptorsNumber = 1;
		descriptorBindingsConfig[3].uboChunkSize            = sizeof(HUD_UBO);
		
		pipelineConfigs[HUD_PIPELINE].vertShader                       = "../VKshaders/hudShaders/hud_vert.spv";
		pipelineConfigs[HUD_PIPELINE].fragShader                       = "../VKshaders/hudShaders/hud_frag.spv";
		pipelineConfigs[HUD_PIPELINE].bindingDescription               = Vertex::getBindingDescription();
		pipelineConfigs[HUD_PIPELINE].attributeDescriptions            = Vertex::getAttributeDescriptions();
		pipelineConfigs[HUD_PIPELINE].actualLinkedDescriptorSetsNumber = 1;

		renderPassConfigs[HUD_PIPELINE].actualAttachmentDescriptionNumber        = 2;
		renderPassConfigs[HUD_PIPELINE].attachmentDescriptions[0].flags          = 0;
		renderPassConfigs[HUD_PIPELINE].attachmentDescriptions[0].samples        = VK_SAMPLE_COUNT_1_BIT;
		renderPassConfigs[HUD_PIPELINE].attachmentDescriptions[0].loadOp         = VK_ATTACHMENT_LOAD_OP_LOAD;
		renderPassConfigs[HUD_PIPELINE].attachmentDescriptions[0].storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
		renderPassConfigs[HUD_PIPELINE].attachmentDescriptions[0].stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		renderPassConfigs[HUD_PIPELINE].attachmentDescriptions[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		renderPassConfigs[HUD_PIPELINE].attachmentDescriptions[0].initialLayout  = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		renderPassConfigs[HUD_PIPELINE].attachmentDescriptions[0].finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

		renderPassConfigs[HUD_PIPELINE].attachmentDescriptions[1].flags          = 0;
		renderPassConfigs[HUD_PIPELINE].attachmentDescriptions[1].samples        = VK_SAMPLE_COUNT_1_BIT;
		renderPassConfigs[HUD_PIPELINE].attachmentDescriptions[1].loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
		renderPassConfigs[HUD_PIPELINE].attachmentDescriptions[1].storeOp        = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		renderPassConfigs[HUD_PIPELINE].attachmentDescriptions[1].stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		renderPassConfigs[HUD_PIPELINE].attachmentDescriptions[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		renderPassConfigs[HUD_PIPELINE].attachmentDescriptions[1].initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
		renderPassConfigs[HUD_PIPELINE].attachmentDescriptions[1].finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		
		renderPassConfigs[HUD_PIPELINE].actualAttachmentReferenceNumber          = 2;
		renderPassConfigs[HUD_PIPELINE].attachmentReferences[0].attachment       = 0;
		renderPassConfigs[HUD_PIPELINE].attachmentReferences[0].layout           = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		renderPassConfigs[HUD_PIPELINE].attachmentReferences[1].attachment       = 1;
		renderPassConfigs[HUD_PIPELINE].attachmentReferences[1].layout           = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		
		renderPassConfigs[HUD_PIPELINE].actualSubpassDependencyNumber            = 2;
		/// Order this pass after previous writes to the shared swapchain/depth images.
		renderPassConfigs[HUD_PIPELINE].subpassDependencies[0].srcSubpass        = VK_SUBPASS_EXTERNAL;
		renderPassConfigs[HUD_PIPELINE].subpassDependencies[0].dstSubpass        = 0;
		renderPassConfigs[HUD_PIPELINE].subpassDependencies[0].srcStageMask      = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		renderPassConfigs[HUD_PIPELINE].subpassDependencies[0].dstStageMask      = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		renderPassConfigs[HUD_PIPELINE].subpassDependencies[0].srcAccessMask     = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		renderPassConfigs[HUD_PIPELINE].subpassDependencies[0].dstAccessMask     = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		renderPassConfigs[HUD_PIPELINE].subpassDependencies[0].dependencyFlags   = {};
		renderPassConfigs[HUD_PIPELINE].subpassDependencies[1].srcSubpass        = 0;
		renderPassConfigs[HUD_PIPELINE].subpassDependencies[1].dstSubpass        = VK_SUBPASS_EXTERNAL;
		renderPassConfigs[HUD_PIPELINE].subpassDependencies[1].srcStageMask      = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		renderPassConfigs[HUD_PIPELINE].subpassDependencies[1].dstStageMask      = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		renderPassConfigs[HUD_PIPELINE].subpassDependencies[1].srcAccessMask     = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		renderPassConfigs[HUD_PIPELINE].subpassDependencies[1].dstAccessMask     = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		renderPassConfigs[HUD_PIPELINE].subpassDependencies[1].dependencyFlags   = {};
		
		/// ===================================================================================== ///

		descriptorSetsConfig[FONT_RENDER_UBO].actualLinkedDescriptorBindingsNumber              = 1;
		descriptorSetsConfig[FONT_RENDER_UBO].hostDescriptorNumber                              = 4096;
		descriptorSetsConfig[FONT_RENDER_UBO].isTexture                                         = false;

		descriptorBindingsConfig[4].vkType                  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		descriptorBindingsConfig[4].shaderStageFlag         = VK_SHADER_STAGE_VERTEX_BIT;
		descriptorBindingsConfig[4].binding                 = 0;
		descriptorBindingsConfig[4].shaderDescriptorsNumber = 1;
		descriptorBindingsConfig[4].uboChunkSize            = sizeof(FONT_UBO);

		descriptorSetsConfig[FONT_RENDER_SAMPLER].actualLinkedDescriptorBindingsNumber          = 1;
		descriptorSetsConfig[FONT_RENDER_SAMPLER].hostDescriptorNumber                          = TEXTURE_DESCRIPTOR_SETS_NUMBER;
		descriptorSetsConfig[FONT_RENDER_SAMPLER].isTexture                                     = true;

		descriptorBindingsConfig[5].vkType                  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		descriptorBindingsConfig[5].shaderStageFlag         = VK_SHADER_STAGE_FRAGMENT_BIT;
		descriptorBindingsConfig[5].binding                 = 0;
		descriptorBindingsConfig[5].shaderDescriptorsNumber = 1;
		
		pipelineConfigs[FONT_PIPELINE].vertShader                       = "../VKshaders/fontShaders/font_vert.spv";
		pipelineConfigs[FONT_PIPELINE].fragShader                       = "../VKshaders/fontShaders/font_frag.spv";
		pipelineConfigs[FONT_PIPELINE].bindingDescription               = Vertex::getBindingDescription();
		pipelineConfigs[FONT_PIPELINE].attributeDescriptions            = Vertex::getAttributeDescriptions();
		pipelineConfigs[FONT_PIPELINE].actualLinkedDescriptorSetsNumber = 2;

		renderPassConfigs[FONT_PIPELINE].actualAttachmentDescriptionNumber        = 2;
		renderPassConfigs[FONT_PIPELINE].attachmentDescriptions[0].flags          = 0;
		renderPassConfigs[FONT_PIPELINE].attachmentDescriptions[0].samples        = VK_SAMPLE_COUNT_1_BIT;
		renderPassConfigs[FONT_PIPELINE].attachmentDescriptions[0].loadOp         = VK_ATTACHMENT_LOAD_OP_LOAD;
		renderPassConfigs[FONT_PIPELINE].attachmentDescriptions[0].storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
		renderPassConfigs[FONT_PIPELINE].attachmentDescriptions[0].stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		renderPassConfigs[FONT_PIPELINE].attachmentDescriptions[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		renderPassConfigs[FONT_PIPELINE].attachmentDescriptions[0].initialLayout  = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		renderPassConfigs[FONT_PIPELINE].attachmentDescriptions[0].finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		
		renderPassConfigs[FONT_PIPELINE].attachmentDescriptions[1].flags          = 0;
		renderPassConfigs[FONT_PIPELINE].attachmentDescriptions[1].samples        = VK_SAMPLE_COUNT_1_BIT;
		renderPassConfigs[FONT_PIPELINE].attachmentDescriptions[1].loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
		renderPassConfigs[FONT_PIPELINE].attachmentDescriptions[1].storeOp        = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		renderPassConfigs[FONT_PIPELINE].attachmentDescriptions[1].stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		renderPassConfigs[FONT_PIPELINE].attachmentDescriptions[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		renderPassConfigs[FONT_PIPELINE].attachmentDescriptions[1].initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
		renderPassConfigs[FONT_PIPELINE].attachmentDescriptions[1].finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		
		renderPassConfigs[FONT_PIPELINE].actualAttachmentReferenceNumber          = 2;
		renderPassConfigs[FONT_PIPELINE].attachmentReferences[0].attachment       = 0;
		renderPassConfigs[FONT_PIPELINE].attachmentReferences[0].layout           = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		
		renderPassConfigs[FONT_PIPELINE].attachmentReferences[1].attachment       = 1;
		renderPassConfigs[FONT_PIPELINE].attachmentReferences[1].layout           = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		
		renderPassConfigs[FONT_PIPELINE].actualSubpassDependencyNumber            = 2;
		/// Order this pass after previous writes to the shared swapchain/depth images.
		renderPassConfigs[FONT_PIPELINE].subpassDependencies[0].srcSubpass        = VK_SUBPASS_EXTERNAL;
		renderPassConfigs[FONT_PIPELINE].subpassDependencies[0].dstSubpass        = 0;
		renderPassConfigs[FONT_PIPELINE].subpassDependencies[0].srcStageMask      = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		renderPassConfigs[FONT_PIPELINE].subpassDependencies[0].dstStageMask      = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		renderPassConfigs[FONT_PIPELINE].subpassDependencies[0].srcAccessMask     = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		renderPassConfigs[FONT_PIPELINE].subpassDependencies[0].dstAccessMask     = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		renderPassConfigs[FONT_PIPELINE].subpassDependencies[0].dependencyFlags   = {};
		renderPassConfigs[FONT_PIPELINE].subpassDependencies[1].srcSubpass        = 0;
		renderPassConfigs[FONT_PIPELINE].subpassDependencies[1].dstSubpass        = VK_SUBPASS_EXTERNAL;
		renderPassConfigs[FONT_PIPELINE].subpassDependencies[1].srcStageMask      = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		renderPassConfigs[FONT_PIPELINE].subpassDependencies[1].dstStageMask      = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		renderPassConfigs[FONT_PIPELINE].subpassDependencies[1].srcAccessMask     = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		renderPassConfigs[FONT_PIPELINE].subpassDependencies[1].dstAccessMask     = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		renderPassConfigs[FONT_PIPELINE].subpassDependencies[1].dependencyFlags   = {};
		
		/// ===================================================================================== ///

		descriptorSetsConfig[HUD_SCREEN].actualLinkedDescriptorBindingsNumber                   = 1;
		descriptorSetsConfig[HUD_SCREEN].hostDescriptorNumber                                   = 64;
		descriptorSetsConfig[HUD_SCREEN].isTexture                                              = false;

		descriptorBindingsConfig[6].vkType                  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		descriptorBindingsConfig[6].shaderStageFlag         = VK_SHADER_STAGE_VERTEX_BIT;
		descriptorBindingsConfig[6].binding                 = 0;
		descriptorBindingsConfig[6].shaderDescriptorsNumber = 1;
		descriptorBindingsConfig[6].uboChunkSize            = sizeof(HUD_SCREEN_UBO);
		
		pipelineConfigs[HUD_SCREEN_PIPELINE].vertShader                       = "../VKshaders/hud_screen_shaders/vert_hud_screen.spv";
		pipelineConfigs[HUD_SCREEN_PIPELINE].fragShader                       = "../VKshaders/hud_screen_shaders/frag_hud_screen.spv";
		pipelineConfigs[HUD_SCREEN_PIPELINE].bindingDescription               = Vertex::getBindingDescription();
		pipelineConfigs[HUD_SCREEN_PIPELINE].attributeDescriptions            = Vertex::getAttributeDescriptions();
		pipelineConfigs[HUD_SCREEN_PIPELINE].actualLinkedDescriptorSetsNumber = 1;

		renderPassConfigs[HUD_SCREEN_PIPELINE].actualAttachmentDescriptionNumber        = 2;
		renderPassConfigs[HUD_SCREEN_PIPELINE].attachmentDescriptions[0].flags          = 0;
		renderPassConfigs[HUD_SCREEN_PIPELINE].attachmentDescriptions[0].samples        = VK_SAMPLE_COUNT_1_BIT;
		renderPassConfigs[HUD_SCREEN_PIPELINE].attachmentDescriptions[0].loadOp         = VK_ATTACHMENT_LOAD_OP_LOAD;
		renderPassConfigs[HUD_SCREEN_PIPELINE].attachmentDescriptions[0].storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
		renderPassConfigs[HUD_SCREEN_PIPELINE].attachmentDescriptions[0].stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		renderPassConfigs[HUD_SCREEN_PIPELINE].attachmentDescriptions[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		renderPassConfigs[HUD_SCREEN_PIPELINE].attachmentDescriptions[0].initialLayout  = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		renderPassConfigs[HUD_SCREEN_PIPELINE].attachmentDescriptions[0].finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		
		renderPassConfigs[HUD_SCREEN_PIPELINE].attachmentDescriptions[1].flags          = 0;
		renderPassConfigs[HUD_SCREEN_PIPELINE].attachmentDescriptions[1].samples        = VK_SAMPLE_COUNT_1_BIT;
		renderPassConfigs[HUD_SCREEN_PIPELINE].attachmentDescriptions[1].loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
		renderPassConfigs[HUD_SCREEN_PIPELINE].attachmentDescriptions[1].storeOp        = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		renderPassConfigs[HUD_SCREEN_PIPELINE].attachmentDescriptions[1].stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		renderPassConfigs[HUD_SCREEN_PIPELINE].attachmentDescriptions[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		renderPassConfigs[HUD_SCREEN_PIPELINE].attachmentDescriptions[1].initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
		renderPassConfigs[HUD_SCREEN_PIPELINE].attachmentDescriptions[1].finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		
		renderPassConfigs[HUD_SCREEN_PIPELINE].actualAttachmentReferenceNumber          = 2;
		renderPassConfigs[HUD_SCREEN_PIPELINE].attachmentReferences[0].attachment       = 0;
		renderPassConfigs[HUD_SCREEN_PIPELINE].attachmentReferences[0].layout           = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		
		renderPassConfigs[HUD_SCREEN_PIPELINE].attachmentReferences[1].attachment       = 1;
		renderPassConfigs[HUD_SCREEN_PIPELINE].attachmentReferences[1].layout           = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		
		renderPassConfigs[HUD_SCREEN_PIPELINE].actualSubpassDependencyNumber            = 2;
		/// Order this pass after previous writes to the shared swapchain/depth images.
		renderPassConfigs[HUD_SCREEN_PIPELINE].subpassDependencies[0].srcSubpass        = VK_SUBPASS_EXTERNAL;
		renderPassConfigs[HUD_SCREEN_PIPELINE].subpassDependencies[0].dstSubpass        = 0;
		renderPassConfigs[HUD_SCREEN_PIPELINE].subpassDependencies[0].srcStageMask      = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		renderPassConfigs[HUD_SCREEN_PIPELINE].subpassDependencies[0].dstStageMask      = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		renderPassConfigs[HUD_SCREEN_PIPELINE].subpassDependencies[0].srcAccessMask     = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		renderPassConfigs[HUD_SCREEN_PIPELINE].subpassDependencies[0].dstAccessMask     = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		renderPassConfigs[HUD_SCREEN_PIPELINE].subpassDependencies[0].dependencyFlags   = {};
		renderPassConfigs[HUD_SCREEN_PIPELINE].subpassDependencies[1].srcSubpass        = 0;
		renderPassConfigs[HUD_SCREEN_PIPELINE].subpassDependencies[1].dstSubpass        = VK_SUBPASS_EXTERNAL;
		renderPassConfigs[HUD_SCREEN_PIPELINE].subpassDependencies[1].srcStageMask      = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		renderPassConfigs[HUD_SCREEN_PIPELINE].subpassDependencies[1].dstStageMask      = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		renderPassConfigs[HUD_SCREEN_PIPELINE].subpassDependencies[1].srcAccessMask     = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		renderPassConfigs[HUD_SCREEN_PIPELINE].subpassDependencies[1].dstAccessMask     = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		renderPassConfigs[HUD_SCREEN_PIPELINE].subpassDependencies[1].dependencyFlags   = {};

		/// ===================================================================================== ///

		descriptorSetsConfig[UI].actualLinkedDescriptorBindingsNumber                           = 1;
		descriptorSetsConfig[UI].hostDescriptorNumber                                           = 128;
		descriptorSetsConfig[UI].isTexture                                                      = false;

		descriptorBindingsConfig[7].vkType                  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		descriptorBindingsConfig[7].shaderStageFlag         = VK_SHADER_STAGE_VERTEX_BIT;
		descriptorBindingsConfig[7].binding                 = 0;
		descriptorBindingsConfig[7].shaderDescriptorsNumber = 1;
		descriptorBindingsConfig[7].uboChunkSize            = sizeof(UI_UBO);

		descriptorSetsConfig[UI_SAMPLERS].actualLinkedDescriptorBindingsNumber                  = 1;
		descriptorSetsConfig[UI_SAMPLERS].hostDescriptorNumber                                  = TEXTURE_DESCRIPTOR_SETS_NUMBER;
		descriptorSetsConfig[UI_SAMPLERS].isTexture                                             = true;

		descriptorBindingsConfig[8].vkType                  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		descriptorBindingsConfig[8].shaderStageFlag         = VK_SHADER_STAGE_FRAGMENT_BIT;
		descriptorBindingsConfig[8].binding                 = 0;
		descriptorBindingsConfig[8].shaderDescriptorsNumber = 1;
		
		pipelineConfigs[UI_PIPELINE].vertShader                       = "../VKshaders/ui_shaders/vert_ui.spv";
		pipelineConfigs[UI_PIPELINE].fragShader                       = "../VKshaders/ui_shaders/frag_ui.spv";
		pipelineConfigs[UI_PIPELINE].bindingDescription               = Vertex::getBindingDescription();
		pipelineConfigs[UI_PIPELINE].attributeDescriptions            = Vertex::getAttributeDescriptions();
		pipelineConfigs[UI_PIPELINE].actualLinkedDescriptorSetsNumber = 2;

		renderPassConfigs[UI_PIPELINE].actualAttachmentDescriptionNumber        = 2;
		renderPassConfigs[UI_PIPELINE].attachmentDescriptions[0].flags          = 0;
		renderPassConfigs[UI_PIPELINE].attachmentDescriptions[0].samples        = VK_SAMPLE_COUNT_1_BIT;
		renderPassConfigs[UI_PIPELINE].attachmentDescriptions[0].loadOp         = VK_ATTACHMENT_LOAD_OP_LOAD;
		renderPassConfigs[UI_PIPELINE].attachmentDescriptions[0].storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
		renderPassConfigs[UI_PIPELINE].attachmentDescriptions[0].stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		renderPassConfigs[UI_PIPELINE].attachmentDescriptions[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		renderPassConfigs[UI_PIPELINE].attachmentDescriptions[0].initialLayout  = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		renderPassConfigs[UI_PIPELINE].attachmentDescriptions[0].finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		
		renderPassConfigs[UI_PIPELINE].attachmentDescriptions[1].flags          = 0;
		renderPassConfigs[UI_PIPELINE].attachmentDescriptions[1].samples        = VK_SAMPLE_COUNT_1_BIT;
		renderPassConfigs[UI_PIPELINE].attachmentDescriptions[1].loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
		renderPassConfigs[UI_PIPELINE].attachmentDescriptions[1].storeOp        = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		renderPassConfigs[UI_PIPELINE].attachmentDescriptions[1].stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		renderPassConfigs[UI_PIPELINE].attachmentDescriptions[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		renderPassConfigs[UI_PIPELINE].attachmentDescriptions[1].initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
		renderPassConfigs[UI_PIPELINE].attachmentDescriptions[1].finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		
		renderPassConfigs[UI_PIPELINE].actualAttachmentReferenceNumber          = 2;
		renderPassConfigs[UI_PIPELINE].attachmentReferences[0].attachment       = 0;
		renderPassConfigs[UI_PIPELINE].attachmentReferences[0].layout           = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		
		renderPassConfigs[UI_PIPELINE].attachmentReferences[1].attachment       = 1;
		renderPassConfigs[UI_PIPELINE].attachmentReferences[1].layout           = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		
		renderPassConfigs[UI_PIPELINE].actualSubpassDependencyNumber            = 2;
		/// Order this pass after previous writes to the shared swapchain/depth images.
		renderPassConfigs[UI_PIPELINE].subpassDependencies[0].srcSubpass        = VK_SUBPASS_EXTERNAL;
		renderPassConfigs[UI_PIPELINE].subpassDependencies[0].dstSubpass        = 0;
		renderPassConfigs[UI_PIPELINE].subpassDependencies[0].srcStageMask      = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		renderPassConfigs[UI_PIPELINE].subpassDependencies[0].dstStageMask      = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		renderPassConfigs[UI_PIPELINE].subpassDependencies[0].srcAccessMask     = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		renderPassConfigs[UI_PIPELINE].subpassDependencies[0].dstAccessMask     = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		renderPassConfigs[UI_PIPELINE].subpassDependencies[0].dependencyFlags   = {};
		renderPassConfigs[UI_PIPELINE].subpassDependencies[1].srcSubpass        = 0;
		renderPassConfigs[UI_PIPELINE].subpassDependencies[1].dstSubpass        = VK_SUBPASS_EXTERNAL;
		renderPassConfigs[UI_PIPELINE].subpassDependencies[1].srcStageMask      = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		renderPassConfigs[UI_PIPELINE].subpassDependencies[1].dstStageMask      = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		renderPassConfigs[UI_PIPELINE].subpassDependencies[1].srcAccessMask     = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		renderPassConfigs[UI_PIPELINE].subpassDependencies[1].dstAccessMask     = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		renderPassConfigs[UI_PIPELINE].subpassDependencies[1].dependencyFlags   = {};

		/// ===================================================================================== ///

		descriptorSetsConfig[UI_ICONS].actualLinkedDescriptorBindingsNumber                     = 1;
		descriptorSetsConfig[UI_ICONS].hostDescriptorNumber                                     = 128;
		descriptorSetsConfig[UI_ICONS].isTexture                                                = false;

		descriptorBindingsConfig[9].vkType                  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		descriptorBindingsConfig[9].shaderStageFlag         = VK_SHADER_STAGE_VERTEX_BIT;
		descriptorBindingsConfig[9].binding                 = 0;
		descriptorBindingsConfig[9].shaderDescriptorsNumber = 1;
		descriptorBindingsConfig[9].uboChunkSize            = sizeof(UI_UBO);

		descriptorSetsConfig[UI_ICONS_SAMPLERS].actualLinkedDescriptorBindingsNumber            = 1;
		descriptorSetsConfig[UI_ICONS_SAMPLERS].hostDescriptorNumber                            = TEXTURE_DESCRIPTOR_SETS_NUMBER;
		descriptorSetsConfig[UI_ICONS_SAMPLERS].isTexture                                       = true;

		descriptorBindingsConfig[10].vkType                  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		descriptorBindingsConfig[10].shaderStageFlag         = VK_SHADER_STAGE_FRAGMENT_BIT;
		descriptorBindingsConfig[10].binding                 = 0;
		descriptorBindingsConfig[10].shaderDescriptorsNumber = 1;
		
		pipelineConfigs[UI_ICONS_PIPELINE].vertShader                       = "../VKshaders/ui_icons_shaders/vert_ui_icons.spv";
		pipelineConfigs[UI_ICONS_PIPELINE].fragShader                       = "../VKshaders/ui_icons_shaders/frag_ui_icons.spv";
		pipelineConfigs[UI_ICONS_PIPELINE].bindingDescription               = Vertex::getBindingDescription();
		pipelineConfigs[UI_ICONS_PIPELINE].attributeDescriptions            = Vertex::getAttributeDescriptions();
		pipelineConfigs[UI_ICONS_PIPELINE].actualLinkedDescriptorSetsNumber = 2;

		renderPassConfigs[UI_ICONS_PIPELINE].actualAttachmentDescriptionNumber        = 2;
		renderPassConfigs[UI_ICONS_PIPELINE].attachmentDescriptions[0].flags          = 0;
		renderPassConfigs[UI_ICONS_PIPELINE].attachmentDescriptions[0].samples        = VK_SAMPLE_COUNT_1_BIT;
		renderPassConfigs[UI_ICONS_PIPELINE].attachmentDescriptions[0].loadOp         = VK_ATTACHMENT_LOAD_OP_LOAD;
		renderPassConfigs[UI_ICONS_PIPELINE].attachmentDescriptions[0].storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
		renderPassConfigs[UI_ICONS_PIPELINE].attachmentDescriptions[0].stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		renderPassConfigs[UI_ICONS_PIPELINE].attachmentDescriptions[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		renderPassConfigs[UI_ICONS_PIPELINE].attachmentDescriptions[0].initialLayout  = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		renderPassConfigs[UI_ICONS_PIPELINE].attachmentDescriptions[0].finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		
		renderPassConfigs[UI_ICONS_PIPELINE].attachmentDescriptions[1].flags          = 0;
		renderPassConfigs[UI_ICONS_PIPELINE].attachmentDescriptions[1].samples        = VK_SAMPLE_COUNT_1_BIT;
		renderPassConfigs[UI_ICONS_PIPELINE].attachmentDescriptions[1].loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
		renderPassConfigs[UI_ICONS_PIPELINE].attachmentDescriptions[1].storeOp        = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		renderPassConfigs[UI_ICONS_PIPELINE].attachmentDescriptions[1].stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		renderPassConfigs[UI_ICONS_PIPELINE].attachmentDescriptions[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		renderPassConfigs[UI_ICONS_PIPELINE].attachmentDescriptions[1].initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
		renderPassConfigs[UI_ICONS_PIPELINE].attachmentDescriptions[1].finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		
		renderPassConfigs[UI_ICONS_PIPELINE].actualAttachmentReferenceNumber          = 2;
		renderPassConfigs[UI_ICONS_PIPELINE].attachmentReferences[0].attachment       = 0;
		renderPassConfigs[UI_ICONS_PIPELINE].attachmentReferences[0].layout           = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		
		renderPassConfigs[UI_ICONS_PIPELINE].attachmentReferences[1].attachment       = 1;
		renderPassConfigs[UI_ICONS_PIPELINE].attachmentReferences[1].layout           = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		
		renderPassConfigs[UI_ICONS_PIPELINE].actualSubpassDependencyNumber            = 2;
		/// Order this pass after previous writes to the shared swapchain/depth images.
		renderPassConfigs[UI_ICONS_PIPELINE].subpassDependencies[0].srcSubpass        = VK_SUBPASS_EXTERNAL;
		renderPassConfigs[UI_ICONS_PIPELINE].subpassDependencies[0].dstSubpass        = 0;
		renderPassConfigs[UI_ICONS_PIPELINE].subpassDependencies[0].srcStageMask      = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		renderPassConfigs[UI_ICONS_PIPELINE].subpassDependencies[0].dstStageMask      = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		renderPassConfigs[UI_ICONS_PIPELINE].subpassDependencies[0].srcAccessMask     = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		renderPassConfigs[UI_ICONS_PIPELINE].subpassDependencies[0].dstAccessMask     = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		renderPassConfigs[UI_ICONS_PIPELINE].subpassDependencies[0].dependencyFlags   = {};
		renderPassConfigs[UI_ICONS_PIPELINE].subpassDependencies[1].srcSubpass        = 0;
		renderPassConfigs[UI_ICONS_PIPELINE].subpassDependencies[1].dstSubpass        = VK_SUBPASS_EXTERNAL;
		renderPassConfigs[UI_ICONS_PIPELINE].subpassDependencies[1].srcStageMask      = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		renderPassConfigs[UI_ICONS_PIPELINE].subpassDependencies[1].dstStageMask      = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		renderPassConfigs[UI_ICONS_PIPELINE].subpassDependencies[1].srcAccessMask     = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		renderPassConfigs[UI_ICONS_PIPELINE].subpassDependencies[1].dstAccessMask     = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		renderPassConfigs[UI_ICONS_PIPELINE].subpassDependencies[1].dependencyFlags   = {};

		/// ===================================================================================== ///

		descriptorSetsConfig[VIRTUAL_TEXTURES_UBO].actualLinkedDescriptorBindingsNumber         = 1;
		descriptorSetsConfig[VIRTUAL_TEXTURES_UBO].hostDescriptorNumber                         = 128;
		descriptorSetsConfig[VIRTUAL_TEXTURES_UBO].isTexture                                    = false;

		descriptorBindingsConfig[11].vkType                  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		descriptorBindingsConfig[11].shaderStageFlag         = VK_SHADER_STAGE_VERTEX_BIT;
		descriptorBindingsConfig[11].binding                 = 0;
		descriptorBindingsConfig[11].shaderDescriptorsNumber = 1;
		descriptorBindingsConfig[11].uboChunkSize            = sizeof(VIRTUAL_TEXTURE_UBO);

		descriptorSetsConfig[VIRTUAL_TEXTURES_TILESET].actualLinkedDescriptorBindingsNumber     = 1;
		descriptorSetsConfig[VIRTUAL_TEXTURES_TILESET].hostDescriptorNumber                     = TEXTURE_DESCRIPTOR_SETS_NUMBER;
		descriptorSetsConfig[VIRTUAL_TEXTURES_TILESET].isTexture                                = true;

		descriptorBindingsConfig[12].vkType                  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		descriptorBindingsConfig[12].shaderStageFlag         = VK_SHADER_STAGE_FRAGMENT_BIT;
		descriptorBindingsConfig[12].binding                 = 0;
		descriptorBindingsConfig[12].shaderDescriptorsNumber = 1;
		
		pipelineConfigs[VIRTUAL_TEXTURES_PIPELINE].vertShader                       = "../VKshaders/virtualTextures/virtualTexturesVert.spv";
		pipelineConfigs[VIRTUAL_TEXTURES_PIPELINE].fragShader                       = "../VKshaders/virtualTextures/virtualTexturesFrag.spv";
		pipelineConfigs[VIRTUAL_TEXTURES_PIPELINE].bindingDescription               = Vertex::getBindingDescription();
		pipelineConfigs[VIRTUAL_TEXTURES_PIPELINE].attributeDescriptions            = Vertex::getAttributeDescriptions();
		pipelineConfigs[VIRTUAL_TEXTURES_PIPELINE].actualLinkedDescriptorSetsNumber = 2;

		renderPassConfigs[VIRTUAL_TEXTURES_PIPELINE].actualAttachmentDescriptionNumber        = 2;
		renderPassConfigs[VIRTUAL_TEXTURES_PIPELINE].attachmentDescriptions[0].flags          = 0;
		renderPassConfigs[VIRTUAL_TEXTURES_PIPELINE].attachmentDescriptions[0].samples        = VK_SAMPLE_COUNT_1_BIT;
		renderPassConfigs[VIRTUAL_TEXTURES_PIPELINE].attachmentDescriptions[0].loadOp         = VK_ATTACHMENT_LOAD_OP_LOAD;
		renderPassConfigs[VIRTUAL_TEXTURES_PIPELINE].attachmentDescriptions[0].storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
		renderPassConfigs[VIRTUAL_TEXTURES_PIPELINE].attachmentDescriptions[0].stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		renderPassConfigs[VIRTUAL_TEXTURES_PIPELINE].attachmentDescriptions[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		renderPassConfigs[VIRTUAL_TEXTURES_PIPELINE].attachmentDescriptions[0].initialLayout  = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		renderPassConfigs[VIRTUAL_TEXTURES_PIPELINE].attachmentDescriptions[0].finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		
		renderPassConfigs[VIRTUAL_TEXTURES_PIPELINE].attachmentDescriptions[1].flags          = 0;
		renderPassConfigs[VIRTUAL_TEXTURES_PIPELINE].attachmentDescriptions[1].samples        = VK_SAMPLE_COUNT_1_BIT;
		renderPassConfigs[VIRTUAL_TEXTURES_PIPELINE].attachmentDescriptions[1].loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
		renderPassConfigs[VIRTUAL_TEXTURES_PIPELINE].attachmentDescriptions[1].storeOp        = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		renderPassConfigs[VIRTUAL_TEXTURES_PIPELINE].attachmentDescriptions[1].stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		renderPassConfigs[VIRTUAL_TEXTURES_PIPELINE].attachmentDescriptions[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		renderPassConfigs[VIRTUAL_TEXTURES_PIPELINE].attachmentDescriptions[1].initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
		renderPassConfigs[VIRTUAL_TEXTURES_PIPELINE].attachmentDescriptions[1].finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		
		renderPassConfigs[VIRTUAL_TEXTURES_PIPELINE].actualAttachmentReferenceNumber          = 2;
		renderPassConfigs[VIRTUAL_TEXTURES_PIPELINE].attachmentReferences[0].attachment       = 0;
		renderPassConfigs[VIRTUAL_TEXTURES_PIPELINE].attachmentReferences[0].layout           = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		
		renderPassConfigs[VIRTUAL_TEXTURES_PIPELINE].attachmentReferences[1].attachment       = 1;
		renderPassConfigs[VIRTUAL_TEXTURES_PIPELINE].attachmentReferences[1].layout           = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		
		renderPassConfigs[VIRTUAL_TEXTURES_PIPELINE].actualSubpassDependencyNumber            = 2;
		/// Order this pass after previous writes to the shared swapchain/depth images.
		renderPassConfigs[VIRTUAL_TEXTURES_PIPELINE].subpassDependencies[0].srcSubpass        = VK_SUBPASS_EXTERNAL;
		renderPassConfigs[VIRTUAL_TEXTURES_PIPELINE].subpassDependencies[0].dstSubpass        = 0;
		renderPassConfigs[VIRTUAL_TEXTURES_PIPELINE].subpassDependencies[0].srcStageMask      = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		renderPassConfigs[VIRTUAL_TEXTURES_PIPELINE].subpassDependencies[0].dstStageMask      = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		renderPassConfigs[VIRTUAL_TEXTURES_PIPELINE].subpassDependencies[0].srcAccessMask     = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		renderPassConfigs[VIRTUAL_TEXTURES_PIPELINE].subpassDependencies[0].dstAccessMask     = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		renderPassConfigs[VIRTUAL_TEXTURES_PIPELINE].subpassDependencies[0].dependencyFlags   = {};
		renderPassConfigs[VIRTUAL_TEXTURES_PIPELINE].subpassDependencies[1].srcSubpass        = 0;
		renderPassConfigs[VIRTUAL_TEXTURES_PIPELINE].subpassDependencies[1].dstSubpass        = VK_SUBPASS_EXTERNAL;
		renderPassConfigs[VIRTUAL_TEXTURES_PIPELINE].subpassDependencies[1].srcStageMask      = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		renderPassConfigs[VIRTUAL_TEXTURES_PIPELINE].subpassDependencies[1].dstStageMask      = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		renderPassConfigs[VIRTUAL_TEXTURES_PIPELINE].subpassDependencies[1].srcAccessMask     = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		renderPassConfigs[VIRTUAL_TEXTURES_PIPELINE].subpassDependencies[1].dstAccessMask     = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		renderPassConfigs[VIRTUAL_TEXTURES_PIPELINE].subpassDependencies[1].dependencyFlags   = {};

		/// ===================================================================================== ///

		descriptorSetsConfig[MAIN_RENDER_MATRIX_UBO].actualLinkedDescriptorBindingsNumber       = 1;
		descriptorSetsConfig[MAIN_RENDER_MATRIX_UBO].hostDescriptorNumber                       = 1024;
		descriptorSetsConfig[MAIN_RENDER_MATRIX_UBO].isTexture                                  = false;

		descriptorBindingsConfig[13].vkType                  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		descriptorBindingsConfig[13].shaderStageFlag         = VK_SHADER_STAGE_VERTEX_BIT;
		descriptorBindingsConfig[13].binding                 = 0;
		descriptorBindingsConfig[13].shaderDescriptorsNumber = 1;
		descriptorBindingsConfig[13].uboChunkSize            = sizeof(ModelMatrixUBO);

		descriptorSetsConfig[MAIN_RENDER_LIGHT_DATA_UBO].actualLinkedDescriptorBindingsNumber   = 4;
		descriptorSetsConfig[MAIN_RENDER_LIGHT_DATA_UBO].hostDescriptorNumber                   = 2;
		descriptorSetsConfig[MAIN_RENDER_LIGHT_DATA_UBO].isTexture                              = false;

		descriptorBindingsConfig[14].vkType                  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		descriptorBindingsConfig[14].shaderStageFlag         = VK_SHADER_STAGE_FRAGMENT_BIT;
		descriptorBindingsConfig[14].binding                 = 0;
		descriptorBindingsConfig[14].shaderDescriptorsNumber = 1;
		descriptorBindingsConfig[14].uboChunkSize            = sizeof(LightData);

		descriptorBindingsConfig[15].vkType                  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		descriptorBindingsConfig[15].shaderStageFlag         = VK_SHADER_STAGE_FRAGMENT_BIT;
		descriptorBindingsConfig[15].binding                 = 1;
		descriptorBindingsConfig[15].shaderDescriptorsNumber = 4;

		descriptorBindingsConfig[16].vkType                  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		descriptorBindingsConfig[16].shaderStageFlag         = VK_SHADER_STAGE_FRAGMENT_BIT;
		descriptorBindingsConfig[16].binding                 = 5;
		descriptorBindingsConfig[16].shaderDescriptorsNumber = 32;

		descriptorBindingsConfig[17].vkType                  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		descriptorBindingsConfig[17].shaderStageFlag         = VK_SHADER_STAGE_FRAGMENT_BIT;
		descriptorBindingsConfig[17].binding                 = 37;
		descriptorBindingsConfig[17].shaderDescriptorsNumber = 8;

		descriptorSetsConfig[MAIN_RENDER_SPECULAR_SAMPLER].actualLinkedDescriptorBindingsNumber = 1;
		descriptorSetsConfig[MAIN_RENDER_SPECULAR_SAMPLER].hostDescriptorNumber                 = TEXTURE_DESCRIPTOR_SETS_NUMBER;
		descriptorSetsConfig[MAIN_RENDER_SPECULAR_SAMPLER].isTexture                            = true;

		descriptorBindingsConfig[18].vkType                  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		descriptorBindingsConfig[18].shaderStageFlag         = VK_SHADER_STAGE_FRAGMENT_BIT;
		descriptorBindingsConfig[18].binding                 = 0;
		descriptorBindingsConfig[18].shaderDescriptorsNumber = 1;

		descriptorSetsConfig[MAIN_RENDER_DIFFUSE_SAMPLER].actualLinkedDescriptorBindingsNumber  = 1;
		descriptorSetsConfig[MAIN_RENDER_DIFFUSE_SAMPLER].hostDescriptorNumber                  = TEXTURE_DESCRIPTOR_SETS_NUMBER;
		descriptorSetsConfig[MAIN_RENDER_DIFFUSE_SAMPLER].isTexture                             = true;

		descriptorBindingsConfig[19].vkType                  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		descriptorBindingsConfig[19].shaderStageFlag         = VK_SHADER_STAGE_FRAGMENT_BIT;
		descriptorBindingsConfig[19].binding                 = 0;
		descriptorBindingsConfig[19].shaderDescriptorsNumber = 1;
		
		pipelineConfigs[MAIN_RENDER_PIPELINE].vertShader                       = "../VKshaders/mainRendererShaders/vert.spv";
		pipelineConfigs[MAIN_RENDER_PIPELINE].fragShader                       = "../VKshaders/mainRendererShaders/frag.spv";
		pipelineConfigs[MAIN_RENDER_PIPELINE].bindingDescription               = Vertex::getBindingDescription();
		pipelineConfigs[MAIN_RENDER_PIPELINE].attributeDescriptions            = Vertex::getAttributeDescriptions();
		pipelineConfigs[MAIN_RENDER_PIPELINE].actualLinkedDescriptorSetsNumber = 4;

		renderPassConfigs[MAIN_RENDER_PIPELINE].actualAttachmentDescriptionNumber        = 2;
		renderPassConfigs[MAIN_RENDER_PIPELINE].attachmentDescriptions[0].flags          = 0;
		renderPassConfigs[MAIN_RENDER_PIPELINE].attachmentDescriptions[0].samples        = VK_SAMPLE_COUNT_1_BIT;
		renderPassConfigs[MAIN_RENDER_PIPELINE].attachmentDescriptions[0].loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
		renderPassConfigs[MAIN_RENDER_PIPELINE].attachmentDescriptions[0].storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
		renderPassConfigs[MAIN_RENDER_PIPELINE].attachmentDescriptions[0].stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		renderPassConfigs[MAIN_RENDER_PIPELINE].attachmentDescriptions[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		renderPassConfigs[MAIN_RENDER_PIPELINE].attachmentDescriptions[0].initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
		renderPassConfigs[MAIN_RENDER_PIPELINE].attachmentDescriptions[0].finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		
		renderPassConfigs[MAIN_RENDER_PIPELINE].attachmentDescriptions[1].flags          = 0;
		renderPassConfigs[MAIN_RENDER_PIPELINE].attachmentDescriptions[1].samples        = VK_SAMPLE_COUNT_1_BIT;
		renderPassConfigs[MAIN_RENDER_PIPELINE].attachmentDescriptions[1].loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
		renderPassConfigs[MAIN_RENDER_PIPELINE].attachmentDescriptions[1].storeOp        = VK_ATTACHMENT_STORE_OP_STORE;          ///< The water tank tests and samples the scene depth
		renderPassConfigs[MAIN_RENDER_PIPELINE].attachmentDescriptions[1].stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		renderPassConfigs[MAIN_RENDER_PIPELINE].attachmentDescriptions[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		renderPassConfigs[MAIN_RENDER_PIPELINE].attachmentDescriptions[1].initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
		renderPassConfigs[MAIN_RENDER_PIPELINE].attachmentDescriptions[1].finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		
		renderPassConfigs[MAIN_RENDER_PIPELINE].actualAttachmentReferenceNumber          = 2;
		renderPassConfigs[MAIN_RENDER_PIPELINE].attachmentReferences[0].attachment       = 0;
		renderPassConfigs[MAIN_RENDER_PIPELINE].attachmentReferences[0].layout           = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		
		renderPassConfigs[MAIN_RENDER_PIPELINE].attachmentReferences[1].attachment       = 1;
		renderPassConfigs[MAIN_RENDER_PIPELINE].attachmentReferences[1].layout           = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		
		renderPassConfigs[MAIN_RENDER_PIPELINE].actualSubpassDependencyNumber            = 2;
		/// Order this pass after previous writes to the shared swapchain/depth images.
		renderPassConfigs[MAIN_RENDER_PIPELINE].subpassDependencies[0].srcSubpass        = VK_SUBPASS_EXTERNAL;
		renderPassConfigs[MAIN_RENDER_PIPELINE].subpassDependencies[0].dstSubpass        = 0;
		renderPassConfigs[MAIN_RENDER_PIPELINE].subpassDependencies[0].srcStageMask      = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		renderPassConfigs[MAIN_RENDER_PIPELINE].subpassDependencies[0].dstStageMask      = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		renderPassConfigs[MAIN_RENDER_PIPELINE].subpassDependencies[0].srcAccessMask     = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		renderPassConfigs[MAIN_RENDER_PIPELINE].subpassDependencies[0].dstAccessMask     = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		renderPassConfigs[MAIN_RENDER_PIPELINE].subpassDependencies[0].dependencyFlags   = {};
		renderPassConfigs[MAIN_RENDER_PIPELINE].subpassDependencies[1].srcSubpass        = 0;
		renderPassConfigs[MAIN_RENDER_PIPELINE].subpassDependencies[1].dstSubpass        = VK_SUBPASS_EXTERNAL;
		renderPassConfigs[MAIN_RENDER_PIPELINE].subpassDependencies[1].srcStageMask      = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		renderPassConfigs[MAIN_RENDER_PIPELINE].subpassDependencies[1].dstStageMask      = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		renderPassConfigs[MAIN_RENDER_PIPELINE].subpassDependencies[1].srcAccessMask     = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		renderPassConfigs[MAIN_RENDER_PIPELINE].subpassDependencies[1].dstAccessMask     = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		renderPassConfigs[MAIN_RENDER_PIPELINE].subpassDependencies[1].dependencyFlags   = {};

		/// ===================================================================================== ///

		descriptorSetsConfig[SDF_DATA].actualLinkedDescriptorBindingsNumber                   = 1;
		descriptorSetsConfig[SDF_DATA].hostDescriptorNumber                                   = 64;
		descriptorSetsConfig[SDF_DATA].isTexture                                              = false;

		descriptorBindingsConfig[20].vkType                  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		descriptorBindingsConfig[20].shaderStageFlag         = VK_SHADER_STAGE_VERTEX_BIT;
		descriptorBindingsConfig[20].binding                 = 0;
		descriptorBindingsConfig[20].shaderDescriptorsNumber = 1;
		descriptorBindingsConfig[20].uboChunkSize            = sizeof(SDF_UBO);
		
		pipelineConfigs[SDF_PIPELINE].vertShader                       = "../VKshaders/sdf/sdf_vert.spv";
		pipelineConfigs[SDF_PIPELINE].fragShader                       = "../VKshaders/sdf/sdf_frag.spv";
		pipelineConfigs[SDF_PIPELINE].bindingDescription               = Vertex::getBindingDescription();
		pipelineConfigs[SDF_PIPELINE].attributeDescriptions            = Vertex::getAttributeDescriptions();
		pipelineConfigs[SDF_PIPELINE].actualLinkedDescriptorSetsNumber = 1;

		renderPassConfigs[SDF_PIPELINE].actualAttachmentDescriptionNumber        = 2;
		renderPassConfigs[SDF_PIPELINE].attachmentDescriptions[0].flags          = 0;
		renderPassConfigs[SDF_PIPELINE].attachmentDescriptions[0].samples        = VK_SAMPLE_COUNT_1_BIT;
		renderPassConfigs[SDF_PIPELINE].attachmentDescriptions[0].loadOp         = VK_ATTACHMENT_LOAD_OP_LOAD;
		renderPassConfigs[SDF_PIPELINE].attachmentDescriptions[0].storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
		renderPassConfigs[SDF_PIPELINE].attachmentDescriptions[0].stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		renderPassConfigs[SDF_PIPELINE].attachmentDescriptions[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		renderPassConfigs[SDF_PIPELINE].attachmentDescriptions[0].initialLayout  = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		renderPassConfigs[SDF_PIPELINE].attachmentDescriptions[0].finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		
		renderPassConfigs[SDF_PIPELINE].attachmentDescriptions[1].flags          = 0;
		renderPassConfigs[SDF_PIPELINE].attachmentDescriptions[1].samples        = VK_SAMPLE_COUNT_1_BIT;
		renderPassConfigs[SDF_PIPELINE].attachmentDescriptions[1].loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
		renderPassConfigs[SDF_PIPELINE].attachmentDescriptions[1].storeOp        = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		renderPassConfigs[SDF_PIPELINE].attachmentDescriptions[1].stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		renderPassConfigs[SDF_PIPELINE].attachmentDescriptions[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		renderPassConfigs[SDF_PIPELINE].attachmentDescriptions[1].initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
		renderPassConfigs[SDF_PIPELINE].attachmentDescriptions[1].finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		
		renderPassConfigs[SDF_PIPELINE].actualAttachmentReferenceNumber          = 2;
		renderPassConfigs[SDF_PIPELINE].attachmentReferences[0].attachment       = 0;
		renderPassConfigs[SDF_PIPELINE].attachmentReferences[0].layout           = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		
		renderPassConfigs[SDF_PIPELINE].attachmentReferences[1].attachment       = 1;
		renderPassConfigs[SDF_PIPELINE].attachmentReferences[1].layout           = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		
		renderPassConfigs[SDF_PIPELINE].actualSubpassDependencyNumber            = 2;
		/// Order this pass after previous writes to the shared swapchain/depth images.
		renderPassConfigs[SDF_PIPELINE].subpassDependencies[0].srcSubpass        = VK_SUBPASS_EXTERNAL;
		renderPassConfigs[SDF_PIPELINE].subpassDependencies[0].dstSubpass        = 0;
		renderPassConfigs[SDF_PIPELINE].subpassDependencies[0].srcStageMask      = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		renderPassConfigs[SDF_PIPELINE].subpassDependencies[0].dstStageMask      = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		renderPassConfigs[SDF_PIPELINE].subpassDependencies[0].srcAccessMask     = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		renderPassConfigs[SDF_PIPELINE].subpassDependencies[0].dstAccessMask     = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		renderPassConfigs[SDF_PIPELINE].subpassDependencies[0].dependencyFlags   = {};
		renderPassConfigs[SDF_PIPELINE].subpassDependencies[1].srcSubpass        = 0;
		renderPassConfigs[SDF_PIPELINE].subpassDependencies[1].dstSubpass        = VK_SUBPASS_EXTERNAL;
		renderPassConfigs[SDF_PIPELINE].subpassDependencies[1].srcStageMask      = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		renderPassConfigs[SDF_PIPELINE].subpassDependencies[1].dstStageMask      = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		renderPassConfigs[SDF_PIPELINE].subpassDependencies[1].srcAccessMask     = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		renderPassConfigs[SDF_PIPELINE].subpassDependencies[1].dstAccessMask     = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		renderPassConfigs[SDF_PIPELINE].subpassDependencies[1].dependencyFlags   = {};

		/// ===================================================================================== ///

		descriptorSetsConfig[COLLISIONS_DEBUG_DATA].actualLinkedDescriptorBindingsNumber                   = 1;
		descriptorSetsConfig[COLLISIONS_DEBUG_DATA].hostDescriptorNumber                                   = 1024;
		descriptorSetsConfig[COLLISIONS_DEBUG_DATA].isTexture                                              = false;

		descriptorBindingsConfig[21].vkType                  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		descriptorBindingsConfig[21].shaderStageFlag         = VK_SHADER_STAGE_VERTEX_BIT;
		descriptorBindingsConfig[21].binding                 = 0;
		descriptorBindingsConfig[21].shaderDescriptorsNumber = 1;
		descriptorBindingsConfig[21].uboChunkSize            = sizeof(COLLISIONS_DEBUG_UBO);
		
		pipelineConfigs[COLLISIONS_DEBUG_PIPELINE].vertShader                       = "../VKshaders/debug/collisions/vertDebug.spv";
		pipelineConfigs[COLLISIONS_DEBUG_PIPELINE].fragShader                       = "../VKshaders/debug/collisions/fragDebug.spv";
		pipelineConfigs[COLLISIONS_DEBUG_PIPELINE].bindingDescription               = Vertex::getBindingDescription();
		pipelineConfigs[COLLISIONS_DEBUG_PIPELINE].attributeDescriptions            = Vertex::getAttributeDescriptions();
		pipelineConfigs[COLLISIONS_DEBUG_PIPELINE].actualLinkedDescriptorSetsNumber = 1;
		pipelineConfigs[COLLISIONS_DEBUG_PIPELINE].polygonMode                      = VK_POLYGON_MODE_LINE;

		renderPassConfigs[COLLISIONS_DEBUG_PIPELINE].actualAttachmentDescriptionNumber        = 2;
		renderPassConfigs[COLLISIONS_DEBUG_PIPELINE].attachmentDescriptions[0].flags          = 0;
		renderPassConfigs[COLLISIONS_DEBUG_PIPELINE].attachmentDescriptions[0].samples        = VK_SAMPLE_COUNT_1_BIT;
		renderPassConfigs[COLLISIONS_DEBUG_PIPELINE].attachmentDescriptions[0].loadOp         = VK_ATTACHMENT_LOAD_OP_LOAD;
		renderPassConfigs[COLLISIONS_DEBUG_PIPELINE].attachmentDescriptions[0].storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
		renderPassConfigs[COLLISIONS_DEBUG_PIPELINE].attachmentDescriptions[0].stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		renderPassConfigs[COLLISIONS_DEBUG_PIPELINE].attachmentDescriptions[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		renderPassConfigs[COLLISIONS_DEBUG_PIPELINE].attachmentDescriptions[0].initialLayout  = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		renderPassConfigs[COLLISIONS_DEBUG_PIPELINE].attachmentDescriptions[0].finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		
		renderPassConfigs[COLLISIONS_DEBUG_PIPELINE].attachmentDescriptions[1].flags          = 0;
		renderPassConfigs[COLLISIONS_DEBUG_PIPELINE].attachmentDescriptions[1].samples        = VK_SAMPLE_COUNT_1_BIT;
		renderPassConfigs[COLLISIONS_DEBUG_PIPELINE].attachmentDescriptions[1].loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
		renderPassConfigs[COLLISIONS_DEBUG_PIPELINE].attachmentDescriptions[1].storeOp        = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		renderPassConfigs[COLLISIONS_DEBUG_PIPELINE].attachmentDescriptions[1].stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		renderPassConfigs[COLLISIONS_DEBUG_PIPELINE].attachmentDescriptions[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		renderPassConfigs[COLLISIONS_DEBUG_PIPELINE].attachmentDescriptions[1].initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
		renderPassConfigs[COLLISIONS_DEBUG_PIPELINE].attachmentDescriptions[1].finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		
		renderPassConfigs[COLLISIONS_DEBUG_PIPELINE].actualAttachmentReferenceNumber          = 2;
		renderPassConfigs[COLLISIONS_DEBUG_PIPELINE].attachmentReferences[0].attachment       = 0;
		renderPassConfigs[COLLISIONS_DEBUG_PIPELINE].attachmentReferences[0].layout           = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		
		renderPassConfigs[COLLISIONS_DEBUG_PIPELINE].attachmentReferences[1].attachment       = 1;
		renderPassConfigs[COLLISIONS_DEBUG_PIPELINE].attachmentReferences[1].layout           = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		
		renderPassConfigs[COLLISIONS_DEBUG_PIPELINE].actualSubpassDependencyNumber            = 2;
		/// Order this pass after previous writes to the shared swapchain/depth images.
		renderPassConfigs[COLLISIONS_DEBUG_PIPELINE].subpassDependencies[0].srcSubpass        = VK_SUBPASS_EXTERNAL;
		renderPassConfigs[COLLISIONS_DEBUG_PIPELINE].subpassDependencies[0].dstSubpass        = 0;
		renderPassConfigs[COLLISIONS_DEBUG_PIPELINE].subpassDependencies[0].srcStageMask      = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		renderPassConfigs[COLLISIONS_DEBUG_PIPELINE].subpassDependencies[0].dstStageMask      = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		renderPassConfigs[COLLISIONS_DEBUG_PIPELINE].subpassDependencies[0].srcAccessMask     = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		renderPassConfigs[COLLISIONS_DEBUG_PIPELINE].subpassDependencies[0].dstAccessMask     = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		renderPassConfigs[COLLISIONS_DEBUG_PIPELINE].subpassDependencies[0].dependencyFlags   = {};
		renderPassConfigs[COLLISIONS_DEBUG_PIPELINE].subpassDependencies[1].srcSubpass        = 0;
		renderPassConfigs[COLLISIONS_DEBUG_PIPELINE].subpassDependencies[1].dstSubpass        = VK_SUBPASS_EXTERNAL;
		renderPassConfigs[COLLISIONS_DEBUG_PIPELINE].subpassDependencies[1].srcStageMask      = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		renderPassConfigs[COLLISIONS_DEBUG_PIPELINE].subpassDependencies[1].dstStageMask      = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		renderPassConfigs[COLLISIONS_DEBUG_PIPELINE].subpassDependencies[1].srcAccessMask     = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		renderPassConfigs[COLLISIONS_DEBUG_PIPELINE].subpassDependencies[1].dstAccessMask     = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		renderPassConfigs[COLLISIONS_DEBUG_PIPELINE].subpassDependencies[1].dependencyFlags   = {};

		/// ===================================================================================== ///

		descriptorSetsConfig[SPACIAL_GRID_DEBUG_DATA].actualLinkedDescriptorBindingsNumber                   = 1;
		descriptorSetsConfig[SPACIAL_GRID_DEBUG_DATA].hostDescriptorNumber                                   = 16384;
		descriptorSetsConfig[SPACIAL_GRID_DEBUG_DATA].isTexture                                              = false;

		descriptorBindingsConfig[22].vkType                  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		descriptorBindingsConfig[22].shaderStageFlag         = VK_SHADER_STAGE_VERTEX_BIT;
		descriptorBindingsConfig[22].binding                 = 0;
		descriptorBindingsConfig[22].shaderDescriptorsNumber = 1;
		descriptorBindingsConfig[22].uboChunkSize            = sizeof(COLLISIONS_DEBUG_UBO);


		pipelineConfigs[SPACIAL_GRID_DEBUG_PIPELINE].vertShader                       = "../VKshaders/debug/collisions/vertDebug.spv";
		pipelineConfigs[SPACIAL_GRID_DEBUG_PIPELINE].fragShader                       = "../VKshaders/debug/collisions/fragDebug.spv";
		pipelineConfigs[SPACIAL_GRID_DEBUG_PIPELINE].bindingDescription               = Vertex::getBindingDescription();
		pipelineConfigs[SPACIAL_GRID_DEBUG_PIPELINE].attributeDescriptions            = Vertex::getAttributeDescriptions();
		pipelineConfigs[SPACIAL_GRID_DEBUG_PIPELINE].actualLinkedDescriptorSetsNumber = 1;
		pipelineConfigs[SPACIAL_GRID_DEBUG_PIPELINE].polygonMode                      = VK_POLYGON_MODE_LINE;

		renderPassConfigs[SPACIAL_GRID_DEBUG_PIPELINE].actualAttachmentDescriptionNumber        = 2;
		renderPassConfigs[SPACIAL_GRID_DEBUG_PIPELINE].attachmentDescriptions[0].flags          = 0;
		renderPassConfigs[SPACIAL_GRID_DEBUG_PIPELINE].attachmentDescriptions[0].samples        = VK_SAMPLE_COUNT_1_BIT;
		renderPassConfigs[SPACIAL_GRID_DEBUG_PIPELINE].attachmentDescriptions[0].loadOp         = VK_ATTACHMENT_LOAD_OP_LOAD;
		renderPassConfigs[SPACIAL_GRID_DEBUG_PIPELINE].attachmentDescriptions[0].storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
		renderPassConfigs[SPACIAL_GRID_DEBUG_PIPELINE].attachmentDescriptions[0].stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		renderPassConfigs[SPACIAL_GRID_DEBUG_PIPELINE].attachmentDescriptions[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		renderPassConfigs[SPACIAL_GRID_DEBUG_PIPELINE].attachmentDescriptions[0].initialLayout  = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		renderPassConfigs[SPACIAL_GRID_DEBUG_PIPELINE].attachmentDescriptions[0].finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		
		renderPassConfigs[SPACIAL_GRID_DEBUG_PIPELINE].attachmentDescriptions[1].flags          = 0;
		renderPassConfigs[SPACIAL_GRID_DEBUG_PIPELINE].attachmentDescriptions[1].samples        = VK_SAMPLE_COUNT_1_BIT;
		renderPassConfigs[SPACIAL_GRID_DEBUG_PIPELINE].attachmentDescriptions[1].loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
		renderPassConfigs[SPACIAL_GRID_DEBUG_PIPELINE].attachmentDescriptions[1].storeOp        = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		renderPassConfigs[SPACIAL_GRID_DEBUG_PIPELINE].attachmentDescriptions[1].stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		renderPassConfigs[SPACIAL_GRID_DEBUG_PIPELINE].attachmentDescriptions[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		renderPassConfigs[SPACIAL_GRID_DEBUG_PIPELINE].attachmentDescriptions[1].initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
		renderPassConfigs[SPACIAL_GRID_DEBUG_PIPELINE].attachmentDescriptions[1].finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		
		renderPassConfigs[SPACIAL_GRID_DEBUG_PIPELINE].actualAttachmentReferenceNumber          = 2;
		renderPassConfigs[SPACIAL_GRID_DEBUG_PIPELINE].attachmentReferences[0].attachment       = 0;
		renderPassConfigs[SPACIAL_GRID_DEBUG_PIPELINE].attachmentReferences[0].layout           = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		
		renderPassConfigs[SPACIAL_GRID_DEBUG_PIPELINE].attachmentReferences[1].attachment       = 1;
		renderPassConfigs[SPACIAL_GRID_DEBUG_PIPELINE].attachmentReferences[1].layout           = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		
		renderPassConfigs[SPACIAL_GRID_DEBUG_PIPELINE].actualSubpassDependencyNumber            = 2;
		/// Order this pass after previous writes to the shared swapchain/depth images.
		renderPassConfigs[SPACIAL_GRID_DEBUG_PIPELINE].subpassDependencies[0].srcSubpass        = VK_SUBPASS_EXTERNAL;
		renderPassConfigs[SPACIAL_GRID_DEBUG_PIPELINE].subpassDependencies[0].dstSubpass        = 0;
		renderPassConfigs[SPACIAL_GRID_DEBUG_PIPELINE].subpassDependencies[0].srcStageMask      = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		renderPassConfigs[SPACIAL_GRID_DEBUG_PIPELINE].subpassDependencies[0].dstStageMask      = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		renderPassConfigs[SPACIAL_GRID_DEBUG_PIPELINE].subpassDependencies[0].srcAccessMask     = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		renderPassConfigs[SPACIAL_GRID_DEBUG_PIPELINE].subpassDependencies[0].dstAccessMask     = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		renderPassConfigs[SPACIAL_GRID_DEBUG_PIPELINE].subpassDependencies[0].dependencyFlags   = {};
		renderPassConfigs[SPACIAL_GRID_DEBUG_PIPELINE].subpassDependencies[1].srcSubpass        = 0;
		renderPassConfigs[SPACIAL_GRID_DEBUG_PIPELINE].subpassDependencies[1].dstSubpass        = VK_SUBPASS_EXTERNAL;
		renderPassConfigs[SPACIAL_GRID_DEBUG_PIPELINE].subpassDependencies[1].srcStageMask      = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		renderPassConfigs[SPACIAL_GRID_DEBUG_PIPELINE].subpassDependencies[1].dstStageMask      = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		renderPassConfigs[SPACIAL_GRID_DEBUG_PIPELINE].subpassDependencies[1].srcAccessMask     = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		renderPassConfigs[SPACIAL_GRID_DEBUG_PIPELINE].subpassDependencies[1].dstAccessMask     = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		renderPassConfigs[SPACIAL_GRID_DEBUG_PIPELINE].subpassDependencies[1].dependencyFlags   = {};

		/// ===================================================================================== ///

		descriptorSetsConfig[MATH_OBJECTS_DEBUG_DATA].actualLinkedDescriptorBindingsNumber                   = 1;
		descriptorSetsConfig[MATH_OBJECTS_DEBUG_DATA].hostDescriptorNumber                                   = 64;
		descriptorSetsConfig[MATH_OBJECTS_DEBUG_DATA].isTexture                                              = false;

		descriptorBindingsConfig[23].vkType                  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		descriptorBindingsConfig[23].shaderStageFlag         = VK_SHADER_STAGE_VERTEX_BIT;
		descriptorBindingsConfig[23].binding                 = 0;
		descriptorBindingsConfig[23].shaderDescriptorsNumber = 1;
		descriptorBindingsConfig[23].uboChunkSize            = sizeof(COLLISIONS_DEBUG_UBO);

		pipelineConfigs[MATH_OBJECTS_DEBUG_PIPELINE].vertShader                       = "../VKshaders/debug/math_objects/vertDebug.spv";
		pipelineConfigs[MATH_OBJECTS_DEBUG_PIPELINE].fragShader                       = "../VKshaders/debug/math_objects/fragDebug.spv";
		pipelineConfigs[MATH_OBJECTS_DEBUG_PIPELINE].bindingDescription               = Vertex::getBindingDescription();
		pipelineConfigs[MATH_OBJECTS_DEBUG_PIPELINE].attributeDescriptions            = Vertex::getAttributeDescriptions();
		pipelineConfigs[MATH_OBJECTS_DEBUG_PIPELINE].actualLinkedDescriptorSetsNumber = 1;
		pipelineConfigs[MATH_OBJECTS_DEBUG_PIPELINE].polygonMode                      = VK_POLYGON_MODE_LINE;
		pipelineConfigs[MATH_OBJECTS_DEBUG_PIPELINE].topology                         = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;

		renderPassConfigs[MATH_OBJECTS_DEBUG_PIPELINE].actualAttachmentDescriptionNumber        = 2;
		renderPassConfigs[MATH_OBJECTS_DEBUG_PIPELINE].attachmentDescriptions[0].flags          = 0;
		renderPassConfigs[MATH_OBJECTS_DEBUG_PIPELINE].attachmentDescriptions[0].samples        = VK_SAMPLE_COUNT_1_BIT;
		renderPassConfigs[MATH_OBJECTS_DEBUG_PIPELINE].attachmentDescriptions[0].loadOp         = VK_ATTACHMENT_LOAD_OP_LOAD;
		renderPassConfigs[MATH_OBJECTS_DEBUG_PIPELINE].attachmentDescriptions[0].storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
		renderPassConfigs[MATH_OBJECTS_DEBUG_PIPELINE].attachmentDescriptions[0].stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		renderPassConfigs[MATH_OBJECTS_DEBUG_PIPELINE].attachmentDescriptions[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		renderPassConfigs[MATH_OBJECTS_DEBUG_PIPELINE].attachmentDescriptions[0].initialLayout  = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		renderPassConfigs[MATH_OBJECTS_DEBUG_PIPELINE].attachmentDescriptions[0].finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		
		renderPassConfigs[MATH_OBJECTS_DEBUG_PIPELINE].attachmentDescriptions[1].flags          = 0;
		renderPassConfigs[MATH_OBJECTS_DEBUG_PIPELINE].attachmentDescriptions[1].samples        = VK_SAMPLE_COUNT_1_BIT;
		renderPassConfigs[MATH_OBJECTS_DEBUG_PIPELINE].attachmentDescriptions[1].loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
		renderPassConfigs[MATH_OBJECTS_DEBUG_PIPELINE].attachmentDescriptions[1].storeOp        = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		renderPassConfigs[MATH_OBJECTS_DEBUG_PIPELINE].attachmentDescriptions[1].stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		renderPassConfigs[MATH_OBJECTS_DEBUG_PIPELINE].attachmentDescriptions[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		renderPassConfigs[MATH_OBJECTS_DEBUG_PIPELINE].attachmentDescriptions[1].initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
		renderPassConfigs[MATH_OBJECTS_DEBUG_PIPELINE].attachmentDescriptions[1].finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		
		renderPassConfigs[MATH_OBJECTS_DEBUG_PIPELINE].actualAttachmentReferenceNumber          = 2;
		renderPassConfigs[MATH_OBJECTS_DEBUG_PIPELINE].attachmentReferences[0].attachment       = 0;
		renderPassConfigs[MATH_OBJECTS_DEBUG_PIPELINE].attachmentReferences[0].layout           = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		
		renderPassConfigs[MATH_OBJECTS_DEBUG_PIPELINE].attachmentReferences[1].attachment       = 1;
		renderPassConfigs[MATH_OBJECTS_DEBUG_PIPELINE].attachmentReferences[1].layout           = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		
		renderPassConfigs[MATH_OBJECTS_DEBUG_PIPELINE].actualSubpassDependencyNumber            = 2;
		/// Order this pass after previous writes to the shared swapchain/depth images.
		renderPassConfigs[MATH_OBJECTS_DEBUG_PIPELINE].subpassDependencies[0].srcSubpass        = VK_SUBPASS_EXTERNAL;
		renderPassConfigs[MATH_OBJECTS_DEBUG_PIPELINE].subpassDependencies[0].dstSubpass        = 0;
		renderPassConfigs[MATH_OBJECTS_DEBUG_PIPELINE].subpassDependencies[0].srcStageMask      = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		renderPassConfigs[MATH_OBJECTS_DEBUG_PIPELINE].subpassDependencies[0].dstStageMask      = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		renderPassConfigs[MATH_OBJECTS_DEBUG_PIPELINE].subpassDependencies[0].srcAccessMask     = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		renderPassConfigs[MATH_OBJECTS_DEBUG_PIPELINE].subpassDependencies[0].dstAccessMask     = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		renderPassConfigs[MATH_OBJECTS_DEBUG_PIPELINE].subpassDependencies[0].dependencyFlags   = {};
		renderPassConfigs[MATH_OBJECTS_DEBUG_PIPELINE].subpassDependencies[1].srcSubpass        = 0;
		renderPassConfigs[MATH_OBJECTS_DEBUG_PIPELINE].subpassDependencies[1].dstSubpass        = VK_SUBPASS_EXTERNAL;
		renderPassConfigs[MATH_OBJECTS_DEBUG_PIPELINE].subpassDependencies[1].srcStageMask      = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		renderPassConfigs[MATH_OBJECTS_DEBUG_PIPELINE].subpassDependencies[1].dstStageMask      = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		renderPassConfigs[MATH_OBJECTS_DEBUG_PIPELINE].subpassDependencies[1].srcAccessMask     = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		renderPassConfigs[MATH_OBJECTS_DEBUG_PIPELINE].subpassDependencies[1].dstAccessMask     = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		renderPassConfigs[MATH_OBJECTS_DEBUG_PIPELINE].subpassDependencies[1].dependencyFlags   = {};
		
		/*
		=====================================================================================
                    NOT RELATED TO ANY PIPELINE DESCRIPTOR SETS AND ITS BINDINGS
		=====================================================================================
		*/

		descriptorSetsConfig[RIDABLE_TEXTURES].actualLinkedDescriptorBindingsNumber  = 1;
		descriptorSetsConfig[RIDABLE_TEXTURES].hostDescriptorNumber                  = MAX_TEXTURES;
		descriptorSetsConfig[RIDABLE_TEXTURES].isTexture                             = true;

		descriptorBindingsConfig[24].vkType                  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		descriptorBindingsConfig[24].shaderStageFlag         = VK_SHADER_STAGE_FRAGMENT_BIT;
		descriptorBindingsConfig[24].binding                 = 0;
		descriptorBindingsConfig[24].shaderDescriptorsNumber = MAX_TEXTURES;
	}

	const u32 actualDescriptorBindingsConfigNumber = 25;
	// inline DescriptorSet directionalLightDescriptorSet0{ {}, {{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, DescriptorsTypes::DIRECTIONAL_LIGHT_SHADOW_MAP_MATRIX_UBO,
	// 			VK_SHADER_STAGE_VERTEX_BIT, 0, 1}}, 512};
	// inline DescriptorBinding directionalLightDescriptorBinding0_Set0{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, DescriptorsTypes::DIRECTIONAL_LIGHT_SHADOW_MAP_MATRIX_UBO,
	// 	VK_SHADER_STAGE_VERTEX_BIT, 0, 1 };
}; // namespace GLVM::core


#endif
