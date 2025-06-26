# Application Logging System

This document describes how to enable and configure the application's logging system in CeWinFileCacheFS.

## Overview

CeWinFileCacheFS has a comprehensive logging system that operates **independently** from WinFsp's debug flags. The application logging system provides structured, formatted output for monitoring application behavior, troubleshooting issues, and performance analysis.

## Command Line Configuration

### Log Level Options

Control what severity of messages are logged:

```bash
# Set log level to debug (most verbose)
CeWinFileCacheFS --log-level debug

# Set log level to info (default)
CeWinFileCacheFS --log-level info

# Set log level to error (least verbose)
CeWinFileCacheFS --log-level error

# Disable all logging
CeWinFileCacheFS --log-level off
```

### Log Output Destinations

Control where log messages are sent:

```bash
# Output to console only (default)
CeWinFileCacheFS --log-output console

# Output to file only
CeWinFileCacheFS --log-output file

# Output to both console and file
CeWinFileCacheFS --log-output both

# Output to Windows debug output (DebugView, Visual Studio)
CeWinFileCacheFS --log-output debug

# Disable all output
CeWinFileCacheFS --log-output disabled
```

### Log File Configuration

Specify the log file path when using file or both output:

```bash
# Use custom log file
CeWinFileCacheFS --log-output file --log-file "logs/application.log"

# Use default log file (cewinfilecache.log)
CeWinFileCacheFS --log-output file
```

## Log Levels

| Level | Value | Description | When to Use |
|-------|-------|-------------|-------------|
| **TRACE** | 0 | Most detailed logging | Deep debugging, development |
| **DEBUG** | 1 | Detailed information | Development, troubleshooting |
| **INFO** | 2 | General information | Normal operation (default) |
| **WARN** | 3 | Warning conditions | Potential issues |
| **ERROR** | 4 | Error conditions | Actual problems |
| **FATAL** | 5 | Critical errors | System failures |
| **OFF** | 6 | No logging | Production with no logs |

### Level Hierarchy

When you set a log level, **all messages at that level and higher** are logged:

```
TRACE → Shows: TRACE, DEBUG, INFO, WARN, ERROR, FATAL
DEBUG → Shows: DEBUG, INFO, WARN, ERROR, FATAL  
INFO  → Shows: INFO, WARN, ERROR, FATAL
WARN  → Shows: WARN, ERROR, FATAL
ERROR → Shows: ERROR, FATAL
FATAL → Shows: FATAL only
OFF   → Shows: Nothing
```

## Output Destinations

### Console Output
```bash
CeWinFileCacheFS --log-output console
```
- Logs to stdout (INFO, DEBUG, TRACE) and stderr (WARN, ERROR, FATAL)
- Immediate output, good for development
- Colored output in terminals that support it

### File Output
```bash
CeWinFileCacheFS --log-output file --log-file "app.log"
```
- Logs to specified file (default: `cewinfilecache.log`)
- Persistent logging, good for production
- Automatic file creation if it doesn't exist

### Both Console and File
```bash
CeWinFileCacheFS --log-output both --log-file "app.log"
```
- Logs to both console and file simultaneously
- Best for development and testing
- Immediate feedback plus persistent records

### Windows Debug Output
```bash
CeWinFileCacheFS --log-output debug
```
- Sends logs to Windows debug output system
- Viewable with DebugView, Visual Studio debugger, WinDbg
- Good for Windows service debugging

### Disabled
```bash
CeWinFileCacheFS --log-output disabled
```
- Completely disables all logging output
- Minimal performance overhead
- Use for production when no logging is needed

## Practical Examples

### Development Mode
```bash
# Maximum verbosity, output to both console and file
CeWinFileCacheFS --config dev.json \
    --log-level trace \
    --log-output both \
    --log-file "dev-debug.log"
```

### Testing Mode
```bash
# Debug level with file output for analysis
CeWinFileCacheFS --test --config test.json \
    --log-level debug \
    --log-output file \
    --log-file "test-results.log"
```

### Production Mode
```bash
# Error level only, file output for monitoring
CeWinFileCacheFS --config production.json \
    --mount M: \
    --log-level error \
    --log-output file \
    --log-file "C:/logs/compiler-cache-errors.log"
```

### Service Debugging
```bash
# Debug level with Windows debug output for service troubleshooting
CeWinFileCacheFS --config service.json \
    --mount M: \
    --log-level debug \
    --log-output debug
```

### Performance Testing
```bash
# Info level to capture performance metrics without debug noise
CeWinFileCacheFS --config perf.json \
    --mount M: \
    --log-level info \
    --log-output file \
    --log-file "performance.log"
```

## What Gets Logged

### Application Startup/Shutdown
- Configuration loading and validation
- Service initialization and cleanup
- Mount/unmount operations
- Metrics system initialization

### File System Operations
- File access patterns and cache hits/misses
- Network file downloads and caching
- Directory tree operations
- File metadata updates

### Performance Metrics
- Cache performance statistics
- Download times and throughput
- Memory usage patterns
- Thread pool activity

### Error Conditions
- Configuration errors
- Network connectivity issues
- File system operation failures
- Resource exhaustion conditions

### Debug Information
- Detailed operation traces
- Internal state changes
- Algorithm decision points
- Threading and synchronization events

## Log Message Format

### Standard Format
```
[YYYY-MM-DD HH:MM:SS.mmm] [LEVEL] Message content
```

### Examples
```
[2024-01-15 14:30:25.123] [INFO ] Metrics server started on http://127.0.0.1:8080/metrics
[2024-01-15 14:30:25.124] [DEBUG] Cache hit for file: /msvc-14.40/bin/cl.exe
[2024-01-15 14:30:25.125] [ERROR] Failed to download file: network timeout
[2024-01-15 14:30:25.126] [WARN ] Cache size approaching limit: 95% full
```

## Programmatic Usage

### Basic Logging
```cpp
#include <ce-win-file-cache/logger.hpp>

// Initialize logger (typically done in main())
Logger::initialize(LogLevel::INFO, LogOutput::CONSOLE);

// Simple logging
Logger::info("Application started");
Logger::warn("Cache size approaching limit");
Logger::error("Failed to connect to network share");
```

### Formatted Logging
```cpp
// Using fmt-style formatting
Logger::info("Processed {} files in {:.2f} seconds", fileCount, duration);
Logger::debug("Cache hit rate: {:.1%} ({}/{})", hitRate, hits, total);
Logger::error("Network error {}: {}", errorCode, errorMessage);
```

### Runtime Configuration
```cpp
// Change log level at runtime
Logger::setLevel(LogLevel::DEBUG);

// Change output destination
Logger::setOutput(LogOutput::BOTH);

// Set custom log file
Logger::setLogFile("/path/to/custom.log");
```

### Fallback Logging
```cpp
// For use before Logger::initialize() is called
Logger::warn_fallback("Configuration issue: {}", details);
Logger::error_fallback("Critical startup error: {}", error);
```

## Performance Considerations

### Log Level Impact
- **TRACE/DEBUG**: High overhead, use only during development
- **INFO**: Moderate overhead, suitable for most production use
- **WARN/ERROR**: Minimal overhead, safe for production
- **OFF**: No overhead, but no diagnostic capability

### Output Destination Impact
- **Console**: Minimal overhead, can slow down if output is redirected
- **File**: Low overhead, some I/O cost for disk writes
- **Both**: Combined overhead of console + file
- **Debug**: Platform-specific, generally low overhead
- **Disabled**: No overhead

### Best Practices
1. **Use appropriate log levels** - Don't log at TRACE level in production
2. **Avoid logging in tight loops** - Use DEBUG level for frequent operations
3. **Log important events at INFO level** - Startup, configuration, major operations
4. **Use file output for production** - Easier to analyze and archive
5. **Rotate log files** - Prevent logs from consuming too much disk space

## Troubleshooting

### No Log Output
1. **Check log level**: Ensure it's not set to OFF
2. **Check output destination**: Verify console/file/debug is working
3. **Check file permissions**: Ensure log file can be created/written
4. **Check initialization**: Verify Logger::initialize() was called

### Too Much Log Output
1. **Increase log level**: Use WARN or ERROR instead of DEBUG/TRACE
2. **Filter by category**: Focus on specific subsystems
3. **Use file output**: Avoid console spam
4. **Implement log rotation**: Prevent disk space issues

### Performance Issues
1. **Reduce log level**: Higher levels have less overhead
2. **Use file output**: Usually faster than console
3. **Avoid formatting**: Use simple messages for high-frequency logs
4. **Consider async logging**: For high-throughput scenarios

### Log File Issues
1. **Check permissions**: Ensure write access to log directory
2. **Check disk space**: Ensure sufficient space for log files
3. **Check path**: Verify log file path is valid and accessible
4. **Check locks**: Ensure no other process is locking the log file

## Integration with WinFsp Debug Flags

**Important**: Application logging is separate from WinFsp debug flags:

| System | Purpose | Control | Output |
|--------|---------|---------|---------|
| **Application Logging** | Application behavior | `--log-level`, `--log-output` | Formatted app messages |
| **WinFsp Debug Flags** | File system internals | `-d`, `--debug` | Raw WinFsp operations |

### Using Both Systems
```bash
# Enable both application logging and WinFsp debugging
CeWinFileCacheFS --config dev.json \
    --log-level debug \
    --log-output file \
    --log-file "app.log" \
    --debug 15
```

This gives you:
- **Application logs** in `app.log` with structured, readable messages
- **WinFsp debug output** to debugger with low-level file system operations

## See Also

- [WinFsp Debug Flags Reference](DEBUG_FLAGS.md)
- [Configuration Guide](CONFIGURATION.md)
- [Troubleshooting Guide](TROUBLESHOOTING.md)
- [Performance Tuning](PERFORMANCE.md)