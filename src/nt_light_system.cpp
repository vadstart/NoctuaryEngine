#include "nt_light_system.hpp"
#include "nt_types.hpp"

namespace nt
{

void LightSystem::updateLights(FrameInfo &frameInfo, GlobalUbo &ubo, float O_scale, float O_near, float O_far) {
  int lightIndex = 0;
  for (auto const& entity : entities) {
    assert(lightIndex < MAX_LIGHTS && "Point lights exceed maximum specified!");

    auto& transform = nexus->GetComponent<cTransform>(entity);
    auto& light = nexus->GetComponent<cLight>(entity);

    // copy the light to ubo
    if (light.type != eLightType::Directional)
        ubo.pointLights[lightIndex].position = glm::vec4(transform.translation, 1.f);
    else ubo.pointLights[lightIndex].position = glm::vec4(transform.rotation, 1.f); // We only care about the rotation of directional light
    ubo.pointLights[lightIndex].color = glm::vec4(light.color, light.intensity);
    ubo.pointLights[lightIndex].lightType = static_cast<int>(light.type);

    // Shadows (computing light space matrix)
    if (light.bCastShadows) {
        switch(ubo.pointLights[lightIndex].lightType) {
            case 2: // DIRECTIONAL
                glm::vec3 lightPos = glm::vec3(ubo.pointLights[lightIndex].position);  // For directional, position represents direction
                glm::vec3 sceneCenter = glm::vec3(0.0f, 0.0f, 0.0f);
                glm::vec3 lightDir = glm::normalize(lightPos);

                // Use standard up vector
                glm::vec3 upVector = glm::vec3(0.0f, 1.0f, 0.0f);
                // Avoid gimbal lock if light direction is parallel to up
                if (abs(glm::dot(lightDir, upVector)) > 0.99f) {
                    upVector = glm::vec3(1.0f, 0.0f, 0.0f);
                }

                // Store light direction for shader
                ubo.shadowLightDirection = glm::vec4(lightDir, static_cast<float>(eLightType::Directional));
                glm::mat4 lightViewMatrix = glm::lookAt(lightDir, sceneCenter, upVector);

                // Vulcan clip space correction matrix
                // Converts from OpenGL [-1, 1] to Vulkan [0, 1] depth and flips Y
                glm::mat4 clip = glm::mat4(1.0f, 0.0f, 0.0f, 0.0f,
                                            0.0f,-1.0f, 0.0f, 0.0f,    // Flip Y
                                            0.0f, 0.0f, 0.5f, 0.0f,    // Scale Z from [-1, 1] to [0, 1]
                                            0.0f, 0.0f, 0.5f, 1.0f);   // Translate Z

                float orthoSize = O_scale;
                glm::mat4 lightProjection = clip * glm::ortho(
                    -orthoSize, orthoSize,
                    -orthoSize, orthoSize,
                    O_near, O_far
                );
                lightProjection[1][1] *= -1;

                ubo.lightSpaceMatrix = lightProjection * lightViewMatrix;
            break;
        }
    }
    else {
         ubo.shadowLightDirection = glm::vec4(0.0f, 0.0f, 0.0f, -1.0f);
    }

    lightIndex += 1;
  }
  ubo.numLights = lightIndex;
}

}
