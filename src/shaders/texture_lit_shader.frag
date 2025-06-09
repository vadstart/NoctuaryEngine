#version 450

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 3) in vec3 fragPosWorld;
layout(location = 4) in vec3 fragNormalWorld;

layout(set = 1, binding = 0) uniform sampler2D diffuseTexSampler;
layout(set = 1, binding = 1) uniform sampler2D normalTexSampler;

layout (location = 0) out vec4 outColor;

struct PointLight {
  vec3 position;
  vec4 color;
};

layout(set = 0, binding = 0) uniform GlobalUbo {
  mat4 projection;
  mat4 view;

  mat4 inverseView;
  vec4 ambientLightColor;
  
  PointLight pointLights[10];
  int numLights;
} ubo;

layout(push_constant) uniform Push {
  mat4 modelMatrix;
  mat4 normalMatrix;
} push;

void main() {
  vec3 diffuseLight = ubo.ambientLightColor.xyz * ubo.ambientLightColor.w;
  vec3 specularLight = vec3(0.0);
  vec3 surfaceNormal = normalize(fragNormalWorld);

  vec3 camWorldPos = ubo.inverseView[3].xyz;
  vec3 viewDirection = normalize(camWorldPos - fragPosWorld);

  // vec3 normalMapSample = texture(normalTexSampler, fragTexCoord).rgb;
  // normalMapSample.g = -normalMapSample.g;
  // vec3 surfaceNormal = normalize(normalMapSample);
  // vec3 surfaceNormal = normalize(normalMapSample * 2.0 - 1.0);
  // // Convert from tangent space to world space
  // vec3 surfaceNormal = normalize(TBN * normalMapSample);

  for (int i = 0;i < ubo.numLights; i++) {
    PointLight light = ubo.pointLights[i];

    vec3 directionToLight = light.position - fragPosWorld.xyz;
    float attenuation = 1.0 / dot(directionToLight, directionToLight); // distance squared
    directionToLight = normalize(directionToLight);

    float cosAngIncidence = max(dot(surfaceNormal, directionToLight), 0);
    vec3 intensity = light.color.xyz * light.color.w * attenuation;

    diffuseLight += intensity * cosAngIncidence;

    // Specular
    vec3 halfAngle = normalize(directionToLight + viewDirection);
    float blinnTerm = dot(surfaceNormal, halfAngle);
    blinnTerm = clamp(blinnTerm, 0, 1);
    blinnTerm = pow(blinnTerm, 1024.0);
    specularLight += intensity * blinnTerm;
  }

  // outColor = vec4(diffuseLight * fragColor + specularLight * fragColor, 1.0);
  // outColor = vec4(surfaceNormal, 1.0);

  vec4 texColor = texture(diffuseTexSampler, fragTexCoord);
  outColor = vec4(texColor.rgb * (diffuseLight * fragColor + specularLight * fragColor), texColor.a);
  
  // vec4 texColor = texture(normalTexSampler, fragTexCoord);
  // outColor = vec4(texColor.rgb * fragColor, texColor.a);
}
