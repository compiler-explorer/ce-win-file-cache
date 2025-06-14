# CE Win File Cache - Installation and Usage Guide

## Table of Contents
- [Overview](#overview)
- [Installation](#installation)
- [Configuration](#configuration)
- [Starting the Service](#starting-the-service)
- [Accessing Cached Files](#accessing-cached-files)
- [Real-World Example](#real-world-example)
- [Monitoring Performance](#monitoring-performance)
- [Troubleshooting](#troubleshooting)

## Overview

CE Win File Cache creates a virtual filesystem that sits between your applications and network shares, providing intelligent caching for frequently-used files. When you access files through the cache mount point, they are automatically cached in memory or on local disk for faster subsequent access.

### How It Works

```
Your Application
       ↓
CE Win File Cache (M:\)     ← Virtual mount point with caching
       ↓
Network Share (Z:\)         ← Original network location
```

## Installation

### Prerequisites

1. **Windows 10/11 or Windows Server 2016+** (64-bit)
2. **WinFsp Driver** - Download and install from [WinFsp Releases](https://github.com/winfsp/winfsp/releases)
3. **Administrator privileges** for mounting the filesystem

### Installing CE Win File Cache

1. **Download the latest release** from [GitHub Releases](https://github.com/compiler-explorer/ce-win-file-cache/releases)
2. **Extract the ZIP file** to a directory, e.g., `C:\Program Files\CeWinFileCache\`
3. **Verify the files**:
   ```
   C:\Program Files\CeWinFileCache\
   ├── CeWinFileCacheFS.exe
   ├── winfsp-x64.dll
   ├── winfsp-msil.dll
   └── compilers.example.json
   ```

## Configuration

### Understanding the Configuration Structure

The configuration file (`compilers.json`) maps virtual paths to network locations and defines caching policies.

### Basic Configuration Example

Let's say you have compilers on a network share at `Z:\compilers\`. Here's how to configure CE Win File Cache:

```json
{
  "global": {
    "total_cache_size_mb": 8192,
    "eviction_policy": "lru",
    "cache_directory": "C:\\CeWinFileCache\\cache",
    "download_threads": 6,
    "metrics": {
      "enabled": true,
      "bind_address": "127.0.0.1",
      "port": 8080,
      "endpoint_path": "/metrics"
    }
  },
  "compilers": {
    "msvc-14.40": {
      "network_path": "Z:\\compilers\\msvc\\14.40",
      "cache_size_mb": 2048,
      "cache_always": [
        "bin/Hostx64/x64/*.exe",
        "bin/Hostx64/x64/*.dll"
      ],
      "prefetch_patterns": [
        "bin/Hostx64/x64/cl.exe",
        "bin/Hostx64/x64/link.exe"
      ]
    }
  }
}
```

### Configuration Fields Explained

#### Global Settings
- **`total_cache_size_mb`**: Maximum cache size in megabytes (8192 = 8GB)
- **`eviction_policy`**: Cache eviction strategy (`"lru"` = Least Recently Used)
- **`cache_directory`**: Where to store cached files locally
- **`download_threads`**: Number of concurrent download threads

#### Compiler Settings
- **`network_path`**: The actual network location of the compiler
- **`cache_size_mb`**: Maximum cache size for this specific compiler
- **`cache_always`**: Glob patterns for files to always keep cached
- **`prefetch_patterns`**: Files to cache immediately on startup

### Advanced Configuration Example

For multiple compilers and tools:

```json
{
  "global": {
    "total_cache_size_mb": 16384,
    "eviction_policy": "lru",
    "cache_directory": "D:\\CompilerCache",
    "download_threads": 8
  },
  "compilers": {
    "msvc-14.40": {
      "network_path": "\\\\fileserver\\compilers\\msvc\\14.40",
      "cache_size_mb": 4096,
      "cache_always": [
        "bin/**/*.exe",
        "bin/**/*.dll",
        "include/**/*.h",
        "lib/**/*.lib"
      ],
      "prefetch_patterns": [
        "bin/Hostx64/x64/cl.exe",
        "bin/Hostx64/x64/link.exe"
      ]
    },
    "msvc-14.30": {
      "network_path": "\\\\fileserver\\compilers\\msvc\\14.30",
      "cache_size_mb": 2048,
      "cache_always": [
        "bin/**/*.exe",
        "bin/**/*.dll"
      ]
    },
    "clang-17": {
      "network_path": "\\\\fileserver\\compilers\\clang\\17.0.0",
      "cache_size_mb": 1024,
      "cache_always": [
        "bin/*.exe",
        "lib/*.dll"
      ]
    },
    "build-tools": {
      "network_path": "\\\\fileserver\\tools",
      "cache_size_mb": 512,
      "cache_always": [
        "ninja.exe",
        "cmake/bin/cmake.exe"
      ]
    }
  }
}
```

## Starting the Service

### Basic Start

Open an elevated Command Prompt or PowerShell and run:

```cmd
cd "C:\Program Files\CeWinFileCache"
CeWinFileCacheFS.exe --config compilers.json --mount M:
```

### Advanced Options

```cmd
# Mount to a specific directory instead of drive letter
CeWinFileCacheFS.exe --config compilers.json --mount C:\cached-compilers

# Enable debug logging
CeWinFileCacheFS.exe --config compilers.json --mount M: --debug

# Test configuration before mounting
CeWinFileCacheFS.exe --test-config --config compilers.json
```

### Running as a Windows Service

For production use, you may want to run CE Win File Cache as a Windows Service:

```powershell
# Create service (run as Administrator)
sc create CeWinFileCache binPath= "C:\Program Files\CeWinFileCache\CeWinFileCacheFS.exe --config C:\Program Files\CeWinFileCache\compilers.json --mount M:" start= auto

# Start the service
sc start CeWinFileCache

# Stop the service
sc stop CeWinFileCache

# Delete the service
sc delete CeWinFileCache
```

## Accessing Cached Files

### Path Translation

Once mounted, the virtual filesystem translates paths as follows:

| Network Path | CE Win File Cache Path |
|--------------|------------------------|
| `Z:\compilers\msvc\14.40\bin\Hostx64\x64\cl.exe` | `M:\msvc-14.40\bin\Hostx64\x64\cl.exe` |
| `\\fileserver\compilers\msvc\14.40\include\string.h` | `M:\msvc-14.40\include\string.h` |
| `\\fileserver\tools\ninja.exe` | `M:\build-tools\ninja.exe` |

### Using with Build Systems

Update your build configurations to use the cached paths:

**Before (using network share directly):**
```cmake
set(CMAKE_C_COMPILER "Z:/compilers/msvc/14.40/bin/Hostx64/x64/cl.exe")
set(CMAKE_CXX_COMPILER "Z:/compilers/msvc/14.40/bin/Hostx64/x64/cl.exe")
```

**After (using CE Win File Cache):**
```cmake
set(CMAKE_C_COMPILER "M:/msvc-14.40/bin/Hostx64/x64/cl.exe")
set(CMAKE_CXX_COMPILER "M:/msvc-14.40/bin/Hostx64/x64/cl.exe")
```

## Real-World Example

### Scenario: Speeding Up MSVC Builds

You have:
- MSVC compiler at `Z:\compilers\msvc\14.40\`
- Windows SDK at `Z:\kits\windows\10\`
- Ninja build tool at `Z:\tools\ninja.exe`

#### Step 1: Create Configuration

Save as `compilers.json`:

```json
{
  "global": {
    "total_cache_size_mb": 12288,
    "eviction_policy": "lru",
    "cache_directory": "C:\\CeCache",
    "download_threads": 8,
    "metrics": {
      "enabled": true,
      "bind_address": "127.0.0.1",
      "port": 8080
    }
  },
  "compilers": {
    "msvc-14.40": {
      "network_path": "Z:\\compilers\\msvc\\14.40",
      "cache_size_mb": 4096,
      "cache_always": [
        "bin/Hostx64/x64/cl.exe",
        "bin/Hostx64/x64/link.exe",
        "bin/Hostx64/x64/*.dll",
        "include/**/*.h"
      ]
    },
    "windows-sdk": {
      "network_path": "Z:\\kits\\windows\\10",
      "cache_size_mb": 4096,
      "cache_always": [
        "Include/**/*.h",
        "Lib/**/*.lib"
      ]
    },
    "tools": {
      "network_path": "Z:\\tools",
      "cache_size_mb": 256,
      "cache_always": [
        "ninja.exe"
      ]
    }
  }
}
```

#### Step 2: Start CE Win File Cache

```cmd
CeWinFileCacheFS.exe --config compilers.json --mount M:
```

#### Step 3: Update Build Scripts

**Environment variables:**
```batch
set PATH=M:\msvc-14.40\bin\Hostx64\x64;M:\tools;%PATH%
set INCLUDE=M:\msvc-14.40\include;M:\windows-sdk\Include\10.0.22621.0\ucrt
set LIB=M:\msvc-14.40\lib\x64;M:\windows-sdk\Lib\10.0.22621.0\um\x64
```

**CMake configuration:**
```cmake
cmake -G Ninja -DCMAKE_C_COMPILER=M:/msvc-14.40/bin/Hostx64/x64/cl.exe ..
```

#### Step 4: Build Your Project

```cmd
cd your-project
M:\tools\ninja.exe
```

The first build will cache all accessed files. Subsequent builds will use the cached versions, significantly improving performance.

## Monitoring Performance

### Viewing Metrics

When metrics are enabled, access them at:
```
http://localhost:8080/metrics
```

### Key Metrics to Monitor

- **`cache_hit_rate`**: Percentage of file accesses served from cache
- **`cache_size_bytes`**: Current cache usage
- **`download_queue_depth`**: Number of files being downloaded
- **`filesystem_operations_total`**: Total file operations by type

### Example Metrics Output

```
# HELP cache_hit_total Total number of cache hits
# TYPE cache_hit_total counter
cache_hit_total{operation="file_open"} 15234
cache_hit_total{operation="file_read"} 98765

# HELP cache_miss_total Total number of cache misses
# TYPE cache_miss_total counter
cache_miss_total{operation="file_open"} 1523
cache_miss_total{operation="file_read"} 456

# HELP cache_size_bytes Current cache size in bytes
# TYPE cache_size_bytes gauge
cache_size_bytes{compiler="msvc-14.40"} 2147483648
```

## Troubleshooting

### Common Issues

#### 1. "Mount failed" Error
- **Cause**: WinFsp not installed or not running
- **Solution**: Install WinFsp and restart

#### 2. "Access denied" to Network Shares
- **Cause**: CE Win File Cache runs with your user credentials
- **Solution**: Ensure you have access to the network paths specified in configuration

#### 3. Poor Performance
- **Cause**: Cache misses or insufficient cache size
- **Solution**: 
  - Check metrics for cache hit rate
  - Increase `cache_size_mb` in configuration
  - Add more patterns to `cache_always`

#### 4. Files Not Being Cached
- **Cause**: Files don't match caching patterns
- **Solution**: Update `cache_always` patterns in configuration

### Debug Mode

For detailed logging:
```cmd
CeWinFileCacheFS.exe --config compilers.json --mount M: --debug > debug.log 2>&1
```

### Clearing the Cache

To force a fresh cache:
```cmd
# Stop CE Win File Cache
# Then delete cache directory
rmdir /s /q C:\CeCache
# Restart CE Win File Cache
```

## Best Practices

1. **Cache Size**: Set to 50-75% of available local disk space
2. **Patterns**: Cache executables and libraries with `cache_always`
3. **Prefetch**: Add frequently-used tools to `prefetch_patterns`
4. **Monitoring**: Enable metrics to track performance
5. **Network**: Use wired connections for initial cache population

## Performance Tips

- **First Run**: Allow time for initial cache population
- **SSD Storage**: Use SSD for `cache_directory` for best performance
- **Memory**: System should have enough RAM for in-memory caching
- **Patterns**: Be specific with glob patterns to avoid caching unnecessary files

---

With CE Win File Cache properly configured, you should see significant performance improvements, especially for:
- Repeated compilations
- Header file access
- Library linking
- Tool execution

The cache is transparent to your applications - they access files normally while benefiting from local caching automatically.