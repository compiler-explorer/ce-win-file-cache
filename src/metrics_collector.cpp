#include "../include/ce-win-file-cache/metrics_collector.hpp"

#ifdef HAVE_PROMETHEUS

#include <iostream>

namespace CeWinFileCache
{

MetricsCollector::MetricsCollector(const MetricsConfig &config)
: implementation(std::make_unique<PrometheusMetricsImpl>(config))
{
}

void MetricsCollector::recordCacheHit(std::string_view operation)
{
    if (implementation)
    {
        implementation->recordCacheHit(operation);
    }
}

void MetricsCollector::recordCacheMiss(std::string_view operation)
{
    if (implementation)
    {
        implementation->recordCacheMiss(operation);
    }
}

void MetricsCollector::updateCacheSize(size_t bytes)
{
    if (implementation)
    {
        implementation->updateCacheSize(bytes);
    }
}

void MetricsCollector::updateCacheEntryCount(size_t count)
{
    if (implementation)
    {
        implementation->updateCacheEntryCount(count);
    }
}

void MetricsCollector::recordCacheEviction()
{
    if (implementation)
    {
        implementation->recordCacheEviction();
    }
}

void MetricsCollector::recordDownloadQueued()
{
    if (implementation)
    {
        implementation->recordDownloadQueued();
    }
}

void MetricsCollector::recordDownloadStarted()
{
    if (implementation)
    {
        implementation->recordDownloadStarted();
    }
}

void MetricsCollector::recordDownloadCompleted(double durationSeconds)
{
    if (implementation)
    {
        implementation->recordDownloadCompleted(durationSeconds);
    }
}

void MetricsCollector::recordDownloadFailed(std::string_view reason)
{
    if (implementation)
    {
        implementation->recordDownloadFailed(reason);
    }
}

void MetricsCollector::updateActiveDownloads(size_t count)
{
    if (implementation)
    {
        implementation->updateActiveDownloads(count);
    }
}

void MetricsCollector::updatePendingDownloads(size_t count)
{
    if (implementation)
    {
        implementation->updatePendingDownloads(count);
    }
}

void MetricsCollector::recordFilesystemOperation(std::string_view operation)
{
    if (implementation)
    {
        implementation->recordFilesystemOperation(operation);
    }
}

void MetricsCollector::recordFileOpenDuration(double durationSeconds)
{
    if (implementation)
    {
        implementation->recordFileOpenDuration(durationSeconds);
    }
}

void MetricsCollector::recordNetworkOperation(std::string_view operation, bool success)
{
    if (implementation)
    {
        implementation->recordNetworkOperation(operation, success);
    }
}

void MetricsCollector::recordNetworkLatency(double durationSeconds)
{
    if (implementation)
    {
        implementation->recordNetworkLatency(durationSeconds);
    }
}

std::string MetricsCollector::getMetricsUrl() const
{
    if (implementation)
    {
        return implementation->getMetricsUrl();
    }
    return "metrics not available";
}

// Global metrics singleton implementation
std::unique_ptr<MetricsCollector> GlobalMetrics::metrics_instance = nullptr;

void GlobalMetrics::initialize(const MetricsConfig &config)
{
    if (config.enabled)
    {
        try
        {
            metrics_instance = std::make_unique<MetricsCollector>(config);
            std::cout << "Global metrics initialized: " << metrics_instance->getMetricsUrl() << std::endl;
        }
        catch (const std::exception &e)
        {
            std::cerr << "Failed to initialize global metrics: " << e.what() << std::endl;
            metrics_instance = nullptr;
        }
    }
    else
    {
        std::cout << "Metrics disabled in configuration" << std::endl;
    }
}

void GlobalMetrics::shutdown()
{
    if (metrics_instance)
    {
        std::cout << "Shutting down global metrics" << std::endl;
        metrics_instance.reset();
    }
}

MetricsCollector *GlobalMetrics::instance()
{
    return metrics_instance.get();
}

} // namespace CeWinFileCache

#endif // HAVE_PROMETHEUS