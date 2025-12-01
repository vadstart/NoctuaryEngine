#pragma once

#include "nt_ecs.hpp"
#include "nt_window.hpp"
#include "nt_types.hpp"
#include <GLFW/glfw3.h>

namespace nt {

class InputSystem : public NtSystem
{
  public:
    InputSystem(NtAstral* astral_ptr) : astral(astral_ptr) {};
    ~InputSystem() {};
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

      struct GamepadMappings {
        int moveLeftStick = 0;  // Left stick X/Y axes (0, 1)
        int lookRightStick = 2; // Right stick X/Y axes (2, 3)
        int moveUp = GLFW_GAMEPAD_BUTTON_RIGHT_BUMPER;
        int moveDown = GLFW_GAMEPAD_BUTTON_LEFT_BUMPER;
        int toggleCursor = GLFW_GAMEPAD_BUTTON_START;
        int orbitModifier = GLFW_GAMEPAD_BUTTON_A;
        int panModifier = GLFW_GAMEPAD_BUTTON_B;
      };

    bool gamepadConnected{false};
    int connectedGamepadId{-1};

    void update(NtWindow* ntWindow, float dt, float mouseScrollY, NtEntity camEntity, NtEntity playerEntity);
    // void updateCamFPS(NtWindow* ntWindow, NtGameObject& cameraObject, float dt);
    void updateCamOrbit(NtWindow* ntWindow, float dt, float mouseScrollY, NtEntity camEntity);

    // Gamepad methods
    void checkGamepadConnection();
    bool isGamepadButtonPressed(int button);
    float getGamepadAxis(int axis);
    // void updateCamFPSGamepad(NtGameObject& cameraObject, float dt);
    void updateCamOrbitGamepad(float dt, NtEntity camEntity);

    // Runtime configuration methods
    void setGamepadSensitivity(float sensitivity) { gamepadSensitivity = sensitivity; }
    void setGamepadMoveSpeed(float speed) { gamepadMoveSpeed = speed; }
    void setGamepadDeadzone(float deadzone) { gamepadDeadzone = deadzone; }
    void setGamepadZoomSpeed(float speed) { gamepadZoomSpeed = speed; }

    float getGamepadSensitivity() const { return gamepadSensitivity; }
    float getGamepadMoveSpeed() const { return gamepadMoveSpeed; }
    float getGamepadDeadzone() const { return gamepadDeadzone; }
    float getGamepadZoomSpeed() const { return gamepadZoomSpeed; }

private:
  KeyMappings keys{};
  GamepadMappings gamepad{};

  float distance{2.0f};

  float moveSpeed{25.0f};
  float lookSpeed{1.5f};

  const float sensitivity { 0.002f };
  const float zoomSpeed { .3f };
  const float orbitSpeed { 0.005f };
  const float panSpeed = { 0.005f };

  // Gamepad settings (configurable at runtime)
  float gamepadSensitivity { 2.0f };
  float gamepadMoveSpeed { 25.0f };
  float gamepadDeadzone { 0.15f };
  float gamepadZoomSpeed { 2.0f };

  NtAstral* astral;
};

}
