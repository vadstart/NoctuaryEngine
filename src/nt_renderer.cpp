#include "nt_renderer.hpp"
#include "vulkan/vulkan_core.h"

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
  clearValues[0].color = {0.0f, 0.0f, 0.0f, 1.0f};
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

void NtRenderer::beginDynamicRendering(VkCommandBuffer commandBuffer) {
    // Transition swap chain image from UNDEFINED/PRESENT_SRC to COLOR_ATTACHMENT_OPTIMAL
    VkImageMemoryBarrier swapChainBarrier{};
    swapChainBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    swapChainBarrier.srcAccessMask = 0;  // No prior access
    swapChainBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    swapChainBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;  // We don't care about previous contents
    swapChainBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    swapChainBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    swapChainBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    swapChainBarrier.image = ntSwapChain->getImage(currentImageIndex);
    swapChainBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    swapChainBarrier.subresourceRange.baseMipLevel = 0;
    swapChainBarrier.subresourceRange.levelCount = 1;
    swapChainBarrier.subresourceRange.baseArrayLayer = 0;
    swapChainBarrier.subresourceRange.layerCount = 1;

    // MSAA Color Attachment -> MSAA Image
    VkRenderingAttachmentInfo colorAttachment{};
    colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    colorAttachment.imageView = ntSwapChain->getColorImageView();
    colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachment.resolveMode = VK_RESOLVE_MODE_AVERAGE_BIT;
    colorAttachment.resolveImageView = ntSwapChain->getImageView(currentImageIndex);
    colorAttachment.resolveImageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.clearValue.color = {{0.01f, 0.01f, 0.01f, 1.0f}};

    // MSAA Depth Attachment
    VkRenderingAttachmentInfo depthAttachment{};
    depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    depthAttachment.imageView = ntSwapChain->getDepthImageView();
    depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.clearValue.depthStencil = {1.0f, 0};

    VkRenderingInfo renderingInfo{};
    renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderingInfo.renderArea = {{0, 0}, ntSwapChain->getSwapChainExtent()};
    renderingInfo.layerCount = 1;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachments = &colorAttachment;
    renderingInfo.pDepthAttachment = &depthAttachment;

    vkCmdBeginRendering(commandBuffer, &renderingInfo);

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

void NtRenderer::endDynamicRendering(VkCommandBuffer commandBuffer) {
  vkCmdEndRendering(commandBuffer);

  // Transition swap chain image from COLOR_ATTACHMENT_OPTIMAL to PRESENT_SRC_KHR
      VkImageMemoryBarrier presentBarrier{};
      presentBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
      presentBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
      presentBarrier.dstAccessMask = 0;  // No subsequent access in this command buffer
      presentBarrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
      presentBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
      presentBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      presentBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      presentBarrier.image = ntSwapChain->getImage(currentImageIndex);
      presentBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      presentBarrier.subresourceRange.baseMipLevel = 0;
      presentBarrier.subresourceRange.levelCount = 1;
      presentBarrier.subresourceRange.baseArrayLayer = 0;
      presentBarrier.subresourceRange.layerCount = 1;

      vkCmdPipelineBarrier(
          commandBuffer,
          VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,  // Wait for color writes to finish
          VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,  // Don't block anything
          0,
          0, nullptr,
          0, nullptr,
          1, &presentBarrier
      );
}


}
