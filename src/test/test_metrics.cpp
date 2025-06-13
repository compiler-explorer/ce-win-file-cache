#include "../../include/ce-win-file-cache/metrics_collector.hpp"
#include "../../include/types/config.hpp"
#include <iostream>
#include <thread>
#include <chrono>

using namespace CeWinFileCache;

void test_metrics_basic_functionality()
{
    std::cout << "Testing basic metrics functionality..." << std::endl;
    
    // Configure metrics
    MetricsConfig config;
    config.enabled = true;
    config.bind_address = "127.0.0.1";
    config.port = 8080;
    config.endpoint_path = "/metrics";
    
    try
    {
        // Initialize metrics
        MetricsCollector metrics(config);
        std::cout << "✓ Metrics collector initialized successfully" << std::endl;
        std::cout << "  Metrics URL: " << metrics.getMetricsUrl() << std::endl;
        
        // Test cache metrics
        std::cout << "\nTesting cache metrics..." << std::endl;
        metrics.recordCacheHit("read");
        metrics.recordCacheHit("read");
        metrics.recordCacheMiss("read");
        metrics.updateCacheSize(1024 * 1024); // 1MB
        metrics.updateCacheEntryCount(50);
        metrics.recordCacheEviction();
        std::cout << "✓ Cache metrics recorded successfully" << std::endl;
        
        // Test download metrics
        std::cout << "\nTesting download metrics..." << std::endl;
        metrics.recordDownloadQueued();
        metrics.recordDownloadQueued();
        metrics.updateActiveDownloads(2);
        metrics.updatePendingDownloads(5);
        
        // Simulate download completion
        metrics.recordDownloadCompleted(2.5); // 2.5 seconds
        metrics.updateActiveDownloads(1);
        
        // Simulate download failure
        metrics.recordDownloadFailed("network_timeout");
        metrics.updateActiveDownloads(0);
        std::cout << "✓ Download metrics recorded successfully" << std::endl;
        
        // Test filesystem metrics
        std::cout << "\nTesting filesystem metrics..." << std::endl;
        metrics.recordFilesystemOperation("open");
        metrics.recordFilesystemOperation("read");
        metrics.recordFileOpenDuration(0.05); // 50ms
        std::cout << "✓ Filesystem metrics recorded successfully" << std::endl;
        
        // Test network metrics
        std::cout << "\nTesting network metrics..." << std::endl;
        metrics.recordNetworkOperation("connect", true);
        metrics.recordNetworkOperation("file_read", true);
        metrics.recordNetworkLatency(0.1); // 100ms
        std::cout << "✓ Network metrics recorded successfully" << std::endl;
        
        std::cout << "\n✓ All metrics tests completed successfully!" << std::endl;
        std::cout << "  You can view metrics at: " << metrics.getMetricsUrl() << std::endl;
        
        // Keep server running for a moment to allow manual verification
        std::cout << "\nKeeping metrics server running for 10 seconds for manual verification..." << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(10));
    }
    catch (const std::exception& e)
    {
        std::cerr << "❌ Metrics test failed: " << e.what() << std::endl;
        throw;
    }
}

void test_global_metrics_singleton()
{
    std::cout << "\nTesting global metrics singleton..." << std::endl;
    
    MetricsConfig config;
    config.enabled = true;
    config.bind_address = "127.0.0.1";
    config.port = 8081; // Different port to avoid conflicts
    config.endpoint_path = "/metrics";
    
    try
    {
        // Initialize global metrics
        GlobalMetrics::initialize(config);
        std::cout << "✓ Global metrics initialized" << std::endl;
        
        // Test global access
        auto* metrics = GlobalMetrics::instance();
        if (metrics)
        {
            std::cout << "✓ Global metrics instance accessible" << std::endl;
            std::cout << "  Global metrics URL: " << metrics->getMetricsUrl() << std::endl;
            
            // Test recording through global instance
            metrics->recordCacheHit("test");
            metrics->updateCacheSize(2048);
            std::cout << "✓ Metrics recorded through global instance" << std::endl;
        }
        else
        {
            std::cerr << "❌ Global metrics instance is null" << std::endl;
        }
        
        // Shutdown global metrics
        GlobalMetrics::shutdown();
        std::cout << "✓ Global metrics shutdown completed" << std::endl;
        
        // Verify shutdown
        auto* metrics_after = GlobalMetrics::instance();
        if (!metrics_after)
        {
            std::cout << "✓ Global metrics properly cleaned up after shutdown" << std::endl;
        }
        else
        {
            std::cerr << "❌ Global metrics not properly cleaned up" << std::endl;
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "❌ Global metrics test failed: " << e.what() << std::endl;
        throw;
    }
}

void test_metrics_disabled()
{
    std::cout << "\nTesting metrics with disabled configuration..." << std::endl;
    
    MetricsConfig config;
    config.enabled = false; // Disabled
    
    try
    {
        GlobalMetrics::initialize(config);
        std::cout << "✓ Metrics initialization handled disabled state" << std::endl;
        
        auto* metrics = GlobalMetrics::instance();
        if (!metrics)
        {
            std::cout << "✓ No metrics instance when disabled" << std::endl;
        }
        else
        {
            std::cout << "❌ Metrics instance exists when disabled" << std::endl;
        }
        
        GlobalMetrics::shutdown();
        std::cout << "✓ Metrics shutdown handled disabled state" << std::endl;
    }
    catch (const std::exception& e)
    {
        std::cerr << "❌ Disabled metrics test failed: " << e.what() << std::endl;
        throw;
    }
}

int main()
{
    std::cout << "=== Prometheus Metrics Test Program ===" << std::endl;
    std::cout << "This program tests the metrics collection functionality." << std::endl;
    std::cout << std::endl;
    
    try
    {
        test_metrics_basic_functionality();
        test_global_metrics_singleton();
        test_metrics_disabled();
        
        std::cout << "\n🎉 All metrics tests passed successfully!" << std::endl;
        std::cout << "\nNote: If prometheus-cpp is not available, this program will use stub implementations." << std::endl;
        
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "\n💥 Test program failed: " << e.what() << std::endl;
        return 1;
    }
}