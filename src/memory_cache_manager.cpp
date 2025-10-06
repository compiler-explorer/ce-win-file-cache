#include "../include/ce-win-file-cache/memory_cache_manager.hpp"
#include "../include/ce-win-file-cache/logger.hpp"
#include "../include/ce-win-file-cache/string_utils.hpp"
#include "../include/types/cache_entry.hpp"
#include <algorithm>
#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>

namespace CeWinFileCache
{

namespace
{
std::ifstream openNetworkFile(const std::wstring &network_path)
{
#ifdef _WIN32
    return std::ifstream(network_path, std::ios::binary | std::ios::ate);
#else
    // Convert wstring to UTF-8 string for non-Windows platforms
    std::string narrow_path = StringUtils::wideToUtf8(network_path);
    return std::ifstream(narrow_path, std::ios::binary | std::ios::ate);
#endif
}
} // namespace

std::vector<uint8_t> MemoryCacheManager::loadNetworkFileToMemory(const std::wstring &network_path)
{
    std::vector<uint8_t> content;
    auto start_time = std::chrono::high_resolution_clock::now();

    try
    {
        // Record network operation attempt
        GlobalMetrics::instance().recordNetworkOperation("file_read", false); // Mark as attempt initially

        std::ifstream file = openNetworkFile(network_path);
        if (!file.is_open())
        {
            CeWinFileCache::Logger::error("Failed to open network file: {}", Logger::wstringToString(network_path));
            // Record failed network operation
            auto end_time = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration<double>(end_time - start_time).count();
            GlobalMetrics::instance().recordNetworkLatency(duration);
            return content;
        }

        std::streamsize size = file.tellg();
        file.seekg(0, std::ios::beg);

        content.resize(static_cast<size_t>(size));
        if (!file.read(reinterpret_cast<char *>(content.data()), size))
        {
            CeWinFileCache::Logger::error("Failed to read network file: {}", Logger::wstringToString(network_path));
            content.clear();
        }
        else
        {
            // Record successful network operation
            GlobalMetrics::instance().recordNetworkOperation("file_read", true);
        }
    }
    catch (const std::exception &e)
    {
        CeWinFileCache::Logger::error("Exception loading network file: {} - {}", Logger::wstringToString(network_path),
                                      e.what());
        content.clear();
    }

    // Record network latency
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration<double>(end_time - start_time).count();
    GlobalMetrics::instance().recordNetworkLatency(duration);

    return content;
}

bool MemoryCacheManager::isFileInMemoryCache(const std::wstring virtual_path)
{
    // assert(memory_cache.count() != 0);
    CeWinFileCache::Logger::debug("isFileInMemoryCache({})", Logger::wstringToString(virtual_path));


    std::lock_guard<std::mutex> lock(cache_mutex);

    if (memory_cache.empty())
        return false;

    return memory_cache.find(virtual_path) != memory_cache.end();
}

std::optional<std::vector<uint8_t>> MemoryCacheManager::getMemoryCachedFile(const std::wstring &virtual_path)
{
    std::lock_guard<std::mutex> lock(cache_mutex);

    auto it = memory_cache.find(virtual_path);
    if (it != memory_cache.end())
    {
        // Record cache hit
        GlobalMetrics::instance().recordCacheHit("read");
        return it->second;
    }

    // Record cache miss
    GlobalMetrics::instance().recordCacheMiss("read");
    return std::nullopt;
}

const std::vector<uint8_t> *MemoryCacheManager::getMemoryCachedFilePtr(const std::wstring &virtual_path)
{
    std::lock_guard<std::mutex> lock(cache_mutex);

    auto it = memory_cache.find(virtual_path);
    if (it != memory_cache.end())
    {
        // Return pointer to cached data - MUST NOT be used after cache is modified!
        return &(it->second);
    }

    return nullptr;
}

const std::vector<uint8_t> *MemoryCacheManager::getMemoryCachedFilePtr(CacheEntry *entry)
{
    if (!entry)
    {
        return nullptr;
    }

    std::lock_guard<std::mutex> lock(cache_mutex);

    auto it = memory_cache.find(entry->virtual_path);
    if (it != memory_cache.end())
    {
        // Increment reference count to protect from eviction
        entry->memory_ref_count++;

        // Return pointer to cached data
        return &(it->second);
    }

    return nullptr;
}

void MemoryCacheManager::addFileToMemoryCache(const std::wstring &virtual_path, const std::vector<uint8_t> &content)
{
    std::lock_guard<std::mutex> lock(cache_mutex);

    // Check if file already exists in cache
    auto it = memory_cache.find(virtual_path);
    if (it != memory_cache.end())
    {
        // Subtract old size before replacing
        total_cache_size -= it->second.size();
    }

    // Add new content
    size_t content_size = content.size();
    memory_cache[virtual_path] = content;
    total_cache_size += content_size;

    // Update cache metrics
    GlobalMetrics::instance().updateCacheSize(total_cache_size.load());
    GlobalMetrics::instance().updateCacheEntryCount(memory_cache.size());
}

std::vector<uint8_t> MemoryCacheManager::getFileContent(const std::wstring &virtual_path, const Config &config)
{
    auto cached = getMemoryCachedFile(virtual_path);
    if (cached.has_value())
    {
        return cached.value();
    }

    std::wstring network_path = resolveVirtualToNetworkPath(virtual_path, config);
    if (network_path.empty())
    {
        CeWinFileCache::Logger::error("Failed to resolve virtual path: {}", Logger::wstringToString(virtual_path));
        return std::vector<uint8_t>();
    }

    auto content = loadNetworkFileToMemory(network_path);
    if (!content.empty())
    {
        addFileToMemoryCache(virtual_path, content);
    }

    return content;
}

void MemoryCacheManager::removeFileFromMemoryCache(const std::wstring &virtual_path)
{
    std::lock_guard<std::mutex> lock(cache_mutex);

    auto it = memory_cache.find(virtual_path);
    if (it != memory_cache.end())
    {
        // Subtract size before removing
        total_cache_size -= it->second.size();
        memory_cache.erase(it);

        // Update cache metrics
        GlobalMetrics::instance().updateCacheSize(total_cache_size.load());
        GlobalMetrics::instance().updateCacheEntryCount(memory_cache.size());
        GlobalMetrics::instance().recordCacheEviction();
    }
}

void MemoryCacheManager::clearCache()
{
    std::lock_guard<std::mutex> lock(cache_mutex);
    size_t cleared_entries = memory_cache.size();
    memory_cache.clear();
    total_cache_size = 0;

    // Update cache metrics after clearing
    GlobalMetrics::instance().updateCacheSize(0);
    GlobalMetrics::instance().updateCacheEntryCount(0);
    // Record evictions (clearing counts as multiple evictions)
    for (size_t i = 0; i < cleared_entries; ++i)
    {
        GlobalMetrics::instance().recordCacheEviction();
    }
}

size_t MemoryCacheManager::getCacheSize() const
{
    return total_cache_size.load();
}

size_t MemoryCacheManager::getCachedFileCount() const
{
    std::lock_guard<std::mutex> lock(cache_mutex);
    return memory_cache.size();
}

std::wstring MemoryCacheManager::resolveVirtualToNetworkPath(const std::wstring &virtual_path, const Config &config)
{
    if (virtual_path.empty() || virtual_path[0] != L'/')
    {
        return L"";
    }

    size_t second_slash = virtual_path.find(L'/', 1);
    if (second_slash == std::wstring::npos)
    {
        return L"";
    }

    std::wstring compiler_name = virtual_path.substr(1, second_slash - 1);
    std::wstring relative_path = virtual_path.substr(second_slash + 1);

    auto it = config.compilers.find(compiler_name);
    if (it == config.compilers.end())
    {
        return L"";
    }

    std::wstring network_path = it->second.network_path;
    if (!network_path.empty() && network_path.back() != L'\\')
    {
        network_path += L'\\';
    }

    std::replace(relative_path.begin(), relative_path.end(), L'/', L'\\');

    auto fullpath = network_path + relative_path;

    if (!loaded_config.global.case_sensitive)
    {
        StringUtils::toLower(fullpath);
    }

    return fullpath;
}

} // namespace CeWinFileCache