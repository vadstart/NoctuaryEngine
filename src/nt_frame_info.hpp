#pragma once

#include "nt_camera.hpp"
#include "nt_game_object.hpp"
#include "vulkan/vulkan_core.h"

#include <glm/fwd.hpp>
#include <vulkan/vulkan.h>

namespace nt {

#define MAX_LIGHTS 10

struct PointLight {
  glm::vec4 position{};
  glm::vec4 color{};
  int lightType{0};
  float spotInnerConeAngle{12.5f}; // Inner cone in degrees
  float spotOuterConeAngle{17.5f}; // Outer cone in degrees
  float padding; // for alignment
};

struct GlobalUbo {
  glm::mat4 projection{1.f};
  glm::mat4 view{1.f};
  glm::mat4 inverseView{1.f};

  glm::vec4 ambientLightColor{0.8f, 0.8f, 0.8f, 0.015f};

  glm::mat4 lightSpaceMatrix; // For shadowMapping
  glm::vec4 shadowLightDirection; // xyz = direction, w = light type

  PointLight pointLights[MAX_LIGHTS];
  int numLights;
};


struct FrameInfo {
  int frameIndex;
  float frameTime;
  VkCommandBuffer commandBuffer;
  NtCamera &camera;
  VkDescriptorSet globalDescriptorSet;
  NtGameObject::Map &gameObjects;
};

}
