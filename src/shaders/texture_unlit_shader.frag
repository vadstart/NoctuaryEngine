#version 450

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 3) in vec3 fragPosWorld;
layout(location = 4) in vec3 fragNormalWorld;

layout(set = 1, binding = 0) uniform sampler2D diffuseTexSampler;
layout(set = 1, binding = 1) uniform sampler2D normalTexSampler;

layout (location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform GlobalUbo {
  mat4 projection;
  mat4 view;

  mat4 inverseView;
  vec4 ambientLightColor;
  
  vec3 lightPosition;
  vec4 lightColor;
} ubo;


layout(push_constant) uniform Push {
  mat4 modelMatrix;
  mat4 normalMatrix;
} push;

void main() {
  vec4 texColor = texture(diffuseTexSampler, fragTexCoord);
  outColor = vec4(texColor.rgb , texColor.a);
}
