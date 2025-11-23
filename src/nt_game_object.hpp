#pragma once

#include "nt_model.hpp"
#include "nt_image.hpp"

#include <glm/fwd.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <memory>
#include <unordered_map>

namespace nt {

struct TransformComponent {
  glm::vec3 translation{}; // position offset
  glm::vec3 scale{1.f, 1.f, 1.f};
  glm::vec3 rotation;

  // Tait-Bryan angles, Y(1), X(2), Z(3)
  // https://en.wikipedia.org/wiki/Euler_angles#Rotation_matrix
  glm::mat4 mat4();
  glm::mat3 normalMatrix();
};

struct PointLightComponent {
  float lightIntensity = 1.0f;
};

// struct AnimationComponent {
//     std::unique_ptr<NtBuffer> boneBuffer; // Storage bufer for bone matrices
//     VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
// };

class NtGameObject {
public:
  using id_t = unsigned int;
  using Map = std::unordered_map<id_t, NtGameObject>;

  static NtGameObject createGameObject(bool isObjCharacter = false) {
    static id_t currentId = 0;
    return NtGameObject{currentId++, isObjCharacter};
  }

  static NtGameObject makePointLight(float intensity = 5.0f, float radius = 0.1f, glm::vec3 color = glm::vec3(1.0f));

  NtGameObject(const NtGameObject &) = delete;
  NtGameObject &operator=(const NtGameObject &) = delete;
  NtGameObject(NtGameObject&&) = default;
  NtGameObject &operator=(NtGameObject&&) = default;

  id_t getId() { return id; }

  glm::vec3 color{};
  TransformComponent transform{};
  bool isCharacter = false;
  bool isDebugVisualization = false;

  std::shared_ptr<NtModel> model{};
  VkDescriptorSet materialDescriptorSet = VK_NULL_HANDLE;
  std::shared_ptr<NtImage> diffuseTexture{};
  std::shared_ptr<NtImage> normalTexture{};
  std::shared_ptr<NtImage> roughnessTexture{};
  std::unique_ptr<PointLightComponent> pointLight = nullptr;

  std::unique_ptr<NtAnimator> animator;

private:
  NtGameObject(id_t objId, bool isObjCharacter) : id{objId}, isCharacter{isObjCharacter} {}

  id_t id;
};

}
