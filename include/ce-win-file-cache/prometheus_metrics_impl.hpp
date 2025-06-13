#pragma once

#ifdef HAVE_PROMETHEUS

#include "../types/config.hpp"
#include <memory>
#include <string>

// Forward declarations
namespace prometheus {
    class Registry;
    class Exposer;
    template<typename T>
    class Family;
    class Counter;
    class Gauge;
    class Histogram;
}

namespace CeWinFileCache
{

class PrometheusMetricsImpl
{
public:
    explicit PrometheusMetricsImpl(const MetricsConfig& config);
    ~PrometheusMetricsImpl();
    
    // Cache metrics
    void recordCacheHit(const std::string& operation);
    void recordCacheMiss(const std::string& operation);
    void updateCacheSize(size_t bytes);
    void updateCacheEntryCount(size_t count);
    void recordCacheEviction();

    // Download metrics
    void recordDownloadQueued();
    void recordDownloadStarted();
    void recordDownloadCompleted(double durationSeconds);
    void recordDownloadFailed(const std::string& reason);
    void updateActiveDownloads(size_t count);
    void updatePendingDownloads(size_t count);

    // Filesystem metrics
    void recordFilesystemOperation(const std::string& operation);
    void recordFileOpenDuration(double durationSeconds);

    // Network metrics
    void recordNetworkOperation(const std::string& operation, bool success);
    void recordNetworkLatency(double durationSeconds);

    // Configuration
    std::string getMetricsUrl() const;

private:
    MetricsConfig config;
    std::unique_ptr<prometheus::Exposer> exposer;
    std::shared_ptr<prometheus::Registry> registry;
    
    // Cache metrics
    prometheus::Family<prometheus::Counter>* cacheHitsFamily;
    prometheus::Family<prometheus::Counter>* cacheMissesFamily;
    prometheus::Gauge* cacheSizeBytes;
    prometheus::Gauge* cacheEntriesTotal;
    prometheus::Counter* cacheEvictionsTotal;
    
    // Download metrics
    prometheus::Counter* downloadsQueuedTotal;
    prometheus::Counter* downloadsCompletedTotal;
    prometheus::Family<prometheus::Counter>* downloadsFailedTotalFamily;
    prometheus::Gauge* activeDownloads;
    prometheus::Gauge* pendingDownloads;
    prometheus::Histogram* downloadDurationSeconds;
    
    // Filesystem metrics
    prometheus::Family<prometheus::Counter>* filesystemOperationsTotalFamily;
    prometheus::Histogram* fileOpenDurationSeconds;
    
    // Network metrics
    prometheus::Family<prometheus::Counter>* networkOperationsTotalFamily;
    prometheus::Histogram* networkLatencySeconds;
};

} // namespace CeWinFileCache

#endif // HAVE_PROMETHEUS