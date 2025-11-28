#pragma once

#include "nt_device.hpp"

// vulkan headers
#include <memory>
#include <vulkan/vulkan.h>

// std lib headers
#include <string>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace nt {

class NtSwapChain {
 public:
  static constexpr int MAX_FRAMES_IN_FLIGHT = 2;

  NtSwapChain(NtDevice &deviceRef, VkExtent2D windowExtent);
  NtSwapChain(NtDevice &deviceRef, VkExtent2D windowExtent, std::shared_ptr<NtSwapChain> previous);
  ~NtSwapChain();

  NtSwapChain(const NtSwapChain &) = delete;
  NtSwapChain &operator=(const NtSwapChain &) = delete;

  VkImageView getImageView(int index) { return swapChainImageViews[index]; }
  VkImage getImage(int index) { return swapChainImages[index]; }
  VkImageView getColorImageView() { return colorImageView; }
  VkImage getColorImage() { return colorImage; }
  VkImageView getDepthImageView() { return depthImageView; }
  VkImage getDepthImage() { return depthImage; }
  size_t imageCount() { return swapChainImages.size(); }
  VkFormat getSwapChainImageFormat() { return swapChainImageFormat; }
  VkFormat getSwapChainDepthFormat() { return swapChainDepthFormat; }
  VkExtent2D getSwapChainExtent() { return swapChainExtent; }
  uint32_t width() { return swapChainExtent.width; }
  uint32_t height() { return swapChainExtent.height; }

  float extentAspectRatio() {
    return static_cast<float>(swapChainExtent.width) / static_cast<float>(swapChainExtent.height);
  }
  VkFormat findDepthFormat();

  VkResult acquireNextImage(uint32_t *imageIndex);
  VkResult submitCommandBuffers(const VkCommandBuffer *buffers, uint32_t *imageIndex);

  bool compareSwapFormats(const NtSwapChain& swapChain) const {
    return swapChain.swapChainDepthFormat == swapChainDepthFormat &&
           swapChain.swapChainImageFormat == swapChainImageFormat;
  }

 private:
  void init();
  void createSwapChain();
  void createImageViews();
  void createColorResources();
  void createDepthResources();
  void createSyncObjects();

  // Helper functions
  VkSurfaceFormatKHR chooseSwapSurfaceFormat(
      const std::vector<VkSurfaceFormatKHR> &availableFormats);
  VkPresentModeKHR chooseSwapPresentMode(
      const std::vector<VkPresentModeKHR> &availablePresentModes);
  VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR &capabilities);

  VkFormat swapChainImageFormat;
  VkFormat swapChainDepthFormat;
  VkExtent2D swapChainExtent;

  VkImage colorImage;
  VkDeviceMemory colorImageMemory;
  VkImageView colorImageView;

  VkImage depthImage;
  VkDeviceMemory depthImageMemory;
  VkImageView depthImageView;

  std::vector<VkImage> swapChainImages;
  std::vector<VkImageView> swapChainImageViews;

  NtDevice &device;
  VkExtent2D windowExtent;

  VkSwapchainKHR swapChain;
  std::shared_ptr<NtSwapChain> oldSwapChain;

  std::vector<VkSemaphore> imageAvailableSemaphores;
  std::vector<VkSemaphore> renderFinishedSemaphores;
  std::vector<VkFence> inFlightFences;
  std::vector<VkFence> imagesInFlight;
  size_t currentFrame = 0;
};

}
