#include "nt_input.hpp"
#include "nt_game_object.hpp"
#include <glm/geometric.hpp>
#include <iostream>
#include <limits>

namespace nt {

void NtInputController::update(NtWindow* ntWindow, NtGameObject& gameObject, NtGameObject& targetObject, float dt, float mouseScrollY) 
{
  GLFWwindow* window = ntWindow->getGLFWwindow();
  // ROTATION
  // - Keyboard
  // glm::vec3 rotate{0};
  // if (glfwGetKey(window, keys.lookRight) == GLFW_PRESS) rotate.y += 1.f;
  // if (glfwGetKey(window, keys.lookLeft) == GLFW_PRESS) rotate.y -= 1.f;
  // if (glfwGetKey(window, keys.lookUp) == GLFW_PRESS) rotate.x += 1.f;
  // if (glfwGetKey(window, keys.lookDown) == GLFW_PRESS) rotate.x -= 1.f;

  // - Mouse
  static double lastX = ntWindow->getExtent().width / 2.0, lastY = ntWindow->getExtent().height / 2.0;
  double xpos, ypos;
  glfwGetCursorPos(window, &xpos, &ypos);

  float deltaX = static_cast<float>(xpos - lastX);
  float deltaY = static_cast<float>(ypos - lastY);
  lastX = xpos;
  lastY = ypos;

  bool middleMouse = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_MIDDLE) == GLFW_PRESS;
  bool alt = glfwGetKey(window, GLFW_KEY_LEFT_ALT) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_RIGHT_ALT) == GLFW_PRESS;;
  bool shift = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS;
  
  if (middleMouse || alt) {
    if (shift) {
      // Pan the target point
      glm::vec3 right = glm::vec3{glm::cos(gameObject.transform.rotation.y), 0, -glm::sin(gameObject.transform.rotation.y)};
      glm::vec3 up = glm::vec3{0, 1, 0};
      targetObject.transform.translation -= -right * deltaX * panSpeed + up * deltaY * panSpeed;
    } else {
      // Orbit around the target
      gameObject.transform.rotation.y += deltaX * orbitSpeed;
      gameObject.transform.rotation.y = glm::mod(gameObject.transform.rotation.y, glm::two_pi<float>());
      gameObject.transform.rotation.x -= deltaY * orbitSpeed;
      gameObject.transform.rotation.x = glm::clamp(gameObject.transform.rotation.x, -glm::half_pi<float>() + 0.01f, glm::half_pi<float>() - 0.01f);
    }
  }

  // Zoom (scroll wheel)
  static float distance = 2.0f;
  static float orthoZoom = 1.0f;

  if (camType == CameraType::Perspective) {
    distance -= mouseScrollY * zoomSpeed;
    distance = glm::clamp(distance, 0.5f, 100.0f);
  } else {
    orthoZoom -= mouseScrollY * zoomSpeed * 0.2f;
    orthoZoom = glm::clamp(orthoZoom, 0.1f, 100.0f);
  }

  // Convert spherical to Cartesian coordinates
  float usedDistance = (camType == CameraType::Perspective) ? distance : 2.0f;
  gameObject.transform.translation.x = targetObject.transform.translation.x + usedDistance * glm::cos(gameObject.transform.rotation.x) * glm::sin(gameObject.transform.rotation.y);
  gameObject.transform.translation.y = targetObject.transform.translation.y + usedDistance * glm::sin(gameObject.transform.rotation.x);
  gameObject.transform.translation.z = targetObject.transform.translation.z + usedDistance * glm::cos(gameObject.transform.rotation.x) * glm::cos(gameObject.transform.rotation.y);
  
  this->orthoZoomLevel = orthoZoom;

  // gameObject.transform.rotation.y += deltaX * sensitivity;  // gameObject.transform.rotation.y
  // gameObject.transform.rotation.x -= deltaY * sensitivity;  // gameObject.transform.rotation.x
  //
  // // Clamp gameObject.transform.rotation.x to avoid gimbal lock
  // // gameObject.transform.rotation.x = glm::clamp(gameObject.transform.rotation.x, -glm::half_pi<float>() + 0.01f, glm::half_pi<float>() - 0.01f);
  //
  // if (glm::dot(rotate, rotate) > std::numeric_limits<float>::epsilon()) {
  //   gameObject.transform.rotation += lookSpeed * dt * glm::normalize(rotate);
  // }
  //
  // // limit gameObject.transform.rotation.x values between about +/- 85ish degrees
  // gameObject.transform.rotation.x = glm::clamp(gameObject.transform.rotation.x, -1.5f, 1.5f);
  // gameObject.transform.rotation.y = glm::mod(gameObject.transform.rotation.y, glm::two_pi<float>());
  //
  // // MOVEMENT
  // float gameObject.transform.rotation.y = gameObject.transform.rotation.y;
  // const glm::vec3 forwardDir{sin(gameObject.transform.rotation.y), 0.f, cos(gameObject.transform.rotation.y)};
  // const glm::vec3 rightDir{forwardDir.z, 0.f, -forwardDir.x};
  // const glm::vec3 upDir{0.f, -1.f, 0.f};
  //
  // glm::vec3 moveDir{0.f};
  // if (glfwGetKey(window, keys.moveForward) == GLFW_PRESS) moveDir += forwardDir;
  // if (glfwGetKey(window, keys.moveBackward) == GLFW_PRESS) moveDir -= forwardDir;
  // if (glfwGetKey(window, keys.moveRight) == GLFW_PRESS) moveDir += rightDir;
  // if (glfwGetKey(window, keys.moveLeft) == GLFW_PRESS) moveDir -= rightDir;
  // if (glfwGetKey(window, keys.moveUp) == GLFW_PRESS) moveDir += upDir;
  // if (glfwGetKey(window, keys.moveDown) == GLFW_PRESS) moveDir -= upDir;
  //
  // if (glm::dot(moveDir, moveDir) > std::numeric_limits<float>::epsilon()) {
  //   gameObject.transform.translation += moveSpeed * dt * glm::normalize(moveDir);
  // }

}

}
