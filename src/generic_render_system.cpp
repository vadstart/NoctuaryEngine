#include "generic_render_system.hpp"
#include "imgui/imgui.h"
#include "nt_device.hpp"
#include "nt_frame_info.hpp"
#include "nt_pipeline.hpp"
#include "nt_types.hpp"
#include "vulkan/vulkan_core.h"
#include <cstdint>
#include <iostream>
#include <vector>

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

struct GridPushConstants {
    glm::mat4 modelMatrix;
    glm::vec3 cameraPos;
    float gridSpacing = 1.0f;
    float lineThickness = 1.0f;
    float fadeDistance = 50.0f;
};

struct NtPushConstantData {
  alignas(16) glm::mat4 modelMatrix{1.f};
  alignas(16) glm::mat4 normalMatrix{1.f};
};

GenericRenderSystem::GenericRenderSystem(NtDevice &device, VkRenderPass renderPass, VkDescriptorSetLayout globalSetLayout) : ntDevice{device} {
  createPipelineLayout(globalSetLayout);
  createPipeline(renderPass);
}
GenericRenderSystem::~GenericRenderSystem() {
  vkDestroyPipelineLayout(ntDevice.device(), pipelineLayout, nullptr);
}

void GenericRenderSystem::createPipelineLayout(VkDescriptorSetLayout globalSetLayout) {

  VkPushConstantRange pushConstantRange{};
  pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
  pushConstantRange.offset = 0;
  pushConstantRange.size = sizeof(NtPushConstantData);

  std::vector<VkDescriptorSetLayout> descriptorSetLayouts{globalSetLayout};

  VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
  pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(descriptorSetLayouts.size());
  pipelineLayoutInfo.pSetLayouts = descriptorSetLayouts.data();
  pipelineLayoutInfo.pushConstantRangeCount = 1;
  pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

 if (vkCreatePipelineLayout(ntDevice.device(), &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
    throw std::runtime_error("failed to create pipeline layout!");
  }
}

void GenericRenderSystem::createPipeline(VkRenderPass renderPass) {
    assert(pipelineLayout != nullptr && "Cannot create pipeline before pipeline layout");

    PipelineConfigInfo debGridPipelineConfig{};
    NtPipeline::defaultPipelineConfigInfo(debGridPipelineConfig, nt::RenderMode::DebugGrid);
    debGridPipelineConfig.renderPass = renderPass;
    debGridPipelineConfig.pipelineLayout = pipelineLayout;

    debugGridPipeline = std::make_unique<NtPipeline>(
        ntDevice,
        debGridPipelineConfig,
        "shaders/debug_grid.vert.spv",
        "shaders/debug_grid.frag.spv");

    PipelineConfigInfo pipelineConfig{};
    NtPipeline::defaultPipelineConfigInfo(pipelineConfig, nt::RenderMode::Lit);
    pipelineConfig.renderPass = renderPass;
    pipelineConfig.pipelineLayout = pipelineLayout;

    litPipeline = std::make_unique<NtPipeline>(
        ntDevice,
        pipelineConfig,
        "shaders/simple_shader.vert.spv",
        "shaders/simple_shader.frag.spv");

    PipelineConfigInfo wirePipelineConfig{};
    NtPipeline::defaultPipelineConfigInfo(wirePipelineConfig, nt::RenderMode::Wireframe);
    wirePipelineConfig.renderPass = renderPass;
    wirePipelineConfig.pipelineLayout = pipelineLayout;

    wireframePipeline = std::make_unique<NtPipeline>(
        ntDevice,
        wirePipelineConfig,
        "shaders/simple_line_shader.vert.spv",
        "shaders/simple_shader.frag.spv");

    PipelineConfigInfo normalsPipelineConfig{};
    NtPipeline::defaultPipelineConfigInfo(normalsPipelineConfig, nt::RenderMode::Normals);
    normalsPipelineConfig.renderPass = renderPass;
    normalsPipelineConfig.pipelineLayout = pipelineLayout;

    normalsPipeline = std::make_unique<NtPipeline>(
        ntDevice,
        normalsPipelineConfig,
        "shaders/normals_shader.vert.spv",
        "shaders/simple_shader.frag.spv");
}

void GenericRenderSystem::renderGameObjects(FrameInfo &frameInfo, std::vector<NtGameObject> &gameObjects, glm::vec3 cameraPos) {
  switch (currentRenderMode) {
        case nt::RenderMode::Wireframe:
          wireframePipeline->bind(frameInfo.commandBuffer);
          break;

        case nt::RenderMode::Normals:
          normalsPipeline->bind(frameInfo.commandBuffer);
          break;

        default:
          litPipeline->bind(frameInfo.commandBuffer);
          break;
  }

  vkCmdBindDescriptorSets(
    frameInfo.commandBuffer,
    VK_PIPELINE_BIND_POINT_GRAPHICS,
    pipelineLayout,
    0, 1,
    &frameInfo.globalDescriptorSet,
    0, nullptr);

  for (auto& obj: gameObjects) {
    if (obj.getId() == 0) {
      debugGridPipeline->bind(frameInfo.commandBuffer);

      GridPushConstants push{};
      push.modelMatrix = obj.transform.mat4();
      push.cameraPos = cameraPos;
      push.gridSpacing = 1.0f;
      push.lineThickness = 1.0f;
      push.fadeDistance = 50.0f;

      vkCmdPushConstants(
        frameInfo.commandBuffer,
        pipelineLayout,
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        0,
        sizeof(GridPushConstants),
        &push);

      obj.model->bind(frameInfo.commandBuffer);
      obj.model->draw(frameInfo.commandBuffer);

      switch (currentRenderMode) {
        case nt::RenderMode::Wireframe:
          wireframePipeline->bind(frameInfo.commandBuffer);
          break;

        case nt::RenderMode::Normals:
          normalsPipeline->bind(frameInfo.commandBuffer);
          break;

        default:
          litPipeline->bind(frameInfo.commandBuffer);
          break;
      }
    }
    else {
      NtPushConstantData push{};
      push.modelMatrix = obj.transform.mat4();
      push.normalMatrix = obj.transform.normalMatrix();

      vkCmdPushConstants(
        frameInfo.commandBuffer,
        pipelineLayout,
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        0,
        sizeof(NtPushConstantData),
        &push);

      obj.model->bind(frameInfo.commandBuffer);
      obj.model->draw(frameInfo.commandBuffer);
  }
  }

  if (currentRenderMode == RenderMode::LitWireframe) {
    wireframePipeline->bind(frameInfo.commandBuffer);

    for (auto& obj: gameObjects) {
      if (obj.getId() != 0) {
        NtPushConstantData push{};
        push.modelMatrix = obj.transform.mat4();
        push.normalMatrix = obj.transform.normalMatrix();

        vkCmdSetDepthBias(
          frameInfo.commandBuffer,
          -0.5f,  // depthBiasConstantFactor
          0.0f,   // depthBiasClamp
          -0.25f   // depthBiasSlopeFactor
        );

        vkCmdPushConstants(
          frameInfo.commandBuffer,
          pipelineLayout,
          VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
          0,
          sizeof(NtPushConstantData),
          &push);

        obj.model->bind(frameInfo.commandBuffer);
        obj.model->draw(frameInfo.commandBuffer);
      }
    }
  }
}

void GenericRenderSystem::switchRenderMode(RenderMode newRenderMode) {
  currentRenderMode = newRenderMode;
}


}
