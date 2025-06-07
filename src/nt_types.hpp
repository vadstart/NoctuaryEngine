#pragma once

namespace nt {

enum class CameraType {
  Perspective,
  Orthographic
};

enum class RenderMode { 
  Lit,
  Unlit,
  Lighting,
  Normals,
  LitWireframe,
  Wireframe,
  DebugGrid
};

}
