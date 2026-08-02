#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <functional>
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

		bool shouldClose() { return glfwWindowShouldClose(window_); }
        VkExtent2D getExtent() { return {static_cast<uint32_t>(width), static_cast<uint32_t>(height)}; }
        double getAspectRatio() const { return aspectRatio; }
        bool wasWindowResized() { return framebufferResized; }
        void resetWindowResizedFlag() { framebufferResized = false; }

        void createWindowSurface(VkInstance instance, VkSurfaceKHR *surface);
        void setKeyCallback(std::function<void(int key, int scancode, int action, int mods)> callback);

        GLFWwindow* getGLFWwindow() const { return window_; }

	private:
		void initWindow();

		static void framebufferResizeCallback(GLFWwindow* window, int width, int height);
		static void keyCallbackTrampoline(GLFWwindow* window, int key, int scancode, int action, int mods);

		int width;
		int height;
        bool framebufferResized = false;
        double aspectRatio;

		string windowName;
		GLFWwindow* window_;
		std::function<void(int, int, int, int)> keyCallback_;
	};
}
