#version 450

layout(location = 0) in vec4 fragPosView;

layout(location = 0) out vec4 outColor;

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
    float depth = fragPosView.z / 50.0; // TODO:Dynamically replace with CameraFar
    depth = pow(depth, 1.0 / 2.2);
    depth = 1.0 - depth;

    outColor = vec4(vec3(depth), 1.0f);
}
