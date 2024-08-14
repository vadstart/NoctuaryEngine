#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <string>
using std::string;

namespace nt
{
	class NtWindow
	{
	public:
		NtWindow(int w, int h, string name);
		~NtWindow();

		// Delete the Copy Constructor and Copy operator
		// Resource creation = initialization. Cleanups performed by destructors
		// For a case where NtWindow is copied and both copies point at the same glfwWindow(which can be terminated by the destructor of one of them)
		NtWindow(const NtWindow&) = delete;
		NtWindow& operator=(const NtWindow&) = delete;

		bool shouldClose() { return glfwWindowShouldClose(window); }

	private:
		void initWindow();

		const int width;
		const int height;

		string windowName;
		GLFWwindow* window;
	};
}
