#pragma once

#include "../types/config.hpp"

#ifdef HAVE_PROMETHEUS

#include "prometheus_metrics_impl.hpp"
#include <memory>
#include <string>

namespace CeWinFileCache
{

class MetricsCollector
{
public:
    explicit MetricsCollector(const MetricsConfig& config);
    ~MetricsCollector() = default;

    // Disable copy/move for simplicity
    MetricsCollector(const MetricsCollector&) = delete;
    MetricsCollector& operator=(const MetricsCollector&) = delete;
    MetricsCollector(MetricsCollector&&) = delete;
    MetricsCollector& operator=(MetricsCollector&&) = delete;

    // Cache metrics
    void recordCacheHit(const std::string& operation = "read");
    void recordCacheMiss(const std::string& operation = "read");
    void updateCacheSize(size_t bytes);
    void updateCacheEntryCount(size_t count);
    void recordCacheEviction();

    // Download metrics
    void recordDownloadQueued();
    void recordDownloadStarted();
    void recordDownloadCompleted(double durationSeconds);
    void recordDownloadFailed(const std::string& reason = "unknown");
    void updateActiveDownloads(size_t count);
    void updatePendingDownloads(size_t count);

    // Filesystem metrics
    void recordFilesystemOperation(const std::string& operation);
    void recordFileOpenDuration(double durationSeconds);

    // Network metrics
    void recordNetworkOperation(const std::string& operation, bool success);
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
    static void initialize(const MetricsConfig& config);
    static void shutdown();
    static MetricsCollector* instance();

    private:
    static std::unique_ptr<MetricsCollector> metrics_instance;
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
    explicit MetricsCollector(const MetricsConfig&) {}
    ~MetricsCollector() = default;

    // Stub methods - no-op when metrics disabled
    void recordCacheHit(const std::string& = "read") {}
    void recordCacheMiss(const std::string& = "read") {}
    void updateCacheSize(size_t) {}
    void updateCacheEntryCount(size_t) {}
    void recordCacheEviction() {}

    void recordDownloadQueued() {}
    void recordDownloadStarted() {}
    void recordDownloadCompleted(double) {}
    void recordDownloadFailed(const std::string& = "unknown") {}
    void updateActiveDownloads(size_t) {}
    void updatePendingDownloads(size_t) {}

    void recordFilesystemOperation(const std::string&) {}
    void recordFileOpenDuration(double) {}

    void recordNetworkOperation(const std::string&, bool) {}
    void recordNetworkLatency(double) {}

    std::string getMetricsUrl() const { return "metrics disabled"; }
};

class GlobalMetrics
{
    public:
    static void initialize(const MetricsConfig&) {}
    static void shutdown() {}
    static MetricsCollector* instance() { return nullptr; }
};

} // namespace CeWinFileCache

#endif // HAVE_PROMETHEUS