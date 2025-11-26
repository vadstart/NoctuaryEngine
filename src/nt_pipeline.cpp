#include "nt_pipeline.hpp"
#include "nt_model.hpp"
#include "nt_types.hpp"
#include "vulkan/vulkan_core.h"

#include <cstdint>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <cassert>

namespace nt {

NtPipeline::NtPipeline(
        NtDevice &device,
        const PipelineConfigInfo& configInfo,
        const string& vertFilepath,
        const string& fragFilepath)
        : ntDevice{device} {
  createGraphicalPipeline(configInfo, vertFilepath, fragFilepath);
}
NtPipeline::~NtPipeline() {
  vkDestroyShaderModule(ntDevice.device(), vertShaderModule, nullptr);
  vkDestroyShaderModule(ntDevice.device(), fragShaderModule, nullptr);
  vkDestroyPipeline(ntDevice.device(), graphicsPipeline, nullptr);
}

 vector<char> NtPipeline::readFile(const string& filepath) {
   std::ifstream file{filepath, std::ios::ate | std::ios::binary};

   if (!file.is_open()) {
     std::cout << "failed to open file: " + filepath << std::endl;
     throw std::runtime_error("failed to open file: " + filepath);
   }

   size_t fileSize = static_cast<size_t>(file.tellg());
   vector<char> buffer(fileSize);

   file.seekg(0);
   file.read(buffer.data(), fileSize);

   file.close();
   return buffer;
 }

 void NtPipeline::createGraphicalPipeline(
        const PipelineConfigInfo& configInfo,
        const string& vertFilepath,
        const string& fragFilepath) {

   assert (configInfo.pipelineLayout != VK_NULL_HANDLE &&
       "Cannot create graphics pipeline: no pipelineLayout provided in configInfo");

   if (!configInfo.useDynamicRendering) {
       assert (configInfo.renderPass != VK_NULL_HANDLE &&
           "Cannot create graphics pipeline: no renderPass provided and useDynamicRendering is false");
   } else {
       assert (configInfo.colorAttachmentFormat != VK_FORMAT_UNDEFINED &&
           "Cannot create graphics pipeline: colorAttachmentFormat must be set for dynamic rendering");
   }

   auto vertCode = readFile(vertFilepath);
   auto fragCode = readFile(fragFilepath);

   createShaderModule(vertCode, &vertShaderModule);
   createShaderModule(fragCode, &fragShaderModule);

   VkPipelineShaderStageCreateInfo shaderStages[2];
   shaderStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
   shaderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
   shaderStages[0].module = vertShaderModule;
   shaderStages[0].pName = "main";
   shaderStages[0].flags = 0;
   shaderStages[0].pNext = nullptr;
   shaderStages[0].pSpecializationInfo = nullptr;
   shaderStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
   shaderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
   shaderStages[1].module = fragShaderModule;
   shaderStages[1].pName = "main";
   shaderStages[1].flags = 0;
   shaderStages[1].pNext = nullptr;
   shaderStages[1].pSpecializationInfo = nullptr;

   auto bindingDescriptions = NtModel::Vertex::getBindingDescriptions();
   auto attributeDescriptions = NtModel::Vertex::getAttributeDescriptions();
   VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
   vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
   vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
   vertexInputInfo.vertexBindingDescriptionCount = static_cast<uint32_t>(bindingDescriptions.size());
   vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();
   vertexInputInfo.pVertexBindingDescriptions = bindingDescriptions.data();

   VkPipelineRenderingCreateInfo pipelineRenderingInfo{};
   if (configInfo.useDynamicRendering)
   {
      pipelineRenderingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
      pipelineRenderingInfo.colorAttachmentCount = 1;
      pipelineRenderingInfo.pColorAttachmentFormats = &configInfo.colorAttachmentFormat;
      pipelineRenderingInfo.depthAttachmentFormat = configInfo.depthAttachmentFormat;
      pipelineRenderingInfo.stencilAttachmentFormat = VK_FORMAT_UNDEFINED;
   }

   VkGraphicsPipelineCreateInfo pipelineInfo{};
   pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
   pipelineInfo.stageCount = 2;
   pipelineInfo.pStages = shaderStages;
   pipelineInfo.pVertexInputState = &vertexInputInfo;
   pipelineInfo.pInputAssemblyState = &configInfo.inputAssemblyInfo;
   pipelineInfo.pViewportState = &configInfo.viewportInfo;
   pipelineInfo.pRasterizationState = &configInfo.rasterizationInfo;
   pipelineInfo.pMultisampleState = &configInfo.multisampleInfo;
   pipelineInfo.pColorBlendState = &configInfo.colorBlendInfo;
   pipelineInfo.pDepthStencilState = &configInfo.depthStencilInfo;
   pipelineInfo.pDynamicState = &configInfo.dynamicStateInfo;
   pipelineInfo.layout = configInfo.pipelineLayout;

   // Dynamic / Traditional rendering
   if (configInfo.useDynamicRendering) {
       pipelineInfo.pNext = &pipelineRenderingInfo;
       pipelineInfo.renderPass = VK_NULL_HANDLE;
       pipelineInfo.subpass = 0;
   } else {
       pipelineInfo.renderPass = configInfo.renderPass;
       pipelineInfo.subpass = configInfo.subpass;
   }

   pipelineInfo.basePipelineIndex = -1;
   pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

   if (vkCreateGraphicsPipelines(ntDevice.device(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline) != VK_SUCCESS) {
     throw std::runtime_error("failed to create graphics pipeline");
   }

 }


 void NtPipeline::createShaderModule(const vector<char>& code, VkShaderModule* shaderModule) {
   VkShaderModuleCreateInfo createInfo{};
   createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
   createInfo.codeSize = code.size();
   createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

   if (vkCreateShaderModule(ntDevice.device(), &createInfo, nullptr, shaderModule) != VK_SUCCESS) {
     throw std::runtime_error("failed to create shader module");
   }
 }

 void NtPipeline::bind(VkCommandBuffer commandBuffer) {
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);
  }

 void NtPipeline::defaultPipelineConfigInfo(PipelineConfigInfo& configInfo, RenderMode pipeRenderMode, NtDevice& device) {
   configInfo.inputAssemblyInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
   configInfo.inputAssemblyInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
   configInfo.inputAssemblyInfo.primitiveRestartEnable = VK_FALSE;

   configInfo.viewportInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
   configInfo.viewportInfo.viewportCount = 1;
   configInfo.viewportInfo.pViewports = nullptr;
   configInfo.viewportInfo.scissorCount = 1;
   configInfo.viewportInfo.pScissors = nullptr;

   configInfo.rasterizationInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
   configInfo.rasterizationInfo.depthClampEnable = VK_FALSE;
   configInfo.rasterizationInfo.rasterizerDiscardEnable = VK_FALSE;

   configInfo.multisampleInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
   configInfo.multisampleInfo.sampleShadingEnable = VK_FALSE;
   configInfo.multisampleInfo.rasterizationSamples = device.getMsaaSamples();
   configInfo.multisampleInfo.minSampleShading = 1.0f; // Optional
   configInfo.multisampleInfo.pSampleMask = nullptr; // Optional
   configInfo.multisampleInfo.alphaToCoverageEnable = VK_FALSE; // Optional
   configInfo.multisampleInfo.alphaToOneEnable = VK_FALSE; // Optional

   configInfo.colorBlendAttachment.colorWriteMask =
     VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
     VK_COLOR_COMPONENT_A_BIT;
   configInfo.colorBlendAttachment.blendEnable = VK_FALSE;
   configInfo.colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
   configInfo.colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
   configInfo.colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD; // Optional
   configInfo.colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
   configInfo.colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
   configInfo.colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
   configInfo.colorBlendInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
   configInfo.colorBlendInfo.logicOpEnable = VK_FALSE;
   configInfo.colorBlendInfo.logicOp = VK_LOGIC_OP_COPY; // Optional

   configInfo.colorBlendInfo.attachmentCount = 1;
   configInfo.colorBlendInfo.pAttachments = &configInfo.colorBlendAttachment;
   configInfo.colorBlendInfo.blendConstants[0] = 0.0f; // Optional
   configInfo.colorBlendInfo.blendConstants[1] = 0.0f; // Optional
   configInfo.colorBlendInfo.blendConstants[2] = 0.0f; // Optional
   configInfo.colorBlendInfo.blendConstants[3] = 0.0f; // Optional

   configInfo.depthStencilInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
   configInfo.depthStencilInfo.depthTestEnable = VK_TRUE;
   configInfo.depthStencilInfo.depthWriteEnable = VK_TRUE;
   configInfo.depthStencilInfo.depthCompareOp = VK_COMPARE_OP_LESS;
   configInfo.depthStencilInfo.depthBoundsTestEnable = VK_FALSE;
   configInfo.depthStencilInfo.minDepthBounds = 0.0f; // Optional
   configInfo.depthStencilInfo.maxDepthBounds = 1.0f; // Optional
   configInfo.depthStencilInfo.stencilTestEnable = VK_FALSE;
   configInfo.depthStencilInfo.front = {}; // Optional
   configInfo.depthStencilInfo.back = {}; // Optional
   configInfo.rasterizationInfo.lineWidth = 1.0f;

   switch (pipeRenderMode) {
     case nt::RenderMode::Wireframe:
       configInfo.rasterizationInfo.polygonMode = VK_POLYGON_MODE_LINE;
       configInfo.depthStencilInfo.depthWriteEnable = VK_FALSE;
       configInfo.rasterizationInfo.depthBiasEnable = VK_TRUE;
       configInfo.rasterizationInfo.depthBiasConstantFactor = 1.0f;
       configInfo.rasterizationInfo.depthBiasClamp = 0.0f;
       configInfo.rasterizationInfo.depthBiasSlopeFactor = 1.0f;
       break;

     case nt::RenderMode::DebugGrid:
       configInfo.rasterizationInfo.polygonMode = VK_POLYGON_MODE_FILL;
       configInfo.depthStencilInfo.depthWriteEnable = VK_FALSE;
       configInfo.rasterizationInfo.depthBiasEnable = VK_FALSE;
       configInfo.colorBlendAttachment.blendEnable = VK_TRUE;
       configInfo.colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
       configInfo.colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
       configInfo.colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
       configInfo.colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
       configInfo.colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
       configInfo.colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
       break;

     case nt::RenderMode::Billboard:
       configInfo.rasterizationInfo.polygonMode = VK_POLYGON_MODE_FILL;
       configInfo.rasterizationInfo.cullMode = VK_CULL_MODE_NONE;
       configInfo.depthStencilInfo.depthTestEnable = VK_TRUE;
       configInfo.depthStencilInfo.depthWriteEnable = VK_FALSE;
       configInfo.rasterizationInfo.depthBiasEnable = VK_FALSE;
       configInfo.colorBlendAttachment.blendEnable = VK_TRUE;
       configInfo.colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
       configInfo.colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
       configInfo.colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
       configInfo.colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
       configInfo.colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
       configInfo.colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
       break;

     case nt::RenderMode::Unlit:
        configInfo.rasterizationInfo.cullMode = VK_CULL_MODE_NONE;
     break;

     default:
       configInfo.rasterizationInfo.polygonMode = VK_POLYGON_MODE_FILL;
       configInfo.rasterizationInfo.depthBiasEnable = VK_FALSE;
       configInfo.colorBlendAttachment.blendEnable = VK_TRUE;
       configInfo.colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
       configInfo.colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
       configInfo.colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
       configInfo.colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
       configInfo.colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
       configInfo.colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
       configInfo.rasterizationInfo.cullMode = VK_CULL_MODE_BACK_BIT;
       configInfo.rasterizationInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
       break;
   }

   configInfo.dynamicStateEnables = {
    VK_DYNAMIC_STATE_VIEWPORT,
    VK_DYNAMIC_STATE_SCISSOR,
    VK_DYNAMIC_STATE_DEPTH_BIAS
   };
   configInfo.dynamicStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
   configInfo.dynamicStateInfo.pDynamicStates = configInfo.dynamicStateEnables.data();
   configInfo.dynamicStateInfo.dynamicStateCount = static_cast<uint32_t>(configInfo.dynamicStateEnables.size());
   configInfo.dynamicStateInfo.flags = 0;
 }

}
