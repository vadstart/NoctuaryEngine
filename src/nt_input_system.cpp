#include "nt_input_system.hpp"
#include "nt_components.hpp"
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
    // if (camControlType == CameraControlType::FPS) {
    //     updateCamFPS(ntWindow, gameObject, dt);
    //     if (gamepadConnected) {
    //         updateCamFPSGamepad(gameObject, dt);
    //     }
    // } else {
    updateCamOrbit(ntWindow, dt, mouseScrollY, camEntity);
    if (gamepadConnected) {
        updateCamOrbitGamepad(dt, camEntity);
    }
    // }

    // PLAYER
    // Get input
    float x = -getGamepadAxis(GLFW_GAMEPAD_AXIS_LEFT_X);
    float y = -getGamepadAxis(GLFW_GAMEPAD_AXIS_LEFT_Y);

    // Move in camera direction
    auto& camera = astral->GetComponent<cCamera>(camEntity);
    glm::vec3 forward = camera.position.getForward();
    glm::vec3 right = camera.position.getRight();
    forward.y = 0.0f; right.y = 0.0f;
    forward = glm::normalize(forward);
    right = glm::normalize(right);

    auto& playerPos = astral->GetComponent<cTransform>(camEntity);
    glm::vec3 moveDirection = (forward * y + right * x);
    playerPos.translation += moveDirection * 5.0f * dt;

    // Rotate player to face movement direction
    if (glm::length(moveDirection) > 0.001f) {
        float targetAngle = std::atan2(-moveDirection.x, -moveDirection.z);

        // Smooth rotation (lerp)
        float currentAngle = playerPos.rotation.y;
        float angleDiff = targetAngle - currentAngle;

        // Normalize angle difference to [-PI, PI]
        while (angleDiff > glm::pi<float>()) angleDiff -= 2.0f * glm::pi<float>();
        while (angleDiff < -glm::pi<float>()) angleDiff += 2.0f * glm::pi<float>();

        // Apply smooth rotation
        float rotationStep = 5.0f * dt;
        if (std::abs(angleDiff) < rotationStep) {
            playerPos.rotation.y = targetAngle;
        } else {
            playerPos.rotation.y += std::copysign(rotationStep, angleDiff);
        }
    }
}

/*
void NtInputController::updateCamFPS(NtWindow* ntWindow, NtGameObject& cameraObject, float dt) {
    GLFWwindow* window = ntWindow->getGLFWwindow();

    // Mouse look - only when cursor is locked
    // if (cursorLocked) {
    if(!ntWindow->getShowCursor()) {
        static double lastX = ntWindow->getExtent().width / 2.0, lastY = ntWindow->getExtent().height / 2.0;
        double xpos, ypos;
        glfwGetCursorPos(window, &xpos, &ypos);

        float deltaX = static_cast<float>(xpos - lastX);
        float deltaY = static_cast<float>(ypos - lastY);
        lastX = xpos;
        lastY = ypos;

        cameraObject.transform.rotation.y += deltaX * sensitivity;
        cameraObject.transform.rotation.x -= deltaY * sensitivity;

        // limit rotation.x values between about +/- 85ish degrees
        cameraObject.transform.rotation.x = glm::clamp(cameraObject.transform.rotation.x, -1.5f, 1.5f);
        cameraObject.transform.rotation.y = glm::mod(cameraObject.transform.rotation.y, glm::two_pi<float>());
    }

    // MOVEMENT - Calculate direction vectors based on camera rotation
    const float cosY = glm::cos(cameraObject.transform.rotation.y);
    const float sinY = glm::sin(cameraObject.transform.rotation.y);
    const float cosX = glm::cos(cameraObject.transform.rotation.x);
    const float sinX = glm::sin(cameraObject.transform.rotation.x);

    // Forward direction considering both pitch and yaw
    const glm::vec3 forwardDir{sinY * cosX, -sinX, cosY * cosX};
    // Right direction (perpendicular to forward, on horizontal plane)
    const glm::vec3 rightDir{cosY, 0.f, -sinY};
    // Up direction (world up)
    const glm::vec3 upDir{0.f, 1.f, 0.f};

    glm::vec3 moveDir{0.f};
    if (glfwGetKey(window, keys.moveForward) == GLFW_PRESS) moveDir += forwardDir;
    if (glfwGetKey(window, keys.moveBackward) == GLFW_PRESS) moveDir -= forwardDir;
    if (glfwGetKey(window, keys.moveRight) == GLFW_PRESS) moveDir += rightDir;
    if (glfwGetKey(window, keys.moveLeft) == GLFW_PRESS) moveDir -= rightDir;
    if (glfwGetKey(window, keys.moveUp) == GLFW_PRESS) moveDir += upDir;
    if (glfwGetKey(window, keys.moveDown) == GLFW_PRESS) moveDir -= upDir;

    if (glm::dot(moveDir, moveDir) > std::numeric_limits<float>::epsilon()) {
      cameraObject.transform.translation += moveSpeed * dt * glm::normalize(moveDir);
    }
    }*/

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

    bool middleMouse = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_MIDDLE) == GLFW_PRESS;
    bool alt = glfwGetKey(window, GLFW_KEY_LEFT_ALT) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_RIGHT_ALT) == GLFW_PRESS;;
    bool shift = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS;

    auto& transform = astral->GetComponent<cTransform>(camEntity);
    auto& camera = astral->GetComponent<cCamera>(camEntity);

    if (middleMouse || alt)
    {
        // Orbit around the target
        camera.position.rotation.y += deltaX * orbitSpeed;
        camera.position.rotation.y = glm::mod(camera.position.rotation.y, glm::two_pi<float>());
        camera.position.rotation.x -= deltaY * orbitSpeed;
        camera.position.rotation.x = glm::clamp(camera.position.rotation.x, -glm::half_pi<float>() + 0.01f, glm::half_pi<float>() - 0.01f);
    }

    // Zoom (scroll wheel)
    camera.offset.w -= mouseScrollY * zoomSpeed;
    camera.offset.w = glm::clamp(camera.offset.w, 0.5f, 100.0f);

    // Convert spherical to Cartesian coordinates
    camera.position.translation.x = (transform.translation.x + camera.offset.x) + camera.offset.w * glm::cos(camera.position.rotation.x) * glm::sin(camera.position.rotation.y);
    camera.position.translation.y = (transform.translation.y + camera.offset.y) + camera.offset.w * glm::sin(camera.position.rotation.x);
    camera.position.translation.z = (transform.translation.z + camera.offset.z) + camera.offset.w * glm::cos(camera.position.rotation.x) * glm::cos(camera.position.rotation.y);
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

/*
void InputSystem::updateCamFPSGamepad(NtGameObject& cameraObject, float dt) {
    if (!gamepadConnected) return;

    // Movement with left stick
    float leftStickX = getGamepadAxis(GLFW_GAMEPAD_AXIS_LEFT_X);
    float leftStickY = getGamepadAxis(GLFW_GAMEPAD_AXIS_LEFT_Y);

    // Look with right stick
    float rightStickX = getGamepadAxis(GLFW_GAMEPAD_AXIS_RIGHT_X);
    float rightStickY = getGamepadAxis(GLFW_GAMEPAD_AXIS_RIGHT_Y);

    // Apply look input
    if (std::abs(rightStickX) > 0.0f || std::abs(rightStickY) > 0.0f) {
        cameraObject.transform.rotation.y += rightStickX * gamepadSensitivity * dt;
        cameraObject.transform.rotation.x -= rightStickY * gamepadSensitivity * dt;

        // Limit pitch rotation
        cameraObject.transform.rotation.x = glm::clamp(cameraObject.transform.rotation.x, -1.5f, 1.5f);
        cameraObject.transform.rotation.y = glm::mod(cameraObject.transform.rotation.y, glm::two_pi<float>());
    }

    // Calculate direction vectors based on camera rotation
    const float cosY = glm::cos(cameraObject.transform.rotation.y);
    const float sinY = glm::sin(cameraObject.transform.rotation.y);
    const float cosX = glm::cos(cameraObject.transform.rotation.x);
    const float sinX = glm::sin(cameraObject.transform.rotation.x);

    const glm::vec3 forwardDir{sinY * cosX, -sinX, cosY * cosX};
    const glm::vec3 rightDir{cosY, 0.f, -sinY};
    const glm::vec3 upDir{0.f, 1.f, 0.f};

    // Apply movement input
    glm::vec3 moveDir{0.f};
    if (std::abs(leftStickX) > 0.0f || std::abs(leftStickY) > 0.0f) {
        moveDir += rightDir * leftStickX;
        moveDir += forwardDir * (-leftStickY); // Invert Y for forward/backward
    }

    // Vertical movement with bumpers
    if (isGamepadButtonPressed(gamepad.moveUp)) moveDir += upDir;
    if (isGamepadButtonPressed(gamepad.moveDown)) moveDir -= upDir;

    if (glm::dot(moveDir, moveDir) > std::numeric_limits<float>::epsilon()) {
        cameraObject.transform.translation += gamepadMoveSpeed * dt * glm::normalize(moveDir);
    }
}
*/

void InputSystem::updateCamOrbitGamepad(float dt, NtEntity camEntity) {
    if (!gamepadConnected) return;

    // Right stick for orbit/pan control
    float rightStickX = getGamepadAxis(GLFW_GAMEPAD_AXIS_RIGHT_X);
    float rightStickY = getGamepadAxis(GLFW_GAMEPAD_AXIS_RIGHT_Y);

    // Left stick for zoom and additional controls
    float leftStickY = getGamepadAxis(GLFW_GAMEPAD_AXIS_LEFT_Y);

    bool orbitMode = isGamepadButtonPressed(gamepad.orbitModifier);
    // bool panMode = isGamepadButtonPressed(gamepad.panModifier);

    auto& transform = astral->GetComponent<cTransform>(camEntity);
    auto& camera = astral->GetComponent<cCamera>(camEntity);

    if (std::abs(rightStickX) > 0.0f || std::abs(rightStickY) > 0.0f) {
            // Orbit around the target
            camera.position.rotation.y += rightStickX * gamepadSensitivity * dt;
            camera.position.rotation.y = glm::mod(camera.position.rotation.y, glm::two_pi<float>());
            camera.position.rotation.x -= rightStickY * gamepadSensitivity * dt;
            camera.position.rotation.x = glm::clamp(camera.position.rotation.x, -glm::half_pi<float>() + 0.01f, glm::half_pi<float>() - 0.01f);
        // }
    }

    // Zoom with left stick Y or triggers
    // float zoomInput = -leftStickY;
    float leftTrigger = getGamepadAxis(GLFW_GAMEPAD_AXIS_LEFT_TRIGGER);
    float rightTrigger = getGamepadAxis(GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER);

    // Use triggers for zoom if stick isn't being used much
    // if (std::abs(zoomInput) < 0.3f) {
    float zoomInput = rightTrigger - leftTrigger;
    // }

    if (std::abs(zoomInput) > 0.0f) {
        // Apply zoom similar to mouse scroll
        float zoomAmount = zoomInput * gamepadSensitivity * 2 * dt;

        camera.offset.w -= zoomAmount;
        camera.offset.w = glm::clamp(camera.offset.w, 0.5f, 100.0f);

        // Update camera position based on new distance
        camera.position.translation.x = (transform.translation.x + camera.offset.x) + camera.offset.w * glm::cos(camera.position.rotation.x) * glm::sin(camera.position.rotation.y);
        camera.position.translation.y = (transform.translation.y + camera.offset.y) + camera.offset.w * glm::sin(camera.position.rotation.x);
        camera.position.translation.z = (transform.translation.z + camera.offset.z) + camera.offset.w * glm::cos(camera.position.rotation.x) * glm::cos(camera.position.rotation.y);
        }
    }

} // namespace nt
