#include "astral_app.hpp"
#include "nt_log.hpp"

#include <cstdlib>
#include <iostream>
#include <filesystem>

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
        NT_LOG_VERBOSE(LogCore, "Working directory set to: {}", std::filesystem::current_path().c_str());
    } else {
        NT_LOG_ERROR(LogCore, "Failed to get executable path on macOS.");
    }
#elif defined(_WIN32)
    if (GetModuleFileNameA(NULL, exePath, sizeof(exePath))) {
        // std::filesystem::path path = std::filesystem::path(exePath).parent_path();
        std::filesystem::path path = "C:\\Users\\vadsama\\Documents\\Projects\\NoctuaryEngine";
        std::filesystem::current_path(path);
        NT_LOG_VERBOSE(LogCore, "Working directory set to: {}", std::filesystem::current_path().c_str());
    } else {
        NT_LOG_ERROR(LogCore, "Failed to get executable path on Windows.");
    }
#elif defined(__linux__)
    ssize_t count = readlink("/proc/self/exe", exePath, sizeof(exePath));
    if (count != -1) {
        exePath[count] = '\0';
        std::filesystem::path path = std::filesystem::path(exePath).parent_path();
        std::filesystem::current_path(path);
        NT_LOG_VERBOSE(LogCore, "Working directory set to: {}", std::filesystem::current_path().c_str());
    } else {
        NT_LOG_ERROR(LogCore, "Failed to get executable path on Linux.");
    }
#else
    NT_LOG_FATAL(LogCore, "Unsupported platform.");
#endif
}

int main()
{
  nt::LogInit("engine.log", true);

  setWorkingDirectory();

  nt::AstralApp app{};

  std::cout << "°˖  Welcome to the Noctuary Engine  ˖°" << std::endl;

	try {
		app.run();
	}
	catch (const std::exception& e) {
	    NT_LOG_FATAL(LogCore, "Application crashed: {}", e.what());
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
