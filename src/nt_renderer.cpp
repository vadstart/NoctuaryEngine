#include "nt_renderer.h"

// Std
#include <array>
#include <cassert>
#include <iostream>
#include <memory>
#include <stdexcept>

namespace nt
{

NtRenderer::NtRenderer(NtWindow &window, NtDevice &device) : ntWindow{window}, ntDevice{device} {
  recreateSwapChain();
  createCommandBuffers();
}
NtRenderer::~NtRenderer() {
  freeCommandBuffers();
}

void NtRenderer::recreateSwapChain()
{
  auto extent = ntWindow.getExtent();

  while (extent.width == 0 || extent.height == 0) {
    extent = ntWindow.getExtent();
    glfwWaitEvents();
  }
  vkDeviceWaitIdle(ntDevice.device());

  if (ntSwapChain == nullptr) {
    ntSwapChain = std::make_unique<NtSwapChain>(ntDevice, extent);
  }
  else {
    std::shared_ptr<NtSwapChain> oldSwapChain = std::move(ntSwapChain);
    ntSwapChain = std::make_unique<NtSwapChain>(ntDevice, extent, oldSwapChain);

    if (!oldSwapChain->compareSwapFormats(*ntSwapChain.get())) {
      throw std::runtime_error("Swap chain image (or depth) format has changed!");
    }
  }
} 

void NtRenderer::createCommandBuffers() {
  commandBuffers.resize(NtSwapChain::MAX_FRAMES_IN_FLIGHT);

  VkCommandBufferAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocInfo.commandPool = ntDevice.getCommandPool();
  allocInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers.size());

  if (vkAllocateCommandBuffers(ntDevice.device(), &allocInfo, commandBuffers.data()) != VK_SUCCESS) {
    throw std::runtime_error("failed to allocate command buffers!");
  }
}

void NtRenderer::freeCommandBuffers() {
  vkFreeCommandBuffers(
    ntDevice.device(),
    ntDevice.getCommandPool(),
    static_cast<uint32_t>(commandBuffers.size()), 
    commandBuffers.data());

  commandBuffers.clear();
}

VkCommandBuffer NtRenderer::beginFrame() {
  assert(!isFrameStarted && "Can't call beginFrame while already in progress");

  auto result = ntSwapChain->acquireNextImage(&currentImageIndex);

  if (result == VK_ERROR_OUT_OF_DATE_KHR) {
    recreateSwapChain();
    return nullptr;
  }
  else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
    throw std::runtime_error("failed to acquire swap chain image!");
  }

  isFrameStarted = true;

  auto commandBuffer = getCurrentCommandBuffer();
  VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
      throw std::runtime_error("failed to begin recording command buffer!");
    }
  return commandBuffer;
}

void NtRenderer::endFrame() {
  assert(isFrameStarted && "Can't call endFrame while frame is not in progress");

  auto commandBuffer = getCurrentCommandBuffer();
  if(vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
    throw std::runtime_error("failed to record command buffer!");
  }

  auto result = ntSwapChain->submitCommandBuffers(&commandBuffer, &currentImageIndex);

  if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
    ntWindow.resetWindowResizedFlag();
    recreateSwapChain();
  } 
  else if (result != VK_SUCCESS) {
    throw std::runtime_error("failed to present swap chain image!");
  }

  isFrameStarted = false;
  currentFrameIndex = (currentFrameIndex + 1) % NtSwapChain::MAX_FRAMES_IN_FLIGHT;
}

void NtRenderer::beginSwapChainRenderPass(VkCommandBuffer commandBuffer) {
  assert(isFrameStarted && "Can't call beginSwapChainRenderPass if frame is not in progress");
  assert(commandBuffer == getCurrentCommandBuffer() && "Can't begin render pass on command buffer from a different frame");

  VkRenderPassBeginInfo renderPassInfo{};
  renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  renderPassInfo.renderPass = ntSwapChain->getRenderPass();
  renderPassInfo.framebuffer = ntSwapChain->getFrameBuffer(currentImageIndex);

  renderPassInfo.renderArea.offset = {0, 0};
  renderPassInfo.renderArea.extent = ntSwapChain->getSwapChainExtent();

  std::array<VkClearValue, 2> clearValues{};
  clearValues[0].color = {0.02f, 0.02f, 0.02f, 1.0f};
  clearValues[1].depthStencil = {1.0f, 0};
  renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
  renderPassInfo.pClearValues = clearValues.data();

  vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

  VkViewport viewport{};
  viewport.x = 0.0f;
  viewport.y = 0.0f;
  viewport.width = static_cast<float>(ntSwapChain->getSwapChainExtent().width);
  viewport.height = static_cast<float>(ntSwapChain->getSwapChainExtent().height);
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;
  VkRect2D scissor{{0, 0}, ntSwapChain->getSwapChainExtent()};
  vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
  vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
}

void NtRenderer::endSwapChainRenderPass(VkCommandBuffer commandBuffer) {
  assert(isFrameStarted && "Can't call endSwapChainRenderPass if frame is not in progress");
  assert(commandBuffer == getCurrentCommandBuffer() && "Can't end render pass on command buffer from a different frame");
  
  vkCmdEndRenderPass(commandBuffer);
}


}
