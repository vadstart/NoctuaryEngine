#pragma once

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

}
