#pragma once

#include "../types/config.hpp"

#ifdef HAVE_PROMETHEUS

#include <memory>
#include <string>
#include <chrono>

// Forward declarations to avoid including prometheus headers in public interface
namespace prometheus {
    class Registry;
    class Exposer;
    class Counter;
    class Gauge;
    class Histogram;
}

namespace CeWinFileCache
{

class MetricsCollector
{
    public:
    explicit MetricsCollector(const MetricsConfig& config);
    ~MetricsCollector();

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
    void recordDownloadCompleted(double duration_seconds);
    void recordDownloadFailed(const std::string& reason = "unknown");
    void updateActiveDownloads(size_t count);
    void updatePendingDownloads(size_t count);

    // Filesystem metrics
    void recordFilesystemOperation(const std::string& operation);
    void recordFileOpenDuration(double duration_seconds);

    // Network metrics
    void recordNetworkOperation(const std::string& operation, bool success);
    void recordNetworkLatency(double duration_seconds);

    // Get metrics endpoint URL
    std::string getMetricsUrl() const;

    private:
    class Impl;
    std::unique_ptr<Impl> pimpl;
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