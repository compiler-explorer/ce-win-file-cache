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

void Logger::trace(const std::string &message)
{
    if (isEnabled(LogLevel::TRACE))
    {
        getInstance().writeLog(LogLevel::TRACE, message);
    }
}

void Logger::debug(const std::string &message)
{
    if (isEnabled(LogLevel::DEBUG))
    {
        getInstance().writeLog(LogLevel::DEBUG, message);
    }
}

void Logger::info(const std::string &message)
{
    if (isEnabled(LogLevel::INFO))
    {
        getInstance().writeLog(LogLevel::INFO, message);
    }
}

void Logger::warn(const std::string &message)
{
    if (isEnabled(LogLevel::WARN))
    {
        getInstance().writeLog(LogLevel::WARN, message);
    }
}

void Logger::error(const std::string &message)
{
    if (isEnabled(LogLevel::ERR))
    {
        getInstance().writeLog(LogLevel::ERR, message);
    }
}

void Logger::fatal(const std::string &message)
{
    if (isEnabled(LogLevel::FATAL))
    {
        getInstance().writeLog(LogLevel::FATAL, message);
    }
}

bool Logger::isEnabled(LogLevel level)
{
    const Logger &instance = getInstance();
    return instance.initialized && instance.output_type != LogOutput::DISABLED && level >= instance.current_level;
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

void Logger::writeLog(LogLevel level, const std::string &message)
{
    std::lock_guard<std::mutex> lock(log_mutex);

    if (!initialized || output_type == LogOutput::DISABLED)
    {
        return;
    }

    switch (output_type)
    {
    case LogOutput::CONSOLE:
        writeToConsole(level, message);
        break;
    case LogOutput::FILE:
        writeToFile(level, message);
        break;
    case LogOutput::BOTH:
        writeToConsole(level, message);
        writeToFile(level, message);
        break;
    case LogOutput::DEBUG_OUTPUT:
        writeToDebugOutput(level, message);
        break;
    case LogOutput::DISABLED:
        // Do nothing
        break;
    }
}

void Logger::writeToConsole(LogLevel level, const std::string &message)
{
    const std::string timestamp = getCurrentTimestamp();
    const std::string level_str = levelToString(level);

    // Use stderr for warnings, errors, and fatal messages
    std::ostream &output = (level >= LogLevel::WARN) ? std::cerr : std::cout;

    output << fmt::format("[{}] [{}] {}\n", timestamp, level_str, message);
}

void Logger::writeToFile(LogLevel level, const std::string &message)
{
    if (!log_file || !log_file->is_open())
    {
        return;
    }

    const std::string timestamp = getCurrentTimestamp();
    const std::string level_str = levelToString(level);

    *log_file << fmt::format("[{}] [{}] {}\n", timestamp, level_str, message);
    log_file->flush(); // Ensure immediate write for important logs
}

void Logger::writeToDebugOutput(LogLevel level, const std::string &message)
{
    const std::string timestamp = getCurrentTimestamp();
    const std::string level_str = levelToString(level);

    std::string formatted_message = fmt::format("[{}] [{}] {}\n", timestamp, level_str, message);

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