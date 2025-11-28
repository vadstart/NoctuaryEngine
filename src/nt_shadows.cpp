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


}
