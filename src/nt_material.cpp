#include "nt_material.hpp"
#include "nt_device.hpp"
#include "nt_log.hpp"
#include <stdexcept>
#include <array>
#include <vulkan/vulkan_core.h>

namespace nt {

// NtMaterial::NtMaterial(NtDevice &device, const MaterialData &materialData)
//     : ntDevice{device}, materialData{materialData} {
// }

// NtMaterial::~NtMaterial() {
//     // Descriptor sets are automatically freed when the pool is destroyed
// }

// void NtMaterial::updateDescriptorSet(VkDescriptorSetLayout layout, VkDescriptorPool pool) {
//     createDescriptorSet(layout, pool);
// }

// void NtMaterial::createDescriptorSet(VkDescriptorSetLayout layout, VkDescriptorPool pool) {
//     VkDescriptorSetAllocateInfo allocInfo{};
//     allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
//     allocInfo.descriptorPool = pool;
//     allocInfo.descriptorSetCount = 1;
//     allocInfo.pSetLayouts = &layout;

//     if (vkAllocateDescriptorSets(ntDevice.device(), &allocInfo, &descriptorSet) != VK_SUCCESS) {
//         throw std::runtime_error("Failed to allocate descriptor set for material");
//     }

//     std::vector<VkWriteDescriptorSet> descriptorWrites;
//     std::vector<VkDescriptorImageInfo> imageInfos;

//     // Reserve space for potential descriptors
//     // TODO: Reserve only the needed amount after hasTexture() checks and not always 5?
//     imageInfos.reserve(3); // Base color, metallic-roughness, normal. (occlusion, emissive)

//     // Base color texture (binding 0)
//     if (hasBaseColorTexture()) {
//         VkDescriptorImageInfo imageInfo{};
//         imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
//         imageInfo.imageView = materialData.pbrMetallicRoughness.baseColorTexture->getImageView();
//         imageInfo.sampler = materialData.pbrMetallicRoughness.baseColorTexture->getSampler();
//         imageInfos.push_back(imageInfo);

//         VkWriteDescriptorSet descriptorWrite{};
//         descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
//         descriptorWrite.dstSet = descriptorSet;
//         descriptorWrite.dstBinding = 0;
//         descriptorWrite.dstArrayElement = 0;
//         descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
//         descriptorWrite.descriptorCount = 1;
//         descriptorWrite.pImageInfo = &imageInfos.back();
//         descriptorWrites.push_back(descriptorWrite);
//     }

//     // Normal texture (binding 1)
//     if (hasNormalTexture()) {
//         VkDescriptorImageInfo imageInfo{};
//         imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
//         imageInfo.imageView = materialData.normalTexture->getImageView();
//         imageInfo.sampler = materialData.normalTexture->getSampler();
//         imageInfos.push_back(imageInfo);

//         VkWriteDescriptorSet descriptorWrite{};
//         descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
//         descriptorWrite.dstSet = descriptorSet;
//         descriptorWrite.dstBinding = 1;
//         descriptorWrite.dstArrayElement = 0;
//         descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
//         descriptorWrite.descriptorCount = 1;
//         descriptorWrite.pImageInfo = &imageInfos.back();
//         descriptorWrites.push_back(descriptorWrite);
//     }

//     // Metallic-roughness texture (binding 2)
//     if (hasMetallicRoughnessTexture()) {
//         VkDescriptorImageInfo imageInfo{};
//         imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
//         imageInfo.imageView = materialData.pbrMetallicRoughness.metallicRoughnessTexture->getImageView();
//         imageInfo.sampler = materialData.pbrMetallicRoughness.metallicRoughnessTexture->getSampler();
//         imageInfos.push_back(imageInfo);

//         VkWriteDescriptorSet descriptorWrite{};
//         descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
//         descriptorWrite.dstSet = descriptorSet;
//         descriptorWrite.dstBinding = 2;
//         descriptorWrite.dstArrayElement = 0;
//         descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
//         descriptorWrite.descriptorCount = 1;
//         descriptorWrite.pImageInfo = &imageInfos.back();
//         descriptorWrites.push_back(descriptorWrite);
//     }

//     if (!descriptorWrites.empty()) {
//         vkUpdateDescriptorSets(ntDevice.device(), static_cast<uint32_t>(descriptorWrites.size()),
//                               descriptorWrites.data(), 0, nullptr);
//     }
// }

NtMaterialLibrary::NtMaterialLibrary(NtDevice& device, VkRenderPass renderPass)
    : device{device}, renderPass{renderPass} {
        createDefaultMaterials();
    }

void NtMaterialLibrary::createDefaultMaterials() {
    // Main PBR
    {
        NtMaterial::Config config;
        config.type = MaterialType::PBR;
        config.vertexShader = "shaders/pbr.vert.spv";
        config.fragmentShader = "shaders/pbr.frag.spv";
        config.cullMode = VK_CULL_MODE_BACK_BIT;
        config.bAlphaBlending = false;

        materials[MaterialType::PBR] = std::make_shared<NtMaterial>(device, config);
    }

    // NPR (Toon Shading)
    {
        NtMaterial::Config config;
        config.type = MaterialType::NPR;
        config.vertexShader = "shaders/npr.vert.spv";
        config.fragmentShader = "shaders/npr.frag.spv";
        config.cullMode = VK_CULL_MODE_BACK_BIT;
        config.bAlphaBlending = false;

        materials[MaterialType::NPR] = std::make_shared<NtMaterial>(device, config);
    }

    // UNLIT (GUI)
    // {
    //     NtMaterial::Config config;
    //     config.type = MaterialType::UNLIT;
    //     config.vertexShader = "shaders/unlit.vert.spv";
    //     config.fragmentShader = "shaders/unlit.frag.spv";
    //     config.cullMode = VK_CULL_MODE_BACK_BIT;
    //     config.bAlphaBlending = false;

    //     materials[MaterialType::UNLIT] = std::make_shared<NtMaterial>(device, config);
    // }

    // Scrolling UV
    {
        NtMaterial::Config config;
        config.type = MaterialType::SCROLLING_UV;
        config.vertexShader = "shaders/scroll_uv.vert.spv";
        config.fragmentShader = "shaders/scroll_uv.frag.spv";
        config.cullMode = VK_CULL_MODE_BACK_BIT;
        config.bAlphaBlending = true;

        materials[MaterialType::SCROLLING_UV] = std::make_shared<NtMaterial>(device, config);
    }
}

std::shared_ptr<NtMaterial> NtMaterialLibrary::getMaterial(MaterialType type)
{
    auto it = materials.find(type);
    if (it != materials.end()) {
        return it->second;
    }

    NT_LOG_ERROR(LogAssets, "Material type not found!");
    throw std::runtime_error("Material type not found!");
}

}
