#pragma once

#include "nt_device.hpp"
#include "nt_pipeline.hpp"
#include "nt_buffer.hpp"
#include "nt_shadows.hpp"
#include "nt_descriptors.hpp"
#include "nt_model.hpp"

#include <memory>
#include <vulkan/vulkan.h>

namespace nt {

struct DebugQuadPushConstants {
    glm::vec2 offset;
    glm::vec2 scale;
};

class DebugQuadSystem {
public:
    DebugQuadSystem(NtDevice& device, VkDescriptorSetLayout descriptorSetLayout);
    ~DebugQuadSystem();

    DebugQuadSystem(const DebugQuadSystem&) = delete;
    DebugQuadSystem& operator=(const DebugQuadSystem&) = delete;

    void render(VkCommandBuffer commandBuffer, VkDescriptorSet descriptorSet);

private:
    void createPipelineLayout(VkDescriptorSetLayout descriptorSetLayout);
    void createPipeline();
    void createQuadModel();

    NtDevice& ntDevice;

    std::unique_ptr<NtPipeline> pipeline;
    VkPipelineLayout pipelineLayout;

    std::unique_ptr<NtModel> quadModel;

    std::unique_ptr<NtBuffer> vertexBuffer;
    std::unique_ptr<NtBuffer> indexBuffer;
    uint32_t indexCount;
};

}
