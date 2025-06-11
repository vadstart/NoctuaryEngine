#version 450

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 color;
layout(location = 2) in vec3 normal;
layout(location = 3) in vec2 uv;

layout(location = 0) out vec3 fragColor;

layout(set = 0, binding = 0) uniform GlobalUbo {
  mat4 projection;
  mat4 view;

  mat4 inverseView;
  vec4 ambientLightColor;
} ubo;

layout(push_constant) uniform Push {
  mat4 modelMatrix;
  mat4 normalMatrix;
} push;

void main() {
  gl_Position = ubo.projection * ubo.view * push.modelMatrix * vec4(position, 1.0);

  fragColor = normal * 0.3 + 0.3;
}
