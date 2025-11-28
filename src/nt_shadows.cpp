#include "nt_shadows.hpp"

#include <stdexcept>
#include <vulkan/vulkan_core.h>

namespace nt {

NtShadowMap::NtShadowMap(NtDevice& device, uint32_t width, uint32_t height)
    : ntDevice{device}, width{width}, height{height} {
        createShadowImage();
        createShadowImageView();
        createShadowSampler();
        createShadowDebugSampler();
}

NtShadowMap::~NtShadowMap() {
    vkDestroySampler(ntDevice.device(), shadowSampler, nullptr);
    vkDestroySampler(ntDevice.device(), shadowDebugSampler, nullptr);
    vkDestroyImageView(ntDevice.device(), shadowImageView, nullptr);
    vkDestroyImage(ntDevice.device(), shadowImage, nullptr);
    vkFreeMemory(ntDevice.device(), shadowImageMemory, nullptr);
}

void NtShadowMap::createShadowImage() {
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = VK_FORMAT_D32_SFLOAT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
        VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;

    if (vkCreateImage(ntDevice.device(), &imageInfo, nullptr, &shadowImage) != VK_SUCCESS) {
      throw std::runtime_error("[ERROR]failed to create shadow image");
    }

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(ntDevice.device(), shadowImage, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = ntDevice.findMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (vkAllocateMemory(ntDevice.device(), &allocInfo, nullptr, &shadowImageMemory) != VK_SUCCESS) {
      throw std::runtime_error("[ERROR] failed to allocate shadow image memory");
    }

    vkBindImageMemory(ntDevice.device(), shadowImage, shadowImageMemory, 0);
}

void NtShadowMap::createShadowImageView() {
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = shadowImage;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = VK_FORMAT_D32_SFLOAT;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    if (vkCreateImageView(ntDevice.device(), &viewInfo, nullptr, &shadowImageView) != VK_SUCCESS) {
      throw std::runtime_error("[ERROR] failed to create shadow image view");
    }
}

void NtShadowMap::createShadowSampler() {
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_TRUE;
    samplerInfo.compareOp = VK_COMPARE_OP_LESS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = 1.0f;

    if (vkCreateSampler(ntDevice.device(), &samplerInfo, nullptr, &shadowSampler) != VK_SUCCESS) {
      throw std::runtime_error("[ERROR] failed to create shadow sampler");
    }
}

void NtShadowMap::createShadowDebugSampler() {
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE; // No depth comparison
    samplerInfo.compareOp = VK_COMPARE_OP_NEVER;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = 1.0f;

    if (vkCreateSampler(ntDevice.device(), &samplerInfo, nullptr, &shadowDebugSampler) != VK_SUCCESS) {
      throw std::runtime_error("[ERROR] failed to create shadow debug sampler");
    }
}

NtShadowCubeMap::NtShadowCubeMap(NtDevice& device, uint32_t size)
    : ntDevice{device}, size{size} {
    createShadowCubeImage();
    createShadowCubeImageViews();
    createShadowCubeSamplers();
}

NtShadowCubeMap::~NtShadowCubeMap() {
    vkDestroySampler(ntDevice.device(), shadowCubeSampler, nullptr);
    vkDestroySampler(ntDevice.device(), shadowCubeDebugSampler, nullptr);

    for (auto& view : faceImageViews) {
        vkDestroyImageView(ntDevice.device(), view, nullptr);
    }
    vkDestroyImageView(ntDevice.device(), shadowCubeImageView, nullptr);
    vkDestroyImage(ntDevice.device(), shadowCubeImage, nullptr);
    vkFreeMemory(ntDevice.device(), shadowCubeImageMemory, nullptr);
}

void NtShadowCubeMap::createShadowCubeImage() {
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = size;
    imageInfo.extent.height = size;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 6; // 6 faces for cubemap
    imageInfo.format = VK_FORMAT_D32_SFLOAT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT; // Critical for cubemap!

    if (vkCreateImage(ntDevice.device(), &imageInfo, nullptr, &shadowCubeImage) != VK_SUCCESS) {
        throw std::runtime_error("[ERROR] failed to create shadow cube image");
    }

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(ntDevice.device(), shadowCubeImage, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = ntDevice.findMemoryType(
        memRequirements.memoryTypeBits,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    );

    if (vkAllocateMemory(ntDevice.device(), &allocInfo, nullptr, &shadowCubeImageMemory) != VK_SUCCESS) {
        throw std::runtime_error("[ERROR] failed to allocate shadow cube image memory");
    }

    vkBindImageMemory(ntDevice.device(), shadowCubeImage, shadowCubeImageMemory, 0);
}

void NtShadowCubeMap::createShadowCubeImageViews() {
    // Create view for entire cubemap (used for sampling)
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = shadowCubeImage;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
    viewInfo.format = VK_FORMAT_D32_SFLOAT;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 6; // All 6 faces

    if (vkCreateImageView(ntDevice.device(), &viewInfo, nullptr, &shadowCubeImageView) != VK_SUCCESS) {
        throw std::runtime_error("[ERROR] failed to create shadow cube image view");
    }

    // Create individual face views (used for rendering)
    for (uint32_t face = 0; face < 6; ++face) {
        VkImageViewCreateInfo faceViewInfo{};
        faceViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        faceViewInfo.image = shadowCubeImage;
        faceViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D; // 2D view of single face
        faceViewInfo.format = VK_FORMAT_D32_SFLOAT;
        faceViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        faceViewInfo.subresourceRange.baseMipLevel = 0;
        faceViewInfo.subresourceRange.levelCount = 1;
        faceViewInfo.subresourceRange.baseArrayLayer = face; // Specific face
        faceViewInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(ntDevice.device(), &faceViewInfo, nullptr, &faceImageViews[face]) != VK_SUCCESS) {
            throw std::runtime_error("[ERROR] failed to create shadow cube face image view");
        }
    }
}

void NtShadowCubeMap::createShadowCubeSamplers() {
    // Shadow sampler (with comparison)
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_TRUE;
    samplerInfo.compareOp = VK_COMPARE_OP_LESS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

    if (vkCreateSampler(ntDevice.device(), &samplerInfo, nullptr, &shadowCubeSampler) != VK_SUCCESS) {
        throw std::runtime_error("[ERROR] failed to create shadow cube sampler");
    }

    // Debug sampler (no comparison)
    samplerInfo.compareEnable = VK_FALSE;
    if (vkCreateSampler(ntDevice.device(), &samplerInfo, nullptr, &shadowCubeDebugSampler) != VK_SUCCESS) {
        throw std::runtime_error("[ERROR] failed to create shadow cube debug sampler");
    }
}

}
