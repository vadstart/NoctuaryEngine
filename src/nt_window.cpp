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
		glfwDestroyWindow(window);
		glfwTerminate();
	}

	void NtWindow::initWindow()
	{
		glfwInit();
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);	// No need for a standard OpenGL_API
		glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
		window = glfwCreateWindow(width, height, windowName.c_str(), nullptr, nullptr);
	}


  void NtWindow::createWindowSurface(VkInstance instance, VkSurfaceKHR *surface) {
    if (glfwCreateWindowSurface(instance, window, nullptr, surface) != VK_SUCCESS) {
      throw std::runtime_error("failed to create window surface");
    }
  }
}
