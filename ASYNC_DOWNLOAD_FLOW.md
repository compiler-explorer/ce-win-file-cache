# AsyncDownloadManager Flow Visualization

## Overview

The AsyncDownloadManager implements a thread pool pattern for asynchronous file downloads from network paths to memory cache. This document visualizes the data flow, synchronization mechanisms, and interactions between components.

## Data Structures & Synchronization

```
┌─────────────────────────────────────────────────────────────────┐
│                     AsyncDownloadManager                        │
├─────────────────────────────────────────────────────────────────┤
│ Data Structures:                                                │
│ • std::vector<std::thread> worker_threads                       │
│ • std::queue<DownloadTask> download_queue                       │
│ • std::unordered_map<path, DownloadTask> active_downloads       │
│                                                                 │
│ Synchronization:                                                │
│ • std::mutex queue_mutex (protects queue + active_downloads)    │
│ • std::condition_variable queue_condition (worker signaling)    │
│ • std::atomic<bool> shutdown_requested                          │
│ • std::atomic<size_t> pending_count, active_count              │
└─────────────────────────────────────────────────────────────────┘
```

## Main Flow Diagram

```
┌─────────────────┐    ┌──────────────────┐    ┌─────────────────┐
│  HybridFileSystem │    │  User/Filesystem │    │   Worker Thread │
│                 │    │    Request       │    │     Pool        │
└─────────┬───────┘    └─────────┬────────┘    └─────────┬───────┘
          │                      │                       │
          │ ensureFileAvailable()│                       │
          ▼                      │                       │
┌─────────────────────────────────┼───────────────────────┼─────────┐
│              queueDownload()    │                       │         │
│                                 │                       │         │
│ 1. 🔒 LOCK queue_mutex         │                       │         │
│                                 │                       │         │
│ 2. Check shutdown_requested     │                       │         │
│    ├─ TRUE: return UNSUCCESSFUL │                       │         │
│    └─ FALSE: continue           │                       │         │
│                                 │                       │         │
│ 3. Check active_downloads[path] │                       │         │
│    ├─ EXISTS: return PENDING    │                       │         │
│    └─ NOT EXISTS: continue      │                       │         │
│                                 │                       │         │
│ 4. Create DownloadTask          │                       │         │
│    ├─ virtual_path              │                       │         │
│    ├─ network_path              │                       │         │
│    ├─ cache_entry               │                       │         │
│    ├─ policy                    │                       │         │
│    └─ callback function         │                       │         │
│                                 │                       │         │
│ 5. download_queue.push(task)    │                       │         │
│                                 │                       │         │
│ 6. active_downloads[path] = task│                       │         │
│                                 │                       │         │
│ 7. pending_count++ (atomic)     │                       │         │
│                                 │                       │         │
│ 8. 🔓 UNLOCK queue_mutex        │                       │         │
│                                 │                       │         │
│ 9. queue_condition.notify_one() │────────────────────── │→ WAKE   │
│                                 │                       │  WORKER │
│10. return STATUS_PENDING        │                       │         │
└─────────────────────────────────┼───────────────────────┼─────────┘
          │                      │                       │
          │ STATUS_PENDING       │                       │
          ▼                      │                       │
┌─────────────────┐              │                       │
│ WinFsp will     │              │                       │
│ retry later     │              │                       │
└─────────────────┘              │                       │
```

## Worker Thread Flow

```
                    ┌──────────────────────────────────┐
                    │       workerThread()             │
                    │   (runs in each worker thread)   │
                    └──────────────┬───────────────────┘
                                   │
                                   ▼
                    ┌──────────────────────────────────┐
                    │ 1. 🔒 LOCK queue_mutex           │
                    └──────────────┬───────────────────┘
                                   │
                                   ▼
                    ┌──────────────────────────────────┐
                    │ 2. queue_condition.wait()        │
                    │    Wait until:                   │
                    │    • !download_queue.empty() OR  │
                    │    • shutdown_requested          │
                    └──────────────┬───────────────────┘
                                   │
                                   ▼
                    ┌──────────────────────────────────┐
                    │ 3. Check shutdown_requested       │
                    │    ├─ TRUE: break loop           │
                    │    └─ FALSE: continue            │
                    └──────────────┬───────────────────┘
                                   │
                                   ▼
                    ┌──────────────────────────────────┐
                    │ 4. Get task from queue            │
                    │    • task = queue.front()        │
                    │    • queue.pop()                 │
                    │    • pending_count--             │
                    │    • active_count++              │
                    └──────────────┬───────────────────┘
                                   │
                                   ▼
                    ┌──────────────────────────────────┐
                    │ 5. 🔓 UNLOCK queue_mutex          │
                    └──────────────┬───────────────────┘
                                   │
                                   ▼
                    ┌──────────────────────────────────┐
                    │ 6. processDownload(task)         │
                    │    Process task outside lock      │
                    └──────────────┬───────────────────┘
                                   │
                                   ▼
                    ┌──────────────────────────────────┐
                    │ 7. active_count--                │
                    └──────────────┬───────────────────┘
                                   │
                                   ▼
                    ┌──────────────────────────────────┐
                    │ 8. 🔒 LOCK queue_mutex           │
                    │    active_downloads.erase(path)   │
                    │    🔓 UNLOCK queue_mutex         │
                    └──────────────┬───────────────────┘
                                   │
                                   ▼
                    ┌──────────────────────────────────┐
                    │ 9. Loop back to step 1           │
                    └──────────────────────────────────┘
```

## Download Processing Flow

```
┌─────────────────────────────────────────────────────────┐
│                  processDownload(task)                  │
├─────────────────────────────────────────────────────────┤
│                                                         │
│ 1. Check cache policy:                                 │
│    ├─ ALWAYS_CACHE: Download file                      │
│    ├─ ON_DEMAND: Download file                         │
│    └─ NEVER_CACHE: Mark as network-only                │
│                                                         │
│ 2. downloadFile(network_path, virtual_path):           │
│    ├─ Open file from network path                      │
│    ├─ Read entire file content                         │
│    └─ Store in memory_cache                            │
│                                                         │
│ 3. On success, update cache_entry:                     │
│    ├─ file_size = content.size()                       │
│    ├─ state = FileState::CACHED                        │
│    ├─ last_used = now()                                │
│    ├─ access_count++                                   │
│    └─ local_path.clear()                               │
│                                                         │
│ 4. Call callback function:                             │
│    ├─ Success: callback(STATUS_SUCCESS, L"")           │
│    └─ Failure: callback(STATUS_UNSUCCESSFUL, error)    │
│                                                         │
└─────────────────────────────────────────────────────────┘
```

## Synchronization Details

### Mutex Protection Zones

```
┌─────────────────────────────────────────────────────┐
│ queue_mutex protects:                               │
│ • download_queue (push/pop operations)              │
│ • active_downloads (insert/erase/lookup)            │
│ • shutdown_requested (write access)                 │
└─────────────────────────────────────────────────────┘
```

### Atomic Variables (Lock-free)

```
┌─────────────────────────────────────────────────────┐
│ • pending_count: tracks queued tasks                │
│ • active_count: tracks processing tasks             │
│ • shutdown_requested: read-only in workers          │
└─────────────────────────────────────────────────────┘
```

### Condition Variable Usage

```
┌─────────────────────────────────────────────────────┐
│ queue_condition.wait() blocks until:                │
│ • !download_queue.empty() OR                        │
│ • shutdown_requested == true                        │
│                                                     │
│ queue_condition.notify_one() wakes one worker      │
│ queue_condition.notify_all() wakes all (shutdown)  │
└─────────────────────────────────────────────────────┘
```

## Race Condition Prevention

### 1. Duplicate Download Prevention

```cpp
// In queueDownload()
std::lock_guard<std::mutex> lock(queue_mutex);
if (active_downloads.find(virtual_path) != active_downloads.end()) {
    // File already being downloaded
    return STATUS_PENDING;
}
active_downloads[virtual_path] = task;
```

### 2. Queue Thread Safety

All queue operations happen inside mutex lock:
- Push (in queueDownload)
- Pop (in workerThread)
- Check empty (in condition wait)

### 3. Clean Shutdown

```cpp
// Shutdown sequence:
1. Set shutdown_requested = true (with lock)
2. queue_condition.notify_all() - wake all workers
3. Join all threads
4. Clear thread vector
```

### 4. Work Stealing Prevention

Each task is removed from queue atomically, preventing multiple workers from processing same task.

## Memory Management

- **DownloadTask**: Uses `shared_ptr` for safe cross-thread ownership
- **File Content**: Stored directly in MemoryCacheManager (thread-safe)
- **Cache Entries**: Updated atomically after successful download
- **RAII**: Destructor ensures all threads are joined

## Performance Characteristics

### Concurrency Level
- Configurable worker threads (default: 4, configurable via YAML)
- Each thread can download one file at a time
- Queue allows unlimited pending tasks

### Lock Contention Points
- **queueDownload()**: Brief lock to add task
- **workerThread()**: Brief lock to get task
- **Task completion**: Brief lock to remove from active_downloads
- **Processing**: Happens outside locks for maximum concurrency

### Scalability
- Linear scaling with thread count for I/O-bound downloads
- Memory cache access is thread-safe (separate locks)
- Atomic counters for lock-free status reporting

## Usage Example

```cpp
// From HybridFileSystem::ensureFileAvailable()
if (entry->state != FileState::CACHED) {
    NTSTATUS status = download_manager->queueDownload(
        entry->virtual_path,
        entry->network_path,
        entry,
        entry->policy,
        [this, entry](NTSTATUS download_status, const std::wstring& error) {
            if (download_status == STATUS_SUCCESS) {
                // File now in memory cache
                std::wcout << L"Download completed: " << entry->virtual_path << std::endl;
            } else {
                // Download failed
                entry->state = FileState::NETWORK_ONLY;
                std::wcerr << L"Download failed: " << error << std::endl;
            }
        }
    );
    
    return status; // Returns STATUS_PENDING
}
```

## Configuration

Set download thread count in `compilers.yaml`:

```yaml
global:
  total_cache_size_mb: 8192
  eviction_policy: "lru"
  cache_directory: "D:\\CompilerCache"
  download_threads: 6  # Number of concurrent download threads
```