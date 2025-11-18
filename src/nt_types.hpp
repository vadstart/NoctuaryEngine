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
  Lit,
  Unlit,
  Normals,
  NormalTangents,
  Depth,
  Lighting,
  LitWireframe,
  Wireframe,
  DebugGrid,
  Billboard
};

}
