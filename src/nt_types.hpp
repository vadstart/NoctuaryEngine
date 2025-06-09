#pragma once

namespace nt {

enum class CameraType {
  Perspective,
  Orthographic
};

enum class RenderMode { 
  Lit,
  Unlit,
  Normals,
  LitWireframe,
  Wireframe,
  DebugGrid
};

}
