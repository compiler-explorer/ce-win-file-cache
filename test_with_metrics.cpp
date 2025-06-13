#include "include/ce-win-file-cache/async_download_manager.hpp"
#include "include/ce-win-file-cache/memory_cache_manager.hpp"
#include "include/ce-win-file-cache/metrics_collector.hpp"
#include "include/types/config.hpp"
#include <chrono>
#include <fstream>
#include <iostream>
#include <thread>

using namespace CeWinFileCache;

void simulateCacheOperations()
{
    std::cout << "Starting cache operations simulation..." << std::endl;

    Config config;
    MemoryCacheManager cache_manager;

    // Simulate various cache operations
    for (int i = 0; i < 10; ++i)
    {
        std::wstring virtual_path = L"/test-compiler/file" + std::to_wstring(i) + L".txt";

        // First check if file is in cache (will be cache miss)
        auto cached = cache_manager.getMemoryCachedFile(virtual_path);

        // Create some test content and add to cache
        std::string content = "This is test file content for file " + std::to_string(i);
        std::vector<uint8_t> file_content(content.begin(), content.end());
        cache_manager.addFileToMemoryCache(virtual_path, file_content);

        // Now try to get it again (will be cache hit)
        cached = cache_manager.getMemoryCachedFile(virtual_path);

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::cout << "Cache operations completed. Cache size: " << cache_manager.getCacheSize()
              << " bytes, Files: " << cache_manager.getCachedFileCount() << std::endl;
}

void simulateAsyncDownloads()
{
    std::cout << "Starting async download simulation..." << std::endl;

    Config config;
    MemoryCacheManager memory_cache;
    AsyncDownloadManager download_manager(memory_cache, config, 2);

    std::atomic<int> completed{ 0 };
    std::atomic<int> failed{ 0 };

    // Queue several downloads
    for (int i = 0; i < 5; ++i)
    {
        std::wstring virtual_path = L"/test-compiler/download" + std::to_wstring(i) + L".txt";
        std::wstring network_path = L"/tmp/test_file" + std::to_wstring(i) + L".txt";

        // Create a test file
        std::string filename = "/tmp/test_file" + std::to_string(i) + ".txt";
        std::ofstream test_file(filename);
        test_file << "Test content for download " << i;
        test_file.close();

        download_manager.queueDownload(virtual_path, network_path,
                                       nullptr, // No cache entry for this test
                                       CachePolicy::ALWAYS_CACHE,
                                       [&completed, &failed, i](NTSTATUS status, const std::wstring &error)
                                       {
                                           if (status == STATUS_SUCCESS)
                                           {
                                               completed++;
                                               std::cout << "Download " << i << " completed successfully" << std::endl;
                                           }
                                           else
                                           {
                                               failed++;
                                               std::cout << "Download " << i << " failed" << std::endl;
                                           }
                                       });
    }

    // Wait for downloads to complete
    std::cout << "Waiting for downloads to complete..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(3));

    std::cout << "Downloads completed: " << completed.load() << ", failed: " << failed.load() << std::endl;
}

int main()
{
    std::cout << "=== Cache and Async Test with Metrics ===" << std::endl;

    // Initialize metrics
    MetricsConfig metrics_config;
    metrics_config.enabled = true;
    metrics_config.bind_address = "127.0.0.1";
    metrics_config.port = 8080;
    metrics_config.endpoint_path = "/metrics";

    GlobalMetrics::initialize(metrics_config);

    if (auto *metrics = GlobalMetrics::instance())
    {
        std::cout << "Metrics available at: " << metrics->getMetricsUrl() << std::endl;
        std::cout << "You can fetch metrics with: curl " << metrics->getMetricsUrl() << std::endl;
        std::cout << std::endl;

        std::cout << "Sleeping for 2 seconds to allow initial metrics fetch..." << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(2));

        simulateCacheOperations();

        std::cout << std::endl;
        std::cout << "--- Metrics after cache operations available ---" << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(2));

        simulateAsyncDownloads();

        std::cout << std::endl;
        std::cout << "--- Final metrics available ---" << std::endl;
        std::cout << "Keeping server running for 10 seconds for final metrics collection..." << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(10));
    }
    else
    {
        std::cout << "Metrics not available" << std::endl;
    }

    GlobalMetrics::shutdown();
    std::cout << "Test completed." << std::endl;

    return 0;
}