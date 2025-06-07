#pragma once

#include "nt_model.hpp"
#include "nt_image.hpp"

#include <glm/gtc/matrix_transform.hpp>

// std
#include <memory>

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

class NtGameObject {
public:
  using id_t = unsigned int;

  static NtGameObject createGameObject() {
    static id_t currentId = 0;
    return NtGameObject{currentId++};
  }

  NtGameObject(const NtGameObject &) = delete;
  NtGameObject &operator=(const NtGameObject &) = delete;
  NtGameObject(NtGameObject&&) = default;
  NtGameObject &operator=(NtGameObject&&) = default;

  id_t getId() { return id; }

  std::shared_ptr<NtModel> model{};
  VkDescriptorSet materialDescriptorSet = VK_NULL_HANDLE;
  std::shared_ptr<NtImage> texture{};
  glm::vec3 color{};
  TransformComponent transform{};

private:
  NtGameObject(id_t objId) : id{objId} {}

  id_t id;
};

}
