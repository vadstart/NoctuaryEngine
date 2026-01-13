#include "nt_im3d_renderer.hpp"
#include "nt_log.hpp"

#include <fstream>
#include <stdexcept>
#include <cmath>

namespace nt {

NtIm3dRenderer::NtIm3dRenderer(NtDevice& device, NtSwapChain& swapChain, VkDescriptorSetLayout globalSetLayout)
    : ntDevice{device}
{
    createPipelineLayout(globalSetLayout);
    createPipelines(swapChain);

    // Create vertex buffer with enough space for im3d data
    vertexBuffer = std::make_unique<NtBuffer>(
        ntDevice,
        sizeof(Im3d::VertexData),
        MAX_VERTICES,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );

    NT_LOG_INFO(LogRendering, "Im3d renderer initialized");
}

NtIm3dRenderer::~NtIm3dRenderer()
{
    if (pointsPipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(ntDevice.device(), pointsPipeline, nullptr);
    }
    if (linesPipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(ntDevice.device(), linesPipeline, nullptr);
    }
    if (trianglesPipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(ntDevice.device(), trianglesPipeline, nullptr);
    }
    if (pipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(ntDevice.device(), pipelineLayout, nullptr);
    }
}

void NtIm3dRenderer::beginFrame(const glm::vec3& cameraPosition, const glm::vec3& cameraDirection,
                                 const glm::vec2& viewportSize, float fovY, float deltaTime)
{
    Im3d::AppData& appData = Im3d::GetAppData();

    appData.m_viewOrigin = Im3d::Vec3(cameraPosition.x, cameraPosition.y, cameraPosition.z);
    appData.m_viewDirection = Im3d::Vec3(cameraDirection.x, cameraDirection.y, cameraDirection.z);
    appData.m_viewportSize = Im3d::Vec2(viewportSize.x, viewportSize.y);
    appData.m_projScaleY = std::tan(fovY * 0.5f) * 2.0f; // For perspective projection
    appData.m_projOrtho = false;
    appData.m_deltaTime = deltaTime;
    appData.m_worldUp = Im3d::Vec3(0.0f, 1.0f, 0.0f);

    Im3d::NewFrame();
}

void NtIm3dRenderer::endFrame()
{
    Im3d::EndFrame();
}

void NtIm3dRenderer::render(FrameInfo& frameInfo)
{
    const Im3d::DrawList* drawLists = Im3d::GetDrawLists();
    Im3d::U32 drawListCount = Im3d::GetDrawListCount();

    if (drawListCount == 0) {
        return;
    }

    // Bind descriptor set once
    vkCmdBindDescriptorSets(
        frameInfo.commandBuffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        pipelineLayout,
        0, 1,
        &frameInfo.globalDescriptorSet,
        0, nullptr);

    VkBuffer buffers[] = {vertexBuffer->getBuffer()};
    VkDeviceSize offsets[] = {0};

    size_t bufferOffset = 0;

    for (Im3d::U32 i = 0; i < drawListCount; ++i) {
        const Im3d::DrawList& drawList = drawLists[i];

        if (drawList.m_vertexCount == 0) {
            continue;
        }

        // Copy vertex data to buffer
        size_t dataSize = drawList.m_vertexCount * sizeof(Im3d::VertexData);
        if (bufferOffset + dataSize > MAX_VERTICES * sizeof(Im3d::VertexData)) {
            NT_LOG_WARN(LogRendering, "Im3d vertex buffer overflow, skipping draw list");
            continue;
        }

        vertexBuffer->map(dataSize, bufferOffset);
        vertexBuffer->writeToBuffer((void*)drawList.m_vertexData, dataSize);
        vertexBuffer->unmap();

        // Select pipeline based on primitive type
        VkPipeline pipeline = VK_NULL_HANDLE;
        switch (drawList.m_primType) {
            case Im3d::DrawPrimitive_Points:
                pipeline = pointsPipeline;
                break;
            case Im3d::DrawPrimitive_Lines:
                pipeline = linesPipeline;
                break;
            case Im3d::DrawPrimitive_Triangles:
                pipeline = trianglesPipeline;
                break;
            default:
                continue;
        }

        vkCmdBindPipeline(frameInfo.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

        VkDeviceSize currentOffset = bufferOffset;
        vkCmdBindVertexBuffers(frameInfo.commandBuffer, 0, 1, buffers, &currentOffset);
        vkCmdDraw(frameInfo.commandBuffer, drawList.m_vertexCount, 1, 0, 0);

        bufferOffset += dataSize;
    }
}

void NtIm3dRenderer::createPipelineLayout(VkDescriptorSetLayout globalSetLayout)
{
    std::vector<VkDescriptorSetLayout> descriptorSetLayouts{globalSetLayout};

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(descriptorSetLayouts.size());
    pipelineLayoutInfo.pSetLayouts = descriptorSetLayouts.data();
    pipelineLayoutInfo.pushConstantRangeCount = 0;
    pipelineLayoutInfo.pPushConstantRanges = nullptr;

    if (vkCreatePipelineLayout(ntDevice.device(), &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create im3d pipeline layout");
    }
}

void NtIm3dRenderer::createPipelines(NtSwapChain& swapChain)
{
    // Vertex input state - matches Im3d::VertexData
    VkVertexInputBindingDescription bindingDescription{};
    bindingDescription.binding = 0;
    bindingDescription.stride = sizeof(Im3d::VertexData);
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    std::vector<VkVertexInputAttributeDescription> attributeDescriptions(2);

    // Position + Size (vec4)
    attributeDescriptions[0].binding = 0;
    attributeDescriptions[0].location = 0;
    attributeDescriptions[0].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    attributeDescriptions[0].offset = offsetof(Im3d::VertexData, m_positionSize);

    // Color (packed U32)
    attributeDescriptions[1].binding = 0;
    attributeDescriptions[1].location = 1;
    attributeDescriptions[1].format = VK_FORMAT_R32_UINT;
    attributeDescriptions[1].offset = offsetof(Im3d::VertexData, m_color);

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

    VkPipelineViewportStateCreateInfo viewportInfo{};
    viewportInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportInfo.viewportCount = 1;
    viewportInfo.pViewports = nullptr;
    viewportInfo.scissorCount = 1;
    viewportInfo.pScissors = nullptr;

    VkPipelineRasterizationStateCreateInfo rasterizationInfo{};
    rasterizationInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizationInfo.depthClampEnable = VK_FALSE;
    rasterizationInfo.rasterizerDiscardEnable = VK_FALSE;
    rasterizationInfo.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizationInfo.lineWidth = 1.0f;
    rasterizationInfo.cullMode = VK_CULL_MODE_NONE;
    rasterizationInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizationInfo.depthBiasEnable = VK_FALSE;

    VkPipelineMultisampleStateCreateInfo multisampleInfo{};
    multisampleInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampleInfo.sampleShadingEnable = VK_FALSE;
    multisampleInfo.rasterizationSamples = ntDevice.getMsaaSamples();

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_TRUE;
    colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

    VkPipelineColorBlendStateCreateInfo colorBlendInfo{};
    colorBlendInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlendInfo.logicOpEnable = VK_FALSE;
    colorBlendInfo.attachmentCount = 1;
    colorBlendInfo.pAttachments = &colorBlendAttachment;

    VkPipelineDepthStencilStateCreateInfo depthStencilInfo{};
    depthStencilInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencilInfo.depthTestEnable = VK_TRUE;
    depthStencilInfo.depthWriteEnable = VK_FALSE;
    depthStencilInfo.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    depthStencilInfo.depthBoundsTestEnable = VK_FALSE;
    depthStencilInfo.stencilTestEnable = VK_FALSE;

    std::vector<VkDynamicState> dynamicStateEnables = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };
    VkPipelineDynamicStateCreateInfo dynamicStateInfo{};
    dynamicStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicStateInfo.pDynamicStates = dynamicStateEnables.data();
    dynamicStateInfo.dynamicStateCount = static_cast<uint32_t>(dynamicStateEnables.size());

    // Load shaders
    auto vertCode = readFile("shaders/im3d.vert.spv");
    auto fragCode = readFile("shaders/im3d.frag.spv");

    VkShaderModule vertShaderModule = createShaderModule(vertCode);
    VkShaderModule fragShaderModule = createShaderModule(fragCode);

    VkPipelineShaderStageCreateInfo shaderStages[2]{};
    shaderStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    shaderStages[0].module = vertShaderModule;
    shaderStages[0].pName = "main";

    shaderStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    shaderStages[1].module = fragShaderModule;
    shaderStages[1].pName = "main";

    VkFormat colorFormat = swapChain.getSwapChainImageFormat();
    VkFormat depthFormat = swapChain.getSwapChainDepthFormat();

    VkPipelineRenderingCreateInfo pipelineRenderingInfo{};
    pipelineRenderingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    pipelineRenderingInfo.colorAttachmentCount = 1;
    pipelineRenderingInfo.pColorAttachmentFormats = &colorFormat;
    pipelineRenderingInfo.depthAttachmentFormat = depthFormat;
    pipelineRenderingInfo.stencilAttachmentFormat = VK_FORMAT_UNDEFINED;

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pViewportState = &viewportInfo;
    pipelineInfo.pRasterizationState = &rasterizationInfo;
    pipelineInfo.pMultisampleState = &multisampleInfo;
    pipelineInfo.pColorBlendState = &colorBlendInfo;
    pipelineInfo.pDepthStencilState = &depthStencilInfo;
    pipelineInfo.pDynamicState = &dynamicStateInfo;
    pipelineInfo.layout = pipelineLayout;
    pipelineInfo.pNext = &pipelineRenderingInfo;
    pipelineInfo.renderPass = VK_NULL_HANDLE;
    pipelineInfo.subpass = 0;
    pipelineInfo.basePipelineIndex = -1;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

    // Create Points pipeline
    VkPipelineInputAssemblyStateCreateInfo pointsAssembly{};
    pointsAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    pointsAssembly.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
    pointsAssembly.primitiveRestartEnable = VK_FALSE;

    pipelineInfo.pInputAssemblyState = &pointsAssembly;
    if (vkCreateGraphicsPipelines(ntDevice.device(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pointsPipeline) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create im3d points pipeline");
    }

    // Create Lines pipeline
    VkPipelineInputAssemblyStateCreateInfo linesAssembly{};
    linesAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    linesAssembly.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
    linesAssembly.primitiveRestartEnable = VK_FALSE;

    pipelineInfo.pInputAssemblyState = &linesAssembly;
    if (vkCreateGraphicsPipelines(ntDevice.device(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &linesPipeline) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create im3d lines pipeline");
    }

    // Create Triangles pipeline
    VkPipelineInputAssemblyStateCreateInfo trianglesAssembly{};
    trianglesAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    trianglesAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    trianglesAssembly.primitiveRestartEnable = VK_FALSE;

    pipelineInfo.pInputAssemblyState = &trianglesAssembly;
    if (vkCreateGraphicsPipelines(ntDevice.device(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &trianglesPipeline) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create im3d triangles pipeline");
    }

    // Clean up shader modules
    vkDestroyShaderModule(ntDevice.device(), vertShaderModule, nullptr);
    vkDestroyShaderModule(ntDevice.device(), fragShaderModule, nullptr);

    NT_LOG_INFO(LogRendering, "Im3d pipelines created");
}

std::vector<char> NtIm3dRenderer::readFile(const std::string& filepath)
{
    std::ifstream file{filepath, std::ios::ate | std::ios::binary};

    if (!file.is_open()) {
        NT_LOG_ERROR(LogRendering, "Failed to open shader file: {}", filepath);
        throw std::runtime_error("Failed to open shader file: " + filepath);
    }

    size_t fileSize = static_cast<size_t>(file.tellg());
    std::vector<char> buffer(fileSize);

    file.seekg(0);
    file.read(buffer.data(), fileSize);

    file.close();
    return buffer;
}

VkShaderModule NtIm3dRenderer::createShaderModule(const std::vector<char>& code)
{
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

    VkShaderModule shaderModule;
    if (vkCreateShaderModule(ntDevice.device(), &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create shader module");
    }

    return shaderModule;
}

} // namespace nt
