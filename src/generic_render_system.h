#pragma once

#include "nt_game_object.h"
#include "nt_pipeline.h"
#include "nt_device.h"
#include "nt_camera.h"
// #include "vulkan/vulkan_core.h"

#include <memory>
using std::vector;

namespace nt
{
	class GenericRenderSystem
	{
	public:
    GenericRenderSystem(NtDevice &device, VkRenderPass renderPass);
    ~GenericRenderSystem();

    GenericRenderSystem(const GenericRenderSystem&) = delete;
		GenericRenderSystem& operator=(const GenericRenderSystem&) = delete;

    void renderGameObjects(VkCommandBuffer commandBuffer, std::vector<NtGameObject> &gameObjects, const NtCamera &camera, float deltaTime);

	private:
    void createPipelineLayout();
    void createPipeline(VkRenderPass renderPass);
    
    NtDevice &ntDevice;

    std::unique_ptr<NtPipeline> ntPipeline;
    VkPipelineLayout pipelineLayout;
	};
}
