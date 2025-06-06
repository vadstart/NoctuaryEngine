#version 450
layout(location = 0) in vec3 position;
layout(location = 1) in vec3 color;

layout(set = 0, binding = 0) uniform GlobalUbo {
  mat4 projectionViewMatrix;
  vec3 directionToLight;
} ubo;

layout(push_constant) uniform GridPush {
  mat4 modelMatrix;
  vec3 cameraPos;
  float gridSpacing;
  float lineThickness;
  float fadeDistance;
} push;

layout(location = 0) out vec3 worldPos;
layout(location = 1) out vec3 fragColor;

void main() {
    worldPos = position;
    fragColor = color;
    gl_Position = ubo.projectionViewMatrix * push.modelMatrix * vec4(position, 1.0);
}
