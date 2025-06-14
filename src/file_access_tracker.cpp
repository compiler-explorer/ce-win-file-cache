#include <algorithm>
#include <ce-win-file-cache/file_access_tracker.hpp>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace CeWinFileCache
{

// Helper functions for formatting
static std::wstring formatTimestamp(std::chrono::system_clock::time_point tp)
{
    auto time_t = std::chrono::system_clock::to_time_t(tp);

    std::tm tm;
#ifdef _WIN32
    localtime_s(&tm, &time_t);
#else
    localtime_r(&time_t, &tm);
#endif

    std::wostringstream oss;
    oss << std::put_time(&tm, L"%Y-%m-%d %H:%M:%S");
    return oss.str();
}

static std::wstring fileStateToString(FileState state)
{
    switch (state)
    {
    case FileState::VIRTUAL:
        return L"Virtual";
    case FileState::CACHED:
        return L"Cached";
    case FileState::PLACEHOLDER:
        return L"Placeholder";
    case FileState::FETCHING:
        return L"Fetching";
    case FileState::NETWORK_ONLY:
        return L"Network Only";
    default:
        return L"Unknown";
    }
}

FileAccessTracker::FileAccessTracker() : tracking_start_time_(std::chrono::system_clock::now())
{
}

FileAccessTracker::~FileAccessTracker()
{
    stopReporting();
}

void FileAccessTracker::initialize(const std::wstring &report_directory, std::chrono::minutes report_interval, size_t top_files_count)
{
    std::wcout << L"[DEBUG] Setting member variables..." << std::endl;
    report_directory_ = report_directory;
    report_interval_ = report_interval;
    top_files_count_ = top_files_count;
    std::wcout << L"[DEBUG] Member variables set" << std::endl;

    // Create report directory if it doesn't exist (macOS-compatible)
    std::wcout << L"[DEBUG] About to create directory..." << std::endl;
#ifdef __APPLE__
    // On macOS, use system() call for directory creation
    std::string narrow_dir(report_directory.begin(), report_directory.end());
    std::string mkdir_cmd = "mkdir -p \"" + narrow_dir + "\"";
    std::wcout << L"[DEBUG] Running mkdir command..." << std::endl;
    int result = std::system(mkdir_cmd.c_str());
    std::wcout << L"[DEBUG] mkdir command completed with result: " << result << std::endl;
    if (result != 0)
    {
        std::wcerr << L"[FileAccessTracker] Warning: Could not create report directory via system call" << std::endl;
    }
#else
    try 
    {
        std::filesystem::path report_path(report_directory);
        std::filesystem::create_directories(report_path);
    }
    catch (const std::exception& e)
    {
        std::wcerr << L"[FileAccessTracker] Warning: Could not create report directory: " 
                   << std::wstring(e.what(), e.what() + strlen(e.what())) << std::endl;
    }
#endif
    std::wcout << L"[DEBUG] Directory creation section completed" << std::endl;
}

void FileAccessTracker::recordAccess(const std::wstring &virtual_path,
                                     const std::wstring &network_path,
                                     uint64_t file_size,
                                     FileState state,
                                     bool is_cache_hit,
                                     bool is_memory_cached,
                                     double access_time_ms,
                                     const std::wstring &cache_policy)
{
    std::wcout << L"[DEBUG] recordAccess called for: " << virtual_path << std::endl;
    std::lock_guard<std::mutex> lock(mutex_);
    std::wcout << L"[DEBUG] Mutex acquired" << std::endl;

    auto &info = file_access_map_[virtual_path];
    if (!info)
    {
        info = std::make_unique<FileAccessInfo>();
        info->virtual_path = virtual_path;
        info->network_path = network_path;
        info->file_size = file_size;
        info->first_access = std::chrono::system_clock::now();
        info->cache_policy = cache_policy;
    }

    info->access_count++;
    info->last_access = std::chrono::system_clock::now();
    info->current_state = state;
    info->is_memory_cached = is_memory_cached;

    if (is_cache_hit)
    {
        info->cache_hits++;
        total_cache_hits_++;
    }
    else
    {
        info->cache_misses++;
        total_cache_misses_++;
    }

    // Update average access time
    double current_avg = info->average_access_time_ms;
    uint64_t count = info->access_count.load();
    info->average_access_time_ms = (current_avg * (count - 1) + access_time_ms) / count;

    total_accesses_++;
}

void FileAccessTracker::startReporting()
{
    if (reporting_enabled_.exchange(true))
        return; // Already running

    reporting_thread_ = std::make_unique<std::thread>(&FileAccessTracker::reportingThreadFunc, this);
}

void FileAccessTracker::stopReporting()
{
    reporting_enabled_ = false;
    if (reporting_thread_ && reporting_thread_->joinable())
    {
        reporting_thread_->join();
    }
}

void FileAccessTracker::reportingThreadFunc()
{
    while (reporting_enabled_)
    {
        auto next_report_time = std::chrono::steady_clock::now() + report_interval_;

        // Wait for the interval or until stopped
        while (reporting_enabled_ && std::chrono::steady_clock::now() < next_report_time)
        {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        if (reporting_enabled_)
        {
            generateReport();
        }
    }
}

void FileAccessTracker::generateReport()
{
    auto timestamp = getCurrentTimestamp();

    // Generate detailed CSV report
    std::wstring csv_filename = report_directory_ + L"/file_access_" + timestamp + L".csv";
    writeCSVReport(csv_filename);

    // Generate summary report
    std::wstring summary_filename = report_directory_ + L"/access_summary_" + timestamp + L".txt";
    writeSummaryReport(summary_filename);

    std::wcout << L"[FileAccessTracker] Generated reports: " << csv_filename << L" and " << summary_filename << std::endl;
}

void FileAccessTracker::writeCSVReport(const std::wstring &filename)
{
    try
    {
        std::filesystem::path filepath(filename);
        
        // Ensure parent directory exists
#ifdef __APPLE__
        std::string parent_dir_str = filepath.parent_path().string();
        std::string mkdir_cmd = "mkdir -p \"" + parent_dir_str + "\"";
        std::system(mkdir_cmd.c_str());
#else
        std::filesystem::create_directories(filepath.parent_path());
#endif
        
        std::wofstream file(filepath);
        if (!file.is_open())
        {
            std::wcerr << L"[FileAccessTracker] Failed to create CSV report: " << filename << std::endl;
            return;
        }

    // Write CSV header
    file << L"Virtual Path,Network Path,File Size (MB),Access Count,Cache Hits,Cache Misses,"
         << L"Hit Rate %,State,Memory Cached,Avg Access Time (ms),First Access,Last Access,"
         << L"Time Since First Access,Cache Policy\n";

    std::lock_guard<std::mutex> lock(mutex_);

    // Sort files by access count for the report
    std::vector<FileAccessInfo *> sorted_files;
    for (const auto &[path, info] : file_access_map_)
    {
        sorted_files.push_back(info.get());
    }

    std::sort(sorted_files.begin(), sorted_files.end(),
              [](const FileAccessInfo *a, const FileAccessInfo *b)
              {
                  return a->access_count.load() > b->access_count.load();
              });

    // Write data rows
    for (const auto *info : sorted_files)
    {
        double hit_rate = 0.0;
        uint64_t hits = info->cache_hits.load();
        uint64_t misses = info->cache_misses.load();
        if (hits + misses > 0)
        {
            hit_rate = (double)hits / (hits + misses) * 100.0;
        }

        auto time_since_first = info->last_access - info->first_access;

        file << L"\"" << info->virtual_path << L"\"," << L"\"" << info->network_path << L"\"," << std::fixed
             << std::setprecision(2) << (info->file_size / (1024.0 * 1024.0)) << L"," << info->access_count.load()
             << L"," << hits << L"," << misses << L"," << std::fixed << std::setprecision(1) << hit_rate << L","
             << fileStateToString(info->current_state) << L"," << (info->is_memory_cached ? L"Yes" : L"No") << L","
             << std::fixed << std::setprecision(2) << info->average_access_time_ms << L","
             << formatTimestamp(info->first_access) << L"," << formatTimestamp(info->last_access) << L","
             << formatDuration(time_since_first) << L"," << info->cache_policy << L"\n";
    }

        file.close();
    }
    catch (const std::exception& e)
    {
        std::wcerr << L"[FileAccessTracker] Error writing CSV report: " 
                   << std::wstring(e.what(), e.what() + strlen(e.what())) << std::endl;
    }
}

void FileAccessTracker::writeSummaryReport(const std::wstring &filename)
{
    try
    {
        std::filesystem::path filepath(filename);
        
        // Ensure parent directory exists
#ifdef __APPLE__
        std::string parent_dir_str = filepath.parent_path().string();
        std::string mkdir_cmd = "mkdir -p \"" + parent_dir_str + "\"";
        std::system(mkdir_cmd.c_str());
#else
        std::filesystem::create_directories(filepath.parent_path());
#endif
        
        std::wofstream file(filepath);
        if (!file.is_open())
        {
            std::wcerr << L"[FileAccessTracker] Failed to create summary report: " << filename << std::endl;
            return;
        }

    auto stats = getStatistics();
    auto now = std::chrono::system_clock::now();
    auto tracking_duration = now - tracking_start_time_;

    file << L"CE Win File Cache - File Access Summary Report\n";
    file << L"==============================================\n\n";
    file << L"Report Generated: " << formatTimestamp(now) << L"\n";
    file << L"Tracking Duration: " << formatDuration(tracking_duration) << L"\n\n";

    file << L"Overall Statistics\n";
    file << L"------------------\n";
    file << L"Total Files Tracked: " << stats.total_files_tracked << L"\n";
    file << L"Total File Accesses: " << stats.total_accesses << L"\n";
    file << L"Total Cache Hits: " << stats.total_cache_hits << L"\n";
    file << L"Total Cache Misses: " << stats.total_cache_misses << L"\n";
    file << L"Overall Hit Rate: " << std::fixed << std::setprecision(1) << stats.cache_hit_rate << L"%\n";
    file << L"Total Bytes Accessed: " << formatFileSize(stats.total_bytes_accessed) << L"\n";
    file << L"Cached Bytes: " << formatFileSize(stats.cached_bytes) << L"\n\n";

    file << L"Top " << stats.top_accessed_files.size() << L" Most Accessed Files\n";
    file << L"--------------------------------\n";
    for (size_t i = 0; i < stats.top_accessed_files.size(); ++i)
    {
        file << std::setw(3) << (i + 1) << L". " << stats.top_accessed_files[i].first << L" ("
             << stats.top_accessed_files[i].second << L" accesses)\n";
    }

    file << L"\nLargest Cached Files\n";
    file << L"--------------------\n";
    for (size_t i = 0; i < stats.largest_cached_files.size(); ++i)
    {
        file << std::setw(3) << (i + 1) << L". " << stats.largest_cached_files[i].first << L" ("
             << formatFileSize(stats.largest_cached_files[i].second) << L")\n";
    }

    file << L"\nSlowest Average Access Times\n";
    file << L"----------------------------\n";
    for (size_t i = 0; i < stats.slowest_access_files.size(); ++i)
    {
        file << std::setw(3) << (i + 1) << L". " << stats.slowest_access_files[i].first << L" (" << std::fixed
             << std::setprecision(2) << stats.slowest_access_files[i].second << L" ms)\n";
    }

        file.close();
    }
    catch (const std::exception& e)
    {
        std::wcerr << L"[FileAccessTracker] Error writing summary report: " 
                   << std::wstring(e.what(), e.what() + strlen(e.what())) << std::endl;
    }
}

FileAccessStatistics FileAccessTracker::getStatistics() const
{
    std::lock_guard<std::mutex> lock(mutex_);

    FileAccessStatistics stats;
    stats.total_files_tracked = file_access_map_.size();
    stats.total_accesses = total_accesses_.load();
    stats.total_cache_hits = total_cache_hits_.load();
    stats.total_cache_misses = total_cache_misses_.load();

    if (stats.total_accesses > 0)
    {
        stats.cache_hit_rate = (double)stats.total_cache_hits / stats.total_accesses * 100.0;
    }

    // Collect file info for sorting
    std::vector<FileAccessInfo *> all_files;
    for (const auto &[path, info] : file_access_map_)
    {
        all_files.push_back(info.get());
        stats.total_bytes_accessed += info->file_size * info->access_count.load();
        if (info->current_state == FileState::CACHED || info->is_memory_cached)
        {
            stats.cached_bytes += info->file_size;
        }
    }

    // Sort by access count for top files
    std::sort(all_files.begin(), all_files.end(),
              [](const FileAccessInfo *a, const FileAccessInfo *b)
              {
                  return a->access_count.load() > b->access_count.load();
              });

    size_t count = std::min(top_files_count_, all_files.size());
    for (size_t i = 0; i < count; ++i)
    {
        stats.top_accessed_files.emplace_back(all_files[i]->virtual_path, all_files[i]->access_count.load());
    }

    // Sort by file size for largest cached files
    std::vector<FileAccessInfo *> cached_files;
    std::copy_if(all_files.begin(), all_files.end(), std::back_inserter(cached_files),
                 [](const FileAccessInfo *info)
                 {
                     return info->current_state == FileState::CACHED || info->is_memory_cached;
                 });

    std::sort(cached_files.begin(), cached_files.end(),
              [](const FileAccessInfo *a, const FileAccessInfo *b)
              {
                  return a->file_size > b->file_size;
              });

    count = std::min(size_t(20), cached_files.size());
    for (size_t i = 0; i < count; ++i)
    {
        stats.largest_cached_files.emplace_back(cached_files[i]->virtual_path, cached_files[i]->file_size);
    }

    // Sort by access time for slowest files
    std::sort(all_files.begin(), all_files.end(),
              [](const FileAccessInfo *a, const FileAccessInfo *b)
              {
                  return a->average_access_time_ms > b->average_access_time_ms;
              });

    count = std::min(size_t(20), all_files.size());
    for (size_t i = 0; i < count; ++i)
    {
        stats.slowest_access_files.emplace_back(all_files[i]->virtual_path, all_files[i]->average_access_time_ms);
    }

    return stats;
}

std::wstring FileAccessTracker::formatFileSize(uint64_t bytes) const
{
    const wchar_t *units[] = { L"B", L"KB", L"MB", L"GB", L"TB" };
    int unit_index = 0;
    double size = static_cast<double>(bytes);

    while (size >= 1024.0 && unit_index < 4)
    {
        size /= 1024.0;
        unit_index++;
    }

    std::wostringstream oss;
    oss << std::fixed << std::setprecision(2) << size << L" " << units[unit_index];
    return oss.str();
}

std::wstring FileAccessTracker::formatDuration(std::chrono::system_clock::duration duration) const
{
    auto seconds = std::chrono::duration_cast<std::chrono::seconds>(duration).count();

    if (seconds < 60)
        return std::to_wstring(seconds) + L" seconds";
    else if (seconds < 3600)
        return std::to_wstring(seconds / 60) + L" minutes";
    else if (seconds < 86400)
        return std::to_wstring(seconds / 3600) + L" hours";
    else
        return std::to_wstring(seconds / 86400) + L" days";
}

std::wstring FileAccessTracker::getCurrentTimestamp() const
{
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);

    std::tm tm;
#ifdef _WIN32
    localtime_s(&tm, &time_t);
#else
    localtime_r(&time_t, &tm);
#endif

    std::wostringstream oss;
    oss << std::put_time(&tm, L"%Y%m%d_%H%M%S");
    return oss.str();
}

} // namespace CeWinFileCache