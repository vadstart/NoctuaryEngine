#include "nt_material.hpp"
#include <stdexcept>
#include <array>

namespace nt {

NtMaterial::NtMaterial(NtDevice &device, const MaterialData &materialData)
    : ntDevice{device}, materialData{materialData} {
}

NtMaterial::~NtMaterial() {
    // Descriptor sets are automatically freed when the pool is destroyed
}

void NtMaterial::updateDescriptorSet(VkDescriptorSetLayout layout, VkDescriptorPool pool) {
    createDescriptorSet(layout, pool);
}

void NtMaterial::createDescriptorSet(VkDescriptorSetLayout layout, VkDescriptorPool pool) {
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = pool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &layout;

    if (vkAllocateDescriptorSets(ntDevice.device(), &allocInfo, &descriptorSet) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate descriptor set for material");
    }

    std::vector<VkWriteDescriptorSet> descriptorWrites;
    std::vector<VkDescriptorImageInfo> imageInfos;

    // Reserve space for potential descriptors
    // TODO: Reserve only the needed amount after hasTexture() checks and not always 5?
    imageInfos.reserve(3); // Base color, metallic-roughness, normal. (occlusion, emissive)

    // Base color texture (binding 0)
    if (hasBaseColorTexture()) {
        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView = materialData.pbrMetallicRoughness.baseColorTexture->getImageView();
        imageInfo.sampler = materialData.pbrMetallicRoughness.baseColorTexture->getSampler();
        imageInfos.push_back(imageInfo);

        VkWriteDescriptorSet descriptorWrite{};
        descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrite.dstSet = descriptorSet;
        descriptorWrite.dstBinding = 0;
        descriptorWrite.dstArrayElement = 0;
        descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrite.descriptorCount = 1;
        descriptorWrite.pImageInfo = &imageInfos.back();
        descriptorWrites.push_back(descriptorWrite);
    }

    // Normal texture (binding 1)
    if (hasNormalTexture()) {
        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView = materialData.normalTexture->getImageView();
        imageInfo.sampler = materialData.normalTexture->getSampler();
        imageInfos.push_back(imageInfo);

        VkWriteDescriptorSet descriptorWrite{};
        descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrite.dstSet = descriptorSet;
        descriptorWrite.dstBinding = 1;
        descriptorWrite.dstArrayElement = 0;
        descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrite.descriptorCount = 1;
        descriptorWrite.pImageInfo = &imageInfos.back();
        descriptorWrites.push_back(descriptorWrite);
    }

    // Metallic-roughness texture (binding 2)
    if (hasMetallicRoughnessTexture()) {
        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView = materialData.pbrMetallicRoughness.metallicRoughnessTexture->getImageView();
        imageInfo.sampler = materialData.pbrMetallicRoughness.metallicRoughnessTexture->getSampler();
        imageInfos.push_back(imageInfo);

        VkWriteDescriptorSet descriptorWrite{};
        descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrite.dstSet = descriptorSet;
        descriptorWrite.dstBinding = 2;
        descriptorWrite.dstArrayElement = 0;
        descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrite.descriptorCount = 1;
        descriptorWrite.pImageInfo = &imageInfos.back();
        descriptorWrites.push_back(descriptorWrite);
    }

    // Occlusion texture (binding 3)
    // if (hasOcclusionTexture()) {
    //     VkDescriptorImageInfo imageInfo{};
    //     imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    //     imageInfo.imageView = materialData.occlusionTexture->getImageView();
    //     imageInfo.sampler = materialData.occlusionTexture->getSampler();
    //     imageInfos.push_back(imageInfo);

    //     VkWriteDescriptorSet descriptorWrite{};
    //     descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    //     descriptorWrite.dstSet = descriptorSet;
    //     descriptorWrite.dstBinding = 3;
    //     descriptorWrite.dstArrayElement = 0;
    //     descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    //     descriptorWrite.descriptorCount = 1;
    //     descriptorWrite.pImageInfo = &imageInfos.back();
    //     descriptorWrites.push_back(descriptorWrite);
    // }

    // Emissive texture (binding 4)
    // if (hasEmissiveTexture()) {
    //     VkDescriptorImageInfo imageInfo{};
    //     imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    //     imageInfo.imageView = materialData.emissiveTexture->getImageView();
    //     imageInfo.sampler = materialData.emissiveTexture->getSampler();
    //     imageInfos.push_back(imageInfo);

    //     VkWriteDescriptorSet descriptorWrite{};
    //     descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    //     descriptorWrite.dstSet = descriptorSet;
    //     descriptorWrite.dstBinding = 4;
    //     descriptorWrite.dstArrayElement = 0;
    //     descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    //     descriptorWrite.descriptorCount = 1;
    //     descriptorWrite.pImageInfo = &imageInfos.back();
    //     descriptorWrites.push_back(descriptorWrite);
    // }

    if (!descriptorWrites.empty()) {
        vkUpdateDescriptorSets(ntDevice.device(), static_cast<uint32_t>(descriptorWrites.size()),
                              descriptorWrites.data(), 0, nullptr);
    }
}

}
