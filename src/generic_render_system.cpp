#include "generic_render_system.hpp"
#include "imgui/imgui.h"
#include "nt_device.hpp"
#include "nt_frame_info.hpp"
#include "nt_pipeline.hpp"
#include "nt_swap_chain.hpp"
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

struct PointLightPushConstants {
  glm::vec4 position{};
  glm::vec4 color{};
};

struct NtPushConstantData {
  alignas(16) glm::mat4 modelMatrix{1.f};
  alignas(16) glm::mat4 normalMatrix{1.f};
  alignas(8) glm::vec2 uvScale{1.0f, 1.0f};
  alignas(8) glm::vec2 uvOffset{0.0f, 0.0f};
  alignas(4) float uvRotation{0.0f};
  alignas(4) int hasNormalTexture{0};
  alignas(4) int hasMetallicRoughnessTexture{0};
  alignas(4) float metallicFactor{1.0f};
  alignas(4) float roughnessFactor{1.0f};
  alignas(16) glm::vec3 lightColor{1.0f, 1.0f, 1.0f};
  alignas(4) float lightIntensity{1.0f};
  alignas(4) float billboardSize{1.0f};
  alignas(4) int isAnimated{0};
};

GenericRenderSystem::GenericRenderSystem(NtDevice &device, NtSwapChain &swapChain, VkDescriptorSetLayout globalSetLayout,
    VkDescriptorSetLayout modelSetLayout, VkDescriptorSetLayout boneSetLayout) : ntDevice{device} {
  createPipelineLayout(globalSetLayout, modelSetLayout, boneSetLayout);
  createPipelines(swapChain);
}
GenericRenderSystem::~GenericRenderSystem() {
  vkDestroyPipelineLayout(ntDevice.device(), pipelineLayout, nullptr);
}

void GenericRenderSystem::createPipelineLayout(VkDescriptorSetLayout globalSetLayout, VkDescriptorSetLayout modelSetLayout, VkDescriptorSetLayout boneSetLayout) {

  VkPushConstantRange pushConstantRange{};
  pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
  pushConstantRange.offset = 0;
  pushConstantRange.size = sizeof(NtPushConstantData);

  std::vector<VkDescriptorSetLayout> descriptorSetLayouts{
    globalSetLayout,
    modelSetLayout,
    boneSetLayout
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

void GenericRenderSystem::createPipelines(NtSwapChain &swapChain) {
    assert(pipelineLayout != nullptr && "Cannot create pipeline before pipeline layout");

    // SHADOW MAP
    PipelineConfigInfo shadowMapPipelineConfig{};
    NtPipeline::defaultPipelineConfigInfo(shadowMapPipelineConfig, nt::RenderMode::ShadowMap, ntDevice);
    shadowMapPipelineConfig.pipelineLayout = pipelineLayout;

    shadowMapPipelineConfig.depthAttachmentFormat = VK_FORMAT_D32_SFLOAT;
    shadowMapPipelineConfig.multisampleInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineRenderingCreateInfo shadowPipelineRenderingInfo{};
     shadowPipelineRenderingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
     shadowPipelineRenderingInfo.colorAttachmentCount = 0;
     shadowPipelineRenderingInfo.pColorAttachmentFormats = nullptr;
     shadowPipelineRenderingInfo.depthAttachmentFormat = shadowMapPipelineConfig.depthAttachmentFormat;
     shadowPipelineRenderingInfo.stencilAttachmentFormat = VK_FORMAT_UNDEFINED;

    shadowMapPipeline = std::make_unique<NtPipeline>(
        ntDevice,
        shadowMapPipelineConfig,
        shadowPipelineRenderingInfo,
        "shaders/shadowmap.vert.spv",
        "shaders/shadowmap.frag.spv");

    // LIT
    PipelineConfigInfo litConfig{};
    NtPipeline::defaultPipelineConfigInfo(litConfig, nt::RenderMode::Lit, ntDevice);
    litConfig.pipelineLayout = pipelineLayout;

    litConfig.colorAttachmentFormat = swapChain.getSwapChainImageFormat();
    litConfig.depthAttachmentFormat = swapChain.getSwapChainDepthFormat();

    VkPipelineRenderingCreateInfo litPipelineRenderingInfo{};
     litPipelineRenderingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
     litPipelineRenderingInfo.colorAttachmentCount = 1;
     litPipelineRenderingInfo.pColorAttachmentFormats = &litConfig.colorAttachmentFormat;
     litPipelineRenderingInfo.depthAttachmentFormat = litConfig.depthAttachmentFormat;
     litPipelineRenderingInfo.stencilAttachmentFormat = VK_FORMAT_UNDEFINED;

    litPipeline = std::make_unique<NtPipeline>(
        ntDevice,
        litConfig,
        litPipelineRenderingInfo,
        "shaders/pbr.vert.spv",
        "shaders/pbr.frag.spv");

    // UNLIT
    PipelineConfigInfo unlitConfig{};
    NtPipeline::defaultPipelineConfigInfo(unlitConfig, nt::RenderMode::Unlit, ntDevice);
    unlitConfig.pipelineLayout = pipelineLayout;

    unlitConfig.colorAttachmentFormat = swapChain.getSwapChainImageFormat();
    unlitConfig.depthAttachmentFormat = swapChain.getSwapChainDepthFormat();

    VkPipelineRenderingCreateInfo unlitPipelineRenderingInfo{};
     unlitPipelineRenderingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
     unlitPipelineRenderingInfo.colorAttachmentCount = 1;
     unlitPipelineRenderingInfo.pColorAttachmentFormats = &unlitConfig.colorAttachmentFormat;
     unlitPipelineRenderingInfo.depthAttachmentFormat = unlitConfig.depthAttachmentFormat;
     unlitPipelineRenderingInfo.stencilAttachmentFormat = VK_FORMAT_UNDEFINED;

    unlitPipeline = std::make_unique<NtPipeline>(
        ntDevice,
        unlitConfig,
        unlitPipelineRenderingInfo,
        "shaders/npr.vert.spv",
        "shaders/npr.frag.spv");

    // BILLBOARDS
    PipelineConfigInfo billboardPipelineConfig{};
    NtPipeline::defaultPipelineConfigInfo(billboardPipelineConfig, nt::RenderMode::Billboard, ntDevice);
    billboardPipelineConfig.pipelineLayout = pipelineLayout;

    billboardPipelineConfig.colorAttachmentFormat = swapChain.getSwapChainImageFormat();
    billboardPipelineConfig.depthAttachmentFormat = swapChain.getSwapChainDepthFormat();

    VkPipelineRenderingCreateInfo billPipelineRenderingInfo{};
     billPipelineRenderingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
     billPipelineRenderingInfo.colorAttachmentCount = 1;
     billPipelineRenderingInfo.pColorAttachmentFormats = &billboardPipelineConfig.colorAttachmentFormat;
     billPipelineRenderingInfo.depthAttachmentFormat = billboardPipelineConfig.depthAttachmentFormat;
     billPipelineRenderingInfo.stencilAttachmentFormat = VK_FORMAT_UNDEFINED;

    billboardPipeline = std::make_unique<NtPipeline>(
        ntDevice,
        billboardPipelineConfig,
        billPipelineRenderingInfo,
        "shaders/billboard.vert.spv",
        "shaders/billboard.frag.spv");
}

void GenericRenderSystem::updateLights(FrameInfo &frameInfo, GlobalUbo &ubo, glm::vec3 O_dir, float O_scale, float O_near, float O_far) {
  int lightIndex = 0;
  for (auto& kv: frameInfo.gameObjects) {
    auto &obj = kv.second;
    if (obj.pointLight == nullptr) continue;

    assert(lightIndex < MAX_LIGHTS && "Point lights exceed maximum specified!");

    // copy the light to ubo
    if (obj.pointLight->lightType != 2)
        ubo.pointLights[lightIndex].position = glm::vec4(obj.transform.translation, 1.f);
    else ubo.pointLights[lightIndex].position = glm::vec4(obj.transform.rotation, 1.f); // We only care about the rotation of directional light
    ubo.pointLights[lightIndex].color = glm::vec4(obj.color, obj.pointLight->lightIntensity);
    ubo.pointLights[lightIndex].lightType = obj.pointLight->lightType;

    // Shadows (computing light space matrix)
    if (lightIndex == 0) {
        int shadowCasterIndex = 0;

            switch(ubo.pointLights[0].lightType) {
                case 2: // DIRECTIONAL
                  glm::vec3 lightPos = glm::vec3(ubo.pointLights[shadowCasterIndex].position);
                  // glm::vec3 lightDir = glm::normalize(lightPos); // For directional, position represents direction
                  glm::vec3 sceneCenter = glm::vec3(0.0f, 0.0f, 0.0f);
                  glm::vec3 lightDir = glm::normalize(O_dir);

                  // Use standard up vector
                    glm::vec3 upVector = glm::vec3(0.0f, 1.0f, 0.0f);
                    // Avoid gimbal lock if light direction is parallel to up
                    if (abs(glm::dot(lightDir, upVector)) > 0.99f) {
                        upVector = glm::vec3(1.0f, 0.0f, 0.0f);
                    }

                  // Store light direction for shader
                  ubo.shadowLightDirection = glm::vec4(lightDir, static_cast<float>(eLightType::Directional));

                  glm::mat4 lightViewMatrix = glm::lookAt(lightDir, sceneCenter, upVector);

                  // Vulcan clip space correction matrix
                  // Converts from OpenGL [-1, 1] to Vulkan [0, 1] depth and flips Y
                  glm::mat4 clip = glm::mat4(1.0f, 0.0f, 0.0f, 0.0f,
                                             0.0f,-1.0f, 0.0f, 0.0f,    // Flip Y
                                             0.0f, 0.0f, 0.5f, 0.0f,    // Scale Z from [-1, 1] to [0, 1]
                                             0.0f, 0.0f, 0.5f, 1.0f);   // Translate Z

                  float orthoSize = O_scale;
                  glm::mat4 lightProjection = clip * glm::ortho(
                      -orthoSize, orthoSize,
                      -orthoSize, orthoSize,
                      O_near, O_far
                  );
                  lightProjection[1][1] *= -1;

                  ubo.lightSpaceMatrix = lightProjection * lightViewMatrix;
                break;
        }

        }

    lightIndex += 1;
  }
  ubo.numLights = lightIndex;

}

void GenericRenderSystem::renderGameObjects(FrameInfo &frameInfo) {

  switch (currentRenderMode) {
        case nt::RenderMode::ShadowMap:
            shadowMapPipeline->bind(frameInfo.commandBuffer);
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
    if (obj.model == nullptr || obj.pointLight != nullptr || obj.isCharacter || currentRenderMode == nt::RenderMode::ShadowMap) continue;

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
      push.isAnimated = obj.animator != nullptr ? 1 : 0;

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
if(currentRenderMode != nt::RenderMode::ShadowMap)
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
        if (obj.model->getSkeleton()->isAnimated) {
            if (obj.model->getBoneDescriptorSet() != VK_NULL_HANDLE) {
                vkCmdBindDescriptorSets(
                    frameInfo.commandBuffer,
                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                    pipelineLayout,
                    2,  // Set 2
                    1,
                    &obj.model->getBoneDescriptorSet(),
                    0, nullptr);
            }
        }

        NtPushConstantData push{};
        push.modelMatrix = obj.transform.mat4();
        push.normalMatrix = obj.transform.normalMatrix();
        push.isAnimated = obj.animator != nullptr ? 1 : 0;

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
  // if (currentRenderMode == RenderMode::LitWireframe) {
  //   wireframePipeline->bind(frameInfo.commandBuffer);

  //   vkCmdBindDescriptorSets(
  //     frameInfo.commandBuffer,
  //     VK_PIPELINE_BIND_POINT_GRAPHICS,
  //     pipelineLayout,
  //     0, 1,
  //     &frameInfo.globalDescriptorSet,
  //     0, nullptr);

  //   for (auto& kv: frameInfo.gameObjects) {
  //     auto& obj = kv.second;
  //     if (obj.model == nullptr || obj.pointLight != nullptr) continue;

  //     for (uint32_t meshIndex = 0; meshIndex < obj.model->getMeshCount(); ++meshIndex) {

  //       NtPushConstantData push{};
  //       push.modelMatrix = obj.transform.mat4();

  //       vkCmdSetDepthBias(
  //         frameInfo.commandBuffer,
  //         -0.5f,  // depthBiasConstantFactor
  //         0.0f,   // depthBiasClamp
  //         -0.25f   // depthBiasSlopeFactor
  //       );

  //       vkCmdPushConstants(
  //         frameInfo.commandBuffer,
  //         pipelineLayout,
  //         VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
  //         0,
  //         sizeof(NtPushConstantData),
  //         &push);

  //       obj.model->bind(frameInfo.commandBuffer, meshIndex);
  //       obj.model->draw(frameInfo.commandBuffer, meshIndex);
  //     }
  //   }
  // }

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

} // namespace nt
