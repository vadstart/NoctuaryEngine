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
  

private:
  void createImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlagBits properties);

  NtDevice &ntDevice;

  VkImage textureImage;
  VkDeviceMemory textureImageMemory;

};

}
