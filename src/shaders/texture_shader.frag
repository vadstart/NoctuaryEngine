#version 450

layout(location = 0) in vec2 fragTexCoord;
layout(location = 1) in float lightIntensity;

layout(set = 0, binding = 1) uniform sampler2D texSampler;

layout (location = 0) out vec4 outColor;

layout(push_constant) uniform Push {
  mat4 modelMatrix;
  mat4 normalMatrix;
} push;

void main() {
  vec4 texColor = texture(texSampler, fragTexCoord);
  outColor = vec4(texColor.rgb * lightIntensity, texColor.a);
}
