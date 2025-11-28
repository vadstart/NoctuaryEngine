#version 450

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 color;
layout(location = 2) in vec3 normal;
layout(location = 3) in vec2 uv;
layout(location = 4) in vec4 tangent;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragTexCoord;
layout(location = 3) out vec3 fragPosWorld;
layout(location = 4) out vec3 fragNormalWorld;
layout(location = 5) out vec4 fragTangentWorld;

struct PointLight {
    vec3 position;
    vec4 color;
};

layout(set = 0, binding = 0) uniform GlobalUbo {
    mat4 projection;
    mat4 view;

    mat4 inverseView;
    vec4 ambientLightColor;
    mat4 lightSpaceMatrix;

    PointLight pointLights[10];
    int numLights;
} ubo;

layout(push_constant) uniform Push {
    mat4 modelMatrix;
    mat4 normalMatrix;
    vec2 uvScale;
    vec2 uvOffset;
    float uvRotation;
    int hasNormalTexture;
    int hasMetallicRoughnessTexture;
    int debugMode;
    float metallicFactor;
    float roughnessFactor;
    vec3 lightColor;
    float lightIntensity;
} push;

vec2 transformUV(vec2 uv, vec2 scale, vec2 offset, float rotation) {
    // Apply scale and offset first
    vec2 transformed = uv * scale + offset;

    // Apply rotation around (0.5, 0.5) pivot
    if (rotation != 0.0) {
        vec2 pivot = vec2(0.5, 0.5);
        transformed -= pivot;

        float cosR = cos(rotation);
        float sinR = sin(rotation);
        mat2 rotMatrix = mat2(cosR, -sinR, sinR, cosR);
        transformed = rotMatrix * transformed;

        transformed += pivot;
    }

    return transformed;
}

void main() {
    // Transform to World Space
    vec4 positionWorld = push.modelMatrix * vec4(position, 1.0);
    gl_Position = ubo.projection * ubo.view * positionWorld;

    fragColor = color;
    // fragTexCoord = uv;
    fragTexCoord = transformUV(uv, push.uvScale, push.uvOffset, push.uvRotation);
    fragNormalWorld = normalize(mat3(push.normalMatrix) * normal);
    fragPosWorld = positionWorld.xyz;
    fragTangentWorld = vec4(normalize(mat3(push.normalMatrix) * tangent.xyz), tangent.w);
}
