#pragma once

#include "nt_device.hpp"
#include "nt_image.hpp"
#include "vulkan/vulkan_core.h"
#include <glm/glm.hpp>
#include <memory>
#include <string>
#include <vector>

namespace nt {

enum struct MaterialType {
    PBR,
    NPR,
    UNLIT,
    SCROLLING_UV
};

// class NtMaterial {
// public:
//     enum class AlphaMode {
//         Opaque,
//         Mask,
//         Blend
//     };

//     struct PbrMetallicRoughness {
//         glm::vec4 baseColorFactor{1.0f, 1.0f, 1.0f, 1.0f};
//         float metallicFactor{1.0f};
//         float roughnessFactor{1.0f};

//         std::shared_ptr<NtImage> baseColorTexture;
//         std::shared_ptr<NtImage> metallicRoughnessTexture;

//         int baseColorTexCoord{0};
//         int metallicRoughnessTexCoord{0};
//     };

//     struct MaterialData {
//         std::string name;

//         PbrMetallicRoughness pbrMetallicRoughness;

//         glm::vec2 uvScale{1.0f, 1.0f};
//         glm::vec2 uvOffset{0.0f, 0.0f};
//         float uvRotation{0.0f};

//         std::shared_ptr<NtImage> normalTexture;

//         float normalScale{1.0f};

//         AlphaMode alphaMode{AlphaMode::Opaque};
//         float alphaCutoff{0.5f};

//         bool doubleSided{false};

//         int normalTexCoord{0};
//     };

//     NtMaterial(NtDevice &device, const MaterialData &materialData);
//     ~NtMaterial();

//     NtMaterial(const NtMaterial &) = delete;
//     NtMaterial& operator=(const NtMaterial &) = delete;

//     const MaterialData& getMaterialData() const { return materialData; }
//     const std::string& getName() const { return materialData.name; }

//     bool hasBaseColorTexture() const { return materialData.pbrMetallicRoughness.baseColorTexture != nullptr; }
//     bool hasMetallicRoughnessTexture() const { return materialData.pbrMetallicRoughness.metallicRoughnessTexture != nullptr; }
//     bool hasNormalTexture() const { return materialData.normalTexture != nullptr; }

//     VkDescriptorSet getDescriptorSet() const { return descriptorSet; }

//     void updateDescriptorSet(VkDescriptorSetLayout layout, VkDescriptorPool pool);

// private:
//     void createDescriptorSet(VkDescriptorSetLayout layout, VkDescriptorPool pool);

//     NtDevice &ntDevice;
//     MaterialData materialData;
//     VkDescriptorSet descriptorSet{VK_NULL_HANDLE};
// };

class NtMaterial {
public:
    struct Config {
        MaterialType type;
        std::string vertexShader;
        std::string fragmentShader;
        VkCullModeFlags cullMode = VK_CULL_MODE_BACK_BIT;
        bool bAlphaBlending = false;
        bool depthWrite = true;
    };

    NtMaterial(NtDevice& device, const Config& config);

    void bind(VkCommandBuffer commandBuffer);

    VkPipeline getPipeline() const { return pipeline->getPipeline(); }
    VkPipelineLayout getPipelineLayout() const { return pipelineLayout; }
    MaterialType getType() const { return type; }

private:
    NtDevice& device;
    MaterialType type;
    std::unique_ptr<NtPipeline> pipeline;
    VkPipelineLayout pipelineLayout;
};

class NtMaterialLibrary {
public:
    NtMaterialLibrary(NtDevice& device, VkRenderPass renderPass);

    // Get material by type (or create one if it doesn't exit)
    std::shared_ptr<NtMaterial> getMaterial(MaterialType type);

    void registerMaterial(MaterialType type, const NtMaterial::Config& config);

private:
    NtDevice& device;
    VkRenderPass renderPass;
    std::unordered_map<MaterialType, std::shared_ptr<NtMaterial>> materials;

    void createDefaultMaterials();
};

}
