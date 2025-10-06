#pragma once

#include <cstdint>
#include <fmt/format.h>
#include <fstream>
#include <memory>
#include <mutex>
#include <string>

namespace CeWinFileCache
{

enum class LogLevel : std::uint8_t
{
    TRACE = 0,
    DEBUG = 1,
    INFO = 2,
    WARN = 3,
    ERR = 4, // Renamed from ERROR to avoid Windows macro conflict
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

enum class LogCategory : std::uint32_t
{
    GENERAL = 1 << 0,      // 0x001 - General filesystem operations
    FILESYSTEM = 1 << 1,   // 0x002 - File system calls (Open, Read, etc.)
    CACHE = 1 << 2,        // 0x004 - Cache operations
    NETWORK = 1 << 3,      // 0x008 - Network operations
    MEMORY = 1 << 4,       // 0x010 - Memory cache operations
    ACCESS = 1 << 5,       // 0x020 - File access tracking
    DIRECTORY = 1 << 6,    // 0x040 - Directory operations
    SECURITY = 1 << 7,     // 0x080 - Security operations
    CONFIG = 1 << 8,       // 0x100 - Configuration
    SERVICE = 1 << 9,      // 0x200 - Service management
    ALL = 0xFFFFFFFF       // All categories
};

class Logger
{
    public:
    // Static interface for global logger access
    static void initialize(LogLevel level = LogLevel::INFO, LogOutput output = LogOutput::CONSOLE);
    static void setLevel(LogLevel level);
    static void setOutput(LogOutput output);
    static void setLogFile(const std::string &filename);
    static void setCategories(LogCategory categories);
    static void setCategoriesFromString(const std::string &categories_str);
    static void shutdown();

    // Fallback logging methods for use before initialization
    static void error_fallback(const std::string &message);
    static void warn_fallback(const std::string &message);

    template <typename... Args>
    static void error_fallback(const std::string &format, Args &&...args);

    template <typename... Args>
    static void warn_fallback(const std::string &format, Args &&...args);

    // Main logging methods with categories
    static void trace(LogCategory category, const std::string &message);
    static void debug(LogCategory category, const std::string &message);
    static void info(LogCategory category, const std::string &message);
    static void warn(LogCategory category, const std::string &message);
    static void error(LogCategory category, const std::string &message);
    static void fatal(LogCategory category, const std::string &message);
    
    // Convenience methods (use GENERAL category)
    static void trace(const std::string &message);
    static void debug(const std::string &message);
    static void info(const std::string &message);
    static void warn(const std::string &message);
    static void error(const std::string &message);
    static void fatal(const std::string &message);

    // Template methods for formatted logging with categories
    template <typename... Args>
    static void trace(LogCategory category, const std::string &format, Args &&...args);

    template <typename... Args>
    static void debug(LogCategory category, const std::string &format, Args &&...args);

    template <typename... Args>
    static void info(LogCategory category, const std::string &format, Args &&...args);

    template <typename... Args>
    static void warn(LogCategory category, const std::string &format, Args &&...args);

    template <typename... Args>
    static void error(LogCategory category, const std::string &format, Args &&...args);

    template <typename... Args>
    static void fatal(LogCategory category, const std::string &format, Args &&...args);
    
    // Template methods for formatted logging (use GENERAL category)
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
    static bool isEnabled(LogCategory category);
    static std::string levelToString(LogLevel level);
    static std::string categoryToString(LogCategory category);
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
    void writeLog(LogLevel level, LogCategory category, const std::string &message);
    void writeToConsole(LogLevel level, LogCategory category, const std::string &message);
    void writeToFile(LogLevel level, LogCategory category, const std::string &message);
    void writeToDebugOutput(LogLevel level, LogCategory category, const std::string &message);

    // Template implementation for formatted strings
    template <typename... Args>
    static std::string formatString(const std::string &format, Args &&...args);

    // Member variables
    LogLevel current_level{ LogLevel::INFO };
    LogOutput output_type{ LogOutput::CONSOLE };
    LogCategory enabled_categories{ LogCategory::ALL };
    std::string log_filename;
    std::unique_ptr<std::ofstream> log_file;
    mutable std::mutex log_mutex;
    bool initialized{ false };
};

// Template implementations with categories
template <typename... Args>
void Logger::trace(LogCategory category, const std::string &format, Args &&...args)
{
#ifndef NO_LOGGING
    if (isEnabled(LogLevel::TRACE) && isEnabled(category))
    {
        getInstance().writeLog(LogLevel::TRACE, category, formatString(format, std::forward<Args>(args)...));
    }
#endif
}

template <typename... Args>
void Logger::debug(LogCategory category, const std::string &format, Args &&...args)
{
#ifndef NO_LOGGING
    if (isEnabled(LogLevel::DEBUG) && isEnabled(category))
    {
        getInstance().writeLog(LogLevel::DEBUG, category, formatString(format, std::forward<Args>(args)...));
    }
#endif
}

template <typename... Args>
void Logger::info(LogCategory category, const std::string &format, Args &&...args)
{
#ifndef NO_LOGGING
    if (isEnabled(LogLevel::INFO) && isEnabled(category))
    {
        getInstance().writeLog(LogLevel::INFO, category, formatString(format, std::forward<Args>(args)...));
    }
#endif
}

template <typename... Args>
void Logger::warn(LogCategory category, const std::string &format, Args &&...args)
{
#ifndef NO_LOGGING
    if (isEnabled(LogLevel::WARN) && isEnabled(category))
    {
        getInstance().writeLog(LogLevel::WARN, category, formatString(format, std::forward<Args>(args)...));
    }
#endif
}

template <typename... Args>
void Logger::error(LogCategory category, const std::string &format, Args &&...args)
{
#ifndef NO_LOGGING
    if (isEnabled(LogLevel::ERR) && isEnabled(category))
    {
        getInstance().writeLog(LogLevel::ERR, category, formatString(format, std::forward<Args>(args)...));
    }
#endif
}

template <typename... Args>
void Logger::fatal(LogCategory category, const std::string &format, Args &&...args)
{
#ifndef NO_LOGGING
    if (isEnabled(LogLevel::FATAL) && isEnabled(category))
    {
        getInstance().writeLog(LogLevel::FATAL, category, formatString(format, std::forward<Args>(args)...));
    }
#endif
}

// Template implementations for convenience methods (GENERAL category)
template <typename... Args>
void Logger::trace(const std::string &format, Args &&...args)
{
    trace(LogCategory::GENERAL, format, std::forward<Args>(args)...);
}

template <typename... Args>
void Logger::debug(const std::string &format, Args &&...args)
{
    debug(LogCategory::GENERAL, format, std::forward<Args>(args)...);
}

template <typename... Args>
void Logger::info(const std::string &format, Args &&...args)
{
    info(LogCategory::GENERAL, format, std::forward<Args>(args)...);
}

template <typename... Args>
void Logger::warn(const std::string &format, Args &&...args)
{
    warn(LogCategory::GENERAL, format, std::forward<Args>(args)...);
}

template <typename... Args>
void Logger::error(const std::string &format, Args &&...args)
{
    error(LogCategory::GENERAL, format, std::forward<Args>(args)...);
}

template <typename... Args>
void Logger::fatal(const std::string &format, Args &&...args)
{
    fatal(LogCategory::GENERAL, format, std::forward<Args>(args)...);
}

template <typename... Args>
void Logger::error_fallback(const std::string &format, Args &&...args)
{
    std::string message = formatString(format, std::forward<Args>(args)...);
    error_fallback(message);
}

template <typename... Args>
void Logger::warn_fallback(const std::string &format, Args &&...args)
{
    std::string message = formatString(format, std::forward<Args>(args)...);
    warn_fallback(message);
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

// Note: Use direct function calls with categories:
// CeWinFileCache::Logger::info(LogCategory::FILESYSTEM, "File opened: {}", filename);
// CeWinFileCache::Logger::debug(LogCategory::CACHE, "Cache hit for: {}", path);
// CeWinFileCache::Logger::error(LogCategory::NETWORK, "Network error: {}", details);

} // namespace CeWinFileCache