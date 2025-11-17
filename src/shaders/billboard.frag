#version 450

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

layout(set = 1, binding = 0) uniform sampler2D texSampler;

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
    float billboardSize;
} push;

void main() {
    // Sample the texture
    vec4 texColor = texture(texSampler, fragTexCoord);

    float alpha = texColor.r;
    if (alpha < 0.01) {
        discard;
    }

    // Apply light color and intensity to visible pixels
    vec3 finalColor = fragColor;

    // Output with full opacity for visible pixels
    outColor = vec4(finalColor, alpha);
}
