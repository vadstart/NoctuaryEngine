#pragma once

#include "nt_window.h"
#include "nt_device.h"
#include "nt_swap_chain.h"
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

    VkRenderPass getSwapChainRenderPass() const { return ntSwapChain->getRenderPass(); }
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
    void beginSwapChainRenderPass(VkCommandBuffer commandBuffer);
    void endSwapChainRenderPass(VkCommandBuffer commandBuffer);

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
