#version 450

layout(location = 0) in vec3 position;
layout(location = 3) in vec2 uv;

layout(set = 0, binding = 0) uniform GlobalUbo {
    mat4 projection;
    mat4 view;
} ubo;

layout(push_constant) uniform Push {
    mat4 modelMatrix;
    mat4 normalMatrix;

    vec2 uvScale;
    vec2 uvOffset;
    float uvRotation;

    int hasNormalTexture;
    int hasMetallicRoughnessTexture;
    float metallicFactor;
    float roughnessFactor;

    float billboardSize;
    int isAnimated;

    float time;
    vec2 scrollSpeed;
} push;

layout(location = 0) out vec2 fragTexCoord;

void main() {
    gl_Position = ubo.projection * ubo.view * push.modelMatrix * vec4(position, 1.0);

    // Animated UVs
    vec2 final_uv = uv;
    final_uv += vec2(0.0f, -2.0f) * push.time; // Scroll over time

    fragTexCoord = final_uv;
}
