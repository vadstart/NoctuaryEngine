#include "nt_input_system.hpp"
#include "nt_components.hpp"
#include "nt_log.hpp"
#include "nt_types.hpp"
#include <GLFW/glfw3.h>
#include <glm/geometric.hpp>
#include <glm/glm.hpp>
#include <iostream>
#include <limits>
#include <cmath>
#include <vulkan/vulkan_core.h>

namespace nt {

void InputSystem::update(float dt, float mouseScrollY)
{
    assert (*entities.begin() && "[ERROR] No appropriate entities in the InputSystem");

    checkGamepadConnection();

    updateCamControl(dt, mouseScrollY);
    updatePlayerControl(dt);
}

void InputSystem::windowKeyCallback(int key, int scancode, int action, int mods)
{
    auto window = ntWindow->getGLFWwindow();

    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
    		glfwSetWindowShouldClose(window, GLFW_TRUE);
    else if (key == GLFW_KEY_GRAVE_ACCENT && action == GLFW_PRESS && (mods & GLFW_MOD_SHIFT)) {
        if (bShowImGUI) {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            if (glfwRawMouseMotionSupported())
                glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
            bShowImGUI = false;
        }
        else {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            if (glfwRawMouseMotionSupported())
                glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_FALSE);
            bShowImGUI = true;
        }
    }
}

void InputSystem::updateCamControl(float dt, float mouseScrollY) {
    NtEntity camEntity = *entities.begin();
    GLFWwindow* window = ntWindow->getGLFWwindow();
    // ROTATION
    // - Mouse
    static double lastX = ntWindow->getExtent().width / 2.0, lastY = ntWindow->getExtent().height / 2.0;
    double xpos, ypos;
    glfwGetCursorPos(window, &xpos, &ypos);

    float deltaX = static_cast<float>(xpos - lastX);
    float deltaY = static_cast<float>(ypos - lastY);
    lastX = xpos;
    lastY = ypos;

    float zoomInput = mouseScrollY;

    bool middleMouse = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_MIDDLE) == GLFW_PRESS;
    bool alt = glfwGetKey(window, GLFW_KEY_LEFT_ALT) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_RIGHT_ALT) == GLFW_PRESS;
    bool shift = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS;
    bool bRightStick = false;

    if (gamepadConnected) {
        // Right stick for orbit/pan control
        float rightStickX = getGamepadAxis(GLFW_GAMEPAD_AXIS_RIGHT_X);
        float rightStickY = getGamepadAxis(GLFW_GAMEPAD_AXIS_RIGHT_Y);

        if (std::abs(rightStickX) > 0.0f || std::abs(rightStickY) > 0.0f) {
            deltaX = rightStickX;
            deltaY = rightStickY;
            bRightStick = true;
        }

        // Zoom with triggers
        float leftTrigger = getGamepadAxis(GLFW_GAMEPAD_AXIS_LEFT_TRIGGER);
        float rightTrigger = getGamepadAxis(GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER);

        if (std::abs(rightTrigger - leftTrigger) > 0.0f) {
            zoomInput = (rightTrigger - leftTrigger) * dt;
        }
    }

    auto& transform = nexus->GetComponent<cTransform>(camEntity);
    auto& camera = nexus->GetComponent<cCamera>(camEntity);

    if (middleMouse || alt || bRightStick)
    {
        // Orbit around the target
        camera.position.rotation.y -= deltaX * orbitSpeed * dt;
        camera.position.rotation.y = glm::mod(camera.position.rotation.y, glm::two_pi<float>());
        camera.position.rotation.x += deltaY * orbitSpeed * dt;
        camera.position.rotation.x = glm::clamp(camera.position.rotation.x, -glm::half_pi<float>() + 0.01f, glm::half_pi<float>() - 0.01f);
    }

    // Zoom (scroll wheel)
    zoomInput = zoomInput * zoomSpeed;
    camera.offset.w -= zoomInput;
    camera.offset.w = glm::clamp(camera.offset.w, 2.0f, 25.0f);

    // Calculate camera orientation vectors
    float yaw = camera.position.rotation.y;
    float pitch = camera.position.rotation.x;

    // Camera right vector (for X offset)
    glm::vec3 right = glm::vec3(glm::cos(yaw), 0.0f, -glm::sin(yaw));

    // Camera up vector (for Y offset) - always world up for orbital cameras
    glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);

    // Camera forward vector (for Z offset)
    glm::vec3 forward = glm::vec3(glm::sin(yaw), 0.0f, glm::cos(yaw));

    // Transform offset from camera space to world space
    glm::vec3 worldOffset = right * camera.offset.x + up * camera.offset.y + forward * camera.offset.z;

    // Calculate target position with offset in camera space
    glm::vec3 targetPos = transform.translation + worldOffset;

    // Convert spherical to Cartesian coordinates around the offset target
    camera.position.translation.x = targetPos.x + camera.offset.w * glm::cos(pitch) * glm::sin(yaw);
    camera.position.translation.y = targetPos.y + camera.offset.w * glm::sin(pitch);
    camera.position.translation.z = targetPos.z + camera.offset.w * glm::cos(pitch) * glm::cos(yaw);

}

void InputSystem::updatePlayerControl(float dt) {
    NtEntity camEntity = *entities.begin();
    GLFWwindow* window = ntWindow->getGLFWwindow();

    // PLAYER
    // Get input
    float x{0.0f};
    float y{0.0f};

    auto& playerController = nexus->GetComponent<cPlayerController>(camEntity);

    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        x = -1.0f;
    else if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        x = 1.0f;
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        y = -1.0f;
    else if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        y = 1.0f;

    // Gamepad: use radial deadzone on the full 2D stick vector
    float stickMagnitude = 0.0f;
    if (gamepadConnected) {
        glm::vec2 stick = getGamepadStick(GLFW_GAMEPAD_AXIS_LEFT_X, GLFW_GAMEPAD_AXIS_LEFT_Y);
        x += stick.x;
        y += stick.y;
        stickMagnitude = glm::length(stick);
    }

    // Get camera to calculate movement direction
    auto& camera = nexus->GetComponent<cCamera>(camEntity);
    auto& playerTransform = nexus->GetComponent<cTransform>(camEntity);

    float yaw = camera.position.rotation.y;

    glm::vec3 forward = glm::vec3(glm::sin(yaw), 0.0f, glm::cos(yaw));
    glm::vec3 right = glm::vec3(glm::cos(yaw), 0.0f, -glm::sin(yaw));

    // Calculate movement direction based on input.
    // For keyboard, x/y are already 0 or ±1 so rawLen will be 1.
    // For gamepad, stickMagnitude [0,1] is used to scale velocity proportionally.
    glm::vec3 rawMove = forward * y + right * x;
    float rawLen = glm::length(rawMove);
    glm::vec3 moveDirection = (rawLen > 0.001f) ? rawMove / rawLen : glm::vec3(0.0f);
    float speedScale = (gamepadConnected && stickMagnitude > 0.001f)
        ? glm::min(stickMagnitude, 1.0f)
        : glm::min(rawLen, 1.0f);

    // Physics-based movement
    if (nexus->HasComponent<cCharacterPhysics>(camEntity)) {
        auto& charPhys = nexus->GetComponent<cCharacterPhysics>(camEntity);

        // Set desired velocity (physics system will apply it)
        charPhys.desiredVelocity = moveDirection * playerController.moveSpeed * speedScale;

        // JUMP
        bool jumpButtonPressed = glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS;
        if (gamepadConnected) {
            jumpButtonPressed = jumpButtonPressed || isGamepadButtonPressed(GLFW_GAMEPAD_BUTTON_A);
        }

        // Detect jump button press
        bool jumpJustPressed = jumpButtonPressed && !jumpButtonWasPressedLastFrame;
        jumpButtonWasPressedLastFrame = jumpButtonPressed;

        if (jumpJustPressed && charPhys.isGrounded) {
            charPhys.wantsJump = true;
        }
    }

    // Rotate player to face movement direction
    if (glm::length(moveDirection) > 0.001f) {
        float targetAngle = std::atan2(moveDirection.x, moveDirection.z);

        // Smooth rotation
        float currentAngle = playerTransform.rotation.y;
        float angleDiff = targetAngle - currentAngle;

        // Normalize angle difference to [-PI, PI]
        while (angleDiff > glm::pi<float>()) angleDiff -= 2.0f * glm::pi<float>();
        while (angleDiff < -glm::pi<float>()) angleDiff += 2.0f * glm::pi<float>();

        // Apply smooth rotation
        float rotationStep = playerController.rotationSpeed * dt;
        if (std::abs(angleDiff) < rotationStep) {
            playerTransform.rotation.y = targetAngle;
        } else {
            playerTransform.rotation.y += std::copysign(rotationStep, angleDiff);
        }
    }
}

void InputSystem::checkGamepadConnection() {
    gamepadConnected = false;
    connectedGamepadId = -1;

    for (int jid = GLFW_JOYSTICK_1; jid <= GLFW_JOYSTICK_LAST; ++jid) {
        if (glfwJoystickPresent(jid) && glfwJoystickIsGamepad(jid)) {
            gamepadConnected = true;
            connectedGamepadId = jid;
            break;
        }
    }
}

bool InputSystem::isGamepadButtonPressed(int button) {
    if (!gamepadConnected) return false;

    GLFWgamepadstate state;
    if (glfwGetGamepadState(connectedGamepadId, &state)) {
        return state.buttons[button] == GLFW_PRESS;
    }
    return false;
}

float InputSystem::getGamepadAxis(int axis) {
    if (!gamepadConnected) return 0.0f;

    GLFWgamepadstate state;
    if (glfwGetGamepadState(connectedGamepadId, &state)) {
        float value = state.axes[axis];
        // Apply deadzone
        if (std::abs(value) < gamepadDeadzone) {
            return 0.0f;
        }
        // Normalize beyond deadzone
        if (value > 0) {
            return (value - gamepadDeadzone) / (1.0f - gamepadDeadzone);
        } else {
            return (value + gamepadDeadzone) / (1.0f - gamepadDeadzone);
        }
    }
    return 0.0f;
}

glm::vec2 InputSystem::getGamepadStick(int axisX, int axisY) {
    if (!gamepadConnected) return glm::vec2(0.0f);

    GLFWgamepadstate state;
    if (!glfwGetGamepadState(connectedGamepadId, &state))
        return glm::vec2(0.0f);

    glm::vec2 raw(state.axes[axisX], state.axes[axisY]);
    float magnitude = glm::length(raw);

    if (magnitude < gamepadDeadzone)
        return glm::vec2(0.0f);

    // Rescale so the usable range is [0, 1] and clamp to unit circle
    float scaled = glm::min((magnitude - gamepadDeadzone) / (1.0f - gamepadDeadzone), 1.0f);
    return (raw / magnitude) * scaled;
}

} // namespace nt
