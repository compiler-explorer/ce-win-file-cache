# Remote Development Guide - Cache Implementation

## Quick Start for Remote Development (No MSVC/WinFsp)

This guide helps you implement the caching logic from any PC without needing MSVC compiler or WinFsp dependencies.

### Current Status
- ✅ **Path Resolution**: Virtual paths → Network UNC paths (working)
- ✅ **Config Parsing**: YAML configuration with compiler definitions (working)
- ✅ **Test Framework**: Isolated test functions with `--test-*` commands (working)
- 🔄 **TODO #1**: Core caching logic (in progress)
- 📝 **TODO #2**: LRU eviction (planned)

### What You Need to Implement

The core caching system is **in-memory only** - files are loaded from network locations into RAM and served from memory.

#### Core Data Structure
```cpp
// Global in-memory cache
std::unordered_map<std::wstring, std::vector<uint8_t>> memoryCache;
// Key: virtual path like "/msvc-14.40/bin/cl.exe"
// Value: file content in RAM
```

#### 5 Key Functions to Implement

1. **Load from Network to Memory**
```cpp
std::vector<uint8_t> loadNetworkFileToMemory(const std::wstring& networkPath);
```

2. **Check Memory Cache**
```cpp
bool isFileInMemoryCache(const std::wstring& virtualPath);
```

3. **Get from Memory Cache**
```cpp
std::vector<uint8_t> getMemoryCachedFile(const std::wstring& virtualPath);
```

4. **Add to Memory Cache**
```cpp
void addFileToMemoryCache(const std::wstring& virtualPath, 
                         const std::vector<uint8_t>& content);
```

5. **Get File (Cache + Network Fallback)**
```cpp
std::vector<uint8_t> getFileContent(const std::wstring& virtualPath, 
                                   const Config& config);
```

### Implementation Strategy

#### Phase 1: Basic Memory Operations
- Start with simple synchronous file loading
- Use existing path resolution (already working)
- Test with real files via `--test-cache` command
- **No validation needed** - compiler files are read-only

#### Phase 2: Cache Policy
Files are categorized by patterns in YAML:
- `cache_always`: Load into memory immediately
- `cache_on_demand`: Load into memory on first access
- `never_cache`: Always fetch from network

#### Phase 3: Memory Management
- Track memory usage
- Implement LRU eviction when limits reached
- Add `--test-estimate` for memory usage prediction

### Test-Driven Development

Add to `src/main.cpp`:
```cpp
int testCacheOperations(const Config& config)
{
    std::wcout << L"=== Cache Operations Test ===" << std::endl;
    
    // Test files (use real compiler paths)
    std::vector<std::wstring> testFiles = {
        L"/msvc-14.40/bin/Hostx64/x64/cl.exe",
        L"/msvc-14.40/include/iostream",
        L"/ninja/ninja.exe"
    };
    
    for (const auto& virtualPath : testFiles) {
        // 1. Load from network into memory
        // 2. Check cache hit
        // 3. Retrieve from memory cache
        // 4. Verify network fallback works
    }
    
    return 0;
}
```

Add command line option:
```cpp
else if (arg == L"--test-cache")
{
    options.test_mode = true;
    options.test_cache_operations = true;
}
```

### Key Design Decisions

1. **Storage**: Pure in-memory (`std::unordered_map`)
2. **Validation**: None needed (files are immutable)
3. **Error Handling**: Always fallback to network
4. **Cache Policy**: Pattern-based from YAML config
5. **Testing**: Use real compiler files

### File Locations

- **Header**: `include/ce-win-file-cache/cache_manager.hpp`
- **Implementation**: `src/cache_manager.cpp`
- **Tests**: Add to `src/main.cpp` following existing pattern
- **Config**: Extend existing `compilers.yaml`

### Working Without MSVC/WinFsp

You can implement and test the caching logic entirely through:
1. **Mock network paths** - Use local file paths for testing
2. **Isolated functions** - Test cache operations independently
3. **Memory-only operations** - No disk or filesystem dependencies
4. **Command-line testing** - Use `--test-cache` like existing tests

### Success Criteria

Implementation is complete when:
- `./CeWinFileCacheFS.exe --test-cache` passes all tests
- Files load correctly from network into memory
- Memory cache hits work (no network access)
- Memory cache misses trigger network fallback
- Cache policies apply correctly based on file patterns

### Next Steps After Cache Implementation

1. **LRU Eviction** (TODO #2) - Memory management when limits reached
2. **Async Operations** (TODO #5) - Non-blocking file operations
3. **WinFsp Integration** - Mount as actual filesystem

### Configuration Example

```yaml
compilers:
  msvc-14.40:
    network_path: "\\\\127.0.0.1\\efs\\compilers\\msvc\\14.40.33807-14.40.33811.0"
    cache_size_mb: 2048
    cache_always:
      - "bin/**/*.exe"
      - "bin/**/*.dll"
    cache_on_demand:
      - "include/**/*.h"
    never_cache:
      - "**/*.tmp"
```

This keeps implementation simple and focused on the core caching logic without external dependencies.