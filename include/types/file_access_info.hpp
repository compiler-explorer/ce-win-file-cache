#pragma once

#include "cache_entry.hpp"
#include <atomic>
#include <chrono>
#include <string>

namespace CeWinFileCache
{

struct FileAccessInfo
{
    std::wstring virtual_path;
    std::wstring network_path;
    uint64_t file_size{};
    std::atomic<uint64_t> access_count{ 0 };
    std::atomic<uint64_t> cache_hits{ 0 };
    std::atomic<uint64_t> cache_misses{ 0 };
    std::chrono::system_clock::time_point first_access;
    std::chrono::system_clock::time_point last_access;
    FileState current_state;
    bool is_memory_cached{};
    double average_access_time_ms{ 0.0 };
    std::wstring cache_policy;
};

struct FileAccessStatistics
{
    uint64_t total_files_tracked{};
    uint64_t total_accesses{};
    uint64_t total_cache_hits{};
    uint64_t total_cache_misses{};
    double cache_hit_rate{};
    uint64_t total_bytes_accessed{};
    uint64_t cached_bytes{};
    std::vector<std::pair<std::wstring, uint64_t>> top_accessed_files;
    std::vector<std::pair<std::wstring, uint64_t>> largest_cached_files;
    std::vector<std::pair<std::wstring, double>> slowest_access_files;
};

} // namespace CeWinFileCache