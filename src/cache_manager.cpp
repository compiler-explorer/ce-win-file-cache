#include <algorithm>
#include <ce-win-file-cache/cache_manager.hpp>
#include <filesystem>
#include <iostream>

namespace CeWinFileCache
{

CacheManager::CacheManager(const GlobalConfig &config)
: config(config), current_cache_size(0), cache_hits(0), cache_misses(0), shutdown_requested(false)
{
}

CacheManager::~CacheManager()
{
    shutdown();
}

NTSTATUS CacheManager::initialize()
{
    // Create cache directory if it doesn't exist
    std::error_code ec;
    std::filesystem::create_directories(config.cache_directory, ec);
    if (ec)
    {
        std::wcerr << L"Failed to create cache directory: " << config.cache_directory << std::endl;
        return STATUS_UNSUCCESSFUL;
    }

    // Calculate current cache size by scanning existing files
    try
    {
        for (const auto &entry : std::filesystem::recursive_directory_iterator(config.cache_directory))
        {
            if (entry.is_regular_file())
            {
                current_cache_size += entry.file_size();
            }
        }
    }
    catch (const std::filesystem::filesystem_error &ex)
    {
        std::wcerr << L"Error scanning cache directory: " << ex.what() << std::endl;
        // Continue anyway - this is not fatal
    }

    // Start background eviction thread
    eviction_thread = std::thread(&CacheManager::backgroundEvictionThread, this);

    return STATUS_SUCCESS;
}

void CacheManager::shutdown()
{
    shutdown_requested = true;
    eviction_cv.notify_all();

    if (eviction_thread.joinable())
    {
        eviction_thread.join();
    }
}

NTSTATUS CacheManager::cacheFile(const std::wstring &network_path, const std::wstring &local_path)
{
    std::lock_guard<std::mutex> lock(cache_mutex);

    // Check if file is already cached
    if (cached_files.find(local_path) != cached_files.end())
    {
        cache_hits++;
        return STATUS_SUCCESS;
    }

    cache_misses++;

    // Calculate file size
    size_t file_size = calculateFileSize(network_path);
    if (file_size == 0)
    {
        return STATUS_OBJECT_NAME_NOT_FOUND;
    }

    // Check if we need to evict files to make space
    size_t max_cache_size = config.total_cache_size_mb * 1024 * 1024;
    if (current_cache_size + file_size > max_cache_size)
    {
        NTSTATUS result = performLRUEviction(file_size);
        if (!NT_SUCCESS(result))
        {
            return result;
        }
    }

    // Create cache entry
    auto entry = std::make_unique<CacheEntry>();
    entry->network_path = network_path;
    entry->local_path = local_path;
    entry->file_size = file_size;
    entry->state = FileState::CACHED;
    entry->last_used = std::chrono::steady_clock::now();

    // Add to cache
    cached_files[local_path] = std::move(entry);
    current_cache_size += file_size;

    return STATUS_SUCCESS;
}

NTSTATUS CacheManager::evictFile(const std::wstring &local_path)
{
    std::lock_guard<std::mutex> lock(cache_mutex);

    auto it = cached_files.find(local_path);
    if (it == cached_files.end())
    {
        return STATUS_OBJECT_NAME_NOT_FOUND;
    }

    // Remove from filesystem
    std::error_code ec;
    std::filesystem::remove(local_path, ec);
    if (ec)
    {
        std::wcerr << L"Failed to remove cached file: " << local_path << std::endl;
        // Continue anyway to remove from cache tracking
    }

    current_cache_size -= it->second->file_size;
    cached_files.erase(it);

    return STATUS_SUCCESS;
}

bool CacheManager::isFileCached(const std::wstring &local_path)
{
    std::lock_guard<std::mutex> lock(cache_mutex);
    return cached_files.find(local_path) != cached_files.end();
}

size_t CacheManager::getCurrentCacheSize() const
{
    return current_cache_size;
}

size_t CacheManager::getCacheHitCount() const
{
    return cache_hits;
}

size_t CacheManager::getCacheMissCount() const
{
    return cache_misses;
}

void CacheManager::backgroundEvictionThread()
{
    while (!shutdown_requested)
    {
        std::unique_lock<std::mutex> lock(cache_mutex);

        // Wait for eviction signal or timeout
        eviction_cv.wait_for(lock, std::chrono::minutes(5),
                              [this]
                              {
                                  return shutdown_requested.load();
                              });

        if (shutdown_requested)
        {
            break;
        }

        // Check if we need to evict based on cache size
        size_t max_cache_size = config.total_cache_size_mb * 1024 * 1024;
        if (current_cache_size > max_cache_size * 0.9) // Start evicting at 90% capacity
        {
            size_t bytes_to_evict = current_cache_size - (max_cache_size * 0.8); // Evict down to 80%
            performLRUEviction(bytes_to_evict);
        }
    }
}

NTSTATUS CacheManager::performLRUEviction(size_t bytes_needed)
{
    // Collect candidates for eviction (sorted by last access time)
    std::vector<std::pair<std::chrono::steady_clock::time_point, std::wstring>> candidates;

    for (const auto &[path, entry] : cached_files)
    {
        candidates.emplace_back(entry->last_used, path);
    }

    // Sort by access time (oldest first)
    std::sort(candidates.begin(), candidates.end());

    size_t bytes_evicted = 0;
    for (const auto &[access_time, path] : candidates)
    {
        if (bytes_evicted >= bytes_needed)
        {
            break;
        }

        auto it = cached_files.find(path);
        if (it != cached_files.end())
        {
            bytes_evicted += it->second->file_size;

            // Remove file from disk
            std::error_code ec;
            std::filesystem::remove(path, ec);

            // Remove from cache
            cached_files.erase(it);
        }
    }

    current_cache_size -= bytes_evicted;

    return bytes_evicted >= bytes_needed ? STATUS_SUCCESS : STATUS_DISK_FULL;
}

size_t CacheManager::calculateFileSize(const std::wstring &file_path)
{
    WIN32_FILE_ATTRIBUTE_DATA file_data;
    if (!GetFileAttributesExW(file_path.c_str(), GetFileExInfoStandard, &file_data))
    {
        return 0;
    }

    return ((UINT64)file_data.nFileSizeHigh << 32) | file_data.nFileSizeLow;
}

} // namespace CeWinFileCache