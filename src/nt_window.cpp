#include "nt_window.h"
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
			window_ = glfwCreateWindow(width, height, windowName.c_str(), nullptr, nullptr);

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
		if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
			glfwSetWindowShouldClose(window, GLFW_TRUE);
	}

  void NtWindow::createWindowSurface(VkInstance instance, VkSurfaceKHR *surface) {
    if (glfwCreateWindowSurface(instance, window_, nullptr, surface) != VK_SUCCESS) {
      throw std::runtime_error("failed to create window surface");
    }
  }
}
