# CompilerCacheFS

A WinFsp-based hybrid cache filesystem for Compiler Explorer, designed to efficiently serve multiple MSVC compiler versions by caching frequently-used files locally while falling back to network shares for less-critical files.

## Features

- **Hybrid Caching**: Intelligent caching of compiler executables and headers
- **Network Fallback**: Seamless access to files from network shares
- **Multiple Compilers**: Support for multiple MSVC versions simultaneously
- **LRU Eviction**: Automatic cache management with configurable size limits
- **Pattern-based Caching**: Configure which files should always be cached
- **Performance Optimized**: Designed for high-performance compiler workloads

## Prerequisites

- Windows 10/11 or Windows Server 2016+
- [WinFsp](https://github.com/winfsp/winfsp) installed
- Visual Studio 2019+ or compatible C++ compiler with C++20 support
- CMake 3.20+

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

### Testing the Build

After building, test the installation:
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

## Configuration

Create a `compilers.yaml` file (see included example):

```yaml
compilers:
  msvc-14.40:
    network_path: "\\\\127.0.0.1\\efs\\compilers\\msvc\\14.40.33807-14.40.33811.0"
    cache_size_mb: 2048
    cache_always:
      - "bin/Hostx64/x64/*.exe"
      - "bin/Hostx64/x64/*.dll"
      - "include/**/*.h"
      - "lib/x64/*.lib"
    prefetch_patterns:
      - "include/**/*.h"
      - "include/**/*.hpp"
      
  windows-kits-10:
    network_path: "\\\\127.0.0.1\\efs\\compilers\\windows-kits-10"
    cache_size_mb: 1024
    cache_always:
      - "Include/**/*.h"
      - "Lib/**/*.lib"
      - "bin/**/*.exe"
    prefetch_patterns:
      - "Include/**/*.h"
      
  ninja:
    network_path: "\\\\127.0.0.1\\efs\\compilers\\ninja"
    cache_size_mb: 64
    cache_always:
      - "*.exe"
    prefetch_patterns: []

global:
  total_cache_size_mb: 8192
  eviction_policy: "lru"
  cache_directory: "D:\\CompilerCache"
```

## Usage

### Basic Usage

Mount the filesystem:
```cmd
CompilerCacheFS.exe --config compilers.yaml --mount M:
```

### Command Line Options

#### Runtime Options
- `-c, --config FILE`: Configuration file (default: compilers.yaml)
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
CeWinFileCacheFS.exe --config my-compilers.yaml --mount C:\\compilers

# Enable debug logging
CeWinFileCacheFS.exe --debug --mount M:

# Test specific functionality
CeWinFileCacheFS.exe --test-paths --config compilers.yaml
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
CompilerCacheFS.exe --debug 1 --mount M:
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

## Roadmap

- [ ] Write support for compiler outputs
- [ ] File integrity verification
- [ ] Compression for cached files
- [ ] Metrics and monitoring
- [ ] GUI configuration tool
- [ ] Docker container support