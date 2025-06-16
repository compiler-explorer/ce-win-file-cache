#include "../include/ce-win-file-cache/memory_cache_manager.hpp"
#include "../include/ce-win-file-cache/string_utils.hpp"
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <cassert>

namespace CeWinFileCache
{

std::vector<uint8_t> MemoryCacheManager::loadNetworkFileToMemory(const std::wstring &network_path)
{
    std::vector<uint8_t> content;
    auto start_time = std::chrono::high_resolution_clock::now();

    try
    {
        // Record network operation attempt
        GlobalMetrics::instance().recordNetworkOperation("file_read", false); // Mark as attempt initially

#ifdef _WIN32
        std::ifstream file(network_path, std::ios::binary | std::ios::ate);
#else
        // Convert wstring to UTF-8 string for non-Windows platforms
        std::string narrow_path = StringUtils::wideToUtf8(network_path);
        std::ifstream file(narrow_path, std::ios::binary | std::ios::ate);
#endif
        if (!file.is_open())
        {
            std::wcerr << L"Failed to open network file: " << network_path << std::endl;
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
            std::wcerr << L"Failed to read network file: " << network_path << std::endl;
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
        std::wcerr << L"Exception loading network file: " << network_path << L" - " << e.what() << std::endl;
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
    //assert(memory_cache.count() != 0);
    std::wcout << L"isFileInMemoryCache(" << virtual_path << L")\n";
    std::flush(std::wcout);


    std::lock_guard<std::mutex> lock(cache_mutex);

    if (memory_cache.size() == 0)
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

void MemoryCacheManager::addFileToMemoryCache(const std::wstring &virtual_path, const std::vector<uint8_t> &content)
{
    std::lock_guard<std::mutex> lock(cache_mutex);
    memory_cache[virtual_path] = content;

    // Update cache metrics
    // Update cache size and entry count
    size_t total_size = 0;
    for (const auto &[path, file_content] : memory_cache)
    {
        total_size += file_content.size();
    }
    GlobalMetrics::instance().updateCacheSize(total_size);
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
        std::wcerr << L"Failed to resolve virtual path: " << virtual_path << std::endl;
        return std::vector<uint8_t>();
    }

    auto content = loadNetworkFileToMemory(network_path);
    if (!content.empty())
    {
        addFileToMemoryCache(virtual_path, content);
    }

    return content;
}

void MemoryCacheManager::clearCache()
{
    std::lock_guard<std::mutex> lock(cache_mutex);
    size_t cleared_entries = memory_cache.size();
    memory_cache.clear();

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
    std::lock_guard<std::mutex> lock(cache_mutex);

    size_t total_size = 0;
    for (const auto &[path, content] : memory_cache)
    {
        total_size += content.size();
    }

    return total_size;
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

    return network_path + relative_path;
}

} // namespace CeWinFileCache