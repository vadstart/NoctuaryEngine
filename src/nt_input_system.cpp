#include "nt_input_system.hpp"
#include "nt_components.hpp"
#include "nt_log.hpp"
#include "nt_types.hpp"
#include <glm/geometric.hpp>
#include <iostream>
#include <limits>
#include <cmath>

namespace nt {

void InputSystem::update(NtWindow* ntWindow, float dt, float mouseScrollY, NtEntity camEntity, NtEntity playerEntity)
{
    checkGamepadConnection();

    // CAMERA
    updateCamOrbit(ntWindow, dt, mouseScrollY, camEntity);
    updatePlayerPosition(ntWindow, dt, playerEntity, camEntity);
}

void InputSystem::updateCamOrbit(NtWindow* ntWindow, float dt, float mouseScrollY, NtEntity camEntity) {
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
    bool alt = glfwGetKey(window, GLFW_KEY_LEFT_ALT) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_RIGHT_ALT) == GLFW_PRESS;;
    bool shift = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS;
    bool bRightStick = false;

    if (gamepadConnected) {
        // Right stick for orbit/pan control
        float rightStickX = getGamepadAxis(GLFW_GAMEPAD_AXIS_RIGHT_X);
        float rightStickY = getGamepadAxis(GLFW_GAMEPAD_AXIS_RIGHT_Y);

        if (std::abs(rightStickX) > 0.0f || std::abs(rightStickY) > 0.0f) {
            deltaX = rightStickX * 2;
            deltaY = rightStickY * 2;
            bRightStick = true;
        }

        // Zoom with triggers
        float leftTrigger = getGamepadAxis(GLFW_GAMEPAD_AXIS_LEFT_TRIGGER);
        float rightTrigger = getGamepadAxis(GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER);

        if (std::abs(rightTrigger - leftTrigger) > 0.0f) {
            zoomInput = (rightTrigger - leftTrigger) * 2;
        }
        }

    auto& transform = nexus->GetComponent<cTransform>(camEntity);
    auto& camera = nexus->GetComponent<cCamera>(camEntity);

    if (middleMouse || alt || bRightStick)
    {
        // Orbit around the target
        camera.position.rotation.y += deltaX * orbitSpeed * dt;
        camera.position.rotation.y = glm::mod(camera.position.rotation.y, glm::two_pi<float>());
        camera.position.rotation.x -= deltaY * orbitSpeed * dt;
        camera.position.rotation.x = glm::clamp(camera.position.rotation.x, -glm::half_pi<float>() + 0.01f, glm::half_pi<float>() - 0.01f);
    }

    // Zoom (scroll wheel)
    // if (std::abs(zoomInput) > 0.0f)
    // {
        zoomInput = zoomInput * zoomSpeed * dt;
        camera.offset.w -= zoomInput;
        camera.offset.w = glm::clamp(camera.offset.w, 2.0f, 25.0f);

        // Convert spherical to Cartesian coordinates
        camera.position.translation.x = (transform.translation.x + camera.offset.x) + camera.offset.w * glm::cos(camera.position.rotation.x) * glm::sin(camera.position.rotation.y);
        camera.position.translation.y = (transform.translation.y + camera.offset.y) + camera.offset.w * glm::sin(camera.position.rotation.x);
        camera.position.translation.z = (transform.translation.z + camera.offset.z) + camera.offset.w * glm::cos(camera.position.rotation.x) * glm::cos(camera.position.rotation.y);
    // }
}

void InputSystem::updatePlayerPosition(NtWindow* ntWindow, float dt, NtEntity playerEntity, NtEntity camEntity) {
    GLFWwindow* window = ntWindow->getGLFWwindow();

    // PLAYER
    // Get input
    float x{0.0f};
    float y{0.0f};

    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        x = 1.0f;
    else if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        x = -1.0f;
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        y = 1.0f;
    else if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        y = -1.0f;

    if (gamepadConnected) {
        x += -getGamepadAxis(GLFW_GAMEPAD_AXIS_LEFT_X);
        y += -getGamepadAxis(GLFW_GAMEPAD_AXIS_LEFT_Y);
    }

    // Skip if no input
    if (std::abs(x) < 0.001f && std::abs(y) < 0.001f) return;

    // Get camera to calculate movement direction
    auto& camera = nexus->GetComponent<cCamera>(camEntity);
    // For orbital camera, calculate forward as direction FROM camera TO player
    auto& playerTransform = nexus->GetComponent<cTransform>(playerEntity);
    glm::vec3 cameraToPlayer = glm::normalize(playerTransform.translation - camera.position.translation);

    // Project onto horizontal plane
    glm::vec3 forward = glm::vec3(cameraToPlayer.x, 0.0f, cameraToPlayer.z);
    forward = glm::normalize(forward);

    // Right vector is perpendicular to forward on horizontal plane
    glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f)));

    // Calculate movement direction based on input
    glm::vec3 moveDirection = (forward * y + right * x);
    playerTransform.translation += moveDirection * 7.0f * dt;

    // Rotate player to face movement direction
    if (glm::length(moveDirection) > 0.001f) {
        float targetAngle = std::atan2(-moveDirection.x, -moveDirection.z);

        // Smooth rotation (lerp)
        float currentAngle = playerTransform.rotation.y;
        float angleDiff = targetAngle - currentAngle;

        // Normalize angle difference to [-PI, PI]
        while (angleDiff > glm::pi<float>()) angleDiff -= 2.0f * glm::pi<float>();
        while (angleDiff < -glm::pi<float>()) angleDiff += 2.0f * glm::pi<float>();

        // Apply smooth rotation
        float rotationStep = 5.0f * dt;
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

} // namespace nt
