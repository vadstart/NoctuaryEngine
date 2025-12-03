#pragma once

#include <ctime>
#include <filesystem>
#include <mutex>
#include <ostream>
#include <string>
#include <fstream>
#include <iostream>
#include <chrono>

namespace nt {

enum struct LogLevel {
    Verbose,   // Detailed info for deep debugging
    Log,       // General information
    Warning,   // Something unexpected, but not breaking
    Error,     // Recoverable error
    Fatal      // Unrecoverable error, will crash
};

struct LogCategory {
    bool shouldLog(LogLevel level) const {
        return enabled && level >= threshold;
    }

    std::string name;
    LogLevel threshold = LogLevel::Log;
    bool enabled = true;
};

// Predefined categories
inline LogCategory LogCore{"Core"};
inline LogCategory LogRendering{"Rendering"};
inline LogCategory LogAssets{"Assets"};
inline LogCategory LogAnimation{"Animation"};
inline LogCategory LogPhysics{"Physics"};
inline LogCategory LogAI{"AI"};
inline LogCategory LogInput{"Input"};
inline LogCategory LogAudio{"Audio"};
inline LogCategory LogUI{"UI"};

// Internal state
namespace detail {
    inline std::mutex logMutex;
    inline std::ofstream logFile;
    inline bool logToConsole = true;
    inline bool initialized = false;
}

inline void LogInit(const std::string& logFilePath = "", bool shouldLogToConsole = true) {
    std::lock_guard<std::mutex> lock(detail::logMutex);

    detail::logToConsole = shouldLogToConsole;

    if (!logFilePath.empty()) {
        // Create parent directory if it doesn't exist
        std::filesystem::path filePath(logFilePath);
        if (filePath.has_parent_path()) {
            std::filesystem::create_directories(filePath.parent_path());
        }

        detail::logFile.open(logFilePath, std::ios::out | std::ios::app);
        if (!detail::logFile.is_open()) {
            std::cerr << "Failed to open log file: " << logFilePath << std::endl;
        }
    }

    detail::initialized = true;
}

inline void LogShutdown() {
    std::lock_guard<std::mutex> lock(detail::logMutex);
    if (detail::logFile.is_open()) {
        detail::logFile.close();
    }
    detail::initialized = false;
}

inline const char* LogGetColorCode(LogLevel level) {
    switch (level) {
        case LogLevel::Verbose: return "\033[90m";   // Gray
        case LogLevel::Log: return "\033[37m";       // White
        case LogLevel::Warning: return "\033[33m";   // Yellow
        case LogLevel::Error: return "\033[31m";     // Red
        case LogLevel::Fatal: return "\033[91m";     // Bright Red
        default: return "\033[90m";   // Gray
    }
}

inline const char* LogLevelToString(LogLevel level) {
    switch (level) {
        case LogLevel::Verbose: return "VERBOSE";
        case LogLevel::Log:     return "LOG";
        case LogLevel::Warning: return "WARNING";
        case LogLevel::Error:   return "ERROR";
        case LogLevel::Fatal:   return "FATAL";
        default:                return "VERBOSE";
    }
}

inline std::string LogFormatMessage(LogCategory& category, LogLevel level,
                            const std::string& message,
                            const char* file, int line) {
    std::stringstream ss;

    // Timestamp
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;

    ss << "[" << std::put_time(std::localtime(&time), "%H:%M:%S")
    << "." << std::setfill('0') << std::setw(3) << ms.count() << "] ";

    ss << "[" << LogLevelToString(level) << "] ";

    ss << "[" << category.name << "] ";

    ss << message;

    // File and line
    if (file && line >= 0 && (level == LogLevel::Warning ||
                                level == LogLevel::Error ||
                                level == LogLevel::Fatal)) {
        ss << " (" << file << ":" << line << ")";
    }

    return ss.str();
}

inline void Log(LogCategory& category, LogLevel level, const std::string& message,
    const char* file = nullptr, int line = -1) {
        if (!category.shouldLog(level)) return;

        std::lock_guard<std::mutex> lock(detail::logMutex);

        std::string formattedMessage = LogFormatMessage(category, level, message, file, line);

        if (detail::logToConsole) {
            std::cout << LogGetColorCode(level) << formattedMessage << "\033[0m" << std::endl;
        }

        if (detail::logFile.is_open()) {
            detail::logFile << formattedMessage << std::endl;
            detail::logFile.flush(); // Write immediately
        }
}

inline void SetCategoryEnabled(LogCategory& category, bool enabled) {
    category.enabled = enabled;
}

inline void SetCategoryThreshold(LogCategory& category, LogLevel threshold) {
    category.threshold = threshold;
}

} // namespace nt

// Macros
// ========

// C++20 std::format version
#if __cplusplus >= 202002L

#define NT_LOG(Category, Level, Format, ...) \
    nt::Log(Category, nt::LogLevel::Level, \
        std::format(Format, ##__VA_ARGS__), __FILE__, __LINE__)

#else

// C++17 fallback using variadic template helper
namespace nt {
namespace detail {
    // Helper for string formatting
    inline std::string FormatString(const char* format) {
        return std::string(format);
    }

    template<typename... Args>
    inline std::string FormatString(const char* format, Args&&... args) {
        int size = std::snprintf(nullptr, 0, format, std::forward<Args>(args)...) + 1;
        if (size <= 0) return "";

        std::unique_ptr<char[]> buf(new char[size]);
        std::snprintf(buf.get(), size, format, std::forward<Args>(args)...);
        return std::string(buf.get(), buf.get() + size - 1);
    }
} // namespace detail
} // namespace nt

#define NT_LOG(Category, Level, Format, ...) \
    nt::Logger::Get().Log(Category, nt::LogLevel::Level, \
        nt::detail::FormatString(Format, ##__VA_ARGS__), __FILE__, __LINE__)

#endif

// Shorthand macros
#define NT_LOG_VERBOSE(Category, Format, ...) NT_LOG(Category, Verbose, Format, ##__VA_ARGS__)
#define NT_LOG_INFO(Category, Format, ...) NT_LOG(Category, Log, Format, ##__VA_ARGS__)
#define NT_LOG_WARN(Category, Format, ...) NT_LOG(Category, Warning, Format, ##__VA_ARGS__)
#define NT_LOG_ERROR(Category, Format, ...) NT_LOG(Category, Error, Format, ##__VA_ARGS__)
#define NT_LOG_FATAL(Category, Format, ...) NT_LOG(Category, Fatal, Format, ##__VA_ARGS__)

// Strip logs in release builds
#ifdef NDEBUG
    #undef NT_LOG_VERBOSE
    #define NT_LOG_VERBOSE(Category, Format, ...) ((void)0)
#endif

// Disable verbose asset loading logs after initial development
// nt::Logger.SetCategoryEnabled(AssetLoading, false);

// Only show warnings and above for rendering
// nt::Logger.SetCategoryThreshold(Rendering, nt::LogLevel::Warning);
