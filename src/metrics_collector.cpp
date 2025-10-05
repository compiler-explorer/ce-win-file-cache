#include "../include/ce-win-file-cache/metrics_collector.hpp"

#ifdef HAVE_PROMETHEUS

#include "../include/ce-win-file-cache/logger.hpp"

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

void MetricsCollector::recordCacheEvictionFailed()
{
    if (implementation)
    {
        implementation->recordCacheEvictionFailed();
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
bool GlobalMetrics::auto_initialized = false;

MetricsCollector &GlobalMetrics::instance()
{
    if (!metrics_instance && !auto_initialized)
    {
        // Auto-initialize with default config on first access
        MetricsConfig default_config;
        default_config.enabled = true;
        default_config.bind_address = "127.0.0.1";
        default_config.port = 0; // Let system choose available port
        default_config.endpoint_path = "/metrics";

        initialize(default_config);
        auto_initialized = true;
    }

    if (!metrics_instance)
    {
        // Create no-op instance if initialization failed
        static MetricsCollector no_op_metrics({});
        return no_op_metrics;
    }

    return *metrics_instance;
}

void GlobalMetrics::initialize(const MetricsConfig &config)
{
    if (config.enabled)
    {
        try
        {
            metrics_instance = std::make_unique<MetricsCollector>(config);
            CeWinFileCache::Logger::info("Global metrics initialized: {}", metrics_instance->getMetricsUrl());
        }
        catch (const std::exception &e)
        {
            CeWinFileCache::Logger::error("Failed to initialize global metrics: {}", e.what());
            metrics_instance = nullptr;
        }
    }
    else
    {
        CeWinFileCache::Logger::info("Metrics disabled in configuration");
        metrics_instance = nullptr;
    }
}

void GlobalMetrics::shutdown()
{
    if (metrics_instance)
    {
        CeWinFileCache::Logger::info("Shutting down global metrics");
        metrics_instance.reset();
    }
    auto_initialized = false;
}

} // namespace CeWinFileCache

#endif // HAVE_PROMETHEUS