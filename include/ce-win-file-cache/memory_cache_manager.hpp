#pragma once

#include "../types/config.hpp"
#include "metrics_collector.hpp"
#include <mutex>
#include <atomic>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace CeWinFileCache
{

// Forward declaration
struct CacheEntry;

class MemoryCacheManager
{
    public:
    MemoryCacheManager() = default;
    ~MemoryCacheManager() = default;

    MemoryCacheManager(const MemoryCacheManager &) = delete;
    MemoryCacheManager &operator=(const MemoryCacheManager &) = delete;

    std::vector<uint8_t> loadNetworkFileToMemory(const std::wstring &network_path);

    bool isFileInMemoryCache(const std::wstring virtual_path);

    std::optional<std::vector<uint8_t>> getMemoryCachedFile(const std::wstring &virtual_path);

    const std::vector<uint8_t> *getMemoryCachedFilePtr(const std::wstring &virtual_path);

    // Get pointer and increment reference count for eviction protection
    const std::vector<uint8_t> *getMemoryCachedFilePtr(CacheEntry *entry);

    void addFileToMemoryCache(const std::wstring &virtual_path, const std::vector<uint8_t> &content);

    // Moves the content in without copying, but only when the path is not cached yet (existing content may
    // be in use by open files). Returns false when it was already cached.
    bool addFileToMemoryCache(const std::wstring &virtual_path, std::vector<uint8_t> &&content);

    void removeFileFromMemoryCache(const std::wstring &virtual_path);

    // Removes the entry's content only when no open file holds a pointer to it. Checked under the same
    // mutex that getMemoryCachedFilePtr(CacheEntry *) uses to take a reference, so the two cannot race.
    bool removeFileIfUnreferenced(CacheEntry *entry);

    std::vector<uint8_t> getFileContent(const std::wstring &virtual_path, const Config &config);

    void clearCache();

    size_t getCacheSize() const;

    size_t getCachedFileCount() const;

    private:
    mutable std::mutex cache_mutex;
    std::unordered_map<std::wstring, std::vector<uint8_t>> memory_cache;
    std::atomic<size_t> total_cache_size{0};

    std::wstring resolveVirtualToNetworkPath(const std::wstring &virtual_path, const Config &config);
};

} // namespace CeWinFileCache