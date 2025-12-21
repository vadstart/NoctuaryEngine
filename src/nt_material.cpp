#include "nt_material.hpp"
#include "nt_device.hpp"
#include "nt_log.hpp"
#include "nt_pipeline.hpp"
#include "nt_types.hpp"
#include <stdexcept>
#include <array>
#include <vulkan/vulkan_core.h>

namespace nt {

NtMaterial::NtMaterial(NtDevice &device, const Config& config,
    VkDescriptorSetLayout globalSetLayout,
    VkDescriptorSetLayout modelSetLayout,
    VkDescriptorSetLayout boneSetLayout,
    NtSwapChain& swapChain)
    : device{device}, type{config.type} {

    // Create pipeline layout
    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(NtPushConstantData);

    std::vector<VkDescriptorSetLayout> descriptorSetLayouts{
        globalSetLayout,
        modelSetLayout,
        boneSetLayout
    };

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(descriptorSetLayouts.size());
    pipelineLayoutInfo.pSetLayouts = descriptorSetLayouts.data();
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

    if (vkCreatePipelineLayout(device.device(), &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create pipeline layout!");
    }

    // Create pipeline based on material type
    PipelineConfigInfo pipelineConfig{};

    // Set render mode based on material type
    RenderMode renderMode = RenderMode::PBR;
    if (type == MaterialType::SHADOW_MAP) {
        renderMode = RenderMode::ShadowMap;
    } else if (type == MaterialType::NPR) {
        renderMode = RenderMode::NPR;
    } else if (type == MaterialType::UNLIT) {
        renderMode = RenderMode::NPR;
    } else if (type == MaterialType::SCROLLING_UV) {
        renderMode = RenderMode::NPR;
    }

    NtPipeline::defaultPipelineConfigInfo(pipelineConfig, renderMode, device);
    pipelineConfig.pipelineLayout = pipelineLayout;
    // Shadow map has special configuration
    if (type == MaterialType::SHADOW_MAP) {
        pipelineConfig.depthAttachmentFormat = VK_FORMAT_D32_SFLOAT;
        pipelineConfig.multisampleInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        // Create pipeline rendering info for shadow map (depth only, no color)
        VkPipelineRenderingCreateInfo pipelineRenderingInfo{};
        pipelineRenderingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
        pipelineRenderingInfo.colorAttachmentCount = 0;
        pipelineRenderingInfo.pColorAttachmentFormats = nullptr;
        pipelineRenderingInfo.depthAttachmentFormat = pipelineConfig.depthAttachmentFormat;
        pipelineRenderingInfo.stencilAttachmentFormat = VK_FORMAT_UNDEFINED;

        pipeline = std::make_unique<NtPipeline>(
            device,
            pipelineConfig,
            pipelineRenderingInfo,
            config.vertexShader,
            config.fragmentShader);
    } else {
        // Regular materials
        pipelineConfig.colorAttachmentFormat = swapChain.getSwapChainImageFormat();
        pipelineConfig.depthAttachmentFormat = swapChain.getSwapChainDepthFormat();

        // Override settings from config
        pipelineConfig.rasterizationInfo.cullMode = config.cullMode;

        if (config.bAlphaBlending) {
            pipelineConfig.colorBlendAttachment.blendEnable = VK_TRUE;
            pipelineConfig.colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
            pipelineConfig.colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            pipelineConfig.colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
            pipelineConfig.colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
            pipelineConfig.colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
            pipelineConfig.colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
        }

        if (!config.depthWrite) {
            pipelineConfig.depthStencilInfo.depthWriteEnable = VK_FALSE;
        }

        // Create pipeline rendering info for dynamic rendering
        VkPipelineRenderingCreateInfo pipelineRenderingInfo{};
        pipelineRenderingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
        pipelineRenderingInfo.colorAttachmentCount = 1;
        pipelineRenderingInfo.pColorAttachmentFormats = &pipelineConfig.colorAttachmentFormat;
        pipelineRenderingInfo.depthAttachmentFormat = pipelineConfig.depthAttachmentFormat;
        pipelineRenderingInfo.stencilAttachmentFormat = VK_FORMAT_UNDEFINED;

        pipeline = std::make_unique<NtPipeline>(
            device,
            pipelineConfig,
            pipelineRenderingInfo,
            config.vertexShader,
            config.fragmentShader);
    }
}

NtMaterial::~NtMaterial() {
    if (pipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device.device(), pipelineLayout, nullptr);
    }
}

void NtMaterial::bind(VkCommandBuffer commandBuffer) {
    pipeline->bind(commandBuffer);
}

NtMaterialLibrary::NtMaterialLibrary(NtDevice& device,
    VkDescriptorSetLayout globalSetLayout,
    VkDescriptorSetLayout modelSetLayout,
    VkDescriptorSetLayout boneSetLayout,
    NtSwapChain& swapChain)
    : device{device},
      globalSetLayout{globalSetLayout},
      modelSetLayout{modelSetLayout},
      boneSetLayout{boneSetLayout},
      swapChain{swapChain} {
    createDefaultMaterials();
}

void NtMaterialLibrary::createDefaultMaterials() {
    // Main PBR
    {
        NtMaterial::Config config;
        config.type = MaterialType::PBR;
        config.vertexShader = "shaders/pbr.vert.spv";
        config.fragmentShader = "shaders/pbr.frag.spv";
        config.cullMode = VK_CULL_MODE_BACK_BIT;
        config.bAlphaBlending = false;

        materials[MaterialType::PBR] = std::make_shared<NtMaterial>(
                   device, config, globalSetLayout, modelSetLayout, boneSetLayout, swapChain);
    }

    // NPR (Toon Shading)
    {
        NtMaterial::Config config;
        config.type = MaterialType::NPR;
        config.vertexShader = "shaders/npr.vert.spv";
        config.fragmentShader = "shaders/npr.frag.spv";
        config.cullMode = VK_CULL_MODE_BACK_BIT;
        config.bAlphaBlending = false;

        materials[MaterialType::NPR] = std::make_shared<NtMaterial>(
                           device, config, globalSetLayout, modelSetLayout, boneSetLayout, swapChain);
    }

    // UNLIT (GUI)
    // {
    //     NtMaterial::Config config;
    //     config.type = MaterialType::UNLIT;
    //     config.vertexShader = "shaders/unlit.vert.spv";
    //     config.fragmentShader = "shaders/unlit.frag.spv";
    //     config.cullMode = VK_CULL_MODE_BACK_BIT;
    //     config.bAlphaBlending = false;

    //     materials[MaterialType::UNLIT] = std::make_shared<NtMaterial>(
                       // device, config, globalSetLayout, modelSetLayout, boneSetLayout, swapChain);
    // }

    // Scrolling UV
    {
        NtMaterial::Config config;
        config.type = MaterialType::SCROLLING_UV;
        config.vertexShader = "shaders/scroll_uv.vert.spv";
        config.fragmentShader = "shaders/scroll_uv.frag.spv";
        config.cullMode = VK_CULL_MODE_BACK_BIT;
        config.bAlphaBlending = true;

        materials[MaterialType::SCROLLING_UV] = std::make_shared<NtMaterial>(
                           device, config, globalSetLayout, modelSetLayout, boneSetLayout, swapChain);
    }

    // Shadow Map
    {
        NtMaterial::Config config;
        config.type = MaterialType::SHADOW_MAP;
        config.vertexShader = "shaders/shadowmap.vert.spv";
        config.fragmentShader = "shaders/shadowmap.frag.spv";
        config.cullMode = VK_CULL_MODE_BACK_BIT;
        config.bAlphaBlending = false;
        config.depthWrite = true;

        materials[MaterialType::SHADOW_MAP] = std::make_shared<NtMaterial>(
            device, config, globalSetLayout, modelSetLayout, boneSetLayout, swapChain);
    }
}

std::shared_ptr<NtMaterial> NtMaterialLibrary::getMaterial(MaterialType type)
{
    auto it = materials.find(type);
    if (it != materials.end()) {
        return it->second;
    }

    NT_LOG_ERROR(LogAssets, "Material type not found!");
    throw std::runtime_error("Material type not found!");
}

}
