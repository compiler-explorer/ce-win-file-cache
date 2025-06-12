#include "../include/ce-win-file-cache/async_download_manager.hpp"
#include <iostream>
#include <fstream>
#include <chrono>

namespace CeWinFileCache
{

AsyncDownloadManager::AsyncDownloadManager(MemoryCacheManager& memory_cache, 
                                          const Config& config, 
                                          size_t thread_count)
    : memory_cache_(memory_cache), config_(config), shutdown_requested_(false),
      pending_count_(0), active_count_(0)
{
    for (size_t i = 0; i < thread_count; ++i)
    {
        worker_threads_.emplace_back(&AsyncDownloadManager::workerThread, this);
    }
}

AsyncDownloadManager::~AsyncDownloadManager()
{
    shutdown();
}

NTSTATUS AsyncDownloadManager::queueDownload(const std::wstring& virtual_path,
                                            const std::wstring& network_path,
                                            CacheEntry* cache_entry,
                                            CachePolicy policy,
                                            std::function<void(NTSTATUS, const std::wstring&)> callback)
{
    std::lock_guard<std::mutex> lock(queue_mutex_);
    
    if (shutdown_requested_)
    {
        if (callback)
        {
            callback(STATUS_UNSUCCESSFUL, L"Download manager is shutting down");
        }
        return STATUS_UNSUCCESSFUL;
    }
    
    if (active_downloads_.find(virtual_path) != active_downloads_.end())
    {
        if (callback)
        {
            callback(STATUS_PENDING, L"Download already in progress");
        }
        return STATUS_PENDING;
    }
    
    auto task = std::make_shared<DownloadTask>(virtual_path, network_path, 
                                              cache_entry, policy, callback);
    
    download_queue_.push(task);
    active_downloads_[virtual_path] = task;
    pending_count_++;
    
    queue_condition_.notify_one();
    
    return STATUS_PENDING;
}

bool AsyncDownloadManager::isDownloadInProgress(const std::wstring& virtual_path)
{
    std::lock_guard<std::mutex> lock(queue_mutex_);
    return active_downloads_.find(virtual_path) != active_downloads_.end();
}

void AsyncDownloadManager::cancelDownload(const std::wstring& virtual_path)
{
    std::lock_guard<std::mutex> lock(queue_mutex_);
    
    auto it = active_downloads_.find(virtual_path);
    if (it != active_downloads_.end())
    {
        active_downloads_.erase(it);
    }
}

void AsyncDownloadManager::shutdown()
{
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        shutdown_requested_ = true;
    }
    
    queue_condition_.notify_all();
    
    for (auto& thread : worker_threads_)
    {
        if (thread.joinable())
        {
            thread.join();
        }
    }
    
    worker_threads_.clear();
}

size_t AsyncDownloadManager::getPendingCount() const
{
    return pending_count_.load();
}

size_t AsyncDownloadManager::getActiveCount() const
{
    return active_count_.load();
}

void AsyncDownloadManager::workerThread()
{
    while (!shutdown_requested_)
    {
        std::shared_ptr<DownloadTask> task;
        
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            queue_condition_.wait(lock, [this] { 
                return !download_queue_.empty() || shutdown_requested_; 
            });
            
            if (shutdown_requested_)
            {
                break;
            }
            
            if (!download_queue_.empty())
            {
                task = download_queue_.front();
                download_queue_.pop();
                pending_count_--;
                active_count_++;
            }
        }
        
        if (task)
        {
            processDownload(task);
            active_count_--;
            
            {
                std::lock_guard<std::mutex> lock(queue_mutex_);
                active_downloads_.erase(task->virtual_path);
            }
        }
    }
}

void AsyncDownloadManager::processDownload(std::shared_ptr<DownloadTask> task)
{
    bool success = false;
    std::wstring error_message;
    
    try
    {
        if (task->policy == CachePolicy::ALWAYS_CACHE || 
            task->policy == CachePolicy::ON_DEMAND)
        {
            success = downloadFile(task->network_path, task->virtual_path);
            
            if (success && task->cache_entry)
            {
                auto content = memory_cache_.getFileContent(task->virtual_path, config_);
                if (!content.empty())
                {
                    task->cache_entry->file_size = content.size();
                    task->cache_entry->state = FileState::CACHED;
                    task->cache_entry->last_used = std::chrono::steady_clock::now();
                    task->cache_entry->access_count++;
                    task->cache_entry->local_path.clear();
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
    catch (const std::exception& e)
    {
        success = false;
        std::string narrow_msg = e.what();
        error_message = std::wstring(narrow_msg.begin(), narrow_msg.end());
    }
    
    if (task->callback)
    {
        task->callback(success ? STATUS_SUCCESS : STATUS_UNSUCCESSFUL, 
                      error_message);
    }
}

bool AsyncDownloadManager::downloadFile(const std::wstring& network_path, 
                                       const std::wstring& virtual_path)
{
    try
    {
        std::string narrow_path(network_path.begin(), network_path.end());
        std::ifstream file(narrow_path, std::ios::binary);
        
        if (!file.is_open())
        {
            return false;
        }
        
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
        memory_cache_.addFileToMemoryCache(virtual_path, uint8_buffer);
        return true;
    }
    catch (...)
    {
        return false;
    }
}

} // namespace CeWinFileCache