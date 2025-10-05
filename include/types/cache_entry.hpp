#pragma once

#include <atomic>
#include <ce-win-file-cache/windows_compat.hpp>
#include <chrono>
#include <string>
#include <types/file_state.hpp>
#include <iostream>

namespace CeWinFileCache
{

struct CacheEntry
{
    std::wstring virtual_path;
    std::wstring local_path;
    std::wstring network_path;
    FileState state = FileState::VIRTUAL;
    CachePolicy policy = CachePolicy::ON_DEMAND;

    // File metadata
    DWORD file_attributes{};
    UINT64 file_size{};
    FILETIME creation_time{};
    FILETIME last_access_time{};
    FILETIME last_write_time{};

    PSECURITY_DESCRIPTOR SecDesc = nullptr;

    // Cache metadata
    std::chrono::steady_clock::time_point last_used = std::chrono::steady_clock::now();
    size_t access_count = 0;

    // Download protection - prevents eviction during active downloads
    std::atomic<bool> is_downloading = false;

    // Memory cache tracking - avoids repeated mutex locks to check memory cache status
    std::atomic<bool> is_in_memory_cache = false;

    CacheEntry() = default;
};

} // namespace CeWinFileCache