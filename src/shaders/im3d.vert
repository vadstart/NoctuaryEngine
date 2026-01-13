#version 450

// im3d vertex format: vec4(position.xyz, size), color (packed RGBA)
layout(location = 0) in vec4 aPositionSize;
layout(location = 1) in uint aColor;

layout(location = 0) out vec4 vColor;

layout(set = 0, binding = 0) uniform GlobalUbo {
    mat4 projection;
    mat4 view;
    mat4 inverseView;
    vec4 ambientLightColor;
    vec3 lightPosition;
    vec4 lightColor;
} ubo;

void main() {
    gl_Position = ubo.projection * ubo.view * vec4(aPositionSize.xyz, 1.0);

    // Unpack color from RGBA8 (im3d stores as 0xRRGGBBAA)
    vColor = vec4(
        float((aColor >> 24u) & 0xFFu) / 255.0,
        float((aColor >> 16u) & 0xFFu) / 255.0,
        float((aColor >> 8u) & 0xFFu) / 255.0,
        float(aColor & 0xFFu) / 255.0
    );

    // Point size for point primitives
    gl_PointSize = max(aPositionSize.w, 1.0);
}
