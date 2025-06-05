#pragma once

#include "nt_game_object.hpp"
#include "nt_pipeline.hpp"
#include "nt_device.hpp"
#include "nt_camera.hpp"
#include "nt_types.hpp"

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

    void renderGameObjects(VkCommandBuffer commandBuffer, std::vector<NtGameObject> &gameObjects, const NtCamera &camera, glm::vec3 cameraPos, float deltaTime);
    void switchRenderMode(RenderMode newRenderMode);

	private:
    void createPipelineLayout();
    void createPipeline(VkRenderPass renderPass);
    
    NtDevice &ntDevice;
    
    std::unique_ptr<NtPipeline> debugGridPipeline;
    std::unique_ptr<NtPipeline> litPipeline;
    std::unique_ptr<NtPipeline> wireframePipeline;
    VkPipelineLayout pipelineLayout;

    RenderMode currentRenderMode = RenderMode::Lit;
	};
}
