#pragma once

#include "cache_manager.hpp"
#include "config_parser.hpp"
#include "memory_cache_manager.hpp"
#include "metrics_collector.hpp"
#include "windows_compat.hpp"
#include <atomic>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <unordered_map>

namespace CeWinFileCache
{

struct DownloadTask
{
    std::wstring virtual_path{};
    std::wstring network_path{};
    CacheEntry *cache_entry;
    CachePolicy policy;
    std::function<void(NTSTATUS, const std::wstring &, CacheEntry*)> callback{};

    DownloadTask(const std::wstring &vpath,
                 const std::wstring &npath,
                 CacheEntry *entry,
                 CachePolicy pol,
                 std::function<void(NTSTATUS, const std::wstring &, CacheEntry*)> cb)
    : virtual_path(vpath), network_path(npath), cache_entry(entry), policy(pol), callback(cb)
    {
    }
};

class AsyncDownloadManager
{
    public:
    AsyncDownloadManager(MemoryCacheManager &memory_cache, const Config &config, size_t thread_count = 4);
    ~AsyncDownloadManager();

    NTSTATUS queueDownload(const std::wstring &virtual_path,
                           const std::wstring &network_path,
                           CacheEntry *cache_entry,
                           CachePolicy policy,
                           std::function<void(NTSTATUS, const std::wstring, CacheEntry*)> callback);

    bool isDownloadInProgress(const std::wstring virtual_path);

    void cancelDownload(const std::wstring virtual_path);

    void shutdown();

    size_t getPendingCount() const;
    size_t getActiveCount() const;

    private:
    void workerThread();
    void processDownload(std::shared_ptr<DownloadTask> task);
    bool downloadFile(const std::wstring network_path, const std::wstring virtual_path);

    MemoryCacheManager &memory_cache;
    const Config &config;

    std::vector<std::thread> worker_threads{};
    std::queue<std::shared_ptr<DownloadTask>> download_queue;
    std::unordered_map<std::wstring, std::shared_ptr<DownloadTask>> active_downloads;

    mutable std::mutex queue_mutex{};
    std::condition_variable queue_condition{};
    std::atomic<bool> shutdown_requested{};

    std::atomic<size_t> pending_count{};
    std::atomic<size_t> active_count{};
};

} // namespace CeWinFileCache