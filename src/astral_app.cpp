#include "astral_app.h"


namespace nt
{
	void AstralApp::run() {

		while (!ntWindow.shouldClose()) {
			glfwPollEvents();
		}
	}
}
