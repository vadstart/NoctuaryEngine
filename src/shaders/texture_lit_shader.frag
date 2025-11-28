#version 450
#define LIGHT_TYPE_POINT 0
#define LIGHT_TYPE_SPOT 1
#define LIGHT_TYPE_DIRECTIONAL 2

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
    vec2 uvScale;
    vec2 uvOffset;
    float uvRotation;
    int hasNormalTexture;
    int hasMetallicRoughnessTexture;
    int debugMode; // 0 = normal, 1 = tangent, 2 = bitangent, 3 = normal map, 4 = tangent map
    float metallicFactor;
    float roughnessFactor;
    vec3 lightColor;
    float lightIntensity;
} push;

layout(location = 0) out vec4 outColor;

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
    mat4 lightSpaceCubeMatrices[6]; // 6 view matrices for cubemap faces

    vec4 shadowLightDirection;
    vec4 shadowLightPosition; // xyz = position, w = far plane

    PointLight pointLights[10];
    int numLights;
} ubo;

layout(set = 0, binding = 1) uniform sampler2DShadow shadowMap;
layout(set = 0, binding = 2) uniform sampler2D shadowMapDebug;
layout(set = 0, binding = 3) uniform samplerCubeShadow shadowCubeMap; // New: cubemap for point lights

// Add new function for cubemap shadow calculation
float calculatePointLightShadow(vec3 fragPosWorld) {
    vec3 lightPos = ubo.shadowLightPosition.xyz;
    float farPlane = ubo.shadowLightPosition.w;

    // Vector from fragment to light
    vec3 fragToLight = fragPosWorld - lightPos;
    float currentDepth = length(fragToLight);

    // Normalize for sampling direction
    vec3 sampleDir = normalize(fragToLight);

    // Bias based on distance
    float bias = 0.05;
    float depth = currentDepth - bias;

    // Sample cubemap shadow - need to normalize by far plane
    float shadow = texture(shadowCubeMap, vec4(sampleDir, depth / farPlane));

    return shadow;
}

float calculateShadow(vec3 fragPosWorld, vec3 normal) {
    vec4 fragPosLightSpace = ubo.lightSpaceMatrix * vec4(fragPosWorld, 1.0);
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;

    if (projCoords.z > 1.0 || projCoords.x < 0.0 || projCoords.x > 1.0 ||
            projCoords.y < 0.0 || projCoords.y > 1.0) {
        return 1.0;
    }

    float bias = 0.005;

    // Simple slope-scale bias
    // vec3 lightDir = normalize(ubo.shadowLightDirection.xyz);
    // float bias = max(0.05 * (1.0 - dot(normal, -lightDir)), 0.005);

    // Angle-dependent bias: more bias at grazing angles
    // vec3 lightDir = normalize(ubo.shadowLightDirection.xyz);
    // float cosTheta = clamp(dot(normal, -lightDir), 0.0, 1.0);
    // float bias = 0.005 * tan(acos(cosTheta)); // Increases at shallow angles
    // bias = clamp(bias, 0.0, 0.01); // Clamp to prevent extreme values

    projCoords.z -= bias;

    // PCF Filtering
    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(shadowMap, 0);
    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            shadow += texture(shadowMap, vec3(projCoords.xy + vec2(x, y) * texelSize, projCoords.z));
        }
    }
    return shadow / 9.0;

    return shadow;
}

void main() {
    vec3 diffuseLight = vec3(0.0);
    vec3 specularLight = vec3(0.0);

    // Start with vertex normal
    vec3 surfaceNormal = normalize(fragNormalWorld);

    // Debug visualization modes
    if (push.debugMode == 1) {
        vec3 debugColor = vec3(1.0, 0.0, 1.0); // Default magenta for invalid

        // Visualize normal-mapped surface normals (or vertex normals if no map)
        vec3 N = normalize(fragNormalWorld);
        vec3 T = normalize(fragTangentWorld.xyz);
        T = normalize(T - dot(T, N) * N);
        vec3 B = cross(T, N) * fragTangentWorld.w;

        if (push.hasNormalTexture > 0) {
            vec3 normalMapSample = texture(normalTexSampler, fragTexCoord).rgb;
            normalMapSample = normalMapSample * 2.0 - 1.0;

            mat3 TBN = mat3(T, B, N);
            vec3 transformedNormal = normalize(TBN * normalMapSample);
            debugColor = transformedNormal * 0.5 + 0.5;
        }

        outColor = vec4(debugColor, 1.0);
        return;
    }
    else if (push.debugMode == 2) {
        // Show shadow map coordinates
        vec4 fragPosLightSpace = ubo.lightSpaceMatrix * vec4(fragPosWorld, 1.0);
        vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
        projCoords = projCoords * 0.5 + 0.5;

        // Visualize: Red = out of bounds, Green = in bounds with depth value
        if (projCoords.x < 0.0 || projCoords.x > 1.0 ||
                projCoords.y < 0.0 || projCoords.y > 1.0 ||
                projCoords.z > 1.0) {
            outColor = vec4(1.0, 0.0, 0.0, 1.0); // Red = out of bounds
        } else {
            float depth = texture(shadowMapDebug, projCoords.xy).r;
            outColor = vec4(depth, projCoords.z, 0.0, 1.0); // R=shadow map depth, G=fragment depth
        }
        return;
    }
    else if (push.debugMode == 3) {
        // Show raw shadow value
        float shadow = calculateShadow(fragPosWorld, surfaceNormal);
        outColor = vec4(vec3(shadow), 1.0);
        return;
    }
    else if (push.debugMode == 4) {
        // Visualize shadow direction
        outColor = vec4(vec3(ubo.shadowLightDirection), 1.0);
        return;
    }
    else if (push.debugMode == 5) {
        // Test just NdotL (Lambert term)
        vec3 lightDir = -normalize(ubo.pointLights[0].position.xyz);
        float NdotL = max(dot(surfaceNormal, lightDir), 0.0);
        outColor = vec4(vec3(NdotL), 1.0);
        return;
    }
    else if (push.debugMode == 6) {
        // Test radiance value
        vec3 radiance = ubo.pointLights[0].color.xyz * ubo.pointLights[0].color.w;
        outColor = vec4(radiance, 1.0);
        return;
    }
    else if (push.debugMode == 7) {
        // Test final contribution (radiance * NdotL)
        vec3 lightDir = -normalize(ubo.pointLights[0].position.xyz);
        float NdotL = max(dot(surfaceNormal, lightDir), 0.0);
        vec3 radiance = ubo.pointLights[0].color.xyz * ubo.pointLights[0].color.w;
        outColor = vec4(radiance * NdotL, 1.0);
        return;
    }
    else if (push.debugMode == 8) {
        // Show what color values the shader receives
        outColor = vec4(ubo.pointLights[0].color.xyz, 1.0);
        return;
    }
    else if (push.debugMode == 9) {
        // Show intensity (as brightness)
        float intensity = ubo.pointLights[0].color.w;
        outColor = vec4(vec3(intensity / 10.0), 1.0); // Divide by 10 to visualize
        return;
    }
    else if (push.debugMode == 10) {
        // Show shadow map UV coordinates
        vec4 fragPosLightSpace = ubo.lightSpaceMatrix * vec4(fragPosWorld, 1.0);
        vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
        projCoords = projCoords * 0.5 + 0.5;

        // Color code: Green = in bounds, Red = out of bounds
        if (projCoords.x < 0.0 || projCoords.x > 1.0 ||
                projCoords.y < 0.0 || projCoords.y > 1.0 ||
                projCoords.z < 0.0 || projCoords.z > 1.0) {
            outColor = vec4(1.0, 0.0, 0.0, 1.0); // Red = out of bounds
        } else {
            outColor = vec4(0.0, 1.0, 0.0, 1.0); // Green = in bounds
        }
        return;
    }

    // Apply normal mapping if available
    if (push.hasNormalTexture > 0) {
        vec3 normalMapSample = texture(normalTexSampler, fragTexCoord).rgb;

        // Convert from [0,1] to [-1,1] range
        normalMapSample = normalMapSample * 2.0 - 1.0;

        // Use proper tangent space from vertex attributes
        vec3 N = normalize(fragNormalWorld);
        vec3 T = normalize(fragTangentWorld.xyz);

        // Re-orthogonalize the tangent with respect to the normal
        T = normalize(T - dot(T, N) * N);

        // Calculate bitangent
        vec3 B = normalize(cross(N, T)) * fragTangentWorld.w;

        // Create TBN matrix - the column order is important
        mat3 TBN = mat3(T, B, N);

        // Transform normal from tangent space to world space
        surfaceNormal = normalize(TBN * normalMapSample);
    }

    // Sample metallic-roughness texture if available
    float roughness = push.roughnessFactor; // Use material factor as default
    float metallic = push.metallicFactor; // Use material factor as default

    if (push.hasMetallicRoughnessTexture > 0) {
        vec3 metallicRoughnessSample = texture(metallicRoughnessTexSampler, fragTexCoord).rgb;
        roughness *= metallicRoughnessSample.g; // Green channel = roughness * factor
        metallic *= metallicRoughnessSample.b; // Blue channel = metallic * factor
    }

    // Clamp to prevent artifacts
    roughness = clamp(roughness, 0.04, 1.0);

    vec3 camWorldPos = ubo.inverseView[3].xyz;
    vec3 viewDirection = normalize(camWorldPos - fragPosWorld);

    // Sample base color texture
    vec4 texColor = texture(diffuseTexSampler, fragTexCoord);

    // Base reflictivity (F0) for dielectrics and metals
    vec3 F0 = mix(vec3(0.04), texColor.rgb, metallic);

    // SHADOWS
    float shadow;
    int shadowLightType = int(ubo.shadowLightDirection.w);
    if (shadowLightType == LIGHT_TYPE_POINT) {
        shadow = calculatePointLightShadow(fragPosWorld);
    } else {
        shadow = calculateShadow(fragPosWorld, surfaceNormal);
    }

    // LIGHTING
    for (int i = 0; i < ubo.numLights; i++) {
        PointLight light = ubo.pointLights[i];

        vec3 directionToLight;
        float attenuation; // distance squared
        vec3 radiance;

        if (light.lightType == LIGHT_TYPE_DIRECTIONAL) {
            // DIRECTIONAL LIGHT
            // Position represents direction (already normalized in CPU)
            directionToLight = -normalize(light.position.xyz); // Light direction points TOWARD light
            attenuation = 0.0; // No attenuation
        }
        else if (light.lightType == LIGHT_TYPE_SPOT) {
            // SPOTLIGHT
            directionToLight = light.position.xyz - fragPosWorld;
            float distance = length(directionToLight);
            directionToLight = normalize(directionToLight);

            // Distance attenuation (inverse square)
            float distanceAttenuation = 1.0 / (distance * distance);

            // Angular attenuation (cone falloff)
            vec3 spotDir = normalize(ubo.shadowLightDirection.xyz); // Spot direction stored here
            float theta = dot(directionToLight, -spotDir); // Angle between light dir and spot dir

            // Smooth falloff between inner and outer cone
            // light.spotInnerConeAngle and spotOuterConeAngle are cosines (bigger value = smaller angle)
            float epsilon = light.spotInnerConeAngle - light.spotOuterConeAngle;
            float spotIntensity = clamp((theta - light.spotOuterConeAngle) / epsilon, 0.0, 1.0);

            attenuation = distanceAttenuation * spotIntensity;
        }
        else {
            // POINT LIGHT (default)
            directionToLight = light.position.xyz - fragPosWorld;
            float distance = length(directionToLight);
            attenuation = 1.0 / (distance * distance);
            directionToLight = normalize(directionToLight);
        }
        radiance = light.color.xyz * light.color.w * attenuation;

        vec3 halfVector = normalize(directionToLight + viewDirection);

        // Dot products
        float NdotL = max(dot(surfaceNormal, directionToLight), 0.0);
        float NdotV = max(dot(surfaceNormal, viewDirection), 0.0);
        float NdotH = max(dot(surfaceNormal, halfVector), 0.0);
        float VdotH = max(dot(viewDirection, halfVector), 0.0);

        // SPECULAR
        // 1. Normal Distribution Function (GGX/Trowbridge-Reitz)
        float alpha = roughness * roughness;
        float alphaSquared = alpha * alpha;
        float denom = NdotH * NdotH * (alphaSquared - 1.0) + 1.0;
        float D = alphaSquared / (3.14159265359 * denom * denom);

        // 2. Simplified Fresnel (Schlick approximation)
        vec3 F = F0 + (1.0 - F0) * pow(1.0 - VdotH, 5.0);

        // 3. Geometry function (Smith's Schlick-GGX)
        float k = (roughness + 1.0) * (roughness + 1.0) / 8.0;
        float G_V = NdotV / (NdotV * (1.0 - k) + k);
        float G_L = NdotL / (NdotL * (1.0 - k) + k);
        float G = G_V * G_L;

        // 4. Cook-Torrance BRDF
        vec3 specular = (D * F * G) / max(4.0 * NdotV * NdotL, 0.001);

        // 5. Energy conservation (kD + kS = 1)
        vec3 kS = F; // Specular contribution
        vec3 kD = (1.0 - kS) * (1.0 - metallic); // Diffuse contribution (metals = no diffuse)

        // 6. Lambertian diffuse
        vec3 diffuse = kD * texColor.rgb / 3.14159265359;

        // Combine diffuse and specular
        vec3 BRDF = diffuse + specular;

        // Add to accumulator
        float lightShadow = (i == 0) ? shadow : 1.0;
        diffuseLight += BRDF * radiance * NdotL * lightShadow;
    }

    // Add ambient lighting
    vec3 ambient = ubo.ambientLightColor.xyz * ubo.ambientLightColor.w * texColor.rgb;
    vec3 finalColor = ambient + diffuseLight;

    outColor = vec4(finalColor * fragColor, texColor.a);
}
