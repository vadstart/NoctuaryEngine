#include "nt_window.hpp"
#include <GLFW/glfw3.h>
#include <stdexcept>

namespace nt
{
NtWindow::NtWindow(int w, int h, string name) : width{w}, height{h}, windowName{name}
{
	initWindow();
}

NtWindow::~NtWindow()
{
	glfwDestroyWindow(window_);
	glfwTerminate();
}

void NtWindow::initWindow()
{
	glfwInit();
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);	// No need for a standard OpenGL_API
	glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
	window_ = glfwCreateWindow(width, height, windowName.c_str(), nullptr, nullptr);

	glfwSetInputMode(window_, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	glfwSetWindowUserPointer (window_, this);
	glfwSetFramebufferSizeCallback(window_, framebufferResizeCallback);

	glfwSetKeyCallback(window_, keyCallback);
}

void NtWindow::framebufferResizeCallback(GLFWwindow* window, int width, int height) {
	auto ntWindow = reinterpret_cast<NtWindow *>(glfwGetWindowUserPointer(window));
	ntWindow->framebufferResized = true;
    ntWindow->width = width;
    ntWindow->height = height;
}

void NtWindow::keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    auto ntWindow = reinterpret_cast<NtWindow *>(glfwGetWindowUserPointer(window));
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
    		glfwSetWindowShouldClose(window, GLFW_TRUE);
    else if (key == GLFW_KEY_GRAVE_ACCENT  && action == GLFW_PRESS) {
        if (mods & GLFW_MOD_SHIFT)
        ntWindow->bShowImGUI = !ntWindow->bShowImGUI;
    }
    else if (key == GLFW_KEY_TAB && action == GLFW_PRESS) {
        if (ntWindow->bShowCursor) {
            if (glfwRawMouseMotionSupported())
                glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            ntWindow->bShowCursor = false;
        }
        else {
            glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_FALSE);
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            ntWindow->bShowCursor = true;
        }
    }
}

void NtWindow::createWindowSurface(VkInstance instance, VkSurfaceKHR *surface) {
    if (glfwCreateWindowSurface(instance, window_, nullptr, surface) != VK_SUCCESS) {
        throw std::runtime_error("failed to create window surface");
    }
}
}
