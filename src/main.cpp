#include "astral_app.h"

#include <cstdlib>
#include <iostream>
#include <filesystem>
#include <stdexcept>

#if defined(__APPLE__)
#include <mach-o/dyld.h>
#elif defined(_WIN32)
#include <windows.h>
#elif defined(__linux__)
#include <unistd.h>
#include <limits.h>
#endif

void setWorkingDirectory() {
    char exePath[1024];

#if defined(__APPLE__)
    uint32_t size = sizeof(exePath);
    if (_NSGetExecutablePath(exePath, &size) == 0) {
        std::filesystem::path path = std::filesystem::path(exePath).parent_path();
        std::filesystem::current_path(path);
        std::cout << "Working directory set to: " << std::filesystem::current_path() << std::endl;
    } else {
        std::cerr << "Failed to get executable path on macOS." << std::endl;
    }
#elif defined(_WIN32)
    if (GetModuleFileNameA(NULL, exePath, sizeof(exePath))) {
        std::filesystem::path path = std::filesystem::path(exePath).parent_path();
        std::filesystem::current_path(path);
        std::cout << "Working directory set to: " << std::filesystem::current_path() << std::endl;
    } else {
        std::cerr << "Failed to get executable path on Windows." << std::endl;
    }
#elif defined(__linux__)
    ssize_t count = readlink("/proc/self/exe", exePath, sizeof(exePath));
    if (count != -1) {
        exePath[count] = '\0';
        std::filesystem::path path = std::filesystem::path(exePath).parent_path();
        std::filesystem::current_path(path);
        std::cout << "Working directory set to: " << std::filesystem::current_path() << std::endl;
    } else {
        std::cerr << "Failed to get executable path on Linux." << std::endl;
    }
#else
    std::cerr << "Unsupported platform." << std::endl;
#endif
}

int main() 
{
  setWorkingDirectory();

	nt::AstralApp app{};

  std::cout << "°˖✧ Welcome to the Noctuary Engine ✧˖°" << std::endl;

	try {
		app.run();
	}
	catch (const std::exception& e) {
		std::cerr << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
