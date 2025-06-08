#pragma once

#include "nt_camera.hpp"
#include "nt_game_object.hpp"
#include "vulkan/vulkan_core.h"

#include <vulkan/vulkan.h>

namespace nt {

struct FrameInfo {
  int frameIndex;
  float frameTime;
  VkCommandBuffer commandBuffer;
  NtCamera &camera;
  VkDescriptorSet globalDescriptorSet;
  NtGameObject::Map &gameObjects;
};

}
