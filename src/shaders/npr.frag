#version 450

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 3) in vec3 fragPosWorld;
layout(location = 4) in vec3 fragNormalWorld;

layout(set = 1, binding = 0) uniform sampler2D diffuseTexSampler;
layout(set = 1, binding = 1) uniform sampler2D normalTexSampler;

layout(location = 0) out vec4 outColor;

layout(push_constant) uniform Push {
    mat4 modelMatrix;
    mat4 normalMatrix;
    int hasNormalTexture;
    int hasMetallicRoughnessTexture;
    float metallicFactor;
    float roughnessFactor;
} push;

void main() {
    vec4 texColor = texture(diffuseTexSampler, fragTexCoord);
    // vec4 texColor = vec4(fragColor, 1.0);
    outColor = vec4(texColor.rgb, texColor.a);
}
