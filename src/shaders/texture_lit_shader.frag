#version 450

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 3) in vec3 fragPosWorld;
layout(location = 4) in vec3 fragNormalWorld;
layout(location = 5) in vec4 fragTangentWorld;

layout(set = 1, binding = 0) uniform sampler2D diffuseTexSampler;
layout(set = 1, binding = 1) uniform sampler2D normalTexSampler;
layout(set = 1, binding = 2) uniform sampler2D metallicRoughnessTexSampler;

// Push constant to indicate if normal texture is available
layout(push_constant) uniform Push {
    mat4 modelMatrix;
    mat4 normalMatrix;
    int hasNormalTexture;
    int hasMetallicRoughnessTexture;
    int debugMode; // 0 = normal, 1 = tangent, 2 = bitangent, 3 = normal map
    float metallicFactor;
    float roughnessFactor;
} push;

layout(location = 0) out vec4 outColor;

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

void main() {
    vec3 diffuseLight = ubo.ambientLightColor.xyz * ubo.ambientLightColor.w;
    vec3 specularLight = vec3(0.0);

    // Start with vertex normal
    vec3 surfaceNormal = normalize(fragNormalWorld);

    // Debug visualization modes
    if (push.debugMode > 0) {
        vec3 debugColor = vec3(1.0, 0.0, 1.0); // Default magenta for invalid

        if (push.debugMode == 1) {
            // Visualize tangent vectors
            debugColor = normalize(fragTangentWorld.xyz) * 0.5 + 0.5;
        } else if (push.debugMode == 2) {
            // Visualize bitangent vectors
            vec3 N = normalize(fragNormalWorld);
            vec3 T = normalize(fragTangentWorld.xyz);
            T = normalize(T - dot(T, N) * N);
            vec3 B = normalize(cross(N, T)) * fragTangentWorld.w;
            debugColor = B * 0.5 + 0.5;
        } else if (push.debugMode == 3) {
            // Visualize normal map directly
            if (push.hasNormalTexture > 0) {
                debugColor = texture(normalTexSampler, fragTexCoord).rgb;
            } else {
                debugColor = vec3(0.5, 0.5, 1.0); // Default normal color
            }
        }

        outColor = vec4(debugColor, 1.0);
        return;
    }

    // Apply normal mapping if available
    // if (push.hasNormalTexture > 0) {
    //     vec3 normalMapSample = texture(normalTexSampler, fragTexCoord).rgb;

    //     // Convert from [0,1] to [-1,1] range
    //     normalMapSample = normalMapSample * 2.0 - 1.0;

    //     // Use proper tangent space from vertex attributes
    //     vec3 N = normalize(fragNormalWorld);
    //     vec3 T = normalize(fragTangentWorld.xyz);

    //     // Re-orthogonalize the tangent with respect to the normal
    //     T = normalize(T - dot(T, N) * N);

    //     // Calculate bitangent
    //     vec3 B = normalize(cross(N, T)) * fragTangentWorld.w;

    //     // Create TBN matrix - the column order is important
    //     mat3 TBN = mat3(T, B, N);

    //     // Transform normal from tangent space to world space
    //     surfaceNormal = normalize(TBN * normalMapSample);
    // }

    // Sample metallic-roughness texture if available
    float roughness = push.roughnessFactor; // Use material factor as default
    float metallic = push.metallicFactor; // Use material factor as default

    if (push.hasMetallicRoughnessTexture > 0) {
        vec3 metallicRoughnessSample = texture(metallicRoughnessTexSampler, fragTexCoord).rgb;
        roughness *= metallicRoughnessSample.g; // Green channel = roughness * factor
        metallic *= metallicRoughnessSample.b; // Blue channel = metallic * factor
    }

    vec3 camWorldPos = ubo.inverseView[3].xyz;
    vec3 viewDirection = normalize(camWorldPos - fragPosWorld);

    // Sample base color texture
    vec4 texColor = texture(diffuseTexSampler, fragTexCoord);

    for (int i = 0; i < ubo.numLights; i++) {
        PointLight light = ubo.pointLights[i];

        vec3 directionToLight = light.position - fragPosWorld.xyz;
        float attenuation = 1.0 / dot(directionToLight, directionToLight); // distance squared
        directionToLight = normalize(directionToLight);

        float cosAngIncidence = max(dot(surfaceNormal, directionToLight), 0);
        vec3 intensity = light.color.xyz * light.color.w * attenuation;

        diffuseLight += intensity * cosAngIncidence;

        // PBR Specular calculation
        vec3 halfAngle = normalize(directionToLight + viewDirection);
        float NdotH = max(dot(surfaceNormal, halfAngle), 0.0);

        // Convert roughness to shininess (roughness 0 = very shiny, roughness 1 = very rough)
        // float shininess = mix(1024.0, 1.0, roughness);
        // float shininess = 32.0 / (roughness * roughness + 0.01); // cheap & effective
        roughness = clamp(roughness, 0.04, 1.0); // Prevent extreme diffusion
        float shininess = pow(2.0 / (roughness * roughness) - 2.0, 0.25); // Approximate conversion

        float blinnTerm = pow(NdotH, shininess);

        // Metallic materials have different specular behavior
        vec3 F0 = mix(vec3(0.04), texColor.rgb, metallic); // Fresnel reflectance
        F0 = clamp(F0 * 1.2, 0.0, 1.0);
        // boost specular light for metallic
        float roughFactor = mix(1.0, 0.1, roughness); // Don’t let roughness kill it
        vec3 specular = F0 * blinnTerm * roughFactor * 2.5f;
        // vec3 specular = F0 * blinnTerm * (1.0 - roughness);

        specularLight += intensity * specular;
        // specularLight += F0 * 0.1; // ambient spec
    }

    // For metallic materials, reduce diffuse contribution
    vec3 diffuse = mix(texColor.rgb * diffuseLight, vec3(0.0), metallic);

    outColor = vec4((diffuse + specularLight) * fragColor, texColor.a);
}
