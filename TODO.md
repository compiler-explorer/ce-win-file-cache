# CompilerCacheFS - TODO List

This document tracks all implementation tasks for the CompilerCacheFS project.

## Summary

- **Total TODOs:** 9
- **Completed:** 2 ✅
- **In Progress:** 0 🔄
- **Remaining:** 7 ⏳
- **Critical Priority:** 2 remaining (2 completed)
- **Medium Priority:** 3 remaining
- **Low Priority:** 2 remaining

---

## Project TODOs

### ✅ Completed - Core Functionality

- [x] **3. Implement Proper Path Resolution** *(Completed: 2024-12-06)*
  - **File:** `src/hybrid_filesystem.cpp` → `src/main.cpp`
  - **Line:** 286 → Implemented in `testPathResolution()`
  - **Description:** ✅ Implemented logic to resolve virtual paths like `/msvc-14.40/bin/cl.exe` to network UNC paths
  - **Impact:** Critical - required for multi-compiler support
  - **Implementation:** Isolated test function with comprehensive test cases
  - **Test Command:** `CeWinFileCacheFS.exe --test-paths`

- [x] **4. Implement Network Path Mapping** *(Completed: 2024-12-06)*
  - **File:** `src/hybrid_filesystem.cpp` → `src/main.cpp`
  - **Line:** 307 → Implemented in `testNetworkMapping()`
  - **Description:** ✅ Implemented path mapping logic with validation of expected vs actual network paths
  - **Impact:** Critical - required for network file access
  - **Implementation:** Isolated test function with validation test cases
  - **Test Command:** `CeWinFileCacheFS.exe --test-network`

### ⏳ High Priority - Core Functionality (Remaining)

- [ ] **1. Implement Actual Caching Logic**
  - **File:** `src/hybrid_filesystem.cpp`
  - **Line:** 346
  - **Comment:** `// todo: implement actual caching logic`
  - **Description:** Implement the core functionality to copy files from network locations to local cache storage
  - **Impact:** Critical - this is the main feature of the filesystem
  - **Estimated Effort:** Large
  - **Dependencies:** Path resolution ✅ and network mapping ✅ are complete

- [ ] **2. Implement LRU Eviction**
  - **File:** `src/hybrid_filesystem.cpp`
  - **Line:** 372
  - **Comment:** `// todo: implement LRU eviction`
  - **Description:** Implement Least Recently Used (LRU) cache eviction algorithm for managing cache size limits
  - **Impact:** Critical - required for proper cache management
  - **Estimated Effort:** Medium
  - **Dependencies:** Can be implemented alongside caching logic

### ⏳ Medium Priority - Performance & Features

- [ ] **5. Implement Async File Handling**
  - **File:** `src/hybrid_filesystem.cpp`
  - **Line:** 325
  - **Comment:** `// todo: implement proper async handling`
  - **Description:** Replace synchronous file fetching with asynchronous operations to prevent blocking
  - **Impact:** Medium - improves performance and responsiveness
  - **Estimated Effort:** Large
  - **Dependencies:** Should be implemented after core caching logic is working

- [ ] **6. Implement Cache Policy Configuration**
  - **File:** `src/hybrid_filesystem.cpp`
  - **Line:** 366
  - **Comment:** `// todo: implement based on config patterns`
  - **Description:** Implement cache policy determination based on configuration file patterns and rules
  - **Impact:** Medium - enables flexible caching strategies
  - **Estimated Effort:** Small
  - **Dependencies:** None - can be implemented independently

- [ ] **7. Implement Proper Glob Matching**
  - **File:** `src/hybrid_filesystem.cpp`
  - **Line:** 360
  - **Comment:** `// Simple pattern matching - todo: implement proper glob matching`
  - **Description:** Replace simple string matching with proper glob pattern matching for file patterns
  - **Impact:** Medium - enables more flexible file filtering
  - **Estimated Effort:** Small
  - **Dependencies:** Useful for cache policy configuration

### ⏳ Low Priority - Code Quality

- [ ] **8. Fix Const-Correctness for Wide String Conversion**
  - **File:** `src/main.cpp`
  - **Line:** 125 *(Note: Line number may have changed due to recent additions)*
  - **Comment:** `// todo: find a nice way to convert from const wchar_t* to wchar_t*`
  - **Description:** Implement proper const-correctness for converting wide character strings when setting volume prefix
  - **Impact:** Low - code quality improvement
  - **Estimated Effort:** Small
  - **Dependencies:** None - pure code quality improvement

### 🚫 External Code TODOs (Not Our Responsibility)

- [ ] **9. WinFsp Storage Completion Flag**
  - **File:** `external/winfsp/tst/airfs/persistence.cpp`
  - **Line:** 577
  - **Comment:** `//  TODO: Set and write a flag something like Airfs->UpdatesCompleted here?`
  - **Description:** Implement completion flag mechanism for tracking storage updates (external WinFsp code)
  - **Impact:** Low - external library enhancement
  - **Estimated Effort:** N/A (external code - not our responsibility)

---

## Implementation Priority Recommendations

### ✅ Phase 1 - Core Functionality (COMPLETED)
1. ~~**Path Resolution & Network Mapping**~~ - ✅ TODOs #3 and #4 **COMPLETED**
   - ✅ Essential for basic filesystem operations
   - ✅ Implemented together as isolated, testable functions
   - ✅ Comprehensive test coverage with validation
   - ✅ Ready for integration into main filesystem logic

### 🔄 Phase 2 - Core Implementation (CURRENT FOCUS)
2. **Actual Caching Logic** - TODO #1 ⏳
   - Core feature of the system
   - ✅ Dependencies met: Path resolution and network mapping are complete
   - **Next Step:** Implement isolated caching test functions

3. **LRU Eviction** - TODO #2 ⏳
   - Required for production use to prevent unlimited cache growth
   - Can be implemented alongside caching logic
   - **Recommendation:** Start after basic caching read/write operations work

### 📋 Phase 3 - Performance Improvements (FUTURE)
4. **Cache Policy Configuration** - TODO #6 ⏳
   - Enables flexible caching strategies
   - Relatively simple to implement
   - **Recommendation:** Implement before async handling

5. **Async File Handling** - TODO #5 ⏳
   - Significant performance improvement
   - Should be implemented after core caching is working

6. **Glob Pattern Matching** - TODO #7 ⏳
   - Nice-to-have feature for file filtering
   - Supports more sophisticated cache policies

### 🔧 Phase 4 - Polish & Quality (LOW PRIORITY)
7. **Const-Correctness Fix** - TODO #8 ⏳
   - Code quality improvement
   - No functional impact

---

## Current Status & Next Steps

### ✅ What's Working
- **Configuration System**: YAML parsing with proper regex handling
- **Path Resolution**: Virtual → Network path conversion with testing
- **Network Mapping**: Complete path validation with test cases  
- **Build System**: Native Windows (MSVC) and WSL cross-compilation
- **Test Framework**: Isolated functions with `--test-*` command line options

### 🎯 Immediate Next Step
**Implement TODO #1 (Actual Caching Logic)** using the same isolated, testable approach:
1. Create `testCacheOperations()` function in `main.cpp`
2. Add `--test-cache` command line option
3. Implement basic file copy from network path to local cache
4. Test with mock/real network files
5. Integrate with existing path resolution functions

### 📝 Development Notes
- All project TODOs are focused on the hybrid filesystem implementation
- Configuration parsing, network client, and cache manager modules are working well
- External WinFsp TODO can be ignored as it's not our code to maintain
- Two critical TODOs have been completed with full test coverage
- Ready to move from path mapping to actual file operations

---

## Testing Commands

```cmd
# Test completed functionality
CeWinFileCacheFS.exe --test-config    # ✅ Working
CeWinFileCacheFS.exe --test-paths     # ✅ Working  
CeWinFileCacheFS.exe --test-network   # ✅ Working
CeWinFileCacheFS.exe --test           # ✅ All tests

# Future testing (when implemented)
CeWinFileCacheFS.exe --test-cache     # ⏳ TODO #1
CeWinFileCacheFS.exe --test-eviction  # ⏳ TODO #2
```