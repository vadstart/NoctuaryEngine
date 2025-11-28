#pragma once

#include "nt_shadows.hpp"
#include "nt_window.hpp"
#include "nt_device.hpp"
#include "nt_swap_chain.hpp"
#include "vulkan/vulkan_core.h"

#include <cassert>
#include <memory>

using std::vector;

namespace nt
{
	class NtRenderer
	{
	public:
    NtRenderer(NtWindow &window, NtDevice &device);
    ~NtRenderer();

    NtRenderer(const NtRenderer&) = delete;
		NtRenderer& operator=(const NtRenderer&) = delete;

    NtSwapChain* getSwapChain() const { return ntSwapChain.get(); }
    size_t getSwapChainImageCount() { return ntSwapChain->imageCount(); }
    float getAspectRatio() const { return ntSwapChain->extentAspectRatio(); }
    bool isFrameInProgress() const { return isFrameStarted; }

    VkCommandBuffer getCurrentCommandBuffer() const {
      assert(isFrameStarted && "Cannot get command buffer when frame is not in progress");
      return commandBuffers[currentFrameIndex];
    }

    int getFrameIndex() const {
      assert(isFrameStarted && "Cannot get frame index when frame is not in progress");

      return currentFrameIndex;
  }

    VkCommandBuffer beginFrame();
    void endFrame();

    void beginShadowRendering(VkCommandBuffer commandBuffer, NtShadowMap *shadowMap);
    void endShadowRendering(VkCommandBuffer commandBuffer, NtShadowMap *shadowMap);

    void beginMainRendering(VkCommandBuffer commandBuffer);
    void endMainRendering(VkCommandBuffer commandBuffer);

	private:
    void createCommandBuffers();
    void freeCommandBuffers();
    void recreateSwapChain();

    NtWindow &ntWindow;
    NtDevice &ntDevice;
    std::unique_ptr<NtSwapChain> ntSwapChain;
    std::vector<VkCommandBuffer> commandBuffers;

    uint32_t currentImageIndex;
    int currentFrameIndex{0}; // [0, maxFramesInFlight]
    bool isFrameStarted{false};
	};
}
