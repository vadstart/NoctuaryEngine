#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

namespace nt {

class NtCamera {
public:

  // struct OrbitCameraController {
  //   float yaw = glm::radians(45.0f);
  //   float pitch = glm::radians(-30.0f);
  //   float distance = 5.0f;
  //
  //   glm::vec3 target{0.f, 0.f, 0.f};
  //
  //   float zoomSpeed = 1.0f;
  //   float orbitSpeed = 0.005f;
  //   float panSpeed = 0.01f;
  //
  //   void update(NtWindow* window);
  // };

  void setOrthographicProjection(float left, float right, float top, float bottom, float near, float far);
  void setPerspectiveProjection(float fovy, float aspect, float near, float far);

  void setViewDirection(glm::vec3 position, glm::vec3 direction, glm::vec3 up = glm::vec3{0.f, -1.f, 0.f});
  void setViewTarget(glm::vec3 position, glm::vec3 target, glm::vec3 up = glm::vec3{0.f, -1.f, 0.f});
  void setViewYXZ(glm::vec3 position, glm::vec3 rotation);

  const glm::mat4& getProjection() const { return projectionMatrix; }
  const glm::mat4& getView() const { return viewMatrix; }

private:
  glm::mat4 projectionMatrix{1.f};
  glm::mat4 viewMatrix{1.f};
};


}
