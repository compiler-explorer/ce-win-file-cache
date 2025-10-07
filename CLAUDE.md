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
  - `std::string_view` for string parameters that are read-only (used but not changed)
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
- **Do not use private structs inside classes, keep them outside of the class, and preferable in separate headers**

## Header/Implementation Separation

- **Keep only declarations in header files (.hpp)** - no inline implementations unless they are templates
- **Move all implementations to source files (.cpp)** to improve compilation times
- **Use forward declarations** where possible to reduce header dependencies
- **Inline methods only for templates** or trivial one-liner getters/setters
- **Examples**:
  ```cpp
  // Good: Header file (directory_tree.hpp)
  struct DirectoryNode {
      std::wstring name;
      DirectoryNode *findChild(const std::wstring &child_name);  // Declaration only
  };
  
  // Good: Source file (directory_tree.cpp)
  DirectoryNode *DirectoryNode::findChild(const std::wstring &child_name) {
      // Implementation here
  }
  ```

## Thread Safety

- **Use per-object mutexes** for fine-grained locking when objects can be accessed concurrently
- **Protect all shared data structures** with appropriate synchronization primitives
- **Use `std::atomic<bool>`** for simple flags that need thread-safe access
- **Prefer RAII lock guards** (`std::lock_guard`, `std::unique_lock`) over manual lock/unlock
- **Examples**:
  ```cpp
  // Good: Per-object mutex for fine-grained control
  struct DirectoryNode {
      std::unordered_map<std::wstring, std::unique_ptr<DirectoryNode>> children;
      mutable std::mutex children_mutex;  // Protects children map
      
      DirectoryNode *findChild(const std::wstring &child_name) {
          std::lock_guard<std::mutex> lock(children_mutex);
          auto it = children.find(child_name);
          return (it != children.end()) ? it->second.get() : nullptr;
      }
  };
  ```

## Error Handling

- Use exceptions for critical errors
- Return `std::optional` for functions that may fail
- Implement reasonable limits (e.g., max iterations, buffer sizes)
- Report errors to stderr

## String Parameter Guidelines

- **Use `std::string_view`** for function parameters that accept strings which are only read (not modified or stored)
- **Use `const std::string&`** for parameters that need to be stored or when the function might outlive the caller's string
- **Use `std::string`** for parameters that will be modified or when taking ownership
- **Examples**:
  ```cpp
  // Good: read-only parameter
  bool validatePath(std::string_view path);
  
  // Good: parameter will be stored
  void setConfigPath(const std::string& path);
  
  // Good: parameter will be modified
  void processPath(std::string& path);
  ```

### Critical string_view Rule

**NEVER use `std::string_view` parameters if you need to create a null-terminated string from them**. This defeats the optimization purpose:

```cpp
// BAD: Creates string anyway, optimization is pointless
std::wstring utf8ToWide(std::string_view utf8_str)
{
    std::string utf8_str_nt{utf8_str}; // Creates string!
    return convertToWide(utf8_str_nt.c_str());
}

// GOOD: Use const std::string& for C APIs needing null-terminated strings
std::wstring utf8ToWide(const std::string& utf8_str)
{
    return convertToWide(utf8_str.c_str());
}
```

Functions that interface with:
- C APIs (like `MultiByteToWideChar`, `mbstowcs`)  
- Standard library functions expecting null-terminated strings
- File streams (`std::ofstream`, `std::ifstream`)

Should use `const std::string&` parameters, not `std::string_view`.

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

## Debugging and Development

### Debugging Principles

When investigating bugs or unexpected behavior, follow this systematic approach:

#### 1. Search Comprehensively First

**Before making any changes**, search for ALL related code:

```bash
# Find all functions that might be involved
grep -r "FunctionName" src/
grep -r "RELATED_TYPE" src/
grep -r "keyword" include/

# Use multiple search terms
grep -r "Security" src/ | grep -i "descriptor"
```

**Anti-pattern:** Assuming you know where the bug is and only looking at one file or function.

#### 2. Read Actual Implementation

**Read the code** before making assumptions:

- Don't assume a function does what its name suggests
- Don't assume helper classes are used everywhere
- Don't assume code follows best practices
- Check for hardcoded values, fallbacks, and special cases

**Anti-pattern:** Looking at one function, seeing it's correct, and assuming all related functions work the same way.

#### 3. Verify Actual Behavior

**Test your hypothesis** before fixing:

```bash
# Check actual output
cat output.log | grep "ERROR"

# Verify what's being returned
powershell -Command "Get-Acl Z: | Format-List"

# Check runtime values
./app --debug > debug.log
```

**Anti-pattern:** Blaming external factors (OS, libraries, frameworks) without evidence.

#### 4. Fix the Right Problem

Once you've found the bug:

- Fix the **actual** bug, not a related issue
- Don't fix things that aren't broken
- Don't add workarounds for problems that don't exist

**Anti-pattern:** Finding one issue, fixing it, then continuing to fix "related" things without re-verifying the original problem.

#### 5. Common Debugging Mistakes

❌ **Don't:**
- Blame external systems without proof (OS, libraries, hardware)
- Make assumptions about code you haven't read
- Fix things based on what "should" be there
- Search incompletely (only checking obvious places)
- Add complexity (privileges, workarounds) before understanding the root cause

✅ **Do:**
- Search comprehensively for all related functions
- Read the actual implementation of each function
- Verify behavior with logs, tests, or manual checks
- Fix only the identified bug
- Re-test after the fix to confirm it worked

#### 6. Debugging Mantra

**"Read the code first, speculate second."**

When stuck:
1. What does the code **actually** do? (not what it should do)
2. What is **actually** happening? (logs, output, behavior)
3. Where is the **actual** discrepancy?

See `docs/SECURITY_DESCRIPTOR_DEBUGGING_LESSON.md` for a concrete example of these principles in action.

### Diagnostic Handling

- When a file or directory cannot be found or executed, check pwd to make sure you're in the right directory

## Testing

- Prefer writing unit tests for every new function unless there are good reasons not to

## Build and Execution on WSL

### Building the Project

- **Always use build scripts**: Never run raw `cmake`, `make`, or `ninja` commands directly
- **Use batch files on WSL**: Prefer `.bat` files over shell scripts for build operations
- **Kill running processes before build**: Ensure the filesystem service isn't running
  ```bash
  # Kill any running instance before building
  cmd.exe /c taskkill /F /IM cewinfilecachefs.exe
  ```
- **Build commands**:
  ```bash
  # Debug build
  cmd.exe /c build-msvc.bat
  ```

### Execution

- **Run from WSL**: Execute the built Windows binary from WSL command line
  ```bash
  # Run the built executable
  ./build/bin/ce-win-file-cache.exe --config config.json
  
  # Run with specific logging categories
  ./build/bin/ce-win-file-cache.exe --config config.json --log-categories filesystem,cache
  ```

- **Logging**: Use logging categories to control output verbosity and improve performance
  - Categories: `general`, `filesystem`, `cache`, `network`, `memory`, `access`, `directory`, `security`, `config`, `service`
  - Use `--log-categories all` for full logging or specific categories for targeted debugging

### Debugging

- **Check working directory**: Always verify `pwd` when encountering file/directory access issues
- **Build directory structure**: Ensure build outputs are in expected locations
  - Executable: `build/bin/ce-win-file-cache.exe`
  - Libraries: `build/lib/`
  - Tests: `build/bin/test_*.exe`

### Common Issues

- **Generator conflicts**: CMake remembers the last generator used; clean build directory or use different directory names
- **Path issues**: WSL uses Unix paths but Windows executables expect Windows paths
- **File locking**: Windows processes may lock files preventing rebuild; close applications before rebuilding

## Code Quality Tools

### clang-tidy

clang-tidy is configured to enforce the coding style and catch common issues. 

Run clang-tidy checks:

```bash
# Check all files
./scripts/run-clang-tidy.sh

# Apply fixes automatically
./scripts/run-clang-tidy.sh --fix

# Check specific files
./scripts/run-clang-tidy.sh --file "cache_manager"

# Run with custom build directory
./scripts/run-clang-tidy.sh --build-dir build-debug
```

The configuration (`.clang-tidy`) includes:
- Modern C++20 best practices
- Naming convention enforcement
- Performance optimizations
- Bug-prone pattern detection
- Core Guidelines compliance
```