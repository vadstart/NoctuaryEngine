#include "nt_render_system.hpp"
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

  alignas(4) float billboardSize{1.0f};
  alignas(4) int isAnimated{0};
};

RenderSystem::RenderSystem(NtNexus* nexus_ptr, NtDevice &device, NtSwapChain &swapChain, VkDescriptorSetLayout globalSetLayout,
    VkDescriptorSetLayout modelSetLayout, VkDescriptorSetLayout boneSetLayout) : ntDevice{device} {
  nexus = nexus_ptr;
  createPipelineLayout(globalSetLayout, modelSetLayout, boneSetLayout);
  createPipelines(swapChain);
}
RenderSystem::~RenderSystem() {
  vkDestroyPipelineLayout(ntDevice.device(), pipelineLayout, nullptr);
}

void RenderSystem::createPipelineLayout(VkDescriptorSetLayout globalSetLayout, VkDescriptorSetLayout modelSetLayout, VkDescriptorSetLayout boneSetLayout) {

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

void RenderSystem::createPipelines(NtSwapChain &swapChain) {
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

    // PBR
    PipelineConfigInfo pbrConfig{};
    NtPipeline::defaultPipelineConfigInfo(pbrConfig, nt::RenderMode::PBR, ntDevice);
    pbrConfig.pipelineLayout = pipelineLayout;

    pbrConfig.colorAttachmentFormat = swapChain.getSwapChainImageFormat();
    pbrConfig.depthAttachmentFormat = swapChain.getSwapChainDepthFormat();

    VkPipelineRenderingCreateInfo pbrPipelineRenderingInfo{};
     pbrPipelineRenderingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
     pbrPipelineRenderingInfo.colorAttachmentCount = 1;
     pbrPipelineRenderingInfo.pColorAttachmentFormats = &pbrConfig.colorAttachmentFormat;
     pbrPipelineRenderingInfo.depthAttachmentFormat = pbrConfig.depthAttachmentFormat;
     pbrPipelineRenderingInfo.stencilAttachmentFormat = VK_FORMAT_UNDEFINED;

    pbrPipeline = std::make_unique<NtPipeline>(
        ntDevice,
        pbrConfig,
        pbrPipelineRenderingInfo,
        "shaders/pbr.vert.spv",
        "shaders/pbr.frag.spv");

    // NPR
    PipelineConfigInfo nprConfig{};
    NtPipeline::defaultPipelineConfigInfo(nprConfig, nt::RenderMode::NPR, ntDevice);
    nprConfig.pipelineLayout = pipelineLayout;

    nprConfig.colorAttachmentFormat = swapChain.getSwapChainImageFormat();
    nprConfig.depthAttachmentFormat = swapChain.getSwapChainDepthFormat();

    VkPipelineRenderingCreateInfo nprPipelineRenderingInfo{};
     nprPipelineRenderingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
     nprPipelineRenderingInfo.colorAttachmentCount = 1;
     nprPipelineRenderingInfo.pColorAttachmentFormats = &nprConfig.colorAttachmentFormat;
     nprPipelineRenderingInfo.depthAttachmentFormat = nprConfig.depthAttachmentFormat;
     nprPipelineRenderingInfo.stencilAttachmentFormat = VK_FORMAT_UNDEFINED;

    nprPipeline = std::make_unique<NtPipeline>(
        ntDevice,
        nprConfig,
        nprPipelineRenderingInfo,
        "shaders/npr.vert.spv",
        "shaders/npr.frag.spv");

    // billboardPipeline = std::make_unique<NtPipeline>(
    //     ntDevice,
    //     billboardPipelineConfig,
    //     billPipelineRenderingInfo,
    //     "shaders/billboard.vert.spv",
    //     "shaders/billboard.frag.spv");

    // BILLBOARDS
    // PipelineConfigInfo billboardPipelineConfig{};
    // NtPipeline::defaultPipelineConfigInfo(billboardPipelineConfig, nt::RenderMode::Billboard, ntDevice);
    // billboardPipelineConfig.pipelineLayout = pipelineLayout;

    // billboardPipelineConfig.colorAttachmentFormat = swapChain.getSwapChainImageFormat();
    // billboardPipelineConfig.depthAttachmentFormat = swapChain.getSwapChainDepthFormat();

    // VkPipelineRenderingCreateInfo billPipelineRenderingInfo{};
    //  billPipelineRenderingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    //  billPipelineRenderingInfo.colorAttachmentCount = 1;
    //  billPipelineRenderingInfo.pColorAttachmentFormats = &billboardPipelineConfig.colorAttachmentFormat;
    //  billPipelineRenderingInfo.depthAttachmentFormat = billboardPipelineConfig.depthAttachmentFormat;
    //  billPipelineRenderingInfo.stencilAttachmentFormat = VK_FORMAT_UNDEFINED;

    // billboardPipeline = std::make_unique<NtPipeline>(
    //     ntDevice,
    //     billboardPipelineConfig,
    //     billPipelineRenderingInfo,
    //     "shaders/billboard.vert.spv",
    //     "shaders/billboard.frag.spv");
}

void RenderSystem::renderGameObjects(FrameInfo &frameInfo, bool bShadowPass)
{
    if (bShadowPass)
        shadowMapPipeline->bind(frameInfo.commandBuffer);
    else pbrPipeline->bind(frameInfo.commandBuffer);

  vkCmdBindDescriptorSets(
    frameInfo.commandBuffer,
    VK_PIPELINE_BIND_POINT_GRAPHICS,
    pipelineLayout,
    0, 1,
    &frameInfo.globalDescriptorSet,
    0, nullptr);

  for (auto const& entity : entities)
  {
    auto& transform = nexus->GetComponent<cTransform>(entity);
    const auto& model = nexus->GetComponent<cModel>(entity);

    if ((bShadowPass && !model.bDropShadow) || model.bNPRshading) continue;

    // Render each mesh with its own material
    const auto& materials = model.mesh->getMaterials();
    for (uint32_t meshIndex = 0; meshIndex < model.mesh->getMeshCount(); ++meshIndex)
    {
      // Bind the material descriptor set for this specific mesh
      uint32_t materialIndex = model.mesh->getMaterialIndex(meshIndex);
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
      push.modelMatrix = transform.mat4();
      push.normalMatrix = transform.normalMatrix();

      // Bind bone matrices if animated (binding 2)
      if (model.mesh->hasSkeleton()) {
        if (nexus->HasComponent<cAnimator>(entity)) {
            if (model.mesh->getBoneDescriptorSet() != VK_NULL_HANDLE) {
                vkCmdBindDescriptorSets(
                    frameInfo.commandBuffer,
                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                    pipelineLayout,
                    2,  // Set 2
                    1,
                    &model.mesh->getBoneDescriptorSet(),
                    0, nullptr);
            }
            push.isAnimated = true;
        }
      }

      // Get the material for this specific mesh
        push.uvScale = materials[materialIndex]->getMaterialData().uvScale;
        push.uvOffset = materials[materialIndex]->getMaterialData().uvOffset;
        push.uvRotation = materials[materialIndex]->getMaterialData().uvRotation;
        push.hasNormalTexture = materials[materialIndex]->hasNormalTexture() ? 1 : 0;
        push.hasMetallicRoughnessTexture = materials[materialIndex]->hasMetallicRoughnessTexture() ? 1 : 0;
        push.metallicFactor = materials[materialIndex]->getMaterialData().pbrMetallicRoughness.metallicFactor;
        push.roughnessFactor = materials[materialIndex]->getMaterialData().pbrMetallicRoughness.roughnessFactor;

      vkCmdPushConstants(
        frameInfo.commandBuffer,
        pipelineLayout,
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        0,
        sizeof(NtPushConstantData),
        &push);

      model.mesh->bind(frameInfo.commandBuffer, meshIndex);
      model.mesh->draw(frameInfo.commandBuffer, meshIndex);
    }
  }

//===================================================
//  Character stylized Pipeline
//===================================================
if(!bShadowPass)
    nprPipeline->bind(frameInfo.commandBuffer);

vkCmdBindDescriptorSets(
    frameInfo.commandBuffer,
    VK_PIPELINE_BIND_POINT_GRAPHICS,
    pipelineLayout,
    0, 1,
    &frameInfo.globalDescriptorSet,
    0, nullptr);


for (auto const& entity : entities)
  {
    auto& transform = nexus->GetComponent<cTransform>(entity);
    const auto& model = nexus->GetComponent<cModel>(entity);

    if ((bShadowPass && !model.bDropShadow) || !model.bNPRshading) continue;

    // Render each mesh with its own material
    const auto& materials = model.mesh->getMaterials();
    for (uint32_t meshIndex = 0; meshIndex < model.mesh->getMeshCount(); ++meshIndex) {

        // Bind the material descriptor set for this specific mesh
        uint32_t materialIndex = model.mesh->getMaterialIndex(meshIndex);
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
        push.modelMatrix = transform.mat4();
        push.normalMatrix = transform.normalMatrix();

        if (model.mesh->hasSkeleton()) {
            if (nexus->HasComponent<cAnimator>(entity)) {
                if (model.mesh->getBoneDescriptorSet() != VK_NULL_HANDLE) {
                    vkCmdBindDescriptorSets(
                        frameInfo.commandBuffer,
                        VK_PIPELINE_BIND_POINT_GRAPHICS,
                        pipelineLayout,
                        2,  // Set 2
                        1,
                        &model.mesh->getBoneDescriptorSet(),
                        0, nullptr);
                }
                push.isAnimated = true;
            }
        }

        // Get the material for this specific mesh
        push.uvScale = materials[materialIndex]->getMaterialData().uvScale;
        push.uvOffset = materials[materialIndex]->getMaterialData().uvOffset;
        push.uvRotation = materials[materialIndex]->getMaterialData().uvRotation;
        push.hasNormalTexture = materials[materialIndex]->hasNormalTexture() ? 1 : 0;
        push.hasMetallicRoughnessTexture = materials[materialIndex]->hasMetallicRoughnessTexture() ? 1 : 0;
        push.metallicFactor = materials[materialIndex]->getMaterialData().pbrMetallicRoughness.metallicFactor;
        push.roughnessFactor = materials[materialIndex]->getMaterialData().pbrMetallicRoughness.roughnessFactor;

        vkCmdPushConstants(
        frameInfo.commandBuffer,
        pipelineLayout,
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        0,
        sizeof(NtPushConstantData),
        &push);

        model.mesh->bind(frameInfo.commandBuffer, meshIndex);
        model.mesh->draw(frameInfo.commandBuffer, meshIndex);
    }
    }

}

    // void GenericRenderSystem::renderLightBillboards(FrameInfo &frameInfo) {
    //   billboardPipeline->bind(frameInfo.commandBuffer);

    //   vkCmdBindDescriptorSets(
    //     frameInfo.commandBuffer,
    //     VK_PIPELINE_BIND_POINT_GRAPHICS,
    //     pipelineLayout,
    //     0, 1,
    //     &frameInfo.globalDescriptorSet,
    //     0, nullptr);

    //   for (auto& kv: frameInfo.gameObjects) {
    //     auto& obj = kv.second;
    //     if (obj.pointLight == nullptr || obj.model == nullptr) continue;

    //     // Bind the material descriptor set for the billboard texture
    //     const auto& materials = obj.model->getMaterials();
    //     if (materials.size() > 0 && materials[0]->getDescriptorSet() != VK_NULL_HANDLE) {
    //       VkDescriptorSet materialDescriptorSet = materials[0]->getDescriptorSet();
    //       vkCmdBindDescriptorSets(
    //         frameInfo.commandBuffer,
    //         VK_PIPELINE_BIND_POINT_GRAPHICS,
    //         pipelineLayout,
    //         1, 1,
    //         &materialDescriptorSet,
    //         0, nullptr);
    //     }

    //     NtPushConstantData push{};
    //     push.modelMatrix = obj.transform.mat4();
    //     push.normalMatrix = obj.transform.normalMatrix();
    //     push.hasNormalTexture = 0;
    //     push.hasMetallicRoughnessTexture = 0;
    //     push.metallicFactor = 1.0f;
    //     push.roughnessFactor = 1.0f;
    //     push.lightColor = obj.color;
    //     push.lightIntensity = obj.pointLight->lightIntensity;
    //     push.billboardSize = 0.5f; // Default billboard size

    //     vkCmdPushConstants(
    //       frameInfo.commandBuffer,
    //       pipelineLayout,
    //       VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
    //       0,
    //       sizeof(NtPushConstantData),
    //       &push);

    //     obj.model->drawAll(frameInfo.commandBuffer);
    //   }
    // }

    } // namespace nt
