# Use-After-Free and Double-Free Vulnerability Analysis

This document identifies potential memory safety vulnerabilities in the codebase, specifically use-after-free and double-free issues.

## Executive Summary

**CRITICAL ISSUES FOUND**: 2 confirmed vulnerabilities requiring immediate attention
**HIGH-RISK PATTERNS**: 3 identified patterns with significant risk
**MODERATE-RISK ISSUES**: 3 potential problems requiring review

---

## 🚨 CRITICAL VULNERABILITIES (Fix Immediately)

### 1. Use-After-Free in AsyncDownloadManager Callback

**Location**: `src/hybrid_filesystem.cpp:852-874`  
**Severity**: CRITICAL  
**Impact**: Crashes, data corruption, potential security exploitation

#### Vulnerable Code:
```cpp
download_manager->queueDownload(entry->virtual_path, entry->network_path, entry, entry->policy,
    [this, entry](NTSTATUS download_status, const std::wstring error) // DANGEROUS: raw pointer capture
    {
        if (download_status == STATUS_SUCCESS)
        {
            // entry might be invalid here if cache entry was evicted/moved
            std::wcout << L"Download completed: " << entry->virtual_path << std::endl;
        }
        // ... more access to potentially invalid 'entry'
    });
```

#### Vulnerability Scenario:
1. User requests file A, async download starts with callback capturing raw `entry` pointer
2. Cache eviction occurs due to memory pressure, destroying the CacheEntry  
3. Download completes, callback executes accessing freed memory
4. **Result**: Use-after-free crash or data corruption

#### Fix Strategy:
```cpp
// Option 1: Use weak_ptr pattern
auto weak_entry = std::weak_ptr<CacheEntry>(entry_as_shared_ptr);
download_manager->queueDownload(entry->virtual_path, entry->network_path, entry, entry->policy,
    [this, weak_entry](NTSTATUS download_status, const std::wstring error)
    {
        if (auto entry = weak_entry.lock()) {
            // Safe to access entry
            std::wcout << L"Download completed: " << entry->virtual_path << std::endl;
        }
    });

// Option 2: Capture by value the needed data
std::wstring virtual_path = entry->virtual_path;
download_manager->queueDownload(entry->virtual_path, entry->network_path, entry, entry->policy,
    [this, virtual_path](NTSTATUS download_status, const std::wstring error)
    {
        std::wcout << L"Download completed: " << virtual_path << std::endl;
    });
```

### 2. Double-Delete Risk in FileDescriptor Management

**Location**: `src/hybrid_filesystem.cpp:470`  
**Severity**: CRITICAL  
**Impact**: Heap corruption, crashes

#### Vulnerable Code:
```cpp
// Manual allocation (line 315)
auto *file_desc = new FileDescriptor();

// Manual deletion in Close() (line 470)
VOID HybridFileSystem::Close(PVOID FileNode, PVOID FileDesc)
{
    auto *file_desc = static_cast<FileDescriptor *>(FileDesc);
    delete file_desc; // Manual delete - can cause double-free
}

// FileDescriptor also has destructor with cleanup
~FileDescriptor()
{
    if (handle != INVALID_HANDLE_VALUE)
    {
        CloseHandle(handle);
    }
    if (dir_buffer)
    {
        Fsp::FileSystemBase::DeleteDirectoryBuffer(&dir_buffer);
    }
}
```

#### Vulnerability Scenario:
1. Exception occurs after `new FileDescriptor()` but before successful return
2. Caller cleans up, destructor runs automatically
3. Later, `Close()` is called, attempting second delete
4. **Result**: Double-free heap corruption

#### Fix Strategy:
```cpp
// Use RAII consistently
auto file_desc = std::make_unique<FileDescriptor>();

// In Close(), use release() if ownership must transfer
VOID HybridFileSystem::Close(PVOID FileNode, PVOID FileDesc)
{
    auto *file_desc = static_cast<FileDescriptor *>(FileDesc);
    // Option 1: If this is the final cleanup, just let destructor handle it
    // No manual delete needed
    
    // Option 2: If ownership semantics require manual delete, ensure no double-delete
    if (file_desc && !file_desc->already_deleted) {
        file_desc->already_deleted = true;
        delete file_desc;
    }
}
```

---

## ⚠️ HIGH-RISK PATTERNS

### 3. Iterator Invalidation in Cache Eviction

**Location**: `src/cache_manager.cpp:184-222`  
**Severity**: HIGH  
**Risk**: Use-after-free during exception handling

#### Vulnerable Code:
```cpp
for (const auto &[access_time, path] : candidates)
{
    auto it = cached_files.find(path);
    if (it != cached_files.end())
    {
        bytes_evicted += it->second->file_size;
        // Filesystem operations that might throw exceptions
        removeFromDisk(it->second->local_path);
        cached_files.erase(it); // Iterator becomes invalid
    }
}
```

#### Risk Scenario:
- Exception during filesystem operations
- Iterator `it` becomes invalid
- Subsequent access causes use-after-free

#### Fix Strategy:
```cpp
// Use exception-safe iteration
auto it = cached_files.begin();
while (it != cached_files.end() && bytes_evicted < target_eviction_size)
{
    try {
        bytes_evicted += it->second->file_size;
        removeFromDisk(it->second->local_path);
        it = cached_files.erase(it); // Returns next valid iterator
    } catch (...) {
        ++it; // Skip this entry and continue
    }
}
```

### 4. Race Condition in AsyncDownloadManager Shutdown

**Location**: `src/async_download_manager.cpp:82-100`  
**Severity**: HIGH  
**Risk**: Access to destroyed objects during shutdown

#### Vulnerable Code:
```cpp
void AsyncDownloadManager::shutdown()
{
    {
        std::lock_guard<std::mutex> lock(queue_mutex);
        shutdown_requested = true; // Race window here
    }
    queue_condition.notify_all();
    
    for (auto &thread : worker_threads)
    {
        if (thread.joinable())
        {
            thread.join(); // Worker might still be processing
        }
    }
    worker_threads.clear();
}
```

#### Risk Scenario:
- Worker thread processes task after `shutdown_requested = true`
- Thread accesses queue/active_downloads after they're cleared
- Use-after-free on shared data structures

#### Fix Strategy:
```cpp
void AsyncDownloadManager::shutdown()
{
    {
        std::lock_guard<std::mutex> lock(queue_mutex);
        shutdown_requested = true;
        // Clear queue immediately under lock
        while (!download_queue.empty()) {
            download_queue.pop();
        }
        active_downloads.clear();
    }
    queue_condition.notify_all();
    
    // Join threads before clearing containers
    for (auto &thread : worker_threads)
    {
        if (thread.joinable())
        {
            thread.join();
        }
    }
    worker_threads.clear();
}
```

### 5. Thread Safety in HybridFileSystem Cache Access

**Location**: Multiple locations in `src/hybrid_filesystem.cpp`  
**Severity**: HIGH  
**Risk**: Concurrent modification without synchronization

#### Vulnerable Pattern:
```cpp
// Reading cache_entries without lock
auto cache_entry = getCacheEntry(virtual_path); // No lock

// Meanwhile another thread:
cache_entries.erase(virtual_path); // Modifies map concurrently
```

#### Fix Strategy:
Add proper synchronization to all cache_entries access.

---

## ⚠️ MODERATE-RISK ISSUES

### 6. Use-After-Move in FileAccessTracker

**Location**: `src/file_access_tracker.cpp:113-122`  
**Severity**: MEDIUM  
**Impact**: Undefined behavior

#### Vulnerable Code:
```cpp
auto info = std::make_unique<FileAccessInfo>();
// ... populate info
file_access_map_[virtual_path] = std::move(info);
info->access_count++; // Use after move - undefined behavior
```

#### Fix Strategy:
```cpp
auto& info_ref = file_access_map_[virtual_path];
if (!info_ref) {
    info_ref = std::make_unique<FileAccessInfo>();
    // populate info_ref directly
}
info_ref->access_count++;
```

### 7. Iterator Invalidation in DirectoryNode

**Location**: `include/types/directory_tree.hpp:59-65`  
**Severity**: MEDIUM  
**Risk**: Concurrent modification of children map

#### Fix Strategy:
Add proper locking around DirectoryNode operations in multi-threaded contexts.

### 8. Windows Handle Resource Leaks

**Location**: Multiple locations in `src/hybrid_filesystem.cpp`  
**Severity**: MEDIUM  
**Risk**: Resource leaks on exception paths

#### Vulnerable Pattern:
```cpp
HANDLE handle = CreateFileW(...);
// Operations that might throw
// Handle might leak if exception occurs
CloseHandle(handle);
```

#### Fix Strategy:
```cpp
// Use RAII wrapper
class HandleGuard {
    HANDLE handle_;
public:
    HandleGuard(HANDLE h) : handle_(h) {}
    ~HandleGuard() { if (handle_ != INVALID_HANDLE_VALUE) CloseHandle(handle_); }
    HANDLE get() { return handle_; }
};

HandleGuard handle(CreateFileW(...));
// Operations that might throw - handle auto-closed
```

---

## 🔧 IMMEDIATE ACTION ITEMS

### Priority 1 (Critical - Fix This Week)
1. **Fix AsyncDownloadManager callback capture** - Replace raw pointer with safe capture
2. **Fix FileDescriptor double-delete** - Use consistent RAII pattern
3. **Add thread synchronization** - Protect all shared data structures

### Priority 2 (High - Fix This Sprint)  
1. **Improve cache eviction safety** - Use exception-safe iteration
2. **Fix shutdown race conditions** - Ensure proper cleanup ordering
3. **Add resource guards** - RAII wrappers for Windows handles

### Priority 3 (Medium - Fix Next Sprint)
1. **Fix move semantics issues** - Ensure no use-after-move
2. **Iterator invalidation protection** - Add proper locking
3. **Exception safety audit** - Review all exception paths

---

## 🛡️ PREVENTION STRATEGIES

### Code Review Checklist
- [ ] All async callbacks use safe capture patterns
- [ ] No raw pointer captures in lambdas
- [ ] All shared data protected by synchronization
- [ ] No manual delete/free paired with RAII destructors
- [ ] Exception-safe resource management
- [ ] Iterator invalidation considered in container operations

### Static Analysis Tools
Consider integrating:
- **AddressSanitizer (ASan)** - Detects use-after-free at runtime
- **ThreadSanitizer (TSan)** - Detects race conditions
- **Clang Static Analyzer** - Compile-time vulnerability detection
- **Valgrind** - Memory error detection (Linux)

### Testing Strategies
- **Stress testing** with cache eviction under load
- **Concurrent testing** with multiple threads
- **Exception injection** testing for resource leaks
- **Fuzzing** file system operations

---

## 📊 Risk Assessment Summary

| Issue | Severity | Likelihood | Impact | Priority |
|-------|----------|------------|---------|----------|
| AsyncDownloadManager callback use-after-free | Critical | High | Crash/Corruption | P0 |
| FileDescriptor double-delete | Critical | Medium | Heap corruption | P0 |
| Cache eviction iterator invalidation | High | Medium | Crash | P1 |
| Shutdown race conditions | High | Medium | Crash | P1 |
| Thread safety violations | High | High | Data corruption | P1 |
| Use-after-move bugs | Medium | Low | Undefined behavior | P2 |
| Iterator invalidation | Medium | Low | Crash | P2 |
| Handle leaks | Medium | Medium | Resource exhaustion | P2 |

**Overall Assessment**: The codebase has several critical memory safety issues that require immediate attention, particularly around asynchronous operations and multi-threading. While the code shows good understanding of modern C++, the async patterns introduce significant vulnerability risks.