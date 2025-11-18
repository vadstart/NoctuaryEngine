#version 450

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 color;
layout(location = 2) in vec3 normal;
layout(location = 3) in vec2 uv;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragTexCoord;

layout(set = 0, binding = 0) uniform GlobalUbo {
    mat4 projection;
    mat4 view;
    mat4 inverseView;
    vec4 ambientLightColor;
    vec4 pointLights[10];
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
    float billboardSize;
} push;

void main() {
    // Get the light position from the model matrix
    vec3 lightWorldPos = vec3(push.modelMatrix[3]);

    // Get camera position from inverse view matrix
    vec3 cameraPos = vec3(ubo.inverseView[3]);

    // Calculate billboard orientation vectors
    vec3 forward = normalize(lightWorldPos - cameraPos);
    vec3 right = normalize(cross(vec3(0.0, 1.0, 0.0), forward));
    vec3 up = cross(forward, right);

    // Create billboard matrix that faces the camera
    mat3 billboardMatrix = mat3(right, up, forward);

    // Scale the quad vertices by billboard size
    vec3 scaledPosition = position * push.billboardSize;

    // Transform vertex position to face camera
    vec3 worldPos = lightWorldPos + billboardMatrix * scaledPosition;

    gl_Position = ubo.projection * ubo.view * vec4(worldPos, 1.0);

    fragColor = push.lightColor * push.lightIntensity;
    fragTexCoord = uv;
}
