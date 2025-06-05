#version 450

layout(location = 0) in vec3 worldPos;
layout(location = 1) in vec3 fragColor;

layout (location = 0) out vec4 outColor;

layout(push_constant) uniform GridPush {
  mat4 projectionView;
  vec3 cameraPos;
  float gridSpacing;
  float lineThickness;
  float fadeDistance;
} push;

float gridLine(vec2 coord, float spacing, float thickness) {
  vec2 grid = abs(fract(coord / spacing - 0.5) - 0.5) / fwidth(coord / spacing);
  float line = min(grid.x, grid.y);
  return 1.0 - smoothstep(0.0, thickness, line);
}

void main() {
  float fade = 1.0 - clamp(distance(worldPos.xz, push.cameraPos.xz) / push.fadeDistance, 0.0, 1.0);
  float line = gridLine(worldPos.xz, push.gridSpacing, push.lineThickness);
  float intensity = line * fade;

  outColor = vec4(fragColor * intensity, intensity);
}
