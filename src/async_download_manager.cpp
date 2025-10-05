#include "../include/ce-win-file-cache/async_download_manager.hpp"
#include <chrono>
#include <fstream>
#include <iostream>

namespace CeWinFileCache
{

AsyncDownloadManager::AsyncDownloadManager(MemoryCacheManager &memory_cache, const Config &config, size_t thread_count)
: memory_cache(memory_cache), config(config), shutdown_requested(false), pending_count(0), active_count(0)
{
    for (size_t i = 0; i < thread_count; ++i)
    {
        worker_threads.emplace_back(&AsyncDownloadManager::workerThread, this);
    }
}

AsyncDownloadManager::~AsyncDownloadManager()
{
    shutdown();
}

NTSTATUS AsyncDownloadManager::queueDownload(const std::wstring &virtual_path,
                                             const std::wstring &network_path,
                                             CacheEntry *cache_entry,
                                             CachePolicy policy,
                                             std::function<void(NTSTATUS, const std::wstring, CacheEntry *)> callback)
{
    std::lock_guard<std::mutex> lock(queue_mutex);

    if (shutdown_requested)
    {
        if (callback)
        {
            callback(STATUS_UNSUCCESSFUL, L"Download manager is shutting down", cache_entry);
        }
        return STATUS_UNSUCCESSFUL;
    }

    if (active_downloads.find(virtual_path) != active_downloads.end())
    {
        if (callback)
        {
            callback(STATUS_PENDING, L"Download already in progress", cache_entry);
        }
        return STATUS_PENDING;
    }

    auto task = std::make_shared<DownloadTask>(virtual_path, network_path, cache_entry, policy, callback);

    download_queue.push(task);
    active_downloads[virtual_path] = task;
    pending_count++;

    // Record download queued metric
    GlobalMetrics::instance().recordDownloadQueued();
    GlobalMetrics::instance().updatePendingDownloads(pending_count.load());
    GlobalMetrics::instance().updateActiveDownloads(active_count.load());

    queue_condition.notify_one();

    return STATUS_PENDING;
}

bool AsyncDownloadManager::isDownloadInProgress(const std::wstring virtual_path)
{
    std::lock_guard<std::mutex> lock(queue_mutex);
    return active_downloads.find(virtual_path) != active_downloads.end();
}

void AsyncDownloadManager::cancelDownload(const std::wstring virtual_path)
{
    std::lock_guard<std::mutex> lock(queue_mutex);

    auto it = active_downloads.find(virtual_path);
    if (it != active_downloads.end())
    {
        active_downloads.erase(it);
    }
}

void AsyncDownloadManager::shutdown()
{
    {
        std::lock_guard<std::mutex> lock(queue_mutex);
        shutdown_requested = true;
    }

    queue_condition.notify_all();

    for (auto &thread : worker_threads)
    {
        if (thread.joinable())
        {
            thread.join();
        }
    }

    worker_threads.clear();
}

size_t AsyncDownloadManager::getPendingCount() const
{
    return pending_count.load();
}

size_t AsyncDownloadManager::getActiveCount() const
{
    return active_count.load();
}

void AsyncDownloadManager::workerThread()
{
    while (!shutdown_requested)
    {
        std::shared_ptr<DownloadTask> task;

        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            queue_condition.wait(lock,
                                 [this]
                                 {
                                     return !download_queue.empty() || shutdown_requested;
                                 });

            if (shutdown_requested)
            {
                break;
            }

            if (!download_queue.empty())
            {
                task = download_queue.front();
                download_queue.pop();
                pending_count--;
                active_count++;

                // Update download metrics
                GlobalMetrics::instance().recordDownloadStarted();
                GlobalMetrics::instance().updatePendingDownloads(pending_count.load());
                GlobalMetrics::instance().updateActiveDownloads(active_count.load());
            }
        }

        if (task)
        {
            processDownload(task);
            active_count--;

            // Update active downloads metric
            GlobalMetrics::instance().updateActiveDownloads(active_count.load());

            {
                std::lock_guard<std::mutex> lock(queue_mutex);
                active_downloads.erase(task->virtual_path);
            }
        }
    }
}

void AsyncDownloadManager::processDownload(std::shared_ptr<DownloadTask> task)
{
    bool success = false;
    std::wstring error_message;
    auto start_time = std::chrono::high_resolution_clock::now();

    // Mark entry as downloading to prevent eviction
    if (task->cache_entry)
    {
        task->cache_entry->is_downloading.store(true);
        task->cache_entry->state = FileState::FETCHING;
    }

    try
    {
        if (task->policy == CachePolicy::ALWAYS_CACHE || task->policy == CachePolicy::ON_DEMAND)
        {
            success = downloadFile(task->network_path, task->virtual_path);

            if (success && task->cache_entry)
            {
                auto content = memory_cache.getFileContent(task->virtual_path, config);
                if (!content.empty())
                {
                    task->cache_entry->file_size = content.size();
                    task->cache_entry->state = FileState::CACHED;
                    task->cache_entry->last_used = std::chrono::steady_clock::now();
                    task->cache_entry->access_count++;
                    task->cache_entry->local_path.clear();
                    task->cache_entry->is_in_memory_cache.store(true);
                }
                else
                {
                    success = false;
                    error_message = L"Failed to load file into memory cache";
                }
            }
        }
        else
        {
            if (task->cache_entry)
            {
                task->cache_entry->local_path = task->network_path;
                task->cache_entry->state = FileState::NETWORK_ONLY;
            }
            success = true;
        }
    }
    catch (const std::exception &e)
    {
        success = false;
        std::string narrow_msg = e.what();
        error_message = std::wstring(narrow_msg.begin(), narrow_msg.end());
    }

    // Record download completion metrics
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration<double>(end_time - start_time).count();

    if (success)
    {
        GlobalMetrics::instance().recordDownloadCompleted(duration);
    }
    else
    {
        std::string reason = "unknown";
        if (!error_message.empty())
        {
            reason = std::string(error_message.begin(), error_message.end());
        }
        GlobalMetrics::instance().recordDownloadFailed(reason);
    }

    // Clear download flag regardless of success/failure
    if (task->cache_entry)
    {
        task->cache_entry->is_downloading.store(false);
    }

    if (task->callback)
    {
        task->callback(success ? STATUS_SUCCESS : STATUS_UNSUCCESSFUL, error_message, task->cache_entry);
    }
}

bool AsyncDownloadManager::downloadFile(const std::wstring network_path, const std::wstring virtual_path)
{
    auto start_time = std::chrono::high_resolution_clock::now();

    try
    {
        // Record filesystem operation
        GlobalMetrics::instance().recordFilesystemOperation("download");

        std::string narrow_path(network_path.begin(), network_path.end());
        std::ifstream file(narrow_path, std::ios::binary);

        if (!file.is_open())
        {
            // Record file open duration even for failed opens
            auto end_time = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration<double>(end_time - start_time).count();
            GlobalMetrics::instance().recordFileOpenDuration(duration);
            return false;
        }

        // Record successful file open duration
        auto open_time = std::chrono::high_resolution_clock::now();
        auto open_duration = std::chrono::duration<double>(open_time - start_time).count();
        GlobalMetrics::instance().recordFileOpenDuration(open_duration);

        file.seekg(0, std::ios::end);
        size_t file_size = file.tellg();
        file.seekg(0, std::ios::beg);

        std::vector<char> buffer(file_size);
        file.read(buffer.data(), file_size);

        if (!file.good() && !file.eof())
        {
            return false;
        }

        std::vector<uint8_t> uint8_buffer(buffer.begin(), buffer.end());
        memory_cache.addFileToMemoryCache(virtual_path, uint8_buffer);
        return true;
    }
    catch (...)
    {
        return false;
    }
}

} // namespace CeWinFileCache