#pragma once

#include "nt_game_object.hpp"
#include "nt_pipeline.hpp"
#include "nt_device.hpp"
#include "nt_camera.hpp"
#include "nt_types.hpp"
#include "nt_frame_info.hpp"
#include "vulkan/vulkan_core.h"

#include <memory>
using std::vector;

namespace nt
{
	class GenericRenderSystem
	{
	public:
    GenericRenderSystem(NtDevice &device, VkRenderPass renderPass, VkDescriptorSetLayout globalSetLayout, VkDescriptorSetLayout modelSetLayout);
    ~GenericRenderSystem();

    GenericRenderSystem(const GenericRenderSystem&) = delete;
		GenericRenderSystem& operator=(const GenericRenderSystem&) = delete;

    void updateLights(FrameInfo &frameInfo, GlobalUbo &ubo);

    void renderDebugGrid(FrameInfo &frameInfo, NtGameObject &gridObject, glm::vec3 cameraPos);
    void renderGameObjects(FrameInfo &frameInfo);
    void switchRenderMode(RenderMode newRenderMode);

	private:
    void createPipelineLayout(VkDescriptorSetLayout globalSetLayout, VkDescriptorSetLayout modelSetLayout);
    void createPipeline(VkRenderPass renderPass);
    
    NtDevice &ntDevice;
    
    std::unique_ptr<NtPipeline> debugGridPipeline;
    std::unique_ptr<NtPipeline> litPipeline;
    std::unique_ptr<NtPipeline> unlitPipeline;
    std::unique_ptr<NtPipeline> wireframePipeline;
    std::unique_ptr<NtPipeline> normalsPipeline;
    std::unique_ptr<NtPipeline> depthPipeline;
    VkPipelineLayout pipelineLayout;

    RenderMode currentRenderMode = RenderMode::Lit;
	};
}
