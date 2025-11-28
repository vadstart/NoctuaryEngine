#include "nt_renderer.hpp"
#include "nt_shadows.hpp"
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

void NtRenderer::beginShadowRendering(VkCommandBuffer commandBuffer, NtShadowMap *shadowMap) {
    // Transition shadow image to depth attachment
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = shadowMap->getShadowImage();
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    vkCmdPipelineBarrier(
        commandBuffer,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
        0, 0, nullptr, 0, nullptr, 1, &barrier
    );

    // Depth Attachment
    VkRenderingAttachmentInfo depthAttachment{};
    depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    depthAttachment.imageView = shadowMap->getShadowImageView();
    depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    depthAttachment.clearValue.depthStencil = {1.0f, 0};

    VkRenderingInfo renderingInfo{};
    renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderingInfo.renderArea = {{0, 0}, {shadowMap->getWidth(), shadowMap->getHeight()}};
    renderingInfo.layerCount = 1;
    renderingInfo.colorAttachmentCount = 0;
    renderingInfo.pDepthAttachment = &depthAttachment;

    ntDevice.vkCmdBeginRendering(commandBuffer, &renderingInfo);

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(shadowMap->getWidth());
    viewport.height = static_cast<float>(shadowMap->getHeight());
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
    VkRect2D scissor{{0, 0}, {shadowMap->getWidth(), shadowMap->getHeight()}};
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
}

void NtRenderer::endShadowRendering(VkCommandBuffer commandBuffer, NtShadowMap *shadowMap) {
  ntDevice.vkCmdEndRendering(commandBuffer);

  // Transition to shader read
  VkImageMemoryBarrier barrier{};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image = shadowMap->getShadowImage();
  barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
  barrier.subresourceRange.baseMipLevel = 0;
  barrier.subresourceRange.levelCount = 1;
  barrier.subresourceRange.baseArrayLayer = 0;
  barrier.subresourceRange.layerCount = 1;
    barrier.oldLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(
        commandBuffer,
        VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        0, 0, nullptr, 0, nullptr, 1, &barrier
    );
}

void NtRenderer::beginShadowCubeRendering(VkCommandBuffer commandBuffer, NtShadowCubeMap *shadowCubeMap, uint32_t face) {
    // Only transition on first face (face 0)
    // if (face == 0) {
    // // Transition to depth attachment for this specific face
    // VkImageMemoryBarrier barrier{};
    // barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    // barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    // barrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
    // barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    // barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    // barrier.image = shadowCubeMap->getShadowCubeImage();
    // barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    // barrier.subresourceRange.baseMipLevel = 0;
    // barrier.subresourceRange.levelCount = 1;
    // barrier.subresourceRange.baseArrayLayer = face; // Specific face
    // barrier.subresourceRange.layerCount = 1;
    // barrier.srcAccessMask = 0;
    // barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    // vkCmdPipelineBarrier(
    //     commandBuffer,
    //     VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
    //     VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
    //     0, 0, nullptr, 0, nullptr, 1, &barrier
    // );

    // }

    // Begin rendering to this face
    VkRenderingAttachmentInfo depthAttachment{};
    depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    depthAttachment.imageView = shadowCubeMap->getFaceImageView(face);
    depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    depthAttachment.clearValue.depthStencil = {1.0f, 0};

    VkRenderingInfo renderingInfo{};
    renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderingInfo.renderArea = {{0, 0}, {shadowCubeMap->getSize(), shadowCubeMap->getSize()}};
    renderingInfo.layerCount = 1;
    renderingInfo.colorAttachmentCount = 0;
    renderingInfo.pDepthAttachment = &depthAttachment;

    ntDevice.vkCmdBeginRendering(commandBuffer, &renderingInfo);

    // Set viewport and scissor
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(shadowCubeMap->getSize());
    viewport.height = static_cast<float>(shadowCubeMap->getSize());
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    VkRect2D scissor{{0, 0}, {shadowCubeMap->getSize(), shadowCubeMap->getSize()}};
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
}

void NtRenderer::endShadowCubeRendering(VkCommandBuffer commandBuffer, NtShadowCubeMap *shadowCubeMap) {
    // Transition all faces to shader read layout
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = shadowCubeMap->getShadowCubeImage();
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 6; // All faces
    barrier.oldLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(
        commandBuffer,
        VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        0, 0, nullptr, 0, nullptr, 1, &barrier
    );
}


void NtRenderer::beginMainRendering(VkCommandBuffer commandBuffer) {
    // Batch all layout transitions into one barrier call
    std::array<VkImageMemoryBarrier, 3> barriers{};

    // Helper lambda
    auto setupBarrier = [](VkImageMemoryBarrier& barrier, VkImage image,
                            VkImageAspectFlags aspect, VkImageLayout oldLayout,
                            VkImageLayout newLayout, VkAccessFlags dstAccess) {
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = dstAccess;
        barrier.oldLayout = oldLayout;
        barrier.newLayout = newLayout;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image;
        barrier.subresourceRange.aspectMask = aspect;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;
    };

    // Swap chain image (resolve target)
    setupBarrier(barriers[0],
                    ntSwapChain->getImage(currentImageIndex),
                    VK_IMAGE_ASPECT_COLOR_BIT,
                    VK_IMAGE_LAYOUT_UNDEFINED,
                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                    VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);

    // MSAA color image
    setupBarrier(barriers[1],
                    ntSwapChain->getColorImage(),
                    VK_IMAGE_ASPECT_COLOR_BIT,
                    VK_IMAGE_LAYOUT_UNDEFINED,
                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                    VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);

    // Depth image
    setupBarrier(barriers[2],
                    ntSwapChain->getDepthImage(),
                    VK_IMAGE_ASPECT_DEPTH_BIT,
                    VK_IMAGE_LAYOUT_UNDEFINED,
                    VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                    VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                    VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);

    // Execute all transitions in one call (more efficient)
    vkCmdPipelineBarrier(
        commandBuffer,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,  // "Before everything"
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |   // Color writes need these
        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,       // Depth tests need these
        0,
        0, nullptr,
        0, nullptr,
        static_cast<uint32_t>(barriers.size()), barriers.data()
    );

    // Color Attachment
    VkRenderingAttachmentInfo colorAttachment{};
    colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    colorAttachment.imageView = ntSwapChain->getColorImageView();
    colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachment.resolveMode = VK_RESOLVE_MODE_AVERAGE_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.clearValue.color = {{0.0f, 0.0f, 0.0f, 1.0f}};
    // MSAA
    colorAttachment.resolveImageView = ntSwapChain->getImageView(currentImageIndex);
    colorAttachment.resolveImageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    // Depth Attachment
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

void NtRenderer::endMainRendering(VkCommandBuffer commandBuffer) {
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
