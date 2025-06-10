#pragma once

#include "windows_compat.hpp"
#include "../types/cache_entry.hpp"
#include "../types/config.hpp"
#include <memory>
#include <unordered_map>
#include <mutex>
#include <thread>
#include <condition_variable>

namespace CeWinFileCache
{

class CacheManager
{
public:
    CacheManager(const GlobalConfig& config);
    ~CacheManager();
    
    NTSTATUS initialize();
    void shutdown();
    
    // Cache operations
    NTSTATUS cacheFile(const std::wstring& network_path, const std::wstring& local_path);
    NTSTATUS evictFile(const std::wstring& local_path);
    bool isFileCached(const std::wstring& local_path);
    
    // Statistics
    size_t getCurrentCacheSize() const;
    size_t getCacheHitCount() const;
    size_t getCacheMissCount() const;
    
private:
    void backgroundEvictionThread();
    NTSTATUS performLRUEviction(size_t bytes_needed);
    size_t calculateFileSize(const std::wstring& file_path);
    
    GlobalConfig config_;
    mutable std::mutex cache_mutex_;
    std::unordered_map<std::wstring, std::unique_ptr<CacheEntry>> cached_files_;
    
    // Statistics
    mutable std::atomic<size_t> current_cache_size_;
    mutable std::atomic<size_t> cache_hits_;
    mutable std::atomic<size_t> cache_misses_;
    
    // Background thread
    std::thread eviction_thread_;
    std::condition_variable eviction_cv_;
    std::atomic<bool> shutdown_requested_;
};

} // namespace CeWinFileCache