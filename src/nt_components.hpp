#pragma once

#include "nt_material.hpp"
#include "nt_model.hpp"
#include "nt_types.hpp"
#include "nt_animator.hpp"

#include <glm/glm.hpp>
#include <cstddef>
#include <memory>

namespace nt {

struct cMeta {
    std::string name;
};

struct cTransform {
    glm::vec3 translation{};
    glm::vec3 rotation{};
    glm::vec3 scale{1.f, 1.f, 1.f};

    // Tait-Bryan angles, Y(1), X(2), Z(3)
    // https://en.wikipedia.org/wiki/Euler_angles#Rotation_matrix
    glm::mat4 mat4() const {
      const float c3 = glm::cos(rotation.z);
      const float s3 = glm::sin(rotation.z);
      const float c2 = glm::cos(rotation.x);
      const float s2 = glm::sin(rotation.x);
      const float c1 = glm::cos(rotation.y);
      const float s1 = glm::sin(rotation.y);
      return glm::mat4{
          {
              scale.x * (c1 * c3 + s1 * s2 * s3),
              scale.x * (c2 * s3),
              scale.x * (c1 * s2 * s3 - c3 * s1),
              0.0f,
          },
          {
              scale.y * (c3 * s1 * s2 - c1 * s3),
              scale.y * (c2 * c3),
              scale.y * (c1 * c3 * s2 + s1 * s3),
              0.0f,
          },
          {
              scale.z * (c2 * s1),
              scale.z * (-s2),
              scale.z * (c1 * c2),
              0.0f,
          },
          {translation.x, translation.y, translation.z, 1.0f}};
      };

    glm::mat3 normalMatrix() const {
      const float c3 = glm::cos(rotation.z);
      const float s3 = glm::sin(rotation.z);
      const float c2 = glm::cos(rotation.x);
      const float s2 = glm::sin(rotation.x);
      const float c1 = glm::cos(rotation.y);
      const float s1 = glm::sin(rotation.y);
      const glm::vec3 invScale = 1.0f / scale;

      return glm::mat3{
          {
              invScale.x * (c1 * c3 + s1 * s2 * s3),
              invScale.x * (c2 * s3),
              invScale.x * (c1 * s2 * s3 - c3 * s1),
          },
          {
              invScale.y * (c3 * s1 * s2 - c1 * s3),
              invScale.y * (c2 * c3),
              invScale.y * (c1 * c3 * s2 + s1 * s3),
          },
          {
              invScale.z * (c2 * s1),
              invScale.z * (-s2),
              invScale.z * (c1 * c2),
          },
      };
    };

    glm::vec3 getForward() const {
        // Forward vector from rotation
        glm::vec3 forward;
        forward.x = cos(rotation.y) * cos(rotation.x);
        forward.y = sin(rotation.x);
        forward.z = sin(rotation.y) * cos(rotation.x);
        return glm::normalize(forward);
    }

    glm::vec3 getRight() const {
        // Right vector is perpendicular to forward and world up
        glm::vec3 forward = getForward();
        glm::vec3 worldUp = glm::vec3(0.0f, 1.0f, 0.0f);
        return glm::normalize(glm::cross(forward, worldUp));
    }

    glm::vec3 getUp() const {
        // Up vector is perpendicular to forward and right
        glm::vec3 forward = getForward();
        glm::vec3 right = getRight();
        return glm::normalize(glm::cross(right, forward));
    }
};

struct cCamera {
    float fov{65.f};
    float aspect{1.77f};
    float near_clip{0.1f};
    float far_clip{100.f};
    glm::vec4 offset{0.0f, 0.0f, 0.0f, 5.0f};
    cTransform position;

    bool projectionDirty = true;
    // Type? (Orbital/FPS)
};

struct cLight {
    float intensity{1.0f};
    glm::vec3 color{1.0f};
    bool bCastShadows{false};
    eLightType type{eLightType::Point};
};

struct cModel {
    std::shared_ptr<NtModel> mesh;
    bool bDropShadow = false;

    // MaterialType getMaterialType() const { return mesh->getMaterialType(); }
};

struct cAnimator {
    std::shared_ptr<NtAnimator> animator = std::make_shared<NtAnimator>();

    // Helper
    void play(const std::string& animationName, bool loop = false) {
        animator->play(animationName, loop);
    }
};

struct cPlayerController {
    float moveSpeed = 5.0f;
    float rotationSpeed = 10.0f;
};

//------------------------------

struct cCollider {

};

struct cStats {

};

struct cAIagent {

};

struct cGlobalVolume {
    // Fog
    // Bloom
    // Tonemapping
    // CRT
    // Vertex Jitter
    // Affine Mapping
    // Dithering
    // Low Depth Precision
    // ? Depth Field
    // ? Ambient Light
};

}
