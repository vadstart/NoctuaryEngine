#version 450

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 color;
layout(location = 2) in vec3 normal;
layout(location = 3) in vec2 uv;

layout(location = 0) out vec2 outUV;

layout(push_constant) uniform Push {
    vec2 offset; // Screen position offset
    vec2 scale; // Quad size
} push;

void main() {
    // Transform to NDC space
    // offset and scale are in NDC coordinates [-1, 1]
    vec2 pos = position.xy * push.scale + push.offset;

    gl_Position = vec4(pos, 0.0, 1.0);
    outUV = uv;
}
