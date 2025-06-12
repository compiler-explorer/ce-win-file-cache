#pragma once

#include "../types/cache_entry.hpp"
#include "../types/config.hpp"
#include "windows_compat.hpp"
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>

namespace CeWinFileCache
{

class CacheManager
{
    public:
    CacheManager(const GlobalConfig &config);
    ~CacheManager();

    NTSTATUS initialize();
    void shutdown();

    // Cache operations
    NTSTATUS cacheFile(const std::wstring &network_path, const std::wstring &local_path);
    NTSTATUS evictFile(const std::wstring &local_path);
    bool isFileCached(const std::wstring &local_path);

    // Statistics
    size_t getCurrentCacheSize() const;
    size_t getCacheHitCount() const;
    size_t getCacheMissCount() const;

    private:
    void backgroundEvictionThread();
    NTSTATUS performLRUEviction(size_t bytes_needed);
    size_t calculateFileSize(const std::wstring &file_path);

    GlobalConfig config;
    mutable std::mutex cache_mutex;
    std::unordered_map<std::wstring, std::unique_ptr<CacheEntry>> cached_files;

    // Statistics
    mutable std::atomic<size_t> current_cache_size;
    mutable std::atomic<size_t> cache_hits;
    mutable std::atomic<size_t> cache_misses;

    // Background thread
    std::thread eviction_thread;
    std::condition_variable eviction_cv;
    std::atomic<bool> shutdown_requested;
};

} // namespace CeWinFileCache