#include "include/ce-win-file-cache/logger.hpp"
#include <iostream>

int main()
{
    // Initialize logger with INFO level and console output
    CeWinFileCache::Logger::initialize(CeWinFileCache::LogLevel::DEBUG, CeWinFileCache::LogOutput::CONSOLE);
    
    // Test basic logging
    CeWinFileCache::Logger::info("Logger test started");
    CeWinFileCache::Logger::debug("This is a debug message");
    CeWinFileCache::Logger::warn("This is a warning message");
    CeWinFileCache::Logger::error("This is an error message");
    
    // Test formatted logging
    CeWinFileCache::Logger::info("Testing formatted message with number: {}", 42);
    CeWinFileCache::Logger::debug("Multiple parameters: {} and {}", "string", 123);
    
    // Test wstring conversion
    std::wstring wstr = L"Unicode string: 测试";
    CeWinFileCache::Logger::info("Wide string test: {}", CeWinFileCache::Logger::wstringToString(wstr));
    
    // Test different log levels
    CeWinFileCache::Logger::setLevel(CeWinFileCache::LogLevel::WARN);
    CeWinFileCache::Logger::debug("This debug message should NOT appear");
    CeWinFileCache::Logger::warn("This warning SHOULD appear");
    
    // Test file output
    CeWinFileCache::Logger::setOutput(CeWinFileCache::LogOutput::FILE);
    CeWinFileCache::Logger::setLogFile("test_log.txt");
    CeWinFileCache::Logger::info("This message should go to file only");
    
    // Test both console and file
    CeWinFileCache::Logger::setOutput(CeWinFileCache::LogOutput::BOTH);
    CeWinFileCache::Logger::error("This message should appear in both console and file");
    
    // Test debug output (OutputDebugStringA)
    CeWinFileCache::Logger::setOutput(CeWinFileCache::LogOutput::DEBUG_OUTPUT);
    CeWinFileCache::Logger::setLevel(CeWinFileCache::LogLevel::DEBUG);
    CeWinFileCache::Logger::info("This message should go to OutputDebugStringA (Windows debug output)");
    CeWinFileCache::Logger::debug("Debug message via OutputDebugStringA");
    CeWinFileCache::Logger::warn("Warning message via OutputDebugStringA");
    
    CeWinFileCache::Logger::info("Logger test completed successfully");
    
    // Clean up
    CeWinFileCache::Logger::shutdown();
    
    std::cout << "Test completed. Check console output, test_log.txt file, and debug output (on Windows: use DebugView or Visual Studio output window)." << std::endl;
    return 0;
}