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

1. Clone this repository with submodules:
```cmd
git clone --recursive https://github.com/your-org/ce-win-file-cache.git
```
Or if already cloned:
```cmd
git submodule update --init --recursive
```

2. Install WinFsp development package (optional - libraries included in submodule)
3. Run the build script:

```cmd
build.bat
```

For debug builds:
```cmd
build.bat debug
```

## Configuration

Create a `compilers.yaml` file (see included example):

```yaml
compilers:
  msvc-14.29:
    network_path: "\\\\fileserver\\compilers\\msvc-14.29"
    cache_size_mb: 2048
    cache_always:
      - "bin/**/*.exe"
      - "bin/**/*.dll"
    prefetch_patterns:
      - "**/*.h"

global:
  total_cache_size_mb: 8192
  eviction_policy: "lru"
  cache_directory: "C:\\CompilerCache"
```

## Usage

### Basic Usage

Mount the filesystem:
```cmd
CompilerCacheFS.exe --config compilers.yaml --mount M:
```

### Command Line Options

- `-c, --config FILE`: Configuration file (default: compilers.yaml)
- `-m, --mount POINT`: Mount point (default: M:)
- `-u, --volume-prefix`: Volume prefix for UNC paths
- `-d, --debug [LEVEL]`: Enable debug logging
- `-h, --help`: Show help message

### Example Usage

```cmd
# Mount to drive M: with default config
CompilerCacheFS.exe

# Mount to specific directory with custom config
CompilerCacheFS.exe --config my-compilers.yaml --mount C:\\compilers

# Enable debug logging
CompilerCacheFS.exe --debug --mount M:
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
├── msvc-14.29/
│   ├── bin/           # Executables (always cached)
│   ├── include/       # Headers (on-demand caching)
│   └── lib/          # Libraries (on-demand caching)
├── msvc-14.32/
└── msvc-14.35/
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