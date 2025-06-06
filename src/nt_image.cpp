#include "nt_image.hpp"
#include <iostream>
#include <stdexcept>
#include <vulkan/vulkan_core.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"

namespace nt {

NtImage::NtImage(NtDevice &device) : ntDevice{device} {}

NtImage::~NtImage() {
  if (textureSampler != VK_NULL_HANDLE)
    vkDestroySampler(ntDevice.device(), textureSampler, nullptr);
  if (textureImageView != VK_NULL_HANDLE)
    vkDestroyImageView(ntDevice.device(), textureImageView, nullptr);
  if (textureImage != VK_NULL_HANDLE)
    vkDestroyImage(ntDevice.device(), textureImage, nullptr);
  if (textureImageMemory != VK_NULL_HANDLE)
    vkFreeMemory(ntDevice.device(), textureImageMemory, nullptr);
}

void NtImage::transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout) {
  VkCommandBuffer commandBuffer = ntDevice.beginSingleTimeCommands();

  VkImageMemoryBarrier barrier{};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.oldLayout = oldLayout;
  barrier.newLayout = newLayout;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image = image;
  barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  barrier.subresourceRange.baseMipLevel = 0;
  barrier.subresourceRange.levelCount = 1;
  barrier.subresourceRange.baseArrayLayer = 0;
  barrier.subresourceRange.layerCount = 1;
  
  VkPipelineStageFlags sourceStage;
VkPipelineStageFlags destinationStage;

if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

    sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
} else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
} else {
    throw std::invalid_argument("unsupported layout transition!");
}
  
  vkCmdPipelineBarrier(
    commandBuffer,
    sourceStage, destinationStage,
    0,
    0, nullptr,
    0, nullptr,
    1, &barrier
  );

  ntDevice.endSingleTimeCommands(commandBuffer);
}

void NtImage::copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height) {
  VkCommandBuffer commandBuffer = ntDevice.beginSingleTimeCommands();

  VkBufferImageCopy region{};
  region.bufferOffset = 0;
  region.bufferRowLength = 0;
  region.bufferImageHeight = 0;

  region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  region.imageSubresource.mipLevel = 0;
  region.imageSubresource.baseArrayLayer = 0;
  region.imageSubresource.layerCount = 1;

  region.imageOffset = {0, 0, 0};
  region.imageExtent = {
      width,
      height,
      1
  };

  vkCmdCopyBufferToImage(
    commandBuffer,
    buffer,
    image,
    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
    1,
    &region
  );
    
  ntDevice.endSingleTimeCommands(commandBuffer);
}

std::unique_ptr<NtImage> NtImage::createTextureFromFile(NtDevice &device, const std::string &filepath) {
  int texWidth, texHeight, texChannels;
  stbi_set_flip_vertically_on_load(true);
  stbi_uc* pixels = stbi_load(filepath.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
  VkDeviceSize imageSize = texWidth * texHeight * 4;

  if (!pixels) {
    throw std::runtime_error("failed to load texture image!");
  }

  VkBuffer stagingBuffer;
  VkDeviceMemory stagingBuffMemory;
  device.createBuffer (imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBuffMemory);

  void* data;
  vkMapMemory(device.device(), stagingBuffMemory, 0, imageSize, 0, &data);
  memcpy(data, pixels, static_cast<size_t>(imageSize));
  vkUnmapMemory(device.device(), stagingBuffMemory);

  stbi_image_free(pixels);

  std::unique_ptr<NtImage> image = std::make_unique<NtImage>(device);

  image->createImage(texWidth, texHeight, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

  image->transitionImageLayout(image->textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
  image->copyBufferToImage(stagingBuffer, image->textureImage, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));
  image->transitionImageLayout(image->textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

  vkDestroyBuffer(device.device(), stagingBuffer, nullptr);
  vkFreeMemory(device.device(), stagingBuffMemory, nullptr);

  image->createTextureImageView();
  image->createTextureSampler();

  return image;
}

void NtImage::createImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlagBits properties) {
  VkImageCreateInfo imageInfo{};
  imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  imageInfo.imageType = VK_IMAGE_TYPE_2D;
  imageInfo.extent.width = width;
  imageInfo.extent.height = height;
  imageInfo.extent.depth = 1;
  imageInfo.mipLevels = 1;
  imageInfo.arrayLayers = 1;
  imageInfo.format = format;
  imageInfo.tiling = tiling;
  imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  imageInfo.usage = usage;
  imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
  imageInfo.flags = 0;

  if (vkCreateImage(ntDevice.device(), &imageInfo, nullptr, &textureImage) != VK_SUCCESS) {
    throw std::runtime_error("failed to create image");
  }

  VkMemoryRequirements memRequirements;
  vkGetImageMemoryRequirements(ntDevice.device(), textureImage, &memRequirements);

  VkMemoryAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  allocInfo.allocationSize = memRequirements.size;
  allocInfo.memoryTypeIndex = ntDevice.findMemoryType(memRequirements.memoryTypeBits, properties);

  if (vkAllocateMemory(ntDevice.device(), &allocInfo, nullptr, &textureImageMemory) != VK_SUCCESS) {
    throw std::runtime_error("failed to allocate image memory");
  }

  vkBindImageMemory(ntDevice.device(), textureImage, textureImageMemory, 0);

}

void NtImage::createTextureImageView() {
  VkImageViewCreateInfo viewInfo{};
  viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  viewInfo.image = textureImage;
  viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
  viewInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
  viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  viewInfo.subresourceRange.baseMipLevel = 0;
  viewInfo.subresourceRange.levelCount = 1;
  viewInfo.subresourceRange.baseArrayLayer = 0;
  viewInfo.subresourceRange.layerCount = 1;

  if (vkCreateImageView(ntDevice.device(), &viewInfo, nullptr, &textureImageView) != VK_SUCCESS) {
    throw std::runtime_error("failed to create texture image view!");
  }
}

void NtImage::createTextureSampler() {
  VkSamplerCreateInfo samplerInfo{};
  samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  samplerInfo.magFilter = VK_FILTER_LINEAR;
  samplerInfo.minFilter = VK_FILTER_LINEAR;
  samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  samplerInfo.anisotropyEnable = VK_TRUE;
  samplerInfo.maxAnisotropy = ntDevice.properties.limits.maxSamplerAnisotropy;
  samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
  samplerInfo.unnormalizedCoordinates = VK_FALSE;
  samplerInfo.compareEnable = VK_FALSE;
  samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
  samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
  samplerInfo.mipLodBias = 0.0f;
  samplerInfo.minLod = 0.0f;
  samplerInfo.maxLod = 0.0f;

  if (vkCreateSampler(ntDevice.device(), &samplerInfo, nullptr, &textureSampler) != VK_SUCCESS) {
    throw std::runtime_error("failed to create texture sampler!");
  }

}


}
