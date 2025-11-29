#pragma once

#include "nt_model.hpp"
#include "nt_types.hpp"

#include <glm/glm.hpp>
#include <cstddef>
#include <memory>

namespace nt {

struct cName {
    std::string name;
};

struct cTransform {
    glm::vec3 translation{};
    glm::vec3 rotation{};
    glm::vec3 scale{1.f, 1.f, 1.f};
};

struct cCamera {
    // FOV
    // Clipping Planes
    // Type? (Orbital/FPS)
};

struct cLight {
    float intensity{1.0f};
    glm::vec3 color{1.0f};
    eLightType type{eLightType::Point};
};

struct cModel {
    std::shared_ptr<NtModel> mesh;
    bool bDropShadow = false;
};

//------------------------------

struct cPlayerController {
    float moveSpeed = 5.0f;
    float rotationSpeed = 10.0f;
};

struct cAnimator {
    //NtAnimator animator = std::make_unique<NtAnimator>(*go_Cassandra.model);
};

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
