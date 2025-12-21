#pragma once

// Libraries
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace nt {

enum class CameraProjectionType {
  Perspective,
  Orthographic
};
enum class CameraControlType {
    FPS,
    Orbit
};

enum class RenderMode {
  PBR,
  NPR,
  ShadowMap,
  Wireframe,
  Billboard
};

enum class eLightType : int {
    Point = 0,
    Spot = 1,
    Directional = 2
};

struct NtPushConstantData {
    alignas(16) glm::mat4 modelMatrix{1.f};
    alignas(16) glm::mat4 normalMatrix{1.f};

    alignas(8) glm::vec2 uvScale{1.0f, 1.0f};
    alignas(8) glm::vec2 uvOffset{0.0f, 0.0f};
    alignas(4) float uvRotation{0.0f};

    alignas(4) int hasNormalTexture{0};
    alignas(4) int hasMetallicRoughnessTexture{0};
    alignas(4) float metallicFactor{1.0f};
    alignas(4) float roughnessFactor{1.0f};

    alignas(4) float billboardSize{1.0f};
    alignas(4) int isAnimated{0};

    alignas(4) float time{0.0f};
    alignas(8) glm::vec2 scrollSpeed{0.0f, 0.0f};
};

}
