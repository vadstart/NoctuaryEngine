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
    GenericRenderSystem(NtDevice &device, VkRenderPass renderPass, VkDescriptorSetLayout globalSetLayout);
    ~GenericRenderSystem();

    GenericRenderSystem(const GenericRenderSystem&) = delete;
		GenericRenderSystem& operator=(const GenericRenderSystem&) = delete;

    void renderGameObjects(FrameInfo &frameInfo, std::vector<NtGameObject> &gameObjects, glm::vec3 cameraPos);
    void switchRenderMode(RenderMode newRenderMode);

	private:
    void createPipelineLayout(VkDescriptorSetLayout globalSetLayout);
    void createPipeline(VkRenderPass renderPass);
    
    NtDevice &ntDevice;
    
    std::unique_ptr<NtPipeline> debugGridPipeline;
    std::unique_ptr<NtPipeline> litPipeline;
    std::unique_ptr<NtPipeline> wireframePipeline;
    std::unique_ptr<NtPipeline> normalsPipeline;
    VkPipelineLayout pipelineLayout;

    RenderMode currentRenderMode = RenderMode::Lit;
	};
}
