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

GenericRenderSystem::GenericRenderSystem(NtDevice &device, VkRenderPass renderPass, VkDescriptorSetLayout globalSetLayout, VkDescriptorSetLayout modelSetLayout) : ntDevice{device} {
  createPipelineLayout(globalSetLayout, modelSetLayout);
  createPipeline(renderPass);
}
GenericRenderSystem::~GenericRenderSystem() {
  vkDestroyPipelineLayout(ntDevice.device(), pipelineLayout, nullptr);
}

void GenericRenderSystem::createPipelineLayout(VkDescriptorSetLayout globalSetLayout, VkDescriptorSetLayout modelSetLayout) {

  VkPushConstantRange pushConstantRange{};
  pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
  pushConstantRange.offset = 0;
  pushConstantRange.size = sizeof(NtPushConstantData);

  std::vector<VkDescriptorSetLayout> descriptorSetLayouts{
    globalSetLayout,
    modelSetLayout
  };

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

    PipelineConfigInfo litConfig{};
    NtPipeline::defaultPipelineConfigInfo(litConfig, nt::RenderMode::Lit);
    litConfig.renderPass = renderPass;
    litConfig.pipelineLayout = pipelineLayout;

    litPipeline = std::make_unique<NtPipeline>(
        ntDevice,
        litConfig,
        "shaders/lit_shader.vert.spv",
        "shaders/texture_shader.frag.spv");

    PipelineConfigInfo unlitConfig{};
    NtPipeline::defaultPipelineConfigInfo(unlitConfig, nt::RenderMode::Unlit);
    unlitConfig.renderPass = renderPass;
    unlitConfig.pipelineLayout = pipelineLayout;

    unlitPipeline = std::make_unique<NtPipeline>(
    ntDevice,
    unlitConfig,
    "shaders/unlit_shader.vert.spv",
    "shaders/texture_shader.frag.spv");

    PipelineConfigInfo wirePipelineConfig{};
    NtPipeline::defaultPipelineConfigInfo(wirePipelineConfig, nt::RenderMode::Wireframe);
    wirePipelineConfig.renderPass = renderPass;
    wirePipelineConfig.pipelineLayout = pipelineLayout;

    wireframePipeline = std::make_unique<NtPipeline>(
        ntDevice,
        wirePipelineConfig,
        "shaders/line_shader.vert.spv",
        "shaders/color_shader.frag.spv");

    PipelineConfigInfo normalsPipelineConfig{};
    NtPipeline::defaultPipelineConfigInfo(normalsPipelineConfig, nt::RenderMode::Normals);
    normalsPipelineConfig.renderPass = renderPass;
    normalsPipelineConfig.pipelineLayout = pipelineLayout;

    normalsPipeline = std::make_unique<NtPipeline>(
        ntDevice,
        normalsPipelineConfig,
        "shaders/normals_shader.vert.spv",
        "shaders/color_shader.frag.spv");
}

void GenericRenderSystem::renderGameObjects(FrameInfo &frameInfo, std::vector<NtGameObject> &gameObjects) {

  switch (currentRenderMode) {
        case nt::RenderMode::Unlit:
          unlitPipeline->bind(frameInfo.commandBuffer);
          break;
        
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
      vkCmdBindDescriptorSets(
        frameInfo.commandBuffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        pipelineLayout,
        1, 1, 
        &obj.materialDescriptorSet,
        0, nullptr);

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

//===================================================
//  Visualize Wireframe ON TOP of regular rendering  
//===================================================
  if (currentRenderMode == RenderMode::LitWireframe) {
    wireframePipeline->bind(frameInfo.commandBuffer);

    vkCmdBindDescriptorSets(
      frameInfo.commandBuffer,
      VK_PIPELINE_BIND_POINT_GRAPHICS,
      pipelineLayout,
      0, 1,
      &frameInfo.globalDescriptorSet,
      0, nullptr);

    for (auto& obj: gameObjects) {
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

void GenericRenderSystem::renderDebugGrid(FrameInfo &frameInfo, NtGameObject &gridObject, glm::vec3 cameraPos) {
  debugGridPipeline->bind(frameInfo.commandBuffer);

  vkCmdBindDescriptorSets(
    frameInfo.commandBuffer,
    VK_PIPELINE_BIND_POINT_GRAPHICS,
    pipelineLayout,
    0, 1,
    &frameInfo.globalDescriptorSet,
    0, nullptr);

  GridPushConstants push{};
  push.modelMatrix = gridObject.transform.mat4();
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

  gridObject.model->bind(frameInfo.commandBuffer);
  gridObject.model->draw(frameInfo.commandBuffer);
}


void GenericRenderSystem::switchRenderMode(RenderMode newRenderMode) {
  currentRenderMode = newRenderMode;
}


}
