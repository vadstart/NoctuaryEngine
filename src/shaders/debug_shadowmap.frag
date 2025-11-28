#version 450

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D depthTexture;

void main() {
    float depth = texture(depthTexture, inUV).r;

    // Linearize depth for better visualization
    // Adjust the multiplier (25.0) based on your near/far planes
    float linearDepth = 1.0 - (1.0 - depth) * 1000.0;

    // outColor = vec4(vec3(linearDepth), 1.0);
    outColor = vec4(vec3(depth), 1.0);
}
