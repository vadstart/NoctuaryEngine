#version 450

layout(location = 0) in vec3 position;
layout(location = 3) in vec2 uv;

layout(set = 0, binding = 0) uniform GlobalUbo {
    mat4 projection;
    mat4 view;
    float time;
} ubo;

layout(push_constant) uniform Push {
    mat4 modelMatrix;
    vec2 scrollSpeed;
    vec2 uvTiling;
} push;

layout(location = 0) out vec2 fragTexCoord;

void main() {
    gl_Position = ubo.projection * ubo.view * push.modelMatrix * vec4(position, 1.0);

    // Animated UVs
    vec2 final_uv = uv * push.uvTiling;
    final_uv += push.scrollSpeed * ubo.time; // Scroll over time

    fragTexCoord = final_uv;
}
