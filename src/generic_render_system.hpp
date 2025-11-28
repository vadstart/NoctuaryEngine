#pragma once

#include "nt_game_object.hpp"
#include "nt_pipeline.hpp"
#include "nt_device.hpp"
#include "nt_camera.hpp"
#include "nt_swap_chain.hpp"
#include "nt_types.hpp"
#include "nt_frame_info.hpp"
#include "vulkan/vulkan_core.h"

#include <memory>
using std::vector;

namespace nt
{
    // Cube face orientations (target, up vectors)
    struct CubeFace {
        glm::vec3 target;
        glm::vec3 up;
    };

    static const CubeFace cubeFaces[6] = {
        {{1.0f, 0.0f, 0.0f}, {0.0f, -1.0f, 0.0f}},  // +X (right)
        {{-1.0f, 0.0f, 0.0f}, {0.0f, -1.0f, 0.0f}}, // -X (left)
        {{0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},   // +Y (up)
        {{0.0f, -1.0f, 0.0f}, {0.0f, 0.0f, -1.0f}}, // -Y (down)
        {{0.0f, 0.0f, 1.0f}, {0.0f, -1.0f, 0.0f}},  // +Z (forward)
        {{0.0f, 0.0f, -1.0f}, {0.0f, -1.0f, 0.0f}}  // -Z (back)
    };

    class GenericRenderSystem {
    public:
      GenericRenderSystem(NtDevice &device, NtSwapChain &swapChain,
                          VkDescriptorSetLayout globalSetLayout,
                          VkDescriptorSetLayout modelSetLayout,
                          VkDescriptorSetLayout boneSetLayout);
      ~GenericRenderSystem();

      GenericRenderSystem(const GenericRenderSystem &) = delete;
      GenericRenderSystem &operator=(const GenericRenderSystem &) = delete;

      void updateLights(FrameInfo &frameInfo, GlobalUbo &ubo, glm::vec3 O_dir,
                        float O_scale, float O_near, float O_far);

      void renderDebugGrid(FrameInfo &frameInfo, NtGameObject &gridObject,
                           glm::vec3 cameraPos);
      void renderGameObjects(FrameInfo &frameInfo);
      void renderLightBillboards(FrameInfo &frameInfo);
      void switchRenderMode(RenderMode newRenderMode);
      void setCubeFaceIndex(int faceIndex) { currentCubeFaceIndex = faceIndex; }

    private:
      void createPipelineLayout(VkDescriptorSetLayout globalSetLayout,
                                VkDescriptorSetLayout modelSetLayout,
                                VkDescriptorSetLayout boneSetLayout);
      void createPipelines(NtSwapChain &swapChain);

      glm::mat4 getCubeFaceViewMatrix(const glm::vec3 &lightPos, uint32_t face);

      NtDevice &ntDevice;

      std::unique_ptr<NtPipeline> debugGridPipeline;
      std::unique_ptr<NtPipeline> shadowMapPipeline;
      std::unique_ptr<NtPipeline> litPipeline;
      std::unique_ptr<NtPipeline> unlitPipeline;
      std::unique_ptr<NtPipeline> wireframePipeline;
      std::unique_ptr<NtPipeline> normalsPipeline;
      std::unique_ptr<NtPipeline> depthPipeline;
      std::unique_ptr<NtPipeline> billboardPipeline;
      VkPipelineLayout pipelineLayout;

      RenderMode currentRenderMode = RenderMode::Lit;

      int currentCubeFaceIndex = -1;
    };
}
