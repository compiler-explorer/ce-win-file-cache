# WinFsp Debug Flags Reference

This document describes the debug flags system used in CeWinFileCacheFS for controlling WinFsp's internal logging behavior.

## Overview

The debug flags system uses **WinFsp's built-in debug logging mechanism**, which operates independently from the application's Logger system. These flags control what internal WinFsp operations are logged and where the output is sent.

## Command Line Usage

```bash
# Disable all debug logging (default)
CeWinFileCacheFS -d 0

# Enable all debug logging  
CeWinFileCacheFS -d

# Enable all debug logging (explicit)
CeWinFileCacheFS -d -1

# Enable specific debug categories
CeWinFileCacheFS -d 1     # Enable category 1
CeWinFileCacheFS -d 7     # Enable categories 1, 2, and 4 (bitmask: 0111)
CeWinFileCacheFS -d 15    # Enable categories 1, 2, 4, and 8 (bitmask: 1111)
```

## Debug Flag Values

| Value | Hex | Description |
|-------|-----|-------------|
| `0` | `0x00000000` | **No debug logging** - All logging disabled (default) |
| `-1` | `0xFFFFFFFF` | **Full debug logging** - All categories enabled |
| `1` | `0x00000001` | Enable debug category 1 |
| `2` | `0x00000002` | Enable debug category 2 |
| `4` | `0x00000004` | Enable debug category 3 |
| `8` | `0x00000008` | Enable debug category 4 |
| Custom | Various | **Bitmask combinations** - Enable specific category combinations |

## Bitmask System

The debug flags use a **32-bit bitmask** where each bit represents a different category of WinFsp operations:

```
Bit:  31 30 29 ... 3  2  1  0
Flag:  ?  ?  ? ...  ?  ?  ?  ?
```

- **Each bit = 1**: Enable logging for that category
- **Each bit = 0**: Disable logging for that category
- **Combine multiple categories**: Use bitwise OR of values

### Examples

```bash
# Enable categories 1 and 2
-d 3     # Binary: 0011 = 1 + 2

# Enable categories 1, 4, and 8  
-d 13    # Binary: 1101 = 1 + 4 + 8

# Enable categories 2 and 16
-d 18    # Binary: 10010 = 2 + 16
```

## What Gets Logged

WinFsp debug logging typically includes:

### Core File System Operations
- **File operations**: Create, open, read, write, close, delete
- **Directory operations**: Enumerate, create, delete, rename
- **Attribute queries**: File size, timestamps, permissions
- **Volume operations**: Mount, unmount, query volume info

### Low-Level Operations
- **IRP (I/O Request Packet) processing**: Kernel-level I/O requests
- **Cache operations**: Page cache interactions, write-through behavior
- **Security operations**: Access checks, token validation
- **Handle management**: File handle creation and cleanup

### Performance and Debugging
- **Timing information**: Operation durations, bottlenecks
- **Error conditions**: Failed operations, retry attempts
- **Memory allocation**: Buffer management, memory pressure
- **Thread operations**: Worker thread activity, synchronization

## Output Destinations

WinFsp debug output can be sent to different destinations:

### 1. Windows Debugger (Default)
```
Output goes to attached debugger:
- WinDbg
- Visual Studio debugger  
- DebugView utility
- Kernel debugger
```

### 2. Log File (If Configured)
```cpp
// In application code:
FspDebugLogSetHandle(fileHandle);
```

When a file handle is set, debug output goes to the specified file instead of the debugger.

## Implementation Details

### Command Line Parsing
```cpp
// From main.cpp
if (i + 1 < argc && argv[i + 1][0] != L'-')
{
    options.debug_flags = StringUtils::parseULong(argv[++i]);  // Parse specific value
}
else
{
    options.debug_flags = static_cast<ULONG>(-1);              // Enable all debug
}
```

### WinFsp Integration
```cpp
// Debug flags are passed directly to WinFsp
result = host.Mount(mount_point_copy.data(), nullptr, FALSE, options_.debug_flags);
```

### Data Type
- **Type**: `ULONG` (32-bit unsigned integer)
- **Range**: 0 to 4,294,967,295 (0x00000000 to 0xFFFFFFFF)
- **Default**: 0 (no logging)

## Relationship to Application Logger

**Important**: WinFsp debug flags are **separate** from the application's Logger system:

| System | Purpose | Output | Control |
|--------|---------|---------|---------|
| **WinFsp Debug Flags** | Log internal WinFsp operations | Debugger or file | `-d` flag |
| **Application Logger** | Log application-level messages | Console, file, or debug output | Logger::initialize() |

## Best Practices

### Development
```bash
# Start with minimal logging
-d 1

# Gradually increase if needed
-d 7

# Use full logging only for complex issues
-d -1
```

### Production
```bash
# Disable debug logging for performance
-d 0
```

### Debugging Specific Issues
```bash
# File I/O problems
-d 15    # Enable basic file operations

# Performance issues  
-d 255   # Enable performance-related categories

# Mount/unmount problems
-d -1    # Enable all logging
```

## Common Debug Scenarios

### Scenario 1: File Access Issues
```bash
# Enable file operation logging
CeWinFileCacheFS --config compilers.json --mount M: -d 3
```

### Scenario 2: Performance Investigation
```bash
# Enable timing and cache logging
CeWinFileCacheFS --config compilers.json --mount M: -d 31
```

### Scenario 3: Mount/Service Issues
```bash
# Enable all logging for comprehensive debugging
CeWinFileCacheFS --config compilers.json --mount M: -d -1
```

## Troubleshooting

### No Debug Output Visible
1. **Check if debugger is attached** (use DebugView utility)
2. **Verify debug flags are set**: Look for "Debug flags: 0x..." in application log
3. **Check WinFsp installation**: Ensure WinFsp is properly installed

### Too Much Debug Output
1. **Reduce debug level**: Use smaller bitmask values
2. **Filter output**: Use debugger filtering capabilities
3. **Redirect to file**: Set up file logging to avoid console spam

### Debug Output in Wrong Location
1. **Default**: Goes to debugger, not console
2. **File logging**: Requires application code changes
3. **Console output**: Not directly supported by WinFsp debug system

## See Also

- [WinFsp Documentation](https://winfsp.dev/doc/)
- [WinFsp Tutorial](https://winfsp.dev/doc/WinFsp-Tutorial/)
- [Application Logger Documentation](LOGGING.md)
- [Debugging Guide](DEBUGGING.md)