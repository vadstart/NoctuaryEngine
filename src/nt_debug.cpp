#include "nt_debug.hpp"

#include <array>
#include <stdexcept>

namespace nt {

// Simple vertex structure for debug quad
struct DebugQuadVertex {
    glm::vec2 position;
    glm::vec2 uv;

    static std::vector<VkVertexInputBindingDescription> getBindingDescriptions() {
        std::vector<VkVertexInputBindingDescription> bindingDescriptions(1);
        bindingDescriptions[0].binding = 0;
        bindingDescriptions[0].stride = sizeof(DebugQuadVertex);
        bindingDescriptions[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        return bindingDescriptions;
    }

    static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions() {
        std::vector<VkVertexInputAttributeDescription> attributeDescriptions(2);

        attributeDescriptions[0].binding = 0;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format = VK_FORMAT_R32G32_SFLOAT;
        attributeDescriptions[0].offset = offsetof(DebugQuadVertex, position);

        attributeDescriptions[1].binding = 0;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format = VK_FORMAT_R32G32_SFLOAT;
        attributeDescriptions[1].offset = offsetof(DebugQuadVertex, uv);

        return attributeDescriptions;
    }
};

DebugQuadSystem::DebugQuadSystem(NtDevice& device, VkDescriptorSetLayout descriptorSetLayout)
    : ntDevice{device} {
    createPipelineLayout(descriptorSetLayout);
    createPipeline();
    createQuadModel();
}

DebugQuadSystem::~DebugQuadSystem() {
    vkDestroyPipelineLayout(ntDevice.device(), pipelineLayout, nullptr);
}

void DebugQuadSystem::createPipelineLayout(VkDescriptorSetLayout descriptorSetLayout) {
    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(DebugQuadPushConstants);

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

    if (vkCreatePipelineLayout(ntDevice.device(), &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("failed to create debug quad pipeline layout!");
    }
}

void DebugQuadSystem::createPipeline() {
    PipelineConfigInfo pipelineConfig{};
    NtPipeline::defaultPipelineConfigInfo(pipelineConfig, RenderMode::Lit, ntDevice);

    pipelineConfig.pipelineLayout = pipelineLayout;
    pipelineConfig.depthStencilInfo.depthTestEnable = VK_FALSE;
    pipelineConfig.depthStencilInfo.depthWriteEnable = VK_FALSE;
    pipelineConfig.rasterizationInfo.cullMode = VK_CULL_MODE_NONE;
    pipelineConfig.rasterizationInfo.depthBiasEnable = VK_FALSE;  // Disable depth bias

    // Clear dynamic states to avoid inheriting unwanted states
    pipelineConfig.dynamicStateEnables.clear();
    pipelineConfig.dynamicStateEnables = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};

    pipelineConfig.colorAttachmentFormat = VK_FORMAT_B8G8R8A8_SRGB;  // Match the swapchain format
    pipelineConfig.depthAttachmentFormat = VK_FORMAT_D32_SFLOAT;

    VkPipelineRenderingCreateInfo renderingInfo{};
    renderingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachmentFormats = &pipelineConfig.colorAttachmentFormat;
    renderingInfo.depthAttachmentFormat = pipelineConfig.depthAttachmentFormat;

    pipeline = std::make_unique<NtPipeline>(
        ntDevice,
        pipelineConfig,
        renderingInfo,
        "shaders/debug_shadowmap.vert.spv",
        "shaders/debug_shadowmap.frag.spv");
}

void DebugQuadSystem::createQuadModel() {
    NtModel::Builder modelData{ntDevice};
    modelData.l_meshes.resize(1);

    // Create a simple quad using NtModel::Vertex format
    modelData.l_meshes[0].vertices = {
        {{-1.0f, -1.0f, 0.0f}, {}, {}, {0.0f, 0.0f}},  // Bottom-left
        {{ 1.0f, -1.0f, 0.0f}, {}, {}, {1.0f, 0.0f}},  // Bottom-right
        {{ 1.0f,  1.0f, 0.0f}, {}, {}, {1.0f, 1.0f}},  // Top-right
        {{-1.0f,  1.0f, 0.0f}, {}, {}, {0.0f, 1.0f}},  // Top-left
    };

    modelData.l_meshes[0].indices = {0, 1, 2, 2, 3, 0};

    quadModel = std::make_unique<NtModel>(ntDevice, modelData);
}

void DebugQuadSystem::render(VkCommandBuffer commandBuffer, VkDescriptorSet descriptorSet) {
    pipeline->bind(commandBuffer);

    vkCmdBindDescriptorSets(
        commandBuffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        pipelineLayout,
        0, 1, &descriptorSet,
        0, nullptr);

    // Position in bottom-right corner
    DebugQuadPushConstants push{};
    push.offset = glm::vec2(0.72f, -0.7f);   // Bottom-right corner
    push.scale = glm::vec2(0.25f, 0.25f);   // 35% of screen size

    vkCmdPushConstants(
        commandBuffer,
        pipelineLayout,
        VK_SHADER_STAGE_VERTEX_BIT,
        0,
        sizeof(DebugQuadPushConstants),
        &push);

    quadModel->bind(commandBuffer, 0);
    quadModel->draw(commandBuffer, 0);
}

}
