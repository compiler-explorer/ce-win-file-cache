# CeWinFileCacheFS

[![Tests](https://github.com/compiler-explorer/ce-win-file-cache/actions/workflows/test.yml/badge.svg)](https://github.com/compiler-explorer/ce-win-file-cache/actions/workflows/test.yml)
[![Code Quality](https://github.com/compiler-explorer/ce-win-file-cache/actions/workflows/code-quality.yml/badge.svg)](https://github.com/compiler-explorer/ce-win-file-cache/actions/workflows/code-quality.yml)

A WinFsp-based hybrid cache filesystem for Compiler Explorer, designed to efficiently serve multiple MSVC compiler versions by caching frequently-used files locally while falling back to network shares for less-critical files.

## Features

- **Hybrid Caching**: Intelligent caching of compiler executables and headers with LRU eviction
- **Async Download Manager**: Multi-threaded download system with configurable worker pools
- **Prometheus Metrics**: Comprehensive metrics collection for monitoring cache performance
- **Always-Cached Directory Tree**: Fast directory navigation with complete metadata caching
- **Memory Cache Manager**: High-performance in-memory caching with configurable size limits
- **JSON Configuration**: Flexible configuration with support for multiple compiler versions
- **Cross-Platform Development**: Windows target with macOS development and testing support

## Prerequisites

### For Production (Windows)
- Windows 10/11 or Windows Server 2016+
- [WinFsp](https://github.com/winfsp/winfsp) installed
- Visual Studio 2019+ or compatible C++ compiler with C++20 support
- CMake 3.20+

### For Development (macOS/Linux)
- C++20 compatible compiler (GCC 10+, Clang 12+)
- CMake 3.20+
- prometheus-cpp (automatically downloaded via FetchContent)

## Building

### Native Windows Build (Recommended)

1. Clone this repository with submodules:
```cmd
git clone --recursive https://github.com/your-org/ce-win-file-cache.git
cd ce-win-file-cache
git submodule update --init --recursive
```

2. Install [WinFsp](https://github.com/winfsp/winfsp/releases) if not already installed

3. Run the native Windows build:
```cmd
build-msvc.bat
```

This will:
- Set up the MSVC environment automatically
- Build with optimizations enabled
- Copy required DLLs and config files to the output directory

### Building from WSL with MSVC

If developing from WSL, you can cross-compile using MSVC:

1. **Important**: Source must be on a Windows drive (e.g., `/mnt/d/` or `/mnt/c/`)
2. Run the WSL build script:
```bash
./build-msvc.sh
```

### Alternative Build Methods

#### Manual CMake Build
```cmd
# Set up MSVC environment first (run from Developer Command Prompt)
mkdir build && cd build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release
```

#### Wine Build (Experimental)
For cross-platform development or CI on Linux:
```bash
./build-wine.sh
wine build-wine/bin/CeWinFileCacheFS.exe --help
```

### macOS/Linux Development Build

For cross-platform development and testing:

```bash
# Build with integrated static analysis
./build-macos.sh

# Run comprehensive test suite
./run_all_tests.sh

# Available options
./run_all_tests.sh --help     # Show usage information  
./run_all_tests.sh --clean    # Clean build before testing
./run_all_tests.sh --quick    # Skip CMake configuration
```

The `build-macos.sh` script includes integrated clang-tidy static analysis (if available) to ensure code quality and catch potential issues during development.

The test runner builds and executes 12 comprehensive test programs:
- Cache operations and performance validation
- Async download manager with stress testing
- Prometheus metrics collection and validation  
- Directory tree caching and navigation
- Configuration loading and validation
- Edge case handling and error scenarios
- Glob pattern matching with comprehensive unit tests
- JSON configuration parsing and validation

### Testing the Build

After building, test the installation:

**On Windows:**
```cmd
# Test config parsing
.\build-msvc\bin\CeWinFileCacheFS.exe --test-config

# Test path resolution
.\build-msvc\bin\CeWinFileCacheFS.exe --test-paths

# Test network mapping
.\build-msvc\bin\CeWinFileCacheFS.exe --test-network

# Run all tests
.\build-msvc\bin\CeWinFileCacheFS.exe --test
```

**On macOS/Linux:**
```bash
# Run comprehensive test suite
./run_all_tests.sh
```

## Configuration

Create a `compilers.json` file (see included example):

```json
{
  "global": {
    "total_cache_size_mb": 8192,
    "eviction_policy": "lru",
    "cache_directory": "D:\\CompilerCache",
    "download_threads": 6
  },
  "metrics": {
    "enabled": true,
    "bind_address": "127.0.0.1",
    "port": 8080,
    "endpoint_path": "/metrics"
  },
  "compilers": {
    "msvc-14.40": {
      "network_path": "\\\\127.0.0.1\\efs\\compilers\\msvc\\14.40.33807-14.40.33811.0",
      "cache_size_mb": 2048,
      "cache_always": [
        "bin/Hostx64/x64/*.exe",
        "bin/Hostx64/x64/*.dll",
        "include/**/*.h",
        "lib/x64/*.lib"
      ],
      "prefetch_patterns": [
        "include/**/*.h",
        "include/**/*.hpp"
      ]
    },
    "windows-kits-10": {
      "network_path": "\\\\127.0.0.1\\efs\\compilers\\windows-kits-10",
      "cache_size_mb": 1024,
      "cache_always": [
        "Include/**/*.h",
        "Lib/**/*.lib",
        "bin/**/*.exe"
      ],
      "prefetch_patterns": [
        "Include/**/*.h"
      ]
    },
    "ninja": {
      "network_path": "\\\\127.0.0.1\\efs\\compilers\\ninja",
      "cache_size_mb": 64,
      "cache_always": [
        "*.exe"
      ],
      "prefetch_patterns": []
    }
  }
}
```

## Quick Start

For detailed installation and configuration instructions, see the **[Installation & Usage Guide](docs/INSTALLATION_AND_USAGE.md)**.

### Basic Usage

1. **Install WinFsp** from [GitHub Releases](https://github.com/winfsp/winfsp/releases)
2. **Configure** your compilers in `compilers.json` (see example below)
3. **Mount** the filesystem:
   ```cmd
   CeWinFileCacheFS.exe --config compilers.json --mount M:
   ```
4. **Access** your compilers through the cached mount point (e.g., `M:\msvc-14.40\bin\cl.exe`)

### Example: Network Share to Cached Path

| Network Location | Cached Path |
|-----------------|-------------|
| `Z:\compilers\msvc\14.40\bin\cl.exe` | `M:\msvc-14.40\bin\cl.exe` |
| `\\server\tools\ninja.exe` | `M:\tools\ninja.exe` |

Files accessed through the cached paths are automatically cached for faster subsequent access.

### Command Line Options

#### Runtime Options
- `-c, --config FILE`: Configuration file (default: compilers.json)
- `-m, --mount POINT`: Mount point (default: M:)
- `-u, --volume-prefix`: Volume prefix for UNC paths
- `-d, --debug [LEVEL]`: Enable debug logging
- `-h, --help`: Show help message

#### Testing Options (No WinFsp required)
- `-t, --test`: Run all test modes without mounting
- `--test-config`: Test configuration file parsing only
- `--test-paths`: Test virtual path to network path resolution
- `--test-network`: Test network path mapping validation

### Example Usage

```cmd
# Test configuration before mounting
CeWinFileCacheFS.exe --test

# Mount to drive M: with default config
CeWinFileCacheFS.exe

# Mount to specific directory with custom config
CeWinFileCacheFS.exe --config my-compilers.json --mount C:\\compilers

# Enable debug logging
CeWinFileCacheFS.exe --debug --mount M:

# Test specific functionality
CeWinFileCacheFS.exe --test-paths --config compilers.json
```

## Architecture

### File States

- **VIRTUAL**: File exists in metadata only
- **CACHED**: File is stored locally
- **PLACEHOLDER**: Metadata exists, content fetched on demand
- **FETCHING**: Currently downloading from network
- **NETWORK_ONLY**: Always fetch from network

### Cache Policies

- **ALWAYS_CACHE**: Permanently cache these files
- **ON_DEMAND**: Cache after first access
- **NEVER_CACHE**: Always fetch from network

### Directory Structure

```
/
├── msvc-14.40/
│   ├── bin/Hostx64/x64/    # Executables (always cached)
│   ├── include/            # Headers (on-demand caching)
│   └── lib/x64/           # Libraries (on-demand caching)
├── windows-kits-10/
│   ├── Include/           # Windows SDK headers
│   ├── Lib/              # Windows SDK libraries
│   └── bin/              # SDK tools
└── ninja/
    └── ninja.exe         # Build system executable
```

## Performance Considerations

- Executables (`*.exe`, `*.dll`) are cached on first access
- Headers are cached based on usage patterns
- Cache eviction uses LRU algorithm
- Background prefetching for common files
- Network timeouts handled gracefully

## Troubleshooting

### Common Issues

1. **Mount fails**: Ensure WinFsp is installed and you have admin privileges
2. **Network access denied**: Check credentials and network share permissions
3. **Cache full**: Increase `total_cache_size_mb` or clear cache directory

### Debug Logging

Enable debug logging for troubleshooting:
```cmd
CeWinFileCacheFS.exe --debug 1 --mount M:
```

### Cache Management

Clear cache manually:
```cmd
rmdir /s /q C:\CompilerCache
```

## License

This project is licensed under the MIT License - see the LICENSE file for details.

## Contributing

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Add tests if applicable
5. Submit a pull request

### Continuous Integration

The project includes comprehensive CI/CD workflows:

- **Linux CI**: Runs comprehensive test suite on every push/PR with multiple Ubuntu versions and compilers
- **Windows CI**: Full WinFsp integration testing with MSVC builds on Windows runners  
- **Code Quality**: Static analysis, warning checks, and documentation validation

All tests run on their target platforms - Linux tests cover core logic without WinFsp dependencies, while Windows tests validate the complete WinFsp integration.

## Documentation

Comprehensive documentation is available in the [`docs/`](docs/) directory:

- **[Installation & Usage Guide](docs/INSTALLATION_AND_USAGE.md)** - Complete setup and configuration guide
- **[Architecture Overview](docs/ARCHITECTURE.md)** - System design, components, and visual diagrams
- **[Windows CI Pipeline](docs/WINDOWS_CI_PLAN.md)** - Complete CI/CD implementation details
- **[Testing Guide](docs/TESTING.md)** - Test framework and validation procedures
- **[Caching Design](docs/CACHING_DESIGN.md)** - Cache algorithms and policies
- **[Async Download Flow](docs/ASYNC_DOWNLOAD_FLOW.md)** - Multi-threaded download system
- **[Development Setup](docs/REMOTE_DEV_GUIDE.md)** - Remote development configuration
- **[TODO List](docs/TODO.md)** - Implementation roadmap and pending items

## Monitoring and Metrics

The system provides comprehensive Prometheus metrics for monitoring cache performance:

### Available Metrics

- **Cache metrics**: Hit/miss rates, cache size, eviction counts
- **Download metrics**: Queue depth, completion rates, failure reasons, duration histograms
- **Filesystem metrics**: Operation counts and file open duration
- **Network metrics**: Operation success rates and latency

### Accessing Metrics

When metrics are enabled in configuration:
```bash
# View metrics in Prometheus format
curl http://127.0.0.1:8080/metrics
```

Metrics include dynamic labels for detailed analysis:
- Cache hits/misses by operation type
- Download failures by specific reason
- Network operations by success/failure status

## Current Implementation Status

### ✅ Completed Components
- **Memory Cache Manager**: Full LRU caching with metrics integration
- **Async Download Manager**: Multi-threaded downloads with comprehensive testing
- **Directory Tree Caching**: Always-cached directory structure for fast navigation
- **Prometheus Metrics**: Complete metrics collection with dynamic labels
- **JSON Configuration**: Full configuration parsing and validation
- **Glob Pattern Matching**: Proper glob matching for file patterns (*, **, ?)
- **Test Infrastructure**: Comprehensive test suite with automated runner
- **Windows CI/CD Pipeline**: Complete MSVC build and WinFsp integration testing
- **Cross-Platform Development**: Linux development with Windows production target
- **Security Hardening**: Comprehensive memory safety analysis and fixes
- **Thread Safety**: Per-node locking for concurrent directory operations
- **Static Analysis Integration**: clang-tidy checks integrated into build process

### 📝 Remaining Work
- **WinFsp Filesystem Operations**: Complete file read/write operations
- **Production Deployment**: Enhanced logging, error recovery, and monitoring
- **Performance Optimization**: Profile and optimize cache algorithms

## Roadmap

- [ ] WinFsp filesystem driver integration
- [ ] Write support for compiler outputs  
- [ ] File integrity verification
- [ ] Compression for cached files
- [ ] GUI configuration tool
- [ ] Production logging and monitoring
- [ ] Docker container support