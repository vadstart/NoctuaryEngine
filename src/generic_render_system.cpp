#include "generic_render_system.h"
#include "nt_device.h"
#include "nt_pipeline.h"

// Libraries
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

// Std
#include <cassert> 
#include <memory>
#include <stdexcept>

namespace nt
{
  struct NtPushConstantData {
  alignas(16) glm::mat4 transform{1.f};
  alignas(16) glm::vec3 color;
};

GenericRenderSystem::GenericRenderSystem(NtDevice &device, VkRenderPass renderPass) : ntDevice{device} {
  createPipelineLayout();
  createPipeline(renderPass);
}
GenericRenderSystem::~GenericRenderSystem() {
  vkDestroyPipelineLayout(ntDevice.device(), pipelineLayout, nullptr);
}

void GenericRenderSystem::createPipelineLayout() {

  VkPushConstantRange pushConstantRange{};
  pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
  pushConstantRange.offset = 0;
  pushConstantRange.size = sizeof(NtPushConstantData);

  VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
  pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipelineLayoutInfo.setLayoutCount = 0;
  pipelineLayoutInfo.pSetLayouts = nullptr;
  pipelineLayoutInfo.pushConstantRangeCount = 1;
  pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

  if (vkCreatePipelineLayout(ntDevice.device(), &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
    throw std::runtime_error("failed to create pipeline layout!");
  }
}

void GenericRenderSystem::createPipeline(VkRenderPass renderPass) {
    assert(pipelineLayout != nullptr && "Cannot create pipeline before pipeline layout");

    PipelineConfigInfo pipelineConfig{};
    NtPipeline::defaultPipelineConfigInfo(pipelineConfig);
    pipelineConfig.renderPass = renderPass;
    pipelineConfig.pipelineLayout = pipelineLayout;

    ntPipeline = std::make_unique<NtPipeline>(
        ntDevice,
        pipelineConfig,
        "shaders/simple_shader.vert.spv",
        "shaders/simple_shader.frag.spv");
}

void GenericRenderSystem::renderGameObjects(VkCommandBuffer commandBuffer, std::vector<NtGameObject> &gameObjects, const NtCamera &camera, float deltaTime) {
  ntPipeline->bind(commandBuffer);

  float rotationSpeed = glm::radians(40.0f); // degrees per second

  for (auto& obj: gameObjects) {
    obj.transform.rotation.y = glm::mod(obj.transform.rotation.y + rotationSpeed * deltaTime, glm::two_pi<float>());
    obj.transform.rotation.x = glm::mod(obj.transform.rotation.x + rotationSpeed * deltaTime, glm::two_pi<float>());


    NtPushConstantData push{};
    push.color = obj.color;
    push.transform = camera.getProjection() * obj.transform.mat4();

    vkCmdPushConstants(
      commandBuffer,
      pipelineLayout,
      VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
      0,
      sizeof(NtPushConstantData),
      &push);

    obj.model->bind(commandBuffer);
    obj.model->draw(commandBuffer);
  }
}

}
