#pragma once

#include "nt_ecs.hpp"
#include "nt_pipeline.hpp"
#include "nt_device.hpp"
#include "nt_swap_chain.hpp"
#include "nt_buffer.hpp"
#include "nt_frame_info.hpp"
#include "vulkan/vulkan_core.h"

#include <memory>
#include <vector>

namespace nt
{

struct DebugLineVertex {
    glm::vec3 position;
    glm::vec3 color;

    static std::vector<VkVertexInputBindingDescription> getBindingDescriptions() {
        std::vector<VkVertexInputBindingDescription> bindingDescriptions(1);
        bindingDescriptions[0].binding = 0;
        bindingDescriptions[0].stride = sizeof(DebugLineVertex);
        bindingDescriptions[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        return bindingDescriptions;
    }

    static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions() {
        std::vector<VkVertexInputAttributeDescription> attributeDescriptions(2);

        attributeDescriptions[0].binding = 0;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[0].offset = offsetof(DebugLineVertex, position);

        attributeDescriptions[1].binding = 0;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[1].offset = offsetof(DebugLineVertex, color);

        return attributeDescriptions;
    }
};

class NtLineRenderSystem : public NtSystem
{
public:
    NtLineRenderSystem(NtNexus* nexus_ptr, NtDevice &device,
                        NtSwapChain &swapChain,
                        VkDescriptorSetLayout globalSetLayout);
    ~NtLineRenderSystem();

    NtLineRenderSystem(const NtLineRenderSystem &) = delete;
    NtLineRenderSystem &operator=(const NtLineRenderSystem &) = delete;

    void render(FrameInfo &frameInfo);

    // Helper methods to add debug lines
    void addLine(const glm::vec3& start, const glm::vec3& end, const glm::vec3& color = glm::vec3(1.0f));
    void addDirectionLine(const glm::vec3& position, const glm::vec3& direction, float length, const glm::vec3& color = glm::vec3(1.0f, 0.0f, 0.0f));
    void clearLines();

private:
    void createPipelineLayout(VkDescriptorSetLayout globalSetLayout);
    void createPipeline(NtSwapChain &swapChain);
    void updateBuffers(FrameInfo& frameInfo);

    // Helper methods for manual pipeline creation
    std::vector<char> readFile(const std::string& filepath);
    VkShaderModule createShaderModule(const std::vector<char>& code);

    NtDevice &ntDevice;
    NtNexus* nexus;

    VkPipeline graphicsPipeline;
    VkPipelineLayout pipelineLayout;

    std::vector<DebugLineVertex> lineVertices;
    std::unique_ptr<NtBuffer> vertexBuffer;
    uint32_t vertexCount = 0;
};

}
