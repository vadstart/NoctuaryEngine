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

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragTexCoord;
layout(location = 3) out vec3 fragPosWorld;
layout(location = 4) out vec3 fragNormalWorld;
layout(location = 5) out mat3 fragTBN;

layout(set = 0, binding = 0) uniform GlobalUbo {
    mat4 projection;
    mat4 view;

    mat4 inverseView;
    vec4 ambientLightColor;

    vec3 lightPosition;
    vec4 lightColor;
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
    vec4 positionWorld = push.modelMatrix * animatedPosition;
    gl_Position = ubo.projection * ubo.view * positionWorld;
    fragPosWorld = positionWorld.xyz;

    mat3 normalMatrix = transpose(inverse(mat3(push.normalMatrix) * mat3(jointTransform)));
    fragNormalWorld = normalize(normalMatrix * normal);
    // fragTangent = normalize(normalMatrix * normal);

    fragTexCoord = uv;

    // Visualize bone indices as colors
    fragColor = vec3(
            float(boneWeights.x) / 35.0,
            float(boneWeights.y) / 35.0,
            float(boneWeights.z) / 35.0
        );

    // DEBUG: Visualize bone indices
    // fragColor = vec3(float(boneIndices.x) / 19.0, float(boneIndices.y) / 19.0, float(boneIndices.z) / 19.0);

    // DEBUG: Visualize bone weights
    // fragColor = vec3(boneWeights.x, boneWeights.y, boneWeights.z);
}
