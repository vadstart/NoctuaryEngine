#pragma once

#include "nt_device.hpp"
#include <memory>

namespace nt {

class NtImage {
public:
  NtImage(NtDevice &device);
  ~NtImage();
  
  NtImage(const NtImage &) = delete;
  NtImage& operator=(const NtImage &) = delete;

  static std::unique_ptr<NtImage> createTextureFromFile(NtDevice &device, const std::string &filepath);
  
  VkImageView getImageView() const { return textureImageView; }
  VkSampler getSampler() const { return textureSampler; }

private:
  void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);
  void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);
  void createImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlagBits properties);
  void createTextureImageView();
  void createTextureSampler();

  NtDevice &ntDevice;

  VkImage textureImage;
  VkDeviceMemory textureImageMemory;

  VkImageView textureImageView;
  VkSampler textureSampler;

};

}
