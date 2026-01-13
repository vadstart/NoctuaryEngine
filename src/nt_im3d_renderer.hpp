#pragma once

#include <glm/glm.hpp>

#include "nt_device.hpp"
#include "nt_swap_chain.hpp"
#include "nt_buffer.hpp"
#include "nt_frame_info.hpp"

#include <im3d/im3d.h>
#include <vulkan/vulkan.h>
#include <memory>
#include <vector>

namespace nt {

class NtIm3dRenderer {
public:
    NtIm3dRenderer(NtDevice& device, NtSwapChain& swapChain, VkDescriptorSetLayout globalSetLayout);
    ~NtIm3dRenderer();

    NtIm3dRenderer(const NtIm3dRenderer&) = delete;
    NtIm3dRenderer& operator=(const NtIm3dRenderer&) = delete;

    // Call at the start of each frame before any im3d drawing calls
    void beginFrame(const glm::vec3& cameraPosition, const glm::vec3& cameraDirection,
                    const glm::vec2& viewportSize, float fovY, float deltaTime);

    // Call at the end of frame after all im3d drawing calls, before render()
    void endFrame();

    // Render im3d primitives
    void render(FrameInfo& frameInfo);

private:
    void createPipelineLayout(VkDescriptorSetLayout globalSetLayout);
    void createPipelines(NtSwapChain& swapChain);

    std::vector<char> readFile(const std::string& filepath);
    VkShaderModule createShaderModule(const std::vector<char>& code);

    NtDevice& ntDevice;

    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VkPipeline pointsPipeline = VK_NULL_HANDLE;
    VkPipeline linesPipeline = VK_NULL_HANDLE;
    VkPipeline trianglesPipeline = VK_NULL_HANDLE;

    // Dynamic vertex buffer for im3d data
    std::unique_ptr<NtBuffer> vertexBuffer;
    static constexpr size_t MAX_VERTICES = 1024 * 1024; // 1M vertices max
};

} // namespace nt
