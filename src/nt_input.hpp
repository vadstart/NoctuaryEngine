#pragma once

#include "nt_window.hpp"
#include "nt_game_object.hpp"
#include "nt_types.hpp"

namespace nt {

class NtInputController {
  public:
      struct KeyMappings {
        int moveLeft = GLFW_KEY_A;
        int moveRight = GLFW_KEY_D;
        int moveForward = GLFW_KEY_W;
        int moveBackward = GLFW_KEY_S;
        int moveUp = GLFW_KEY_E;
        int moveDown = GLFW_KEY_Q;
        int lookLeft = GLFW_KEY_LEFT;
        int lookRight = GLFW_KEY_RIGHT;
        int lookUp = GLFW_KEY_UP;
        int lookDown = GLFW_KEY_DOWN;
        int toggleCursor = GLFW_KEY_TAB;
      };

    float orthoZoomLevel{0.0f};
    bool cursorLocked{true};

    void update(NtWindow* ntWindow, NtGameObject& gameObject, NtGameObject& targetObject, float dt, float mouseScrollY,
        CameraProjectionType camProjType, CameraControlType camControlType);
    void updateCamFPS(NtWindow* ntWindow, NtGameObject& cameraObject, float dt);
    void updateCamOrbit(NtWindow* ntWindow, NtGameObject& cameraObject, NtGameObject& targetObject, float dt, float mouseScrollY,
        CameraProjectionType projectionType);

private:
  KeyMappings keys{};

  float moveSpeed{25.0f};
  float lookSpeed{1.5f};

  const float sensitivity { 0.002f };
  const float zoomSpeed { .3f };
  const float orbitSpeed { 0.005f };
  const float panSpeed = { 0.005f };
};

}
