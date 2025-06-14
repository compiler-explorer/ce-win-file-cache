#pragma once

#include <ce-win-file-cache/windows_compat.hpp>
#include <chrono>
#include <string>
#include <types/file_state.hpp>

namespace CeWinFileCache
{

struct CacheEntry
{
    std::wstring virtual_path;
    std::wstring local_path;
    std::wstring network_path;
    FileState state;
    CachePolicy policy;

    // File metadata
    DWORD file_attributes;
    UINT64 file_size;
    FILETIME creation_time;
    FILETIME last_access_time;
    FILETIME last_write_time;

    // Cache metadata
    std::chrono::steady_clock::time_point last_used;
    size_t access_count;
    bool is_dirty;

    CacheEntry()
    : state(FileState::VIRTUAL), policy(CachePolicy::ON_DEMAND), file_attributes(0), file_size(0), creation_time{},
      last_access_time{}, last_write_time{}, last_used(std::chrono::steady_clock::now()), access_count(0), is_dirty(false)
    {
    }
};

} // namespace CeWinFileCache