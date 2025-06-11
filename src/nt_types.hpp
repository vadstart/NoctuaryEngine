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
  Depth,
  Lighting,
  LitWireframe,
  Wireframe,
  DebugGrid
};

}
