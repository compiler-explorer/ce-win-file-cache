#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

// Forward declaration for metrics config
namespace CeWinFileCache
{
struct MetricsConfig;
}

namespace CeWinFileCache
{

struct CompilerConfig
{
    std::wstring network_path;
    std::wstring root_path;
    std::vector<std::wstring> cache_always_patterns;
    size_t cache_size_mb{};
    std::vector<std::wstring> prefetch_patterns;
};

struct MetricsConfig
{
    bool enabled = true; // Enable metrics by default
    std::string bind_address = "127.0.0.1";
    int port = 8080;
    std::string endpoint_path = "/metrics";
};

struct FileTrackingConfig
{
    bool enabled = true;
    std::wstring report_directory = L"./reports";
    uint32_t report_interval_minutes = 5;
    uint32_t top_files_count = 100;
};

struct GlobalConfig
{
    size_t total_cache_size_mb{};
    std::wstring eviction_policy;
    std::wstring cache_directory;
    size_t download_threads = 4; // Default to 4 threads
    MetricsConfig metrics; // Metrics configuration
    FileTrackingConfig file_tracking; // File access tracking configuration
};

struct Config
{
    std::unordered_map<std::wstring, CompilerConfig> compilers;
    GlobalConfig global;
};

} // namespace CeWinFileCache