#include "../include/ce-win-file-cache/metrics_collector.hpp"

#ifdef HAVE_PROMETHEUS

#include <prometheus/counter.h>
#include <prometheus/exposer.h>
#include <prometheus/gauge.h>
#include <prometheus/histogram.h>
#include <prometheus/registry.h>
#include <iostream>
#include <sstream>

namespace CeWinFileCache
{

class MetricsCollector::Impl
{
    public:
    Impl(const MetricsConfig &config) : config_(config)
    {
        try
        {
            // Create HTTP exposer for metrics endpoint
            std::string bind_addr = config_.bind_address + ":" + std::to_string(config_.port);
            exposer_ = std::make_unique<prometheus::Exposer>(bind_addr, 2);  // bind_address, num_threads
            
            // Create registry for metrics
            registry_ = std::make_shared<prometheus::Registry>();
            
            // Register the registry with the exposer (using default endpoint path)
            exposer_->RegisterCollectable(registry_, config_.endpoint_path);
            
            // Initialize cache metrics
            cache_hits_family_ = &prometheus::BuildCounter()
                                .Name("cache_hits_total")
                                .Help("Total number of cache hits")
                                .Register(*registry_);
                          
            cache_misses_family_ = &prometheus::BuildCounter()
                                 .Name("cache_misses_total")
                                 .Help("Total number of cache misses")
                                 .Register(*registry_);
                           
            cache_size_bytes_ = &prometheus::BuildGauge()
                               .Name("cache_size_bytes")
                               .Help("Current cache size in bytes")
                               .Register(*registry_)
                               .Add({});
                               
            cache_entries_total_ = &prometheus::BuildGauge()
                                  .Name("cache_entries_total")
                                  .Help("Current number of cached entries")
                                  .Register(*registry_)
                                  .Add({});
                                  
            cache_evictions_total_ = &prometheus::BuildCounter()
                                    .Name("cache_evictions_total")
                                    .Help("Total number of cache evictions")
                                    .Register(*registry_)
                                    .Add({});
            
            // Initialize download metrics
            downloads_queued_total_ = &prometheus::BuildCounter()
                                     .Name("downloads_queued_total")
                                     .Help("Total number of downloads queued")
                                     .Register(*registry_)
                                     .Add({});
                                     
            downloads_completed_total_ = &prometheus::BuildCounter()
                                        .Name("downloads_completed_total")
                                        .Help("Total number of downloads completed successfully")
                                        .Register(*registry_)
                                        .Add({});
                                        
            downloads_failed_total_family_ = &prometheus::BuildCounter()
                                            .Name("downloads_failed_total")
                                            .Help("Total number of downloads that failed")
                                            .Register(*registry_);
                                     
            active_downloads_ = &prometheus::BuildGauge()
                               .Name("active_downloads")
                               .Help("Current number of active downloads")
                               .Register(*registry_)
                               .Add({});
                               
            pending_downloads_ = &prometheus::BuildGauge()
                                 .Name("pending_downloads")
                                 .Help("Current number of pending downloads")
                                 .Register(*registry_)
                                 .Add({});
            
            // Initialize download duration histogram with buckets
            auto download_duration_buckets = prometheus::Histogram::BucketBoundaries{
                0.01, 0.05, 0.1, 0.25, 0.5, 1.0, 2.5, 5.0, 10.0, 30.0, 60.0
            };
            download_duration_seconds_ = &prometheus::BuildHistogram()
                                        .Name("download_duration_seconds")
                                        .Help("Time taken to download files")
                                        .Register(*registry_)
                                        .Add({}, download_duration_buckets);
            
            // Initialize filesystem metrics
            filesystem_operations_total_family_ = &prometheus::BuildCounter()
                                                 .Name("filesystem_operations_total")
                                                 .Help("Total number of filesystem operations")
                                                 .Register(*registry_);
            
            // Initialize file open duration histogram
            auto file_open_buckets = prometheus::Histogram::BucketBoundaries{
                0.001, 0.005, 0.01, 0.025, 0.05, 0.1, 0.25, 0.5, 1.0, 2.0
            };
            file_open_duration_seconds_ = &prometheus::BuildHistogram()
                                         .Name("file_open_duration_seconds")
                                         .Help("Time taken to open files")
                                         .Register(*registry_)
                                         .Add({}, file_open_buckets);
            
            // Initialize network metrics
            network_operations_total_family_ = &prometheus::BuildCounter()
                                              .Name("network_operations_total")
                                              .Help("Total number of network operations")
                                              .Register(*registry_);
            
            auto network_latency_buckets = prometheus::Histogram::BucketBoundaries{
                0.001, 0.005, 0.01, 0.025, 0.05, 0.1, 0.25, 0.5, 1.0, 2.0, 5.0
            };
            network_latency_seconds_ = &prometheus::BuildHistogram()
                                      .Name("network_latency_seconds")
                                      .Help("Network operation latency")
                                      .Register(*registry_)
                                      .Add({}, network_latency_buckets);
            
            std::cout << "Metrics server started on " << bind_addr << config_.endpoint_path << std::endl;
        }
        catch (const std::exception &e)
        {
            std::cerr << "Failed to initialize metrics: " << e.what() << std::endl;
            throw;
        }
    }
    
    ~Impl() = default;
    
    MetricsConfig config_;
    std::unique_ptr<prometheus::Exposer> exposer_;
    std::shared_ptr<prometheus::Registry> registry_;
    
    // Cache metrics
    prometheus::Family<prometheus::Counter> *cache_hits_family_;
    prometheus::Family<prometheus::Counter> *cache_misses_family_;
    prometheus::Gauge *cache_size_bytes_;
    prometheus::Gauge *cache_entries_total_;
    prometheus::Counter *cache_evictions_total_;
    
    // Download metrics
    prometheus::Counter *downloads_queued_total_;
    prometheus::Counter *downloads_completed_total_;
    prometheus::Family<prometheus::Counter> *downloads_failed_total_family_;
    prometheus::Gauge *active_downloads_;
    prometheus::Gauge *pending_downloads_;
    prometheus::Histogram *download_duration_seconds_;
    
    // Filesystem metrics
    prometheus::Family<prometheus::Counter> *filesystem_operations_total_family_;
    prometheus::Histogram *file_open_duration_seconds_;
    
    // Network metrics
    prometheus::Family<prometheus::Counter> *network_operations_total_family_;
    prometheus::Histogram *network_latency_seconds_;
};

MetricsCollector::MetricsCollector(const MetricsConfig &config) : pimpl(std::make_unique<Impl>(config))
{
}

MetricsCollector::~MetricsCollector() = default;

void MetricsCollector::recordCacheHit(const std::string &operation)
{
    if (pimpl && pimpl->cache_hits_family_)
    {
        auto& counter = pimpl->cache_hits_family_->Add({{"operation", operation}});
        counter.Increment();
    }
}

void MetricsCollector::recordCacheMiss(const std::string &operation)
{
    if (pimpl && pimpl->cache_misses_family_)
    {
        auto& counter = pimpl->cache_misses_family_->Add({{"operation", operation}});
        counter.Increment();
    }
}

void MetricsCollector::updateCacheSize(size_t bytes)
{
    if (pimpl && pimpl->cache_size_bytes_)
    {
        pimpl->cache_size_bytes_->Set(static_cast<double>(bytes));
    }
}

void MetricsCollector::updateCacheEntryCount(size_t count)
{
    if (pimpl && pimpl->cache_entries_total_)
    {
        pimpl->cache_entries_total_->Set(static_cast<double>(count));
    }
}

void MetricsCollector::recordCacheEviction()
{
    if (pimpl && pimpl->cache_evictions_total_)
    {
        pimpl->cache_evictions_total_->Increment();
    }
}

void MetricsCollector::recordDownloadQueued()
{
    if (pimpl && pimpl->downloads_queued_total_)
    {
        pimpl->downloads_queued_total_->Increment();
    }
}

void MetricsCollector::recordDownloadStarted()
{
    // This is tracked via updateActiveDownloads and updatePendingDownloads
}

void MetricsCollector::recordDownloadCompleted(double duration_seconds)
{
    if (pimpl)
    {
        if (pimpl->downloads_completed_total_)
        {
            pimpl->downloads_completed_total_->Increment();
        }
        if (pimpl->download_duration_seconds_)
        {
            pimpl->download_duration_seconds_->Observe(duration_seconds);
        }
    }
}

void MetricsCollector::recordDownloadFailed(const std::string &reason)
{
    if (pimpl && pimpl->downloads_failed_total_family_)
    {
        auto& counter = pimpl->downloads_failed_total_family_->Add({{"reason", reason}});
        counter.Increment();
    }
}

void MetricsCollector::updateActiveDownloads(size_t count)
{
    if (pimpl && pimpl->active_downloads_)
    {
        pimpl->active_downloads_->Set(static_cast<double>(count));
    }
}

void MetricsCollector::updatePendingDownloads(size_t count)
{
    if (pimpl && pimpl->pending_downloads_)
    {
        pimpl->pending_downloads_->Set(static_cast<double>(count));
    }
}

void MetricsCollector::recordFilesystemOperation(const std::string &operation)
{
    if (pimpl && pimpl->filesystem_operations_total_family_)
    {
        auto& counter = pimpl->filesystem_operations_total_family_->Add({{"operation", operation}});
        counter.Increment();
    }
}

void MetricsCollector::recordFileOpenDuration(double duration_seconds)
{
    if (pimpl && pimpl->file_open_duration_seconds_)
    {
        pimpl->file_open_duration_seconds_->Observe(duration_seconds);
    }
}

void MetricsCollector::recordNetworkOperation(const std::string &operation, bool success)
{
    if (pimpl && pimpl->network_operations_total_family_)
    {
        std::string status = success ? "success" : "failure";
        auto& counter = pimpl->network_operations_total_family_->Add({{"operation", operation}, {"status", status}});
        counter.Increment();
    }
}

void MetricsCollector::recordNetworkLatency(double duration_seconds)
{
    if (pimpl && pimpl->network_latency_seconds_)
    {
        pimpl->network_latency_seconds_->Observe(duration_seconds);
    }
}

std::string MetricsCollector::getMetricsUrl() const
{
    if (pimpl)
    {
        return "http://" + pimpl->config_.bind_address + ":" + std::to_string(pimpl->config_.port) + pimpl->config_.endpoint_path;
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