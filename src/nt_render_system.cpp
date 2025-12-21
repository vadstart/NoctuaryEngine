#include "nt_render_system.hpp"
#include "nt_device.hpp"
#include "nt_ecs.hpp"
#include "nt_frame_info.hpp"
#include "nt_log.hpp"
#include "nt_material.hpp"
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

RenderSystem::RenderSystem(NtNexus* nexus_ptr, NtDevice &device,
                    NtSwapChain &swapChain,
                    std::shared_ptr<NtMaterialLibrary> matLibrary)
    : ntDevice{device}, nexus{nexus_ptr}, materialLibrary{matLibrary} {
}

RenderSystem::~RenderSystem() {
}

void RenderSystem::render(FrameInfo& frameInfo) {
    // Group objects by material type
    std::unordered_map<MaterialType, std::vector<NtEntity>> batches;

    for (auto& entity : entities) {
        const auto& modelComp = nexus->GetComponent<cModel>(entity);
        if (!modelComp.mesh) continue;

        MaterialType type = modelComp.mesh->getMaterialType();
        batches[type].push_back(entity);
    }

    NT_LOG_VERBOSE(LogRendering, "Rendering {} material batches", batches.size());

    // Render each material batch
    for (auto& [materialType, batch] : batches) {
        if (batch.empty()) continue;

        NT_LOG_VERBOSE(LogRendering, "Rendering batch with material type {}, {} entities",
                   static_cast<int>(materialType), batch.size());

        // Bind pipeline once for the entire batch
        auto material = materialLibrary->getMaterial(materialType);
        material->bind(frameInfo.commandBuffer);

        // Bind global descriptor set once per material
        vkCmdBindDescriptorSets(
            frameInfo.commandBuffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            material->getPipelineLayout(),
            0, 1,
            &frameInfo.globalDescriptorSet,
            0, nullptr
        );

        // Render all objects with this material
        renderBatch(frameInfo, material, batch);
    }
}

void RenderSystem::renderShadows(FrameInfo& frameInfo) {
    // Get shadow map material
    auto shadowMaterial = materialLibrary->getMaterial(MaterialType::SHADOW_MAP);
    shadowMaterial->bind(frameInfo.commandBuffer);

    // Bind global descriptor set
    vkCmdBindDescriptorSets(
        frameInfo.commandBuffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        shadowMaterial->getPipelineLayout(),
        0, 1,
        &frameInfo.globalDescriptorSet,
        0, nullptr
    );

    // Render all entities that cast shadows
    std::vector<NtEntity> shadowCasters;
    for (auto& entity : entities) {
        const auto& modelComp = nexus->GetComponent<cModel>(entity);
        if (!modelComp.mesh || !modelComp.bDropShadow) continue;
        shadowCasters.push_back(entity);
    }

    if (!shadowCasters.empty()) {
        NT_LOG_VERBOSE(LogRendering, "Rendering {} shadow casting entities", shadowCasters.size());
        renderBatch(frameInfo, shadowMaterial, shadowCasters);
    }
}

void RenderSystem::renderBatch(FrameInfo& frameInfo, std::shared_ptr<NtMaterial> material,
    const std::vector<NtEntity>& batch) {

    for (const auto& entity : batch) {
        const auto& modelComp = nexus->GetComponent<cModel>(entity);
        const auto& transformComp = nexus->GetComponent<cTransform>(entity);

        if (!modelComp.mesh) continue;

        // Render each mesh with its own material data (textures)
        for (uint32_t meshIndex = 0; meshIndex < modelComp.mesh->getMeshCount(); ++meshIndex) {

            // Bind the material descriptor set for this specific mesh (textures)
            uint32_t materialIndex = modelComp.mesh->getMaterialIndex(meshIndex);
            VkDescriptorSet materialDescriptorSet = modelComp.mesh->getMaterialDescriptorSet(materialIndex);

            if (materialDescriptorSet != VK_NULL_HANDLE) {
                vkCmdBindDescriptorSets(
                    frameInfo.commandBuffer,
                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                    material->getPipelineLayout(),
                    1, 1,
                    &materialDescriptorSet,
                    0, nullptr);
            }

            // Setup push constants
            NtPushConstantData push{};
            push.modelMatrix = transformComp.mat4();
            push.normalMatrix = transformComp.normalMatrix();

            // Bind bone matrices if animated (binding 2)
            if (modelComp.mesh->hasSkeleton()) {
                if (nexus->HasComponent<cAnimator>(entity)) {
                    if (modelComp.mesh->hasBoneDescriptor()) {
                        vkCmdBindDescriptorSets(
                            frameInfo.commandBuffer,
                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                            material->getPipelineLayout(),
                            2,  // Set 2
                            1,
                            &modelComp.mesh->getBoneDescriptorSet(),
                            0, nullptr);
                    }
                    push.isAnimated = 1;
                }
            }

            // Get the material data for this specific mesh
            const auto& matData = modelComp.mesh->getMaterialData(materialIndex);
            push.uvScale = matData.uvScale;
            push.uvOffset = matData.uvOffset;
            push.uvRotation = matData.uvRotation;
            push.hasNormalTexture = matData.normalTexture ? 1 : 0;
            push.hasMetallicRoughnessTexture = matData.pbrMetallicRoughness.metallicRoughnessTexture ? 1 : 0;
            push.metallicFactor = matData.pbrMetallicRoughness.metallicFactor;
            push.roughnessFactor = matData.pbrMetallicRoughness.roughnessFactor;
            push.time = frameInfo.elapsedTime;

            vkCmdPushConstants(
                frameInfo.commandBuffer,
                material->getPipelineLayout(),
                VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                0,
                sizeof(NtPushConstantData),
                &push);

            modelComp.mesh->bind(frameInfo.commandBuffer, meshIndex);
            modelComp.mesh->draw(frameInfo.commandBuffer, meshIndex);
        }
    }
}

} // namespace nt
