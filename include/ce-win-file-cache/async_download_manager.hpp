#pragma once

#include "cache_manager.hpp"
#include "memory_cache_manager.hpp"
#include "config_parser.hpp"
#include "windows_compat.hpp"
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <memory>
#include <functional>
#include <unordered_map>

namespace CeWinFileCache
{

struct DownloadTask
{
    std::wstring virtual_path;
    std::wstring network_path;
    CacheEntry* cache_entry;
    CachePolicy policy;
    std::function<void(NTSTATUS, const std::wstring&)> callback;
    
    DownloadTask(const std::wstring& vpath, const std::wstring& npath, 
                 CacheEntry* entry, CachePolicy pol, 
                 std::function<void(NTSTATUS, const std::wstring&)> cb)
        : virtual_path(vpath), network_path(npath), cache_entry(entry), 
          policy(pol), callback(cb) {}
};

class AsyncDownloadManager
{
public:
    AsyncDownloadManager(MemoryCacheManager& memory_cache, const Config& config, 
                        size_t thread_count = 4);
    ~AsyncDownloadManager();
    
    NTSTATUS queueDownload(const std::wstring& virtual_path, 
                          const std::wstring& network_path,
                          CacheEntry* cache_entry,
                          CachePolicy policy,
                          std::function<void(NTSTATUS, const std::wstring&)> callback);
    
    bool isDownloadInProgress(const std::wstring& virtual_path);
    
    void cancelDownload(const std::wstring& virtual_path);
    
    void shutdown();
    
    size_t getPendingCount() const;
    size_t getActiveCount() const;

private:
    void workerThread();
    void processDownload(std::shared_ptr<DownloadTask> task);
    bool downloadFile(const std::wstring& network_path, const std::wstring& virtual_path);
    
    MemoryCacheManager& memory_cache_;
    const Config& config_;
    
    std::vector<std::thread> worker_threads_;
    std::queue<std::shared_ptr<DownloadTask>> download_queue_;
    std::unordered_map<std::wstring, std::shared_ptr<DownloadTask>> active_downloads_;
    
    mutable std::mutex queue_mutex_;
    std::condition_variable queue_condition_;
    std::atomic<bool> shutdown_requested_;
    
    std::atomic<size_t> pending_count_;
    std::atomic<size_t> active_count_;
};

} // namespace CeWinFileCache