#pragma once

#include "../types/config.hpp"

#ifdef HAVE_PROMETHEUS

#include "prometheus_metrics_impl.hpp"
#include <memory>
#include <string>
#include <string_view>

namespace CeWinFileCache
{

class MetricsCollector
{
    public:
    explicit MetricsCollector(const MetricsConfig &config);
    ~MetricsCollector() = default;

    // Disable copy/move for simplicity
    MetricsCollector(const MetricsCollector &) = delete;
    MetricsCollector &operator=(const MetricsCollector &) = delete;
    MetricsCollector(MetricsCollector &&) = delete;
    MetricsCollector &operator=(MetricsCollector &&) = delete;

    // Cache metrics
    void recordCacheHit(std::string_view operation = "read");
    void recordCacheMiss(std::string_view operation = "read");
    void updateCacheSize(size_t bytes);
    void updateCacheEntryCount(size_t count);
    void recordCacheEviction();
    void recordCacheEvictionFailed();

    // Download metrics
    void recordDownloadQueued();
    void recordDownloadStarted();
    void recordDownloadCompleted(double durationSeconds);
    void recordDownloadFailed(std::string_view reason = "unknown");
    void updateActiveDownloads(size_t count);
    void updatePendingDownloads(size_t count);

    // Filesystem metrics
    void recordFilesystemOperation(std::string_view operation);
    void recordFileOpenDuration(double durationSeconds);

    // Network metrics
    void recordNetworkOperation(std::string_view operation, bool success);
    void recordNetworkLatency(double durationSeconds);

    // Get metrics endpoint URL
    std::string getMetricsUrl() const;

    private:
    std::unique_ptr<PrometheusMetricsImpl> implementation;
};

// Global metrics instance (optional singleton pattern)
class GlobalMetrics
{
    public:
    static void initialize(const MetricsConfig &config);
    static void shutdown();
    static MetricsCollector &instance();

    private:
    static std::unique_ptr<MetricsCollector> metrics_instance;
    static bool auto_initialized;
};

} // namespace CeWinFileCache

#else // !HAVE_PROMETHEUS

#include "../types/config.hpp"

// Stub implementation when Prometheus is not available
namespace CeWinFileCache
{

class MetricsCollector
{
    public:
    explicit MetricsCollector(const MetricsConfig &)
    {
    }
    ~MetricsCollector() = default;

    // Stub methods - no-op when metrics disabled
    void recordCacheHit(std::string_view = "read")
    {
    }
    void recordCacheMiss(std::string_view = "read")
    {
    }
    void updateCacheSize(size_t)
    {
    }
    void updateCacheEntryCount(size_t)
    {
    }
    void recordCacheEviction()
    {
    }
    void recordCacheEvictionFailed()
    {
    }

    void recordDownloadQueued()
    {
    }
    void recordDownloadStarted()
    {
    }
    void recordDownloadCompleted(double)
    {
    }
    void recordDownloadFailed(std::string_view = "unknown")
    {
    }
    void updateActiveDownloads(size_t)
    {
    }
    void updatePendingDownloads(size_t)
    {
    }

    void recordFilesystemOperation(std::string_view)
    {
    }
    void recordFileOpenDuration(double)
    {
    }

    void recordNetworkOperation(std::string_view, bool)
    {
    }
    void recordNetworkLatency(double)
    {
    }

    std::string getMetricsUrl() const
    {
        return "metrics disabled";
    }
};

class GlobalMetrics
{
    public:
    static void initialize(const MetricsConfig &)
    {
    }
    static void shutdown()
    {
    }
    static MetricsCollector &instance()
    {
        static MetricsCollector stub_metrics({});
        return stub_metrics;
    }
};

} // namespace CeWinFileCache

#endif // HAVE_PROMETHEUS