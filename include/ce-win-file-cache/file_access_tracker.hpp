#pragma once

#include "../types/file_access_info.hpp"
#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace CeWinFileCache
{

class FileAccessTracker
{
    public:
    FileAccessTracker();
    ~FileAccessTracker();

    // Initialize with report configuration
    void initialize(const std::wstring &report_directory,
                    std::chrono::minutes report_interval = std::chrono::minutes(5),
                    size_t top_files_count = 100);

    // Record file access
    void recordAccess(const std::wstring virtual_path,
                      const std::wstring network_path,
                      uint64_t file_size,
                      FileState state,
                      bool is_cache_hit,
                      bool is_memory_cached,
                      double access_time_ms,
                      const std::wstring &cache_policy);

    // Start/stop periodic reporting
    void startReporting();
    void stopReporting();

    // Generate report on demand
    void generateReport();

    // Get current statistics
    FileAccessStatistics getStatistics() const;

    private:
    void reportingThreadFunc();
    void writeCSVReport(const std::wstring &filename);
    void writeSummaryReport(const std::wstring &filename);

    // Helper to get or create FileAccessInfo (prevents use-after-move)
    FileAccessInfo *getOrCreateAccessInfo(const std::wstring &virtual_path,
                                          const std::wstring &network_path,
                                          uint64_t file_size,
                                          const std::wstring &cache_policy);

    std::wstring formatFileSize(uint64_t bytes) const;
    std::wstring formatDuration(std::chrono::system_clock::duration duration) const;
    std::wstring getCurrentTimestamp() const;

    mutable std::mutex mutex_{};
    std::unordered_map<std::wstring, std::unique_ptr<FileAccessInfo>> file_access_map_;

    std::wstring report_directory_{};
    std::chrono::minutes report_interval_{};
    size_t top_files_count_{};

    std::atomic<bool> reporting_enabled_{ false };
    std::unique_ptr<std::thread> reporting_thread_{};

    std::chrono::system_clock::time_point tracking_start_time_{};
    std::atomic<uint64_t> total_accesses_{ 0 };
    std::atomic<uint64_t> total_cache_hits_{ 0 };
    std::atomic<uint64_t> total_cache_misses_{ 0 };
};

} // namespace CeWinFileCache