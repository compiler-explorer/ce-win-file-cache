#include "../include/ce-win-file-cache/prometheus_metrics_impl.hpp"

#ifdef HAVE_PROMETHEUS

#include "../include/ce-win-file-cache/logger.hpp"
#include <prometheus/counter.h>
#include <prometheus/exposer.h>
#include <prometheus/gauge.h>
#include <prometheus/histogram.h>
#include <prometheus/registry.h>

namespace CeWinFileCache
{

PrometheusMetricsImpl::PrometheusMetricsImpl(const MetricsConfig &config) : config(config)
{
    try
    {
        // Create HTTP exposer for metrics endpoint
        std::string bindAddr = config.bind_address + ":" + std::to_string(config.port);
        exposer = std::make_unique<prometheus::Exposer>(bindAddr, 2); // bind_address, num_threads

        // Create registry for metrics
        registry = std::make_shared<prometheus::Registry>();

        // Register the registry with the exposer (using default endpoint path)
        exposer->RegisterCollectable(registry, config.endpoint_path);

        // Initialize cache metrics
        cacheHitsFamily =
        &prometheus::BuildCounter().Name("cache_hits_total").Help("Total number of cache hits").Register(*registry);

        cacheMissesFamily =
        &prometheus::BuildCounter().Name("cache_misses_total").Help("Total number of cache misses").Register(*registry);

        cacheSizeBytes =
        &prometheus::BuildGauge().Name("cache_size_bytes").Help("Current cache size in bytes").Register(*registry).Add({});

        cacheEntriesTotal = &prometheus::BuildGauge()
                             .Name("cache_entries_total")
                             .Help("Current number of cached entries")
                             .Register(*registry)
                             .Add({});

        cacheEvictionsTotal = &prometheus::BuildCounter()
                               .Name("cache_evictions_total")
                               .Help("Total number of cache evictions")
                               .Register(*registry)
                               .Add({});

        // Initialize download metrics
        downloadsQueuedTotal = &prometheus::BuildCounter()
                                .Name("downloads_queued_total")
                                .Help("Total number of downloads queued")
                                .Register(*registry)
                                .Add({});

        downloadsCompletedTotal = &prometheus::BuildCounter()
                                   .Name("downloads_completed_total")
                                   .Help("Total number of downloads completed successfully")
                                   .Register(*registry)
                                   .Add({});

        downloadsFailedTotalFamily =
        &prometheus::BuildCounter().Name("downloads_failed_total").Help("Total number of downloads that failed").Register(*registry);

        activeDownloads = &prometheus::BuildGauge()
                           .Name("active_downloads")
                           .Help("Current number of active downloads")
                           .Register(*registry)
                           .Add({});

        pendingDownloads = &prometheus::BuildGauge()
                            .Name("pending_downloads")
                            .Help("Current number of pending downloads")
                            .Register(*registry)
                            .Add({});

        // Initialize download duration histogram with buckets
        auto downloadDurationBuckets =
        prometheus::Histogram::BucketBoundaries{ 0.01, 0.05, 0.1, 0.25, 0.5, 1.0, 2.5, 5.0, 10.0, 30.0, 60.0 };
        downloadDurationSeconds = &prometheus::BuildHistogram()
                                   .Name("download_duration_seconds")
                                   .Help("Time taken to download files")
                                   .Register(*registry)
                                   .Add({}, downloadDurationBuckets);

        // Initialize filesystem metrics
        filesystemOperationsTotalFamily =
        &prometheus::BuildCounter().Name("filesystem_operations_total").Help("Total number of filesystem operations").Register(*registry);

        // Initialize file open duration histogram
        auto fileOpenBuckets =
        prometheus::Histogram::BucketBoundaries{ 0.001, 0.005, 0.01, 0.025, 0.05, 0.1, 0.25, 0.5, 1.0, 2.0 };
        fileOpenDurationSeconds = &prometheus::BuildHistogram()
                                   .Name("file_open_duration_seconds")
                                   .Help("Time taken to open files")
                                   .Register(*registry)
                                   .Add({}, fileOpenBuckets);

        // Initialize network metrics
        networkOperationsTotalFamily =
        &prometheus::BuildCounter().Name("network_operations_total").Help("Total number of network operations").Register(*registry);

        auto networkLatencyBuckets =
        prometheus::Histogram::BucketBoundaries{ 0.001, 0.005, 0.01, 0.025, 0.05, 0.1, 0.25, 0.5, 1.0, 2.0, 5.0 };
        networkLatencySeconds = &prometheus::BuildHistogram()
                                 .Name("network_latency_seconds")
                                 .Help("Network operation latency")
                                 .Register(*registry)
                                 .Add({}, networkLatencyBuckets);

        CeWinFileCache::Logger::info("Metrics server started on {}{}", bindAddr, config.endpoint_path);
    }
    catch (const std::exception &e)
    {
        CeWinFileCache::Logger::error("Failed to initialize metrics: {}", e.what());
        throw;
    }
}

PrometheusMetricsImpl::~PrometheusMetricsImpl() = default;

void PrometheusMetricsImpl::recordCacheHit(std::string_view operation)
{
    if (cacheHitsFamily)
    {
        auto &counter = cacheHitsFamily->Add({ { "operation", std::string(operation) } });
        counter.Increment();
    }
}

void PrometheusMetricsImpl::recordCacheMiss(std::string_view operation)
{
    if (cacheMissesFamily)
    {
        auto &counter = cacheMissesFamily->Add({ { "operation", std::string(operation) } });
        counter.Increment();
    }
}

void PrometheusMetricsImpl::updateCacheSize(size_t bytes)
{
    if (cacheSizeBytes)
    {
        cacheSizeBytes->Set(static_cast<double>(bytes));
    }
}

void PrometheusMetricsImpl::updateCacheEntryCount(size_t count)
{
    if (cacheEntriesTotal)
    {
        cacheEntriesTotal->Set(static_cast<double>(count));
    }
}

void PrometheusMetricsImpl::recordCacheEviction()
{
    if (cacheEvictionsTotal)
    {
        cacheEvictionsTotal->Increment();
    }
}

void PrometheusMetricsImpl::recordDownloadQueued()
{
    if (downloadsQueuedTotal)
    {
        downloadsQueuedTotal->Increment();
    }
}

void PrometheusMetricsImpl::recordDownloadStarted()
{
    // This is tracked via updateActiveDownloads and updatePendingDownloads
}

void PrometheusMetricsImpl::recordDownloadCompleted(double durationSeconds)
{
    if (downloadsCompletedTotal)
    {
        downloadsCompletedTotal->Increment();
    }
    if (downloadDurationSeconds)
    {
        downloadDurationSeconds->Observe(durationSeconds);
    }
}

void PrometheusMetricsImpl::recordDownloadFailed(std::string_view reason)
{
    if (downloadsFailedTotalFamily)
    {
        auto &counter = downloadsFailedTotalFamily->Add({ { "reason", std::string(reason) } });
        counter.Increment();
    }
}

void PrometheusMetricsImpl::updateActiveDownloads(size_t count)
{
    if (activeDownloads)
    {
        activeDownloads->Set(static_cast<double>(count));
    }
}

void PrometheusMetricsImpl::updatePendingDownloads(size_t count)
{
    if (pendingDownloads)
    {
        pendingDownloads->Set(static_cast<double>(count));
    }
}

void PrometheusMetricsImpl::recordFilesystemOperation(std::string_view operation)
{
    if (filesystemOperationsTotalFamily)
    {
        auto &counter = filesystemOperationsTotalFamily->Add({ { "operation", std::string(operation) } });
        counter.Increment();
    }
}

void PrometheusMetricsImpl::recordFileOpenDuration(double durationSeconds)
{
    if (fileOpenDurationSeconds)
    {
        fileOpenDurationSeconds->Observe(durationSeconds);
    }
}

void PrometheusMetricsImpl::recordNetworkOperation(std::string_view operation, bool success)
{
    if (networkOperationsTotalFamily)
    {
        std::string status = success ? "success" : "failure";
        auto &counter = networkOperationsTotalFamily->Add({ { "operation", std::string(operation) }, { "status", status } });
        counter.Increment();
    }
}

void PrometheusMetricsImpl::recordNetworkLatency(double durationSeconds)
{
    if (networkLatencySeconds)
    {
        networkLatencySeconds->Observe(durationSeconds);
    }
}

std::string PrometheusMetricsImpl::getMetricsUrl() const
{
    return "http://" + config.bind_address + ":" + std::to_string(config.port) + config.endpoint_path;
}

} // namespace CeWinFileCache

#endif // HAVE_PROMETHEUS