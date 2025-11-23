#include "generic_render_system.hpp"
#include "imgui/imgui.h"
#include "nt_device.hpp"
#include "nt_frame_info.hpp"
#include "nt_pipeline.hpp"
#include "nt_types.hpp"
#include "vulkan/vulkan_core.h"
#include <cstdint>
#include <glm/fwd.hpp>
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
    float gridSpacing = 1.0f;
    float lineThickness = 1.0f;
    float fadeDistance = 50.0f;
};

struct PointLightPushConstants {
  glm::vec4 position{};
  glm::vec4 color{};
  float radius;
};

struct NtPushConstantData {
  alignas(16) glm::mat4 modelMatrix{1.f};
  alignas(16) glm::mat4 normalMatrix{1.f};
  alignas(8) glm::vec2 uvScale{1.0f, 1.0f};
  alignas(8) glm::vec2 uvOffset{0.0f, 0.0f};
  alignas(4) float uvRotation{0.0f};
  alignas(4) int hasNormalTexture{0};
  alignas(4) int hasMetallicRoughnessTexture{0};
  alignas(4) int debugMode{0};
  alignas(4) float metallicFactor{1.0f};
  alignas(4) float roughnessFactor{1.0f};
  alignas(16) glm::vec3 lightColor{1.0f, 1.0f, 1.0f};
  alignas(4) float lightIntensity{1.0f};
  alignas(4) float billboardSize{1.0f};
};

GenericRenderSystem::GenericRenderSystem(NtDevice &device, VkRenderPass renderPass, VkDescriptorSetLayout globalSetLayout,
    VkDescriptorSetLayout modelSetLayout/*, VkDescriptorSetLayout boneSetLayout*/) : ntDevice{device} {
  createPipelineLayout(globalSetLayout, modelSetLayout/*, boneSetLayout*/);
  createPipeline(renderPass);
}
GenericRenderSystem::~GenericRenderSystem() {
  vkDestroyPipelineLayout(ntDevice.device(), pipelineLayout, nullptr);
}

void GenericRenderSystem::createPipelineLayout(VkDescriptorSetLayout globalSetLayout, VkDescriptorSetLayout modelSetLayout/*, VkDescriptorSetLayout boneSetLayout*/) {

  VkPushConstantRange pushConstantRange{};
  pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
  pushConstantRange.offset = 0;
  pushConstantRange.size = sizeof(NtPushConstantData);

  std::vector<VkDescriptorSetLayout> descriptorSetLayouts{
    globalSetLayout,
    modelSetLayout/*,
    boneSetLayout*/
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
        "shaders/texture_lit_shader.frag.spv");

    PipelineConfigInfo unlitConfig{};
    NtPipeline::defaultPipelineConfigInfo(unlitConfig, nt::RenderMode::Unlit);
    unlitConfig.renderPass = renderPass;
    unlitConfig.pipelineLayout = pipelineLayout;

    unlitPipeline = std::make_unique<NtPipeline>(
    ntDevice,
    unlitConfig,
    "shaders/unlit_shader.vert.spv",
    "shaders/texture_unlit_shader.frag.spv");

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

    PipelineConfigInfo depthPipelineConfig{};
    NtPipeline::defaultPipelineConfigInfo(depthPipelineConfig, nt::RenderMode::Depth);
    depthPipelineConfig.renderPass = renderPass;
    depthPipelineConfig.pipelineLayout = pipelineLayout;

    depthPipeline = std::make_unique<NtPipeline>(
        ntDevice,
        depthPipelineConfig,
        "shaders/depth_shader.vert.spv",
        "shaders/depth_shader.frag.spv");

    PipelineConfigInfo billboardPipelineConfig{};
    NtPipeline::defaultPipelineConfigInfo(billboardPipelineConfig, nt::RenderMode::Billboard);
    billboardPipelineConfig.renderPass = renderPass;
    billboardPipelineConfig.pipelineLayout = pipelineLayout;

    billboardPipeline = std::make_unique<NtPipeline>(
        ntDevice,
        billboardPipelineConfig,
        "shaders/billboard.vert.spv",
        "shaders/billboard.frag.spv");
}

void GenericRenderSystem::updateLights(FrameInfo &frameInfo, GlobalUbo &ubo) {
  int lightIndex = 0;
  for (auto& kv: frameInfo.gameObjects) {
    auto &obj = kv.second;
    if (obj.pointLight == nullptr) continue;

    assert(lightIndex < MAX_LIGHTS && "Point lights exceed maximum specified!");

    // copy the light to ubo
    ubo.pointLights[lightIndex].position = glm::vec4(obj.transform.translation, 1.f);
    ubo.pointLights[lightIndex].color = glm::vec4(obj.color, obj.pointLight->lightIntensity);

    lightIndex += 1;
  }
  ubo.numLights = lightIndex;
}

void GenericRenderSystem::renderGameObjects(FrameInfo &frameInfo) {

  switch (currentRenderMode) {
        case nt::RenderMode::Unlit:
          unlitPipeline->bind(frameInfo.commandBuffer);
          break;

        case nt::RenderMode::Wireframe:
          wireframePipeline->bind(frameInfo.commandBuffer);
          break;

        case nt::RenderMode::Depth:
          depthPipeline->bind(frameInfo.commandBuffer);
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

  for (auto& kv: frameInfo.gameObjects) {
    auto& obj = kv.second;
    if (obj.model == nullptr || obj.pointLight != nullptr || obj.isCharacter) continue;

    // Render each mesh with its own material
    const auto& materials = obj.model->getMaterials();
    for (uint32_t meshIndex = 0; meshIndex < obj.model->getMeshCount(); ++meshIndex) {

      // Bind the material descriptor set for this specific mesh
      uint32_t materialIndex = obj.model->getMaterialIndex(meshIndex);
      if (materials.size() > materialIndex && materials[materialIndex]->getDescriptorSet() != VK_NULL_HANDLE) {
        VkDescriptorSet materialDescriptorSet = materials[materialIndex]->getDescriptorSet();
        vkCmdBindDescriptorSets(
          frameInfo.commandBuffer,
          VK_PIPELINE_BIND_POINT_GRAPHICS,
          pipelineLayout,
          1, 1,
          &materialDescriptorSet,
          0, nullptr);
      }

      NtPushConstantData push{};
      push.modelMatrix = obj.transform.mat4();
      push.normalMatrix = obj.transform.normalMatrix();

      if(currentRenderMode != nt::RenderMode::NormalTangents)
        push.debugMode = 0; // Normal rendering
      else push.debugMode = 1;

      // Get the material for this specific mesh
      if (materials.size() > materialIndex) {
        push.uvScale = materials[materialIndex]->getMaterialData().uvScale;
        push.uvOffset = materials[materialIndex]->getMaterialData().uvOffset;
        push.uvRotation = materials[materialIndex]->getMaterialData().uvRotation;
        push.hasNormalTexture = materials[materialIndex]->hasNormalTexture() ? 1 : 0;
        push.hasMetallicRoughnessTexture = materials[materialIndex]->hasMetallicRoughnessTexture() ? 1 : 0;
        push.metallicFactor = materials[materialIndex]->getMaterialData().pbrMetallicRoughness.metallicFactor;
        push.roughnessFactor = materials[materialIndex]->getMaterialData().pbrMetallicRoughness.roughnessFactor;
      } else {
        push.uvScale = glm::vec2(1.0f);
        push.uvOffset = glm::vec2(0.0f);
        push.uvRotation = 0.0f;
        push.hasNormalTexture = 0;
        push.hasMetallicRoughnessTexture = 0;
        push.metallicFactor = 1.0f;
        push.roughnessFactor = 1.0f;
      }

      vkCmdPushConstants(
        frameInfo.commandBuffer,
        pipelineLayout,
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        0,
        sizeof(NtPushConstantData),
        &push);

      obj.model->bind(frameInfo.commandBuffer, meshIndex);
      obj.model->draw(frameInfo.commandBuffer, meshIndex);
    }
  }

//===================================================
//  Character stylized Pipeline
//===================================================
unlitPipeline->bind(frameInfo.commandBuffer);

vkCmdBindDescriptorSets(
    frameInfo.commandBuffer,
    VK_PIPELINE_BIND_POINT_GRAPHICS,
    pipelineLayout,
    0, 1,
    &frameInfo.globalDescriptorSet,
    0, nullptr);


for (auto& kv: frameInfo.gameObjects) {
    auto& obj = kv.second;
    if (obj.model == nullptr || obj.pointLight != nullptr || !obj.isCharacter) continue;

    // Render each mesh with its own material
    const auto& materials = obj.model->getMaterials();
    for (uint32_t meshIndex = 0; meshIndex < obj.model->getMeshCount(); ++meshIndex) {

        // Bind the material descriptor set for this specific mesh
        uint32_t materialIndex = obj.model->getMaterialIndex(meshIndex);
        if (materials.size() > materialIndex && materials[materialIndex]->getDescriptorSet() != VK_NULL_HANDLE) {
            VkDescriptorSet materialDescriptorSet = materials[materialIndex]->getDescriptorSet();
            vkCmdBindDescriptorSets(
                frameInfo.commandBuffer,
                VK_PIPELINE_BIND_POINT_GRAPHICS,
                pipelineLayout,
                1, 1,
                &materialDescriptorSet,
                0, nullptr);
        }

        // Bind bone matrices if animated (binding 2)
        if (obj.animationData != nullptr) {

            if (obj.animationData->descriptorSet != VK_NULL_HANDLE) {
                vkCmdBindDescriptorSets(
                    frameInfo.commandBuffer,
                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                    pipelineLayout,
                    2,  // Set 2
                    1,
                    &obj.animationData->descriptorSet,
                    0, nullptr);
            }
        }



        NtPushConstantData push{};
        push.modelMatrix = obj.transform.mat4();
        push.normalMatrix = obj.transform.normalMatrix();

        // Get the material for this specific mesh
        if (materials.size() > materialIndex) {
            push.uvScale = materials[materialIndex]->getMaterialData().uvScale;
            push.uvOffset = materials[materialIndex]->getMaterialData().uvOffset;
            push.uvRotation = materials[materialIndex]->getMaterialData().uvRotation;
            push.hasNormalTexture = materials[materialIndex]->hasNormalTexture() ? 1 : 0;
            push.hasMetallicRoughnessTexture = materials[materialIndex]->hasMetallicRoughnessTexture() ? 1 : 0;
            push.metallicFactor = materials[materialIndex]->getMaterialData().pbrMetallicRoughness.metallicFactor;
            push.roughnessFactor = materials[materialIndex]->getMaterialData().pbrMetallicRoughness.roughnessFactor;
        } else {
            push.uvScale = glm::vec2(1.0f);
            push.uvOffset = glm::vec2(0.0f);
            push.uvRotation = 0.0f;
            push.hasNormalTexture = 0;
            push.hasMetallicRoughnessTexture = 0;
            push.metallicFactor = 1.0f;
            push.roughnessFactor = 1.0f;
        }

        vkCmdPushConstants(
        frameInfo.commandBuffer,
        pipelineLayout,
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        0,
        sizeof(NtPushConstantData),
        &push);

        obj.model->bind(frameInfo.commandBuffer, meshIndex);
        obj.model->draw(frameInfo.commandBuffer, meshIndex);
    }
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

    for (auto& kv: frameInfo.gameObjects) {
      auto& obj = kv.second;
      if (obj.model == nullptr || obj.pointLight != nullptr) continue;

      for (uint32_t meshIndex = 0; meshIndex < obj.model->getMeshCount(); ++meshIndex) {

        NtPushConstantData push{};
        push.modelMatrix = obj.transform.mat4();

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

        obj.model->bind(frameInfo.commandBuffer, meshIndex);
        obj.model->draw(frameInfo.commandBuffer, meshIndex);
      }
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

  gridObject.model->drawAll(frameInfo.commandBuffer);
}


void GenericRenderSystem::renderLightBillboards(FrameInfo &frameInfo) {
  billboardPipeline->bind(frameInfo.commandBuffer);

  vkCmdBindDescriptorSets(
    frameInfo.commandBuffer,
    VK_PIPELINE_BIND_POINT_GRAPHICS,
    pipelineLayout,
    0, 1,
    &frameInfo.globalDescriptorSet,
    0, nullptr);

  for (auto& kv: frameInfo.gameObjects) {
    auto& obj = kv.second;
    if (obj.pointLight == nullptr || obj.model == nullptr) continue;

    // Bind the material descriptor set for the billboard texture
    const auto& materials = obj.model->getMaterials();
    if (materials.size() > 0 && materials[0]->getDescriptorSet() != VK_NULL_HANDLE) {
      VkDescriptorSet materialDescriptorSet = materials[0]->getDescriptorSet();
      vkCmdBindDescriptorSets(
        frameInfo.commandBuffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        pipelineLayout,
        1, 1,
        &materialDescriptorSet,
        0, nullptr);
    }

    NtPushConstantData push{};
    push.modelMatrix = obj.transform.mat4();
    push.normalMatrix = obj.transform.normalMatrix();
    push.debugMode = 0;
    push.hasNormalTexture = 0;
    push.hasMetallicRoughnessTexture = 0;
    push.metallicFactor = 1.0f;
    push.roughnessFactor = 1.0f;
    push.lightColor = obj.color;
    push.lightIntensity = obj.pointLight->lightIntensity;
    push.billboardSize = 0.5f; // Default billboard size

    vkCmdPushConstants(
      frameInfo.commandBuffer,
      pipelineLayout,
      VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
      0,
      sizeof(NtPushConstantData),
      &push);

    obj.model->drawAll(frameInfo.commandBuffer);
  }
}

void GenericRenderSystem::switchRenderMode(RenderMode newRenderMode) {
  currentRenderMode = newRenderMode;
}


}
