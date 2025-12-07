#pragma once

#include "astral_app.hpp"
#include "nt_ecs.hpp"
#include "nt_window.hpp"
#include "nt_types.hpp"
#include <GLFW/glfw3.h>

namespace nt {

class InputSystem : public NtSystem
{
  public:
    InputSystem(NtNexus* nexus_ptr, NtWindow* ntWindow_ptr) : nexus(nexus_ptr), ntWindow(ntWindow_ptr)
    {
        auto* window = ntWindow->getGLFWwindow();
        glfwSetWindowUserPointer(ntWindow->getGLFWwindow(), this);

        glfwSetKeyCallback(window, [](GLFWwindow* w, int key, int scancode, int action, int mods) {
                auto* self = static_cast<InputSystem*>(glfwGetWindowUserPointer(w));
                if (self) self->windowKeyCallback(key, scancode, action, mods);
            });
    };

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

    void update(float dt, float mouseScrollY);
    void updateCamControl(float dt, float mouseScrollY);
    void updatePlayerControl(float dt);

    // Gamepad methods
    void checkGamepadConnection();
    bool isGamepadButtonPressed(int button);
    float getGamepadAxis(int axis);
    // Runtime configuration methods
    void setGamepadDeadzone(float deadzone) { gamepadDeadzone = deadzone; }
    float getGamepadDeadzone() const { return gamepadDeadzone; }

    bool bShowImGUI = true;

private:
  KeyMappings keys{};
  GamepadMappings gamepad{};

  void windowKeyCallback(int key, int scancode, int action, int mods);

  // const float sensitivity { 0.002f };
  const float zoomSpeed { 2.0f };
  const float orbitSpeed { 2.0f };
  // const float panSpeed = { 0.005f };

  // Gamepad settings (configurable at runtime)
  // float gamepadSensitivity { 2.0f };
  // float gamepadMoveSpeed { 25.0f };
  float gamepadDeadzone { 0.15f };
  // float gamepadZoomSpeed { 2.0f };

  NtWindow* ntWindow;
  NtNexus* nexus;
};

}
