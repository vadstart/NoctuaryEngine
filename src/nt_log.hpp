#pragma once

#include <ctime>
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
    LogLevel threshold;
    bool enabled;
};

// Predefined categories
namespace LogCategories {
    inline LogCategory Core{"Core"};
    inline LogCategory Rendering{"Rendering"};
    inline LogCategory AssetLoading{"AssetLoading"};
    inline LogCategory Animation{"Animation"};
    inline LogCategory Physics{"Physics"};
    inline LogCategory AI{"AI"};
    inline LogCategory Input{"Input"};
    inline LogCategory Audio{"Audio"};
    inline LogCategory UI{"UI"};
}

struct Logger {
    static Logger& Get() {
            static Logger instance;
            return instance;
        }

    void Init(const std::string& logFilePath = "", bool shouldLogToConsole = true) {
        std::lock_guard<std::mutex> lock(mutex);

        logToConsole = shouldLogToConsole;

        if (!logFilePath.empty()) {
            logFile.open(logFilePath, std::ios::out | std::ios::app);
            if (!logFile.is_open()) {
                std::cerr << "Failed to open log file: " << logFilePath << std::endl;
            }
        }

        initialized = true;
    }

    void Shutdown() {
        std::lock_guard<std::mutex> lock(mutex);
        if (logFile.is_open()) {
            logFile.close();
        }
        initialized = false;
    }

    void Log(LogCategory& category, LogLevel level, const std::string& message,
        const char* file = nullptr, int line = -1) {
            if (!category.shouldLog(level)) return;

            std::lock_guard<std::mutex> lock(mutex);

            std::string formattedMessage = FormatMessage(category, level, message, file, line);

            if (logToConsole) {
                std::cout << GetColorCode(level) << formattedMessage << "\033[0m" << std::endl;
            }

            if (logFile.is_open()) {
                logFile << formattedMessage << std::endl;
                logFile.flush(); // Write immediately
            }
    }

    void SetCategoryEnabled(LogCategory& category, bool enabled) {
        category.enabled = enabled;
    }

    void SetCategoryThreshold(LogCategory& category, LogLevel threshold) {
        category.threshold = threshold;
    }

    std::string FormatMessage(LogCategory& category, LogLevel level,
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

        ss << "[" << LevelToString(level) << "] ";

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

    const char* GetColorCode(LogLevel level) {
        switch (level) {
            case LogLevel::Verbose: return "\033[90m";   // Gray
            case LogLevel::Log: return "\033[37m";       // White
            case LogLevel::Warning: return "\033[33m";   // Yellow
            case LogLevel::Error: return "\033[31m";     // Red
            case LogLevel::Fatal: return "\033[91m";     // Bright Red
            default: return "\033[90m";   // Gray
        }
    }

    const char* LevelToString(LogLevel level) {
        switch (level) {
            case LogLevel::Verbose: return "VERBOSE";
            case LogLevel::Log:     return "LOG";
            case LogLevel::Warning: return "WARNING";
            case LogLevel::Error:   return "ERROR";
            case LogLevel::Fatal:   return "FATAL";
            default:                return "VERBOSE";
        }
    }

    std::mutex mutex;
    std::ofstream logFile;
    bool logToConsole = true;
    bool initialized = false;
};

// Macros
// ========

// C++20 std::format version
#define NT_LOG(Category, Level, Format, ...) \
    nt::Logger::Get().Log(Category, nt::LogLevel::Level, \
        std::format(Format, ##__VA_ARGS__), __FILE__, __LINE__)

// Shorthand macros
#define NT_LOG_VERBOSE(Category, Format, ...) NT_LOG(Category, Verbose, Format, ##__VA_ARGS__)
#define NT_LOG_INFO(Category, Format, ...) NT_LOG(Category, Log, Format, ##__VA_ARGS__)
#define NT_LOG_WARN(Category, Format, ...) NT_LOG(Category, Warning, Format, ##__VA_ARGS__)
#define NT_LOG_ERROR(Category, Format, ...) NT_LOG(Category, Error, Format, ##__VA_ARGS__)
#define NT_LOG_FATAL(Category, Format, ...) NT_LOG(Category, Fatal, Format, ##__VA_ARGS__)

// Strip logs in release builds
#ifndef NDEBUG
    #undef NT_LOG_VERBOSE
    #define NT_LOG_VERBOSE(Category, Format, ...) ((void)0)
#endif

}
