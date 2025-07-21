#version 450

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 color;
layout(location = 2) in vec3 normal;
layout(location = 3) in vec2 uv;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragTexCoord;
layout(location = 3) out vec3 fragPosWorld;
layout(location = 4) out vec3 fragNormalWorld;

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
    int hasNormalTexture;
    int hasMetallicRoughnessTexture;
    int debugMode;
    float metallicFactor;
    float roughnessFactor;
} push;

void main() {
    // Transform to World Space
    vec4 positionWorld = push.modelMatrix * vec4(position, 1.0);
    gl_Position = ubo.projection * ubo.view * positionWorld;

    fragColor = vec3(1.0f, 1.0f, 1.0f);
    fragTexCoord = uv;
    fragPosWorld = positionWorld.xyz;
    fragNormalWorld = normal;
}
