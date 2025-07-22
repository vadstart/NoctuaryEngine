#include "nt_input.hpp"
#include "nt_game_object.hpp"
#include "nt_types.hpp"
#include <glm/geometric.hpp>
#include <iostream>
#include <limits>

namespace nt {

void NtInputController::update(NtWindow* ntWindow, NtGameObject& gameObject, NtGameObject& targetObject, float dt, float mouseScrollY,
    CameraProjectionType camProjType, CameraControlType camControlType)
{
    if (camControlType == CameraControlType::FPS) {
        updateCamFPS(ntWindow, gameObject, dt);
    } else {
        updateCamOrbit(ntWindow, gameObject, targetObject, dt, mouseScrollY, camProjType);
    }
}

void NtInputController::updateCamFPS(NtWindow* ntWindow, NtGameObject& cameraObject, float dt) {
    GLFWwindow* window = ntWindow->getGLFWwindow();
    
    // Handle cursor toggle
    static bool previousToggleState = false;
    bool currentToggleState = glfwGetKey(window, keys.toggleCursor) == GLFW_PRESS;
    if (currentToggleState && !previousToggleState) {
        cursorLocked = !cursorLocked;
        if (cursorLocked) {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        } else {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        }
    }
    previousToggleState = currentToggleState;
    
    // Initialize cursor lock if not already done
    static bool cursorLockInitialized = false;
    if (!cursorLockInitialized) {
        if (cursorLocked) {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        }
        cursorLockInitialized = true;
    }

    // Mouse look - only when cursor is locked
    if (cursorLocked) {
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
}

void NtInputController::updateCamOrbit(NtWindow* ntWindow, NtGameObject& cameraObject, NtGameObject& targetObject, float dt, float mouseScrollY,
    CameraProjectionType camProjType) {
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

    if (middleMouse || alt) {
        if (shift) {
        // Pan the target point
        glm::vec3 right = glm::vec3{glm::cos(cameraObject.transform.rotation.y), 0, -glm::sin(cameraObject.transform.rotation.y)};
        glm::vec3 up = glm::vec3{0, 1, 0};
        targetObject.transform.translation -= -right * deltaX * panSpeed + up * deltaY * panSpeed;
        } else {
        // Orbit around the target
        cameraObject.transform.rotation.y += deltaX * orbitSpeed;
        cameraObject.transform.rotation.y = glm::mod(cameraObject.transform.rotation.y, glm::two_pi<float>());
        cameraObject.transform.rotation.x -= deltaY * orbitSpeed;
        cameraObject.transform.rotation.x = glm::clamp(cameraObject.transform.rotation.x, -glm::half_pi<float>() + 0.01f, glm::half_pi<float>() - 0.01f);
        }
    }

    // Zoom (scroll wheel)
    static float distance = 2.0f;
    static float orthoZoom = 1.0f;

    if (camProjType == CameraProjectionType::Perspective) {
        distance -= mouseScrollY * zoomSpeed;
        distance = glm::clamp(distance, 0.5f, 100.0f);
    } else {
        orthoZoom -= mouseScrollY * zoomSpeed * 0.2f;
        orthoZoom = glm::clamp(orthoZoom, 0.1f, 100.0f);
    }

    // Convert spherical to Cartesian coordinates
    float usedDistance = (camProjType == CameraProjectionType::Perspective) ? distance : 2.0f;
    cameraObject.transform.translation.x = targetObject.transform.translation.x + usedDistance * glm::cos(cameraObject.transform.rotation.x) * glm::sin(cameraObject.transform.rotation.y);
    cameraObject.transform.translation.y = targetObject.transform.translation.y + usedDistance * glm::sin(cameraObject.transform.rotation.x);
    cameraObject.transform.translation.z = targetObject.transform.translation.z + usedDistance * glm::cos(cameraObject.transform.rotation.x) * glm::cos(cameraObject.transform.rotation.y);

    this->orthoZoomLevel = orthoZoom;
}

} // namespace nt
