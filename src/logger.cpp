#include "../include/ce-win-file-cache/logger.hpp"
#include "../include/ce-win-file-cache/string_utils.hpp"
#include <chrono>
#include <fmt/chrono.h>
#include <iostream>

// Windows headers for OutputDebugStringA
#if defined(_WIN32) || defined(WIN32)
#include <windows.h>
#elif defined(__APPLE__)
// Include macOS compatibility header for Windows API stubs
#include "../include/ce-win-file-cache/macos_compat.hpp"
#endif

namespace CeWinFileCache
{

void Logger::initialize(LogLevel level, LogOutput output)
{
    Logger &instance = getInstance();
    std::lock_guard<std::mutex> lock(instance.log_mutex);

    instance.current_level = level;
    instance.output_type = output;
    instance.initialized = true;

    if (output == LogOutput::FILE || output == LogOutput::BOTH)
    {
        if (instance.log_filename.empty())
        {
            instance.log_filename = "cewinfilecache.log";
        }

        instance.log_file = std::make_unique<std::ofstream>(instance.log_filename, std::ios::app);

        if (!instance.log_file->is_open())
        {
            // Fallback to console if file can't be opened
            instance.output_type = LogOutput::CONSOLE;
            std::cerr << "[Logger] Warning: Could not open log file '" << instance.log_filename << "', falling back to console output\n";
        }
    }

    // For debug output, verify Windows API is available
    if (output == LogOutput::DEBUG_OUTPUT)
    {
#if !defined(_WIN32) && !defined(WIN32) && !defined(__APPLE__)
        // Fallback to console on platforms without OutputDebugStringA support
        instance.output_type = LogOutput::CONSOLE;
        std::cerr
        << "[Logger] Warning: OutputDebugStringA not available on this platform, falling back to console output\n";
#endif
    }
}

void Logger::setLevel(LogLevel level)
{
    Logger &instance = getInstance();
    std::lock_guard<std::mutex> lock(instance.log_mutex);
    instance.current_level = level;
}

void Logger::setOutput(LogOutput output)
{
    Logger &instance = getInstance();
    std::lock_guard<std::mutex> lock(instance.log_mutex);
    instance.output_type = output;
}

void Logger::setLogFile(const std::string &filename)
{
    Logger &instance = getInstance();
    std::lock_guard<std::mutex> lock(instance.log_mutex);

    instance.log_filename = filename;

    // If we're already logging to file, reopen with new filename
    if ((instance.output_type == LogOutput::FILE || instance.output_type == LogOutput::BOTH) && instance.initialized)
    {
        instance.log_file = std::make_unique<std::ofstream>(filename, std::ios::app);

        if (!instance.log_file->is_open())
        {
            std::cerr << "[Logger] Warning: Could not open log file '" << filename << "'\n";
        }
    }
}

void Logger::setCategories(LogCategory categories)
{
    Logger &instance = getInstance();
    std::lock_guard<std::mutex> lock(instance.log_mutex);
    instance.enabled_categories = categories;
}

void Logger::setCategoriesFromString(const std::string &categories_str)
{
    LogCategory categories = static_cast<LogCategory>(0);

    if (categories_str == "all" || categories_str == "ALL")
    {
        categories = LogCategory::ALL;
    }
    else
    {
        // Parse comma-separated category names
        std::string remaining = categories_str;
        size_t pos = 0;

        while ((pos = remaining.find(',')) != std::string::npos || !remaining.empty())
        {
            std::string category = (pos != std::string::npos) ? remaining.substr(0, pos) : remaining;

            // Trim whitespace
            size_t start = category.find_first_not_of(" \t\r\n");
            size_t end = category.find_last_not_of(" \t\r\n");
            if (start != std::string::npos)
            {
                category = category.substr(start, end - start + 1);
            }

            if (category == "general")
                categories =
                static_cast<LogCategory>(static_cast<uint32_t>(categories) | static_cast<uint32_t>(LogCategory::GENERAL));
            else if (category == "filesystem" || category == "fs")
                categories =
                static_cast<LogCategory>(static_cast<uint32_t>(categories) | static_cast<uint32_t>(LogCategory::FILESYSTEM));
            else if (category == "cache")
                categories =
                static_cast<LogCategory>(static_cast<uint32_t>(categories) | static_cast<uint32_t>(LogCategory::CACHE));
            else if (category == "network")
                categories =
                static_cast<LogCategory>(static_cast<uint32_t>(categories) | static_cast<uint32_t>(LogCategory::NETWORK));
            else if (category == "memory")
                categories =
                static_cast<LogCategory>(static_cast<uint32_t>(categories) | static_cast<uint32_t>(LogCategory::MEMORY));
            else if (category == "access")
                categories =
                static_cast<LogCategory>(static_cast<uint32_t>(categories) | static_cast<uint32_t>(LogCategory::ACCESS));
            else if (category == "directory" || category == "dir")
                categories =
                static_cast<LogCategory>(static_cast<uint32_t>(categories) | static_cast<uint32_t>(LogCategory::DIRECTORY));
            else if (category == "security")
                categories =
                static_cast<LogCategory>(static_cast<uint32_t>(categories) | static_cast<uint32_t>(LogCategory::SECURITY));
            else if (category == "config")
                categories =
                static_cast<LogCategory>(static_cast<uint32_t>(categories) | static_cast<uint32_t>(LogCategory::CONFIG));
            else if (category == "service")
                categories =
                static_cast<LogCategory>(static_cast<uint32_t>(categories) | static_cast<uint32_t>(LogCategory::SERVICE));

            if (pos == std::string::npos)
                break;
            remaining = remaining.substr(pos + 1);
        }
    }

    setCategories(categories);
}

void Logger::shutdown()
{
    Logger &instance = getInstance();
    std::lock_guard<std::mutex> lock(instance.log_mutex);

    if (instance.log_file && instance.log_file->is_open())
    {
        instance.log_file->close();
    }
    instance.log_file.reset();
    instance.initialized = false;
}

// Category-aware logging methods
void Logger::trace(LogCategory category, const std::string &message)
{
#ifndef NO_LOGGING
    if (isEnabled(LogLevel::TRACE) && isEnabled(category))
    {
        getInstance().writeLog(LogLevel::TRACE, category, message);
    }
#endif
}

void Logger::debug(LogCategory category, const std::string &message)
{
#ifndef NO_LOGGING
    if (isEnabled(LogLevel::DEBUG) && isEnabled(category))
    {
        getInstance().writeLog(LogLevel::DEBUG, category, message);
    }
#endif
}

void Logger::info(LogCategory category, const std::string &message)
{
#ifndef NO_LOGGING
    if (isEnabled(LogLevel::INFO) && isEnabled(category))
    {
        getInstance().writeLog(LogLevel::INFO, category, message);
    }
#endif
}

void Logger::warn(LogCategory category, const std::string &message)
{
#ifndef NO_LOGGING
    if (isEnabled(LogLevel::WARN) && isEnabled(category))
    {
        getInstance().writeLog(LogLevel::WARN, category, message);
    }
#endif
}

void Logger::error(LogCategory category, const std::string &message)
{
#ifndef NO_LOGGING
    if (isEnabled(LogLevel::ERR) && isEnabled(category))
    {
        getInstance().writeLog(LogLevel::ERR, category, message);
    }
#endif
}

void Logger::fatal(LogCategory category, const std::string &message)
{
#ifndef NO_LOGGING
    if (isEnabled(LogLevel::FATAL) && isEnabled(category))
    {
        getInstance().writeLog(LogLevel::FATAL, category, message);
    }
#endif
}

// Convenience methods (use GENERAL category)
void Logger::trace(const std::string &message)
{
    trace(LogCategory::GENERAL, message);
}

void Logger::debug(const std::string &message)
{
    debug(LogCategory::GENERAL, message);
}

void Logger::info(const std::string &message)
{
    info(LogCategory::GENERAL, message);
}

void Logger::warn(const std::string &message)
{
    warn(LogCategory::GENERAL, message);
}

void Logger::error(const std::string &message)
{
    error(LogCategory::GENERAL, message);
}

void Logger::fatal(const std::string &message)
{
    fatal(LogCategory::GENERAL, message);
}

bool Logger::isEnabled(LogLevel level)
{
    const Logger &instance = getInstance();
    return instance.initialized && instance.output_type != LogOutput::DISABLED && level >= instance.current_level;
}

bool Logger::isEnabled(LogCategory category)
{
    const Logger &instance = getInstance();
    return (static_cast<uint32_t>(instance.enabled_categories) & static_cast<uint32_t>(category)) != 0;
}

std::string Logger::levelToString(LogLevel level)
{
    switch (level)
    {
    case LogLevel::TRACE:
        return "TRACE";
    case LogLevel::DEBUG:
        return "DEBUG";
    case LogLevel::INFO:
        return "INFO ";
    case LogLevel::WARN:
        return "WARN ";
    case LogLevel::ERR:
        return "ERROR";
    case LogLevel::FATAL:
        return "FATAL";
    case LogLevel::OFF:
        return "OFF  ";
    default:
        return "UNKN ";
    }
}

std::string Logger::categoryToString(LogCategory category)
{
    switch (category)
    {
    case LogCategory::GENERAL:
        return "GEN";
    case LogCategory::FILESYSTEM:
        return "FS ";
    case LogCategory::CACHE:
        return "CAC";
    case LogCategory::NETWORK:
        return "NET";
    case LogCategory::MEMORY:
        return "MEM";
    case LogCategory::ACCESS:
        return "ACC";
    case LogCategory::DIRECTORY:
        return "DIR";
    case LogCategory::SECURITY:
        return "SEC";
    case LogCategory::CONFIG:
        return "CFG";
    case LogCategory::SERVICE:
        return "SVC";
    default:
        return "UNK";
    }
}

std::string Logger::getCurrentTimestamp()
{
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

    std::tm local_tm{};
#ifdef _WIN32
    localtime_s(&local_tm, &time_t);
#else
    local_tm = *std::localtime(&time_t);
#endif

    return fmt::format("{:%Y-%m-%d %H:%M:%S}.{:03d}", local_tm, ms.count());
}

Logger &Logger::getInstance()
{
    static Logger instance;
    return instance;
}

void Logger::writeLog(LogLevel level, LogCategory category, const std::string &message)
{
    if (!initialized || output_type == LogOutput::DISABLED)
    {
        return;
    }

    std::lock_guard<std::mutex> lock(log_mutex);

    switch (output_type)
    {
    case LogOutput::CONSOLE:
        writeToConsole(level, category, message);
        break;
    case LogOutput::FILE:
        writeToFile(level, category, message);
        break;
    case LogOutput::BOTH:
        writeToConsole(level, category, message);
        writeToFile(level, category, message);
        break;
    case LogOutput::DEBUG_OUTPUT:
        writeToDebugOutput(level, category, message);
        break;
    case LogOutput::DISABLED:
        // Do nothing
        break;
    }
}

void Logger::writeToConsole(LogLevel level, LogCategory category, const std::string &message)
{
    const std::string timestamp = getCurrentTimestamp();
    const std::string level_str = levelToString(level);
    const std::string category_str = categoryToString(category);

    // Use stderr for warnings, errors, and fatal messages
    std::ostream &output = (level >= LogLevel::WARN) ? std::cerr : std::cout;

    output << fmt::format("[{}] [{}] [{}] {}\n", timestamp, level_str, category_str, message);
}

void Logger::writeToFile(LogLevel level, LogCategory category, const std::string &message)
{
    if (!log_file || !log_file->is_open())
    {
        return;
    }

    const std::string timestamp = getCurrentTimestamp();
    const std::string level_str = levelToString(level);
    const std::string category_str = categoryToString(category);

    *log_file << fmt::format("[{}] [{}] [{}] {}\n", timestamp, level_str, category_str, message);
    log_file->flush(); // Ensure immediate write for important logs
}

void Logger::writeToDebugOutput(LogLevel level, LogCategory category, const std::string &message)
{
    const std::string timestamp = getCurrentTimestamp();
    const std::string level_str = levelToString(level);
    const std::string category_str = categoryToString(category);

    std::string formatted_message = fmt::format("[{}] [{}] [{}] {}\n", timestamp, level_str, category_str, message);

#if defined(_WIN32) || defined(WIN32)
    // Use native Windows OutputDebugStringA
    OutputDebugStringA(formatted_message.c_str());
#elif defined(__APPLE__)
    // Use macOS compatibility stub (outputs to stderr as fallback)
    OutputDebugStringA(formatted_message.c_str());
#else
    // Fallback to stderr for other platforms
    std::cerr << formatted_message;
#endif
}

std::string Logger::wstringToString(const std::wstring &wstr)
{
    return StringUtils::wideToUtf8(wstr);
}

void Logger::error_fallback(const std::string &message)
{
    // Use fmt::format for consistent formatting, output to stderr
    std::cerr << fmt::format("[FALLBACK ERROR] {}\n", message);
}

void Logger::warn_fallback(const std::string &message)
{
    // Use fmt::format for consistent formatting, output to stderr
    std::cerr << fmt::format("[FALLBACK WARN] {}\n", message);
}

} // namespace CeWinFileCache