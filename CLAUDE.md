# C/C++ Coding Style Guide

This document describes the coding conventions for C/C++ projects.

## Naming Conventions

- **Classes/Structs**: PascalCase (e.g., `TextParser`, `DataProcessor`)
- **Methods/Functions**: camelCase (e.g., `handleData`, `fromStream`)
- **Variables**: snake_case (e.g., `file_index`, `is_valid`)
- **Private Members**: use snake_case like other variables, do NOT add trailing underscore suffixes (e.g., use `member_name` not `member_name_`) 
- **Namespaces**: PascalCase (e.g., `MyProject`)
- **Files**: lowercase with underscores (e.g., `parser.cpp`, `parser.hpp`)

## Formatting

- **Indentation**: 4 spaces (no tabs)
- **Brace Style**: Allman/BSD style - opening braces on new lines
```cpp
if (condition)
{
    // code
}
```
- **Line Length**: ~100-120 characters
- **Spacing**: Consistent spacing around operators and after commas

## Headers and Includes

- **Header Guards**: Use `#pragma once`
- **Include Order**:
  1. Local project headers (e.g., `../types/...`)
  2. Standard library headers (e.g., `<string>`, `<vector>`)

## C++ Features

- **Standard**: C++20
- **Modern Features to Use**:
  - `std::string_view` for string parameters
  - `std::optional` for nullable returns
  - `std::unique_ptr` for ownership
  - `auto` for obvious type inference
  - Range-based for loops
- **STL Containers**: Prefer `std::vector` and `std::unordered_map`

## Code Organization

- Place types in `types/` directory
- Separate components into their own subdirectories
- Use inheritance for polymorphism (e.g., interface base classes)
- Keep utility functions in `utils/`
- Don't use "pimpl" as a technique to hide implementation, always add a properly named headers, sources and classes

## Error Handling

- Use exceptions for critical errors
- Return `std::optional` for functions that may fail
- Implement reasonable limits (e.g., max iterations, buffer sizes)
- Report errors to stderr

## Comments

- Use minimal inline comments
- Format TODOs as `// todo: description`
- No need for extensive documentation headers

## Build Configuration

- Enable strict warnings: `-Werror -Wall -Wextra -Wconversion`
- Use sanitizers in debug builds
- Enable `-O3` and LTO for release builds

## Example Code Style

```cpp
#pragma once

#include "../types/data_line.hpp"
#include <string>
#include <vector>

namespace MyProject
{

class DataParser
{
public:
    std::optional<ParseResult> parse(std::string_view input)
    {
        if (input.empty())
        {
            return std::nullopt;
        }
        
        ParseResult result;
        for (const auto& line : splitLines(input))
        {
            result.lines.push_back(parseLine(line));
        }
        
        return result;
    }
    
private:
    DataLine parseLine(std::string_view line);
    std::vector<std::string_view> splitLines(std::string_view input);
};

} // namespace MyProject
```

## Windows API Style Conventions

### Naming Conventions for Windows Code

- **Classes**: PascalCase with descriptive names (e.g., `SecurityContainer`, `ProcessManager`)
- **Functions**: Either PascalCase for standalone functions (e.g., `CreateResourceName`, `SpawnProcess`) or camelCase for methods
- **Variables**: Generally camelCase (e.g., `hUserToken`, `hResource`)
- **Constants/Enums**: UPPER_CASE with underscores or PascalCase for enum values
- **Windows Types**: Follow Windows conventions (e.g., `HANDLE`, `DWORD`, `PSID`)

### Windows API Integration

- **Headers**: Include Windows headers after project headers
```cpp
#include "../include/config.hpp"
#include <windows.h>
#include <aclapi.h>
```
- **Pragma Comments**: Use for linking libraries
```cpp
#pragma comment(lib, "Userenv.lib")
```
- **Wide Strings**: Use `std::wstring` and `wchar_t*` for Windows APIs
- **Error Handling**: Dedicated error checking functions for Windows APIs
```cpp
CheckWin32(CreateProcessW(...), L"CreateProcessW");
CheckStatus(SetNamedSecurityInfoW(...), L"SetNamedSecurityInfoW");
```

### Windows-Specific Patterns

- **RAII for Windows Resources**: Wrap Windows handles in classes with destructors
```cpp
class ResourceManager
{
    HANDLE resource{};
    ~ResourceManager() { CloseHandle(resource); }
};
```
- **Security Descriptors**: Use `SECURITY_CAPABILITIES` and proper ACL manipulation
- **Process Creation**: Use `STARTUPINFOEX` for extended process attributes
- **Wide Character APIs**: Always use W-suffix functions (e.g., `CreateProcessW` not `CreateProcessA`)

### Error Handling for Windows APIs

- **Wrapper Functions**: Create dedicated error checking wrappers
```cpp
void CheckWin32(BOOL res, const wchar_t *action);
void CheckStatus(DWORD status, const wchar_t *action);
```
- **Error Messages**: Use `FormatMessageW` for human-readable error descriptions
- **Debug Output**: Use `std::wcerr` for wide character error output

### Memory Management

- **Windows Allocations**: Use appropriate free functions
```cpp
LocalFree(sids[i]);  // For LocalAlloc allocations
free(desktop);       // For malloc allocations
```
- **Smart Pointers**: Where possible, wrap Windows resources in RAII classes

### Example Windows API Style

```cpp
namespace MyProject
{

class SecurityContainer
{
private:
    std::wstring name;
    SECURITY_CAPABILITIES sec_cap = {};
    
    void CreateContainer()
    {
        HRESULT hr = CreateAppContainerProfile(this->name.c_str(), 
                                               L"mysandbox", 
                                               L"mysandbox", 
                                               sec_cap.Capabilities,
                                               sec_cap.CapabilityCount, 
                                               &sec_cap.AppContainerSid);
        if (FAILED(hr))
        {
            std::wcerr << L"CreateAppContainerProfile failed: " << hr << L"\n";
            throw std::exception("abort");
        }
    }

public:
    ~SecurityContainer()
    {
        DeleteAppContainerProfile(this->name.c_str());
    }
};

} // namespace MyProject
```

## Building with MSVC from WSL

To compile this project with MSVC from WSL:

1. **IMPORTANT**: Copy the source code to a Windows drive (e.g., `/mnt/c/` or `/mnt/d/`) first. Building from WSL filesystem paths won't work with MSVC.
2. Ensure MSVC compiler is available at: `/mnt/d/efs/compilers/msvc/14.40.33807-14.40.33811.0/bin/Hostx64/x64/cl.exe`
3. Ensure Windows SDK is available at: `/mnt/d/efs/compilers/windows-kits-10`
4. From the Windows drive location, run: `./build-msvc.sh`

The build uses the `msvc-toolchain.cmake` file to configure CMake with the correct paths and flags.

### Native Windows Build (Recommended)

For native Windows development, use the Windows batch script:

```cmd
# Run from Windows Command Prompt or PowerShell
build-msvc.bat
```

This script:
- Sets up MSVC environment variables automatically
- Configures temp directory to avoid disk space issues
- Builds with Ninja for faster compilation
- Copies required DLLs and config files to output directory
- Uses Windows CMake and tools directly

### Testing the Build

After successful build, test the functionality:

```bash
# From WSL
./build-msvc/bin/CeWinFileCacheFS.exe --test

# From Windows
.\build-msvc\bin\CeWinFileCacheFS.exe --test
```

Available test modes:
- `--test-config`: Test YAML configuration parsing
- `--test-paths`: Test virtual path to network path resolution  
- `--test-network`: Test network path mapping validation
- `--test`: Run all tests

### Development Workflow

1. **Initial Setup**: Use native Windows build (`build-msvc.bat`) for best performance
2. **Iterative Development**: Can use WSL build (`./build-msvc.sh`) for faster edit-compile cycles
3. **Testing**: Use isolated test functions to verify functionality before WinFsp mounting
4. **Debugging**: Enable debug builds by modifying CMake configuration

### Quick Build Commands from Windows Drive

```bash
# After copying source to Windows drive
cd /mnt/d/your-path/ce-win-file-cache

# Fast rebuild after changes
./build-msvc.sh

# Clean rebuild
rm -rf build-msvc && ./build-msvc.sh

# Test specific functionality
./build-msvc/bin/CeWinFileCacheFS.exe --test-paths
```

# Project Status and Implementation Notes

## Current Implementation Status

### ✅ Completed (Fully Implemented & Tested)
- **YAML Configuration Parsing**: Fixed regex issues, handles compiler names with hyphens correctly
- **Path Resolution (TODO #3)**: Virtual paths like `/msvc-14.40/bin/cl.exe` → network UNC paths
- **Network Path Mapping (TODO #4)**: Complete validation of path mapping with test cases
- **Build System**: Native Windows build with MSVC, WSL cross-compilation, environment setup, macOS test runner
- **Test Framework**: Isolated test functions with command-line options (`--test-config`, `--test-paths`, `--test-network`)
- **Prometheus Metrics Integration**: Complete metrics collection with dynamic labels, refactored from pimpl pattern
- **Memory Cache Manager**: Fully functional in-memory caching with LRU eviction and metrics integration
- **Async Download Manager**: Multi-threaded download system with comprehensive testing and metrics
- **Directory Tree Caching**: Always-cached directory structure with comprehensive testing

### 📝 Remaining TODOs
- **WinFsp Integration**: Integration with Windows filesystem driver for production use
- **Cache Policy Configuration**: Pattern-based caching rules and fine-grained control
- **Proper Glob Matching**: Enhanced pattern matching beyond basic string operations
- **Production Hardening**: Error recovery, logging, and monitoring for production deployment

## Key Learnings from Development

### Build System Evolution
1. **Started with**: Wine cross-compilation approach
2. **Moved to**: MSVC native compilation as primary target
3. **Current setup**: `build-msvc.bat` for Windows, `build-msvc.sh` for WSL
4. **Key insight**: Windows paths must be used consistently, no `/mnt/d` mixing

### YAML Parsing Challenges
- **Issue**: Regex patterns `^\s+` didn't handle specific whitespace (2 spaces for compilers, 4 for properties)
- **Solution**: Use original line for regex matching, trimmed line for empty/comment detection
- **Lesson**: Always test parsing with real data early in development

### Metrics Implementation Evolution
- **Initial approach**: Pimpl pattern to hide prometheus-cpp dependencies
- **Refactoring**: Created `PrometheusMetricsImpl` class in separate header/source files
- **Key improvement**: Eliminated forward declaration issues and improved code organization
- **Dynamic labels**: Implemented proper Counter families for dynamic reason tracking
- **Coding standards**: Clarified snake_case usage vs trailing underscore suffixes

### Path Resolution Architecture
- **Virtual paths**: `/compiler-name/relative/path`
- **Network paths**: `\\\\server\\share\\compiler-name\\relative\\path`
- **Key insight**: Convert forward slashes to backslashes for Windows compatibility

### Testing Strategy
- **Isolated functions**: Each component implemented as standalone, testable function
- **No WinFsp dependency**: Tests run without mounting filesystem
- **Command-line driven**: Easy to integrate into CI/CD pipelines
- **Comprehensive test runner**: Automated macOS test script (`run_all_tests.sh`) with 10 test programs
- **Metrics validation**: Prometheus metrics tested with dynamic labels and proper cleanup

## Development Recommendations

### For Future Work
1. **WinFsp Integration**: Connect the cache system to the Windows filesystem driver
2. **Production Deployment**: Add proper logging, error recovery, and monitoring
3. **Performance Optimization**: Profile and optimize cache hit rates and memory usage
4. **Configuration Management**: Enhance cache policies and pattern-based rules

### Obsolete Files Identified
These files can be safely removed as they represent outdated build approaches:
- `compile-wine-simple.sh` - Hardcoded paths, deprecated structure
- `setup-vscode.sh` - Wine-focused, superseded by MSVC setup
- `build.bat` - Generic CMake, no MSVC environment setup
- `cl-wrapper.sh` / `link-wrapper.sh` - Complex path conversion, replaced by environment variables

### File Organization Insights
- **Current active files**: `build-msvc.bat`, `build-msvc.sh`, `msvc-toolchain.cmake`, `run_all_tests.sh`
- **Config format**: YAML with escaped backslashes for Windows UNC paths
- **Build outputs**: Always in `build-msvc/` directory with proper DLL/config copying
- **Test infrastructure**: Comprehensive test suite with automated runner for macOS development

## macOS Development and Testing

### Test Runner Script

The project includes a comprehensive test runner for macOS development:

```bash
# Full test suite with build
./run_all_tests.sh

# Available options
./run_all_tests.sh --help     # Show usage information
./run_all_tests.sh --clean    # Clean build before testing
./run_all_tests.sh --quick    # Skip CMake configuration
```

### Test Coverage

The test runner executes 10 comprehensive test programs:

1. **cache_test** - Memory cache operations and performance validation
2. **cache_demo** - Real-world performance demonstration with large files
3. **directory_test** - Directory tree caching and navigation
4. **async_test** - Async download manager with stress testing and metrics
5. **filesystem_async_test** - Filesystem integration simulation
6. **config_threads_test** - YAML configuration loading and validation
7. **config_async_test** - Async manager configuration validation
8. **single_thread_test** - Single-threaded operation edge cases
9. **edge_cases_test** - Edge case handling (0 threads, large counts, rapid operations)
10. **metrics_test** - Prometheus metrics collection with dynamic labels

### Key Testing Features

- **Automated build**: CMake configuration with test and metrics support
- **Config file management**: Auto-creates missing configuration files
- **Proper working directory**: Tests run from correct project root
- **Colored output**: Clear success/failure indicators
- **Comprehensive reporting**: Detailed summary with failure analysis
- **Metrics validation**: Prometheus integration testing with cleanup verification