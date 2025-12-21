#pragma once

#include "nt_device.hpp"
#include "nt_image.hpp"
#include "nt_pipeline.hpp"
#include "nt_swap_chain.hpp"
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
    SCROLLING_UV,
    SHADOW_MAP
};

// Material Data loaded from the model file
struct PbrMetallicRoughness {
    glm::vec4 baseColorFactor{1.0f, 1.0f, 1.0f, 1.0f};
    float metallicFactor{1.0f};
    float roughnessFactor{1.0f};

    std::shared_ptr<NtImage> baseColorTexture;
    std::shared_ptr<NtImage> metallicRoughnessTexture;

    int baseColorTexCoord{0};
    int metallicRoughnessTexCoord{0};
};

enum class AlphaMode {
    Opaque,
    Mask,
    Blend
};

struct MaterialData {
    std::string name;

    PbrMetallicRoughness pbrMetallicRoughness;

    glm::vec2 uvScale{1.0f, 1.0f};
    glm::vec2 uvOffset{0.0f, 0.0f};
    float uvRotation{0.0f};

    std::shared_ptr<NtImage> normalTexture;

    float normalScale{1.0f};

    AlphaMode alphaMode{AlphaMode::Opaque};
    float alphaCutoff{0.5f};

    bool doubleSided{false};

    int normalTexCoord{0};
};

// Material shader
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

    NtMaterial(NtDevice& device, const Config& config, VkDescriptorSetLayout globalSetLayout,
    VkDescriptorSetLayout modelSetLayout,
    VkDescriptorSetLayout boneSetLayout,
    NtSwapChain& swapChain);

    ~NtMaterial();

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
    NtMaterialLibrary(NtDevice& device,
                      VkDescriptorSetLayout globalSetLayout,
                      VkDescriptorSetLayout modelSetLayout,
                      VkDescriptorSetLayout boneSetLayout,
                      NtSwapChain& swapChain);

    // Get material by type (or create one if it doesn't exit)
    std::shared_ptr<NtMaterial> getMaterial(MaterialType type);
    void registerMaterial(MaterialType type, const NtMaterial::Config& config);

private:
    NtDevice& device;
    VkDescriptorSetLayout globalSetLayout;
    VkDescriptorSetLayout modelSetLayout;
    VkDescriptorSetLayout boneSetLayout;
    NtSwapChain& swapChain;
    std::unordered_map<MaterialType, std::shared_ptr<NtMaterial>> materials;

    void createDefaultMaterials();
};

}
