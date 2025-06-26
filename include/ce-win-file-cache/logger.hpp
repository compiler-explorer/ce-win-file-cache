#pragma once

#include <cstdint>
#include <fstream>
#include <memory>
#include <mutex>
#include <string>
#include <fmt/format.h>

namespace CeWinFileCache
{

enum class LogLevel : std::uint8_t
{
    TRACE = 0,
    DEBUG = 1,
    INFO = 2,
    WARN = 3,
    ERR = 4,    // Renamed from ERROR to avoid Windows macro conflict
    FATAL = 5,
    OFF = 6
};

enum class LogOutput : std::uint8_t
{
    CONSOLE = 0,
    FILE = 1,
    BOTH = 2,
    DEBUG_OUTPUT = 3,
    DISABLED = 4
};

class Logger
{
    public:
    // Static interface for global logger access
    static void initialize(LogLevel level = LogLevel::INFO, LogOutput output = LogOutput::CONSOLE);
    static void setLevel(LogLevel level);
    static void setOutput(LogOutput output);
    static void setLogFile(const std::string &filename);
    static void shutdown();

    // Main logging methods
    static void trace(const std::string &message);
    static void debug(const std::string &message);
    static void info(const std::string &message);
    static void warn(const std::string &message);
    static void error(const std::string &message);
    static void fatal(const std::string &message);

    // Template methods for formatted logging
    template <typename... Args>
    static void trace(const std::string &format, Args &&...args);

    template <typename... Args>
    static void debug(const std::string &format, Args &&...args);

    template <typename... Args>
    static void info(const std::string &format, Args &&...args);

    template <typename... Args>
    static void warn(const std::string &format, Args &&...args);

    template <typename... Args>
    static void error(const std::string &format, Args &&...args);

    template <typename... Args>
    static void fatal(const std::string &format, Args &&...args);

    // Utility methods
    static bool isEnabled(LogLevel level);
    static std::string levelToString(LogLevel level);
    static std::string getCurrentTimestamp();
    static std::string wstringToString(const std::wstring &wstr);

    private:
    Logger() = default;
    ~Logger() = default;

    // Non-copyable, non-movable
    Logger(const Logger &) = delete;
    Logger &operator=(const Logger &) = delete;
    Logger(Logger &&) = delete;
    Logger &operator=(Logger &&) = delete;

    // Internal implementation
    static Logger &getInstance();
    void writeLog(LogLevel level, const std::string &message);
    void writeToConsole(LogLevel level, const std::string &message);
    void writeToFile(LogLevel level, const std::string &message);
    void writeToDebugOutput(LogLevel level, const std::string &message);

    // Template implementation for formatted strings
    template <typename... Args>
    static std::string formatString(const std::string &format, Args &&...args);

    // Member variables
    LogLevel current_level{ LogLevel::INFO };
    LogOutput output_type{ LogOutput::CONSOLE };
    std::string log_filename;
    std::unique_ptr<std::ofstream> log_file;
    mutable std::mutex log_mutex;
    bool initialized{ false };
};

// Template implementations
template <typename... Args>
void Logger::trace(const std::string &format, Args &&...args)
{
    if (isEnabled(LogLevel::TRACE))
    {
        getInstance().writeLog(LogLevel::TRACE, formatString(format, std::forward<Args>(args)...));
    }
}

template <typename... Args>
void Logger::debug(const std::string &format, Args &&...args)
{
    if (isEnabled(LogLevel::DEBUG))
    {
        getInstance().writeLog(LogLevel::DEBUG, formatString(format, std::forward<Args>(args)...));
    }
}

template <typename... Args>
void Logger::info(const std::string &format, Args &&...args)
{
    if (isEnabled(LogLevel::INFO))
    {
        getInstance().writeLog(LogLevel::INFO, formatString(format, std::forward<Args>(args)...));
    }
}

template <typename... Args>
void Logger::warn(const std::string &format, Args &&...args)
{
    if (isEnabled(LogLevel::WARN))
    {
        getInstance().writeLog(LogLevel::WARN, formatString(format, std::forward<Args>(args)...));
    }
}

template <typename... Args>
void Logger::error(const std::string &format, Args &&...args)
{
    if (isEnabled(LogLevel::ERR))
    {
        getInstance().writeLog(LogLevel::ERR, formatString(format, std::forward<Args>(args)...));
    }
}

template <typename... Args>
void Logger::fatal(const std::string &format, Args &&...args)
{
    if (isEnabled(LogLevel::FATAL))
    {
        getInstance().writeLog(LogLevel::FATAL, formatString(format, std::forward<Args>(args)...));
    }
}

template <typename... Args>
std::string Logger::formatString(const std::string &format, Args &&...args)
{
    if constexpr (sizeof...(args) == 0)
    {
        return format;
    }
    else
    {
        return fmt::format(fmt::runtime(format), std::forward<Args>(args)...);
    }
}

// Note: Use direct function calls instead of macros:
// CeWinFileCache::Logger::info("message");
// CeWinFileCache::Logger::error("error: {}", details);

} // namespace CeWinFileCache