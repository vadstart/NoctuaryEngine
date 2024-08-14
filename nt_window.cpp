#include "nt_window.h"

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
}