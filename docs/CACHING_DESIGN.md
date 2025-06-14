# In-Memory Caching Logic Design and Implementation Strategy

This document outlines the design approach for implementing TODO #1: Actual Caching Logic in CompilerCacheFS.

## Current Foundation

### ✅ What We Have
- **Path Resolution**: Virtual paths → Network UNC paths (tested)
- **Network Mapping**: Complete path validation (tested)
- **Config System**: YAML parsing with compiler configurations
- **Test Framework**: Isolated functions with command-line testing
- **Build System**: Working MSVC compilation

### 🎯 What We Need
Core in-memory file caching operations that load files from network locations into RAM and serve them from memory with proper management.

---

## Design Options

### Option A: Simple In-Memory Approach (Recommended for MVP)

**Philosophy**: Start with basic in-memory caching, load files into RAM, serve from memory with network fallback.

**Implementation Strategy**:
```cpp
// Core in-memory operations to implement
std::vector<uint8_t> loadFileToMemory(const std::wstring& networkPath);
bool isFileInMemoryCache(const std::wstring& virtualPath);
std::vector<uint8_t> getFileFromCache(const std::wstring& virtualPath);
void addFileToMemoryCache(const std::wstring& virtualPath, std::vector<uint8_t> content);

// In-memory cache storage
std::unordered_map<std::wstring, std::vector<uint8_t>> memoryCache;
```

**Advantages**:
- Fast access once cached (RAM speed)
- No disk I/O for cached files
- Simple to implement and test
- Clean memory management

**Disadvantages**:
- Limited by available RAM
- Large files consume significant memory
- Memory pressure considerations

### Option B: Async In-Memory with Progress Tracking

**Philosophy**: Implement non-blocking memory operations from the start for better user experience.

**Implementation Strategy**:
```cpp
// Async in-memory operations with callbacks
class MemoryCacheOperation {
public:
    enum Status { Pending, InProgress, Completed, Failed };
    
    std::future<std::vector<uint8_t>> loadFileAsync(const std::wstring& networkPath,
                                                    std::function<void(float)> progressCallback);
};
```

**Advantages**:
- Non-blocking operations
- Progress tracking for large files
- Better scalability
- Modern C++ patterns

**Disadvantages**:
- More complex to implement
- Harder to test initially
- Thread safety considerations
- Memory management complexity

### Option C: Hybrid In-Memory Approach (Recommended)

**Philosophy**: Start with simple synchronous in-memory operations for testing, then add async layer.

**Implementation Strategy**:
1. **Phase 1**: Implement synchronous in-memory operations with comprehensive testing
2. **Phase 2**: Add async wrapper around working synchronous memory operations
3. **Phase 3**: Optimize and add advanced memory management features

---

## Recommended Implementation Plan

### Phase 1: Basic Synchronous Caching (TODO #1)

#### 1.1 Test-Driven Development Approach

Create isolated test functions following our established pattern:

```cpp
// In src/main.cpp - following existing pattern
int testCacheOperations(const Config& config);
```

Add command line options:
```cpp
// New command line options
else if (arg == L"--test-cache")
{
    options.test_mode = true;
    options.test_cache_operations = true;
}
else if (arg == L"--test-estimate")
{
    options.test_mode = true;
    options.test_cache_estimation = true;
}
```

#### 1.2 Core Cache Operations (Based on Decisions)

Implement these functions with network fallback and no validation:

```cpp
namespace CeWinFileCache {
namespace CacheOperations {

// Basic in-memory operations with network fallback
std::vector<uint8_t> loadNetworkFileToMemory(const std::wstring& networkPath);

bool isFileInMemoryCache(const std::wstring& virtualPath);

std::vector<uint8_t> getMemoryCachedFile(const std::wstring& virtualPath);

void addFileToMemoryCache(const std::wstring& virtualPath, 
                         const std::vector<uint8_t>& content);

// Network fallback operations
std::vector<uint8_t> getFileContent(const std::wstring& virtualPath, 
                                   const Config& config);

// Cache policy with template support
enum CachePolicy { ALWAYS_CACHE, ON_DEMAND, NEVER_CACHE };

CachePolicy determineCachePolicy(const std::wstring& virtualPath,
                                const std::wstring& compilerName,
                                const Config& config);

// Memory cache stats
struct MemoryCacheStats {
    size_t totalFiles;
    size_t totalMemoryBytes;
    size_t mapOverheadBytes;     // std::unordered_map overhead
    std::vector<std::wstring> cachedCompilers;
};

MemoryCacheStats getMemoryCacheStatistics();

// Memory cache size estimation
struct MemoryCacheEstimation {
    size_t totalMemoryBytes;
    size_t fileCount;
    size_t mapOverheadBytes;     // Memory overhead for std::unordered_map
    size_t keyOverheadBytes;     // Memory for std::wstring keys
    size_t alwaysCacheBytes;     // Files that must be cached in memory
    size_t onDemandBytes;        // Files cached in memory on first access
    size_t neverCacheBytes;      // Files that won't be cached (always network)
};

MemoryCacheEstimation estimateCompilerMemoryUsage(const std::wstring& networkPath, 
                                                  const CompilerConfig& config);

} // namespace CacheOperations
} // namespace CeWinFileCache
```

#### 1.3 Test Cases for Cache Operations (Real Files)

```cpp
int testCacheOperations(const Config& config)
{
    std::wcout << L"=== Cache Operations Test ===" << std::endl;
    
    // Test with real MSVC files
    std::vector<std::wstring> testFiles = {
        L"/msvc-14.40/bin/Hostx64/x64/cl.exe",        // Large executable
        L"/msvc-14.40/include/iostream",              // Header file
        L"/windows-kits-10/Include/10.0.22621.0/um/windows.h",  // SDK header
        L"/ninja/ninja.exe"                           // Simple executable
    };
    
    for (const auto& virtualPath : testFiles) {
        std::wcout << L"Testing cache operations for: " << virtualPath << std::endl;
        
        // Test cases:
        // 1. Load file from network into memory
        // 2. Store file content in memory cache (std::unordered_map)
        // 3. Cache hit detection (memory map lookup)
        // 4. Memory cache retrieval
        // 5. Network fallback (always works)
        // 6. Cache policy determination (template-based)
        // 7. Memory usage statistics (RAM consumption)
        
        // No validation needed - files are read-only
        // Network fallback ensures reliability
    }
    
    return 0;
}

int testCacheEstimation(const Config& config)
{
    std::wcout << L"=== Memory Cache Size Estimation Test ===" << std::endl;
    
    // Estimate memory usage for configured compilers
    size_t totalEstimatedMemory = 0;
    size_t totalFileCount = 0;
    size_t totalMapOverhead = 0;
    
    for (const auto& [compilerName, compilerConfig] : config.compilers) {
        std::wcout << L"Analyzing compiler: " << compilerName << std::endl;
        
        // Scan network path to estimate memory usage
        std::wstring networkPath = compilerConfig.network_path;
        
        // Estimate based on cache policy patterns
        auto estimation = estimateCompilerMemoryUsage(networkPath, compilerConfig);
        
        std::wcout << L"  Network path: " << networkPath << std::endl;
        std::wcout << L"  Estimated memory usage: " << (estimation.totalMemoryBytes / 1024 / 1024) << L" MB" << std::endl;
        std::wcout << L"  Estimated file count: " << estimation.fileCount << std::endl;
        std::wcout << L"  Map overhead: " << (estimation.mapOverheadBytes / 1024) << L" KB" << std::endl;
        std::wcout << L"  Key overhead: " << (estimation.keyOverheadBytes / 1024) << L" KB" << std::endl;
        std::wcout << L"  Always cache files: " << estimation.alwaysCacheBytes / 1024 / 1024 << L" MB" << std::endl;
        std::wcout << L"  On-demand files: " << estimation.onDemandBytes / 1024 / 1024 << L" MB" << std::endl;
        
        totalEstimatedMemory += estimation.totalMemoryBytes;
        totalFileCount += estimation.fileCount;
        totalMapOverhead += estimation.mapOverheadBytes + estimation.keyOverheadBytes;
    }
    
    std::wcout << L"\n=== Total Memory Estimation ===" << std::endl;
    std::wcout << L"Total estimated memory usage: " << (totalEstimatedMemory / 1024 / 1024) << L" MB" << std::endl;
    std::wcout << L"Total file count: " << totalFileCount << std::endl;
    std::wcout << L"Total map overhead: " << (totalMapOverhead / 1024) << L" KB" << std::endl;
    std::wcout << L"Available system RAM: " << L"[query system memory]" << L" MB" << std::endl;
    
    // Note: No configured limit check since users will decide based on this output
    std::wcout << L"\n💡 Use this information to decide on memory limits and LRU policies" << std::endl;
    
    return 0;
}
```

### Phase 2: Cache Integration with Path Resolution

Combine our working path resolution with new caching operations:

```cpp
// Integration function
std::vector<uint8_t> getCachedOrNetworkFile(const std::wstring& virtualPath, 
                                           const Config& config)
{
    // 1. Use existing path resolution to get network path
    // 2. Check if file is in memory cache
    // 3. If not in cache, load from network into memory
    // 4. Return file content from memory
}
```

### Phase 3: LRU Eviction (TODO #2)

Implement memory-based LRU eviction alongside basic caching:

```cpp
class MemoryLRUManager {
public:
    void recordFileAccess(const std::wstring& virtualPath);
    std::vector<std::wstring> getFilesToEvict(size_t memoryBytesNeeded);
    bool evictFileFromMemory(const std::wstring& virtualPath);
    size_t getCurrentMemoryUsage() const;
    
private:
    struct MemoryFileEntry {
        std::wstring virtualPath;
        std::chrono::system_clock::time_point lastAccess;
        size_t memorySizeBytes;
    };
    
    std::list<MemoryFileEntry> accessOrder_;  // LRU order
    std::unordered_map<std::wstring, std::list<MemoryFileEntry>::iterator> fileMap_;
};
```

---

## ✅ Technical Decisions Made

### 1. Cache Storage Structure: **In-Memory Map** ✅
```cpp
// In-memory cache structure
std::unordered_map<std::wstring, std::vector<uint8_t>> memoryCache;
// Key: virtual path (e.g., "/msvc-14.40/bin/Hostx64/x64/cl.exe")
// Value: file content in memory
```

**Rationale**: Fast RAM-based access, no disk I/O for cached files, simple key-value structure.

### 2. File Validation Strategy: **Read-Only Assumption** ✅
```cpp
// No validation needed - files are immutable
// If files change, restart server to clear memory cache
extern std::unordered_map<std::wstring, std::vector<uint8_t>> memoryCache;

bool isFileInMemoryCache(const std::wstring& virtualPath) {
    return memoryCache.find(virtualPath) != memoryCache.end();
}
```

**Rationale**: Compiler files don't change during operation. Simplifies implementation significantly.

### 3. Cache Policy Implementation: **Full Pattern Matching + Shared Templates** ✅

Enhanced YAML configuration with reusable templates:
```yaml
# Shared templates for common compiler types
cache_templates:
  msvc_compiler:
    cache_always:
      - "bin/**/*.exe"
      - "bin/**/*.dll" 
      - "include/**/*.h"
      - "lib/**/*.lib"
    cache_on_demand:
      - "**/*.pdb"
    never_cache:
      - "temp/**/*"
      - "**/*.tmp"
      
  windows_sdk:
    cache_always:
      - "Include/**/*.h"
      - "Lib/**/*.lib"
      - "bin/**/*.exe"
    cache_on_demand:
      - "**/*.winmd"

compilers:
  msvc-14.40:
    extends: "msvc_compiler"  # Inherit from template
    network_path: "\\\\127.0.0.1\\efs\\compilers\\msvc\\14.40.33807-14.40.33811.0"
    cache_size_mb: 2048
    # Can override specific patterns
    cache_always:
      - "bin/Hostx64/x64/*.exe"  # More specific override
      
  windows-kits-10:
    extends: "windows_sdk"    # Inherit from template
    network_path: "\\\\127.0.0.1\\efs\\compilers\\windows-kits-10"
    cache_size_mb: 1024
```

**Rationale**: Reduces duplication, enables consistent policies across similar compilers, allows customization when needed.

### 4. Error Handling Strategy: **Fallback to Network Always** ✅
```cpp
std::vector<uint8_t> getFileContent(const std::wstring& virtualPath, const Config& config) {
    // Try memory cache first
    if (isFileInMemoryCache(virtualPath)) {
        return getMemoryCachedFile(virtualPath);
    }
    
    // Always fallback to network
    std::wstring networkPath = resolveNetworkPath(virtualPath, config);
    return loadNetworkFileToMemory(networkPath);
}
```

**Rationale**: Maximum reliability, cache is pure optimization, never blocks functionality.

### 5. Testing Approach: **Real Files + Size Limits Later** ✅
- Test with actual MSVC installation files
- Implement basic caching first, add size limits as separate feature
- Focus on correctness before optimization

---

## Implementation Timeline

### Week 1: Basic Memory Operations
- [ ] Implement `testCacheOperations()` function
- [ ] Add `--test-cache` command line option
- [ ] Implement basic memory loading operations
- [ ] Add memory cache storage (std::unordered_map)
- [ ] Create comprehensive test cases

### Week 2: Integration
- [ ] Integrate with existing path resolution
- [ ] Implement memory cache hit/miss logic
- [ ] Add memory cache management
- [ ] Test with real network files

### Week 3: Memory-based LRU Eviction
- [ ] Implement memory usage tracking
- [ ] Add memory size monitoring
- [ ] Implement memory-based eviction algorithm
- [ ] Add `--test-eviction` command line option

### Week 4: Polish & Optimization
- [ ] Error handling improvements
- [ ] Memory usage optimization
- [ ] Documentation updates
- [ ] Integration with WinFsp filesystem

## Updated Configuration Format

Based on decisions, here's the enhanced `compilers.yaml` with template support:

```yaml
# Shared templates for common compiler types
cache_templates:
  msvc_compiler:
    cache_always:
      - "bin/**/*.exe"
      - "bin/**/*.dll" 
      - "include/**/*.h"
      - "lib/**/*.lib"
    cache_on_demand:
      - "**/*.pdb"
      - "**/*.ilk"
    never_cache:
      - "temp/**/*"
      - "**/*.tmp"
      - "**/*.log"
      
  windows_sdk:
    cache_always:
      - "Include/**/*.h"
      - "Lib/**/*.lib"
      - "bin/**/*.exe"
    cache_on_demand:
      - "**/*.winmd"
      - "**/*.tlb"
    never_cache:
      - "**/*.tmp"

  build_tool:
    cache_always:
      - "*.exe"
      - "*.dll"
    cache_on_demand: []
    never_cache:
      - "**/*.tmp"

compilers:
  msvc-14.40:
    extends: "msvc_compiler"  # Inherit from template
    network_path: "\\\\127.0.0.1\\efs\\compilers\\msvc\\14.40.33807-14.40.33811.0"
    cache_size_mb: 2048
    # Override with more specific patterns
    cache_always:
      - "bin/Hostx64/x64/*.exe"  # More specific than template
      - "bin/Hostx64/x64/*.dll"
      
  windows-kits-10:
    extends: "windows_sdk"    # Inherit from template
    network_path: "\\\\127.0.0.1\\efs\\compilers\\windows-kits-10"
    cache_size_mb: 1024
    # Template patterns are sufficient, no override needed
      
  ninja:
    extends: "build_tool"     # Simple build tool template
    network_path: "\\\\127.0.0.1\\efs\\compilers\\ninja"
    cache_size_mb: 64

global:
  max_memory_usage_mb: 8192
  eviction_policy: "lru"
```

This eliminates duplication while allowing customization per compiler.

---

## Testing Commands

```cmd
# Test completed functionality
CeWinFileCacheFS.exe --test-config    # ✅ Working
CeWinFileCacheFS.exe --test-paths     # ✅ Working  
CeWinFileCacheFS.exe --test-network   # ✅ Working
CeWinFileCacheFS.exe --test           # ✅ All tests

# Cache testing (when implemented)
CeWinFileCacheFS.exe --test-cache     # ⏳ TODO #1 - Basic cache operations
CeWinFileCacheFS.exe --test-estimate  # ⏳ NEW - Cache size estimation
CeWinFileCacheFS.exe --test-eviction  # ⏳ TODO #2 - LRU eviction
```

### Memory Cache Estimation Example Output:
```
=== Memory Cache Size Estimation Test ===
Analyzing compiler: msvc-14.40
  Network path: \\127.0.0.1\efs\compilers\msvc\14.40.33807-14.40.33811.0
  Estimated memory usage: 1,245 MB
  Estimated file count: 8,432
  Map overhead: 67 KB (std::unordered_map overhead)
  Key overhead: 135 KB (std::wstring keys)
  Always cache files: 856 MB (executables, DLLs)
  On-demand files: 389 MB (headers, libs)

Analyzing compiler: windows-kits-10
  Network path: \\127.0.0.1\efs\compilers\windows-kits-10
  Estimated memory usage: 2,108 MB
  Estimated file count: 15,293
  Map overhead: 122 KB
  Key overhead: 244 KB
  Always cache files: 1,924 MB (headers, libs, tools)
  On-demand files: 184 MB (WinMD files)

Analyzing compiler: ninja
  Network path: \\127.0.0.1\efs\compilers\ninja
  Estimated memory usage: 3 MB
  Estimated file count: 2
  Map overhead: 1 KB
  Key overhead: 1 KB
  Always cache files: 3 MB (ninja.exe)
  On-demand files: 0 MB

=== Total Memory Estimation ===
Total estimated memory usage: 3,356 MB
Total file count: 23,727
Total map overhead: 571 KB
Available system RAM: [query system memory] MB

💡 Use this information to decide on memory limits and LRU policies
```

## Success Criteria

The caching implementation will be considered successful when:

- [ ] `CeWinFileCacheFS.exe --test-cache` passes all test cases
- [ ] `CeWinFileCacheFS.exe --test-estimate` provides accurate memory usage predictions
- [ ] Files are correctly loaded from network into memory
- [ ] Cache hits are detected and served from memory
- [ ] Cache misses trigger network fetches
- [ ] Memory usage estimation helps with capacity planning
- [ ] LRU eviction works when memory limits are reached
- [ ] Integration with existing path resolution is seamless
- [ ] Error conditions are handled gracefully

This foundation will then be ready for WinFsp integration and async optimization.