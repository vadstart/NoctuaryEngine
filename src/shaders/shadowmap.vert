#version 450

#define MAX_JOINTS 100
#define MAX_JOINT_INFLUENCE 4

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 color;
layout(location = 2) in vec3 normal;
layout(location = 3) in vec2 uv;
layout(location = 4) in vec4 tangent;
layout(location = 5) in ivec4 boneIndices;
layout(location = 6) in vec4 boneWeights;

struct PointLight {
    vec4 position;
    vec4 color;
    int lightType;
    float spotInnerConeAngle; // Cosine of inner cone angle (precomputed on CPU)
    float spotOuterConeAngle; // Cosine of outer cone angle (precomputed on CPU)
    float padding3; // 4 bytes
};

layout(set = 0, binding = 0) uniform GlobalUbo {
    mat4 projection;
    mat4 view;

    mat4 inverseView;
    vec4 ambientLightColor;
    mat4 lightSpaceMatrix;
    vec4 shadowLightDirection;

    PointLight pointLights[10];
    int numLights;
} ubo;

layout(set = 2, binding = 0) readonly buffer BoneMatrices {
    mat4 bones[MAX_JOINTS];
} boneData;

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
    vec3 lightColor;
    float lightIntensity;
    float billboardSize;
    int isAnimated;
} push;

void main() {
    vec4 worldPosition;

    if (push.isAnimated < 1) {
        worldPosition = push.modelMatrix * vec4(position, 1.0);
    }
    else {
        vec4 animatedPosition = vec4(0.0f);
        mat4 jointTransform = mat4(0.0f);

        for (int i = 0; i < MAX_JOINT_INFLUENCE; i++) {
            if (boneWeights[i] == 0)
                continue;
            if (boneIndices[i] >= MAX_JOINTS) {
                animatedPosition = vec4(position, 1.0f);
                jointTransform = mat4(1.0f);
                break;
            }

            // retreive joint matrix from ubo
            mat4 jointMatrix = boneData.bones[boneIndices[i]];

            vec4 localPosition = jointMatrix * vec4(position, 1.0f);
            animatedPosition += localPosition * boneWeights[i];
            jointTransform += jointMatrix * boneWeights[i];
        }

        // projection * view * model * position
        worldPosition = push.modelMatrix * animatedPosition;
    }

    gl_Position = ubo.lightSpaceMatrix * worldPosition;
}
