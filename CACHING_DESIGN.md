# Caching Logic Design and Implementation Strategy

This document outlines the design approach for implementing TODO #1: Actual Caching Logic in CompilerCacheFS.

## Current Foundation

### ✅ What We Have
- **Path Resolution**: Virtual paths → Network UNC paths (tested)
- **Network Mapping**: Complete path validation (tested)
- **Config System**: YAML parsing with compiler configurations
- **Test Framework**: Isolated functions with command-line testing
- **Build System**: Working MSVC compilation

### 🎯 What We Need
Core file caching operations that copy files from network locations to local cache storage with proper management.

---

## Design Options

### Option A: Simple File Copy Approach (Recommended for MVP)

**Philosophy**: Start with basic synchronous file operations, get core functionality working, then optimize.

**Implementation Strategy**:
```cpp
// Core operations to implement
bool copyFileToCache(const std::wstring& networkPath, const std::wstring& localPath);
bool isFileInCache(const std::wstring& virtualPath);
std::wstring getCacheFilePath(const std::wstring& virtualPath);
bool ensureCacheDirectory(const std::wstring& cachePath);
```

**Advantages**:
- Simple to implement and test
- Predictable behavior
- Easy to debug
- Fast development iteration

**Disadvantages**:
- Blocking I/O operations
- No progress indication for large files
- Potential timeouts on slow networks

### Option B: Async with Progress Tracking

**Philosophy**: Implement non-blocking operations from the start for better user experience.

**Implementation Strategy**:
```cpp
// Async operations with callbacks
class CacheOperation {
public:
    enum Status { Pending, InProgress, Completed, Failed };
    
    std::future<bool> copyFileAsync(const std::wstring& networkPath, 
                                   const std::wstring& localPath,
                                   std::function<void(float)> progressCallback);
};
```

**Advantages**:
- Non-blocking operations
- Progress tracking
- Better scalability
- Modern C++ patterns

**Disadvantages**:
- More complex to implement
- Harder to test initially
- Thread safety considerations

### Option C: Hybrid Approach (Recommended)

**Philosophy**: Start with simple synchronous operations for testing, then add async layer.

**Implementation Strategy**:
1. **Phase 1**: Implement synchronous core operations with comprehensive testing
2. **Phase 2**: Add async wrapper around working synchronous operations
3. **Phase 3**: Optimize and add advanced features

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

// Basic file operations with network fallback
bool copyNetworkFileToCache(const std::wstring& networkPath, 
                           const std::wstring& localCachePath);

bool isFileInCache(const std::wstring& virtualPath, 
                  const Config& config);

std::wstring getCacheFilePath(const std::wstring& virtualPath, 
                             const Config& config);

bool ensureCacheDirectoryExists(const std::wstring& directoryPath);

// Network fallback operations
std::wstring getFileContent(const std::wstring& virtualPath, 
                           const Config& config);

// Cache policy with template support
enum CachePolicy { ALWAYS_CACHE, ON_DEMAND, NEVER_CACHE };

CachePolicy determineCachePolicy(const std::wstring& virtualPath,
                                const std::wstring& compilerName,
                                const Config& config);

// Cache stats (simple - no size limits yet)
struct CacheStats {
    size_t totalFiles;
    size_t totalSizeBytes;
    std::vector<std::wstring> cachedCompilers;
};

CacheStats getCacheStatistics(const Config& config);

// Cache size estimation
struct CacheEstimation {
    size_t totalBytes;
    size_t fileCount;
    size_t overheadBytes;        // Filesystem overhead, metadata, etc.
    size_t alwaysCacheBytes;     // Files that must be cached
    size_t onDemandBytes;        // Files cached on first access
    size_t neverCacheBytes;      // Files that won't be cached
};

CacheEstimation estimateCompilerCacheSize(const std::wstring& networkPath, 
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
        // 1. Cache directory creation (hierarchical structure)
        // 2. File copy from real network location to cache
        // 3. Cache hit detection (simple existence check)
        // 4. Cache path generation (hierarchical mapping)
        // 5. Network fallback (always works)
        // 6. Cache policy determination (template-based)
        // 7. Cache statistics (file count, total size)
        
        // No validation needed - files are read-only
        // Network fallback ensures reliability
    }
    
    return 0;
}

int testCacheEstimation(const Config& config)
{
    std::wcout << L"=== Cache Size Estimation Test ===" << std::endl;
    
    // Estimate memory usage for configured compilers
    size_t totalEstimatedSize = 0;
    size_t totalFileCount = 0;
    
    for (const auto& [compilerName, compilerConfig] : config.compilers) {
        std::wcout << L"Analyzing compiler: " << compilerName << std::endl;
        
        // Scan network path to estimate sizes
        std::wstring networkPath = compilerConfig.network_path;
        
        // Estimate based on cache policy patterns
        auto estimation = estimateCompilerCacheSize(networkPath, compilerConfig);
        
        std::wcout << L"  Network path: " << networkPath << std::endl;
        std::wcout << L"  Estimated cache size: " << (estimation.totalBytes / 1024 / 1024) << L" MB" << std::endl;
        std::wcout << L"  Estimated file count: " << estimation.fileCount << std::endl;
        std::wcout << L"  Cache overhead: " << (estimation.overheadBytes / 1024) << L" KB" << std::endl;
        std::wcout << L"  Always cache files: " << estimation.alwaysCacheBytes / 1024 / 1024 << L" MB" << std::endl;
        std::wcout << L"  On-demand files: " << estimation.onDemandBytes / 1024 / 1024 << L" MB" << std::endl;
        
        totalEstimatedSize += estimation.totalBytes;
        totalFileCount += estimation.fileCount;
    }
    
    std::wcout << L"\n=== Total Estimation ===" << std::endl;
    std::wcout << L"Total estimated cache size: " << (totalEstimatedSize / 1024 / 1024) << L" MB" << std::endl;
    std::wcout << L"Total file count: " << totalFileCount << std::endl;
    std::wcout << L"Configured cache limit: " << config.global.total_cache_size_mb << L" MB" << std::endl;
    
    if (totalEstimatedSize / 1024 / 1024 > config.global.total_cache_size_mb) {
        std::wcout << L"⚠️  WARNING: Estimated size exceeds configured limit!" << std::endl;
        std::wcout << L"   Consider increasing total_cache_size_mb or adjusting cache policies" << std::endl;
    } else {
        std::wcout << L"✅ Estimated size fits within configured limits" << std::endl;
    }
    
    return 0;
}
```

### Phase 2: Cache Integration with Path Resolution

Combine our working path resolution with new caching operations:

```cpp
// Integration function
std::wstring getCachedOrNetworkFile(const std::wstring& virtualPath, 
                                   const Config& config)
{
    // 1. Use existing path resolution to get network path
    // 2. Check if file is in cache
    // 3. If not in cache, copy from network
    // 4. Return cache path
}
```

### Phase 3: LRU Eviction (TODO #2)

Implement alongside basic caching:

```cpp
class LRUCacheManager {
public:
    void recordFileAccess(const std::wstring& filePath);
    std::vector<std::wstring> getFilesToEvict(size_t bytesNeeded);
    bool evictFile(const std::wstring& filePath);
    
private:
    struct FileEntry {
        std::wstring path;
        std::chrono::system_clock::time_point lastAccess;
        size_t sizeBytes;
    };
    
    std::list<FileEntry> accessOrder_;  // LRU order
    std::unordered_map<std::wstring, std::list<FileEntry>::iterator> fileMap_;
};
```

---

## ✅ Technical Decisions Made

### 1. Cache Directory Structure: **Hierarchical** ✅
```
D:\CompilerCache\
├── msvc-14.40\
│   ├── bin\Hostx64\x64\cl.exe
│   └── include\iostream
└── windows-kits-10\
    └── Include\windows.h
```

**Rationale**: Mirrors original structure, easier to navigate, supports compiler-specific organization.

### 2. File Validation Strategy: **Read-Only Assumption** ✅
```cpp
// No validation needed - files are immutable
// If files change, restart server to invalidate cache
bool isFileInCache(const std::wstring& virtualPath) {
    return std::filesystem::exists(getCacheFilePath(virtualPath));
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
std::wstring getFileContent(const std::wstring& virtualPath, const Config& config) {
    std::wstring cachedPath = getCacheFilePath(virtualPath, config);
    
    // Try cache first
    if (std::filesystem::exists(cachedPath)) {
        return readFromCache(cachedPath);
    }
    
    // Always fallback to network
    std::wstring networkPath = resolveNetworkPath(virtualPath, config);
    return readFromNetwork(networkPath);
}
```

**Rationale**: Maximum reliability, cache is pure optimization, never blocks functionality.

### 5. Testing Approach: **Real Files + Size Limits Later** ✅
- Test with actual MSVC installation files
- Implement basic caching first, add size limits as separate feature
- Focus on correctness before optimization

---

## Implementation Timeline

### Week 1: Basic Operations
- [ ] Implement `testCacheOperations()` function
- [ ] Add `--test-cache` command line option
- [ ] Implement basic file copy operations
- [ ] Add cache directory creation
- [ ] Create comprehensive test cases

### Week 2: Integration
- [ ] Integrate with existing path resolution
- [ ] Implement cache hit/miss logic
- [ ] Add file validation
- [ ] Test with real network files

### Week 3: LRU Eviction
- [ ] Implement LRU tracking
- [ ] Add cache size monitoring
- [ ] Implement eviction algorithm
- [ ] Add `--test-eviction` command line option

### Week 4: Polish & Optimization
- [ ] Error handling improvements
- [ ] Performance testing
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
  total_cache_size_mb: 8192
  eviction_policy: "lru"
  cache_directory: "D:\\CompilerCache"
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

### Cache Estimation Example Output:
```
=== Cache Size Estimation Test ===
Analyzing compiler: msvc-14.40
  Network path: \\127.0.0.1\efs\compilers\msvc\14.40.33807-14.40.33811.0
  Estimated cache size: 1,245 MB
  Estimated file count: 8,432
  Cache overhead: 42 KB (directory entries, metadata)
  Always cache files: 856 MB (executables, DLLs)
  On-demand files: 389 MB (headers, libs)

Analyzing compiler: windows-kits-10
  Network path: \\127.0.0.1\efs\compilers\windows-kits-10
  Estimated cache size: 2,108 MB
  Estimated file count: 15,293
  Cache overhead: 78 KB
  Always cache files: 1,924 MB (headers, libs, tools)
  On-demand files: 184 MB (WinMD files)

Analyzing compiler: ninja
  Network path: \\127.0.0.1\efs\compilers\ninja
  Estimated cache size: 3 MB
  Estimated file count: 2
  Cache overhead: 1 KB
  Always cache files: 3 MB (ninja.exe)
  On-demand files: 0 MB

=== Total Estimation ===
Total estimated cache size: 3,356 MB
Total file count: 23,727
Configured cache limit: 8192 MB
✅ Estimated size fits within configured limits

Cache overhead breakdown:
  - Directory structure: ~121 KB
  - File metadata: ~24 KB  
  - LRU tracking: ~48 KB
  - Total overhead: ~193 KB
```

## Success Criteria

The caching implementation will be considered successful when:

- [ ] `CeWinFileCacheFS.exe --test-cache` passes all test cases
- [ ] `CeWinFileCacheFS.exe --test-estimate` provides accurate size predictions
- [ ] Files are correctly copied from network to cache
- [ ] Cache hits are detected and served from local storage
- [ ] Cache misses trigger network fetches
- [ ] Cache size estimation helps with capacity planning
- [ ] LRU eviction works when cache size limits are reached
- [ ] Integration with existing path resolution is seamless
- [ ] Error conditions are handled gracefully

This foundation will then be ready for WinFsp integration and async optimization.