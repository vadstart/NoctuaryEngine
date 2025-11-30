#pragma once

#include "nt_ecs.hpp"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

namespace nt {

class CameraSystem : public NtSystem
{
public:
  CameraSystem(NtAstral* astral_ptr) : astral(astral_ptr) {};
  ~CameraSystem() {};

  void setPerspectiveProjection();

  void setViewDirection(glm::vec3 position, glm::vec3 direction, glm::vec3 up = glm::vec3{0.f, -1.f, 0.f});
  void setViewTarget(glm::vec3 position, glm::vec3 target, glm::vec3 up = glm::vec3{0.f, -1.f, 0.f});
  void setViewYXZ(glm::vec3 position, glm::vec3 rotation);
  void update();

  const glm::mat4& getProjection() const { return projectionMatrix; }
  const glm::mat4& getView() const { return viewMatrix; }
  const glm::mat4& getInverseView() const { return inverseViewMatrix; }
  NtEntity getActiveCamera() { return *entities.begin(); }

private:
  glm::mat4 projectionMatrix{1.f};
  glm::mat4 viewMatrix{1.f};
  glm::mat4 inverseViewMatrix{1.f};

  NtAstral *astral;
};


}
