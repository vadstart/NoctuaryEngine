#pragma once

#include "nt_device.hpp"
#include "nt_image.hpp"
#include <glm/glm.hpp>
#include <memory>
#include <string>
#include <vector>

namespace nt {

class NtMaterial {
public:
    enum class AlphaMode {
        OPAQUE,
        MASK,
        BLEND
    };

    struct PbrMetallicRoughness {
        glm::vec4 baseColorFactor{1.0f, 1.0f, 1.0f, 1.0f};
        float metallicFactor{1.0f};
        float roughnessFactor{1.0f};

        std::shared_ptr<NtImage> baseColorTexture;
        std::shared_ptr<NtImage> metallicRoughnessTexture;

        int baseColorTexCoord{0};
        int metallicRoughnessTexCoord{0};
    };

    struct MaterialData {
        std::string name;

        PbrMetallicRoughness pbrMetallicRoughness;

        std::shared_ptr<NtImage> normalTexture;
        std::shared_ptr<NtImage> occlusionTexture;
        std::shared_ptr<NtImage> emissiveTexture;

        glm::vec3 emissiveFactor{0.0f, 0.0f, 0.0f};
        float normalScale{1.0f};
        float occlusionStrength{1.0f};

        AlphaMode alphaMode{AlphaMode::OPAQUE};
        float alphaCutoff{0.5f};

        bool doubleSided{false};

        int normalTexCoord{0};
        int occlusionTexCoord{0};
        int emissiveTexCoord{0};
    };

    NtMaterial(NtDevice &device, const MaterialData &materialData);
    ~NtMaterial();

    NtMaterial(const NtMaterial &) = delete;
    NtMaterial& operator=(const NtMaterial &) = delete;

    const MaterialData& getMaterialData() const { return materialData; }
    const std::string& getName() const { return materialData.name; }

    bool hasBaseColorTexture() const { return materialData.pbrMetallicRoughness.baseColorTexture != nullptr; }
    bool hasMetallicRoughnessTexture() const { return materialData.pbrMetallicRoughness.metallicRoughnessTexture != nullptr; }
    bool hasNormalTexture() const { return materialData.normalTexture != nullptr; }
    bool hasOcclusionTexture() const { return materialData.occlusionTexture != nullptr; }
    bool hasEmissiveTexture() const { return materialData.emissiveTexture != nullptr; }

    VkDescriptorSet getDescriptorSet() const { return descriptorSet; }

    void updateDescriptorSet(VkDescriptorSetLayout layout, VkDescriptorPool pool);

private:
    void createDescriptorSet(VkDescriptorSetLayout layout, VkDescriptorPool pool);

    NtDevice &ntDevice;
    MaterialData materialData;
    VkDescriptorSet descriptorSet{VK_NULL_HANDLE};
};

}
