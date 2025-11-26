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
    // Prepare barriers for all images that need layout transitions
    // We transition from UNDEFINED every frame since we're clearing/resolving and don't need previous contents
    std::array<VkImageMemoryBarrier, 3> barriers{};

    // Barrier 0: Transition swap chain resolve target from UNDEFINED to COLOR_ATTACHMENT_OPTIMAL
    // Using UNDEFINED is correct here since we're resolving into it (don't care about previous contents)
    barriers[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barriers[0].srcAccessMask = 0;
    barriers[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    barriers[0].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barriers[0].newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    barriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barriers[0].image = ntSwapChain->getImage(currentImageIndex);
    barriers[0].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barriers[0].subresourceRange.baseMipLevel = 0;
    barriers[0].subresourceRange.levelCount = 1;
    barriers[0].subresourceRange.baseArrayLayer = 0;
    barriers[0].subresourceRange.layerCount = 1;

    // Barrier 1: Transition MSAA color image from UNDEFINED to COLOR_ATTACHMENT_OPTIMAL
    // Using UNDEFINED is correct since we're clearing it with loadOp = CLEAR
    barriers[1].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barriers[1].srcAccessMask = 0;
    barriers[1].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    barriers[1].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barriers[1].newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    barriers[1].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barriers[1].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barriers[1].image = ntSwapChain->getColorImage();
    barriers[1].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barriers[1].subresourceRange.baseMipLevel = 0;
    barriers[1].subresourceRange.levelCount = 1;
    barriers[1].subresourceRange.baseArrayLayer = 0;
    barriers[1].subresourceRange.layerCount = 1;

    // Barrier 2: Transition depth image from UNDEFINED to DEPTH_ATTACHMENT_OPTIMAL
    // Using UNDEFINED is correct since we're clearing it with loadOp = CLEAR
    barriers[2].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barriers[2].srcAccessMask = 0;
    barriers[2].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    barriers[2].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barriers[2].newLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
    barriers[2].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barriers[2].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barriers[2].image = ntSwapChain->getDepthImage();
    barriers[2].subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    barriers[2].subresourceRange.baseMipLevel = 0;
    barriers[2].subresourceRange.levelCount = 1;
    barriers[2].subresourceRange.baseArrayLayer = 0;
    barriers[2].subresourceRange.layerCount = 1;

    // Execute all layout transitions before rendering begins
    vkCmdPipelineBarrier(
        commandBuffer,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
        0,
        0, nullptr,
        0, nullptr,
        static_cast<uint32_t>(barriers.size()), barriers.data()
    );

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
    colorAttachment.clearValue.color = {{0.0f, 0.0f, 0.0f, 1.0f}};



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

    ntDevice.vkCmdBeginRendering(commandBuffer, &renderingInfo);

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
  ntDevice.vkCmdEndRendering(commandBuffer);

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
