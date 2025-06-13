#include "../../include/ce-win-file-cache/async_download_manager.hpp"
#include "../../include/ce-win-file-cache/config_parser.hpp"
#include "../../include/ce-win-file-cache/memory_cache_manager.hpp"
#include "../../include/ce-win-file-cache/metrics_collector.hpp"
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <thread>
#include <cstdlib>

using namespace CeWinFileCache;
namespace fs = std::filesystem;

class TestAsyncDownload
{
    public:
    static void fetchMetrics(std::string_view stage)
    {
        std::cout << "\n=== METRICS " << stage << " ===" << std::endl;
        int result = std::system("curl -s http://127.0.0.1:8082/metrics 2>/dev/null || echo 'Metrics server not available'");
        std::cout << "\n=== END METRICS " << stage << " ===\n" << std::endl;
        (void)result; // Suppress unused variable warning
    }
    static void createTestFiles()
    {
        std::cout << "Creating test files..." << std::endl;

        // Create test directory
        fs::create_directories("test_files");

        // Create various sized test files
        createTestFile("test_files/small.txt", 1024); // 1 KB
        createTestFile("test_files/medium.txt", 1024 * 100); // 100 KB
        createTestFile("test_files/large.txt", 1024 * 1024); // 1 MB
        createTestFile("test_files/huge.txt", 1024 * 1024 * 10); // 10 MB
    }

    static void createTestFile(const std::string &path, size_t size)
    {
        std::ofstream file(path, std::ios::binary);
        std::vector<char> data(size);

        // Fill with pattern
        for (size_t i = 0; i < size; ++i)
        {
            data[i] = static_cast<char>('A' + (i % 26));
        }

        file.write(data.data(), size);
        file.close();

        std::cout << "  Created " << path << " (" << size / 1024 << " KB)" << std::endl;
    }

    static void runBasicTest()
    {
        std::cout << "\n=== Basic Async Download Test ===" << std::endl;

        // Setup
        Config config;
        MemoryCacheManager memory_cache;
        AsyncDownloadManager download_manager(memory_cache, config, 2); // 2 worker threads

        // Track download completion
        std::atomic<int> completed_count(0);
        std::atomic<int> failed_count(0);

        // Queue multiple downloads
        std::vector<std::wstring> test_files = { L"test_files/small.txt", L"test_files/medium.txt", L"test_files/large.txt" };

        auto start_time = std::chrono::steady_clock::now();

        for (const auto &file : test_files)
        {
            std::wstring virtual_path = L"/cache/" + file;

            std::wcout << L"Queueing: " << virtual_path << std::endl;

            download_manager.queueDownload(virtual_path, file,
                                           nullptr, // No cache entry for this test
                                           CachePolicy::ALWAYS_CACHE,
                                           [&completed_count, &failed_count,
                                            vpath = virtual_path](NTSTATUS status, const std::wstring &error)
                                           {
                                               if (status == STATUS_SUCCESS)
                                               {
                                                   completed_count++;
                                                   std::wcout << L"  ✓ Downloaded: " << vpath << std::endl;
                                               }
                                               else if (status == STATUS_UNSUCCESSFUL)
                                               {
                                                   failed_count++;
                                                   std::wcout << L"  ✗ Failed: " << vpath;
                                                   if (!error.empty())
                                                   {
                                                       std::wcout << L" - " << error;
                                                   }
                                                   std::wcout << std::endl;
                                               }
                                               else if (status == STATUS_PENDING)
                                               {
                                                   std::wcout << L"  ⏳ Already downloading: " << vpath << std::endl;
                                               }
                                           });
        }

        // Wait for all downloads to complete
        while (completed_count + failed_count < static_cast<int>(test_files.size()))
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));

            // Show progress
            std::cout << "\rPending: " << download_manager.getPendingCount()
                      << ", Active: " << download_manager.getActiveCount() << ", Completed: " << completed_count
                      << ", Failed: " << failed_count << std::flush;
        }

        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

        std::cout << "\n\nResults:" << std::endl;
        std::cout << "  Total files: " << test_files.size() << std::endl;
        std::cout << "  Completed: " << completed_count << std::endl;
        std::cout << "  Failed: " << failed_count << std::endl;
        std::cout << "  Time taken: " << duration.count() << " ms" << std::endl;

        // Verify files are in memory cache
        std::cout << "\nVerifying memory cache:" << std::endl;
        for (const auto &file : test_files)
        {
            std::wstring virtual_path = L"/cache/" + file;
            auto content = memory_cache.getMemoryCachedFile(virtual_path);
            if (content.has_value() && !content->empty())
            {
                std::wcout << L"  ✓ " << virtual_path << L" is cached (" << content->size() / 1024 << L" KB)" << std::endl;
            }
            else
            {
                std::wcout << L"  ✗ " << virtual_path << L" is NOT cached" << std::endl;
            }
        }
    }

    static void runStressTest()
    {
        std::cout << "\n=== Stress Test - Many Small Files ===" << std::endl;

        // Create many small files
        std::cout << "Creating 50 small test files..." << std::endl;
        for (int i = 0; i < 50; ++i)
        {
            std::string filename = "test_files/stress_" + std::to_string(i) + ".txt";
            createTestFile(filename, 1024 + (i * 100)); // Varying sizes
        }

        // Setup
        Config config;
        MemoryCacheManager memory_cache;
        AsyncDownloadManager download_manager(memory_cache, config, 4); // 4 worker threads

        std::atomic<int> completed_count(0);
        auto start_time = std::chrono::steady_clock::now();

        // Queue all downloads
        for (int i = 0; i < 50; ++i)
        {
            std::wstring filename = L"test_files/stress_" + std::to_wstring(i) + L".txt";
            std::wstring virtual_path = L"/cache/" + filename;

            download_manager.queueDownload(virtual_path, filename, nullptr, CachePolicy::ALWAYS_CACHE,
                                           [&completed_count](NTSTATUS status, const std::wstring &error)
                                           {
                                               (void)error; // Suppress unused warning
                                               if (status == STATUS_SUCCESS)
                                               {
                                                   completed_count++;
                                               }
                                           });
        }

        // Progress monitoring
        while (completed_count < 50)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            std::cout << "\rProgress: " << completed_count << "/50 files"
                      << " (Pending: " << download_manager.getPendingCount()
                      << ", Active: " << download_manager.getActiveCount() << ")" << std::flush;
        }

        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

        std::cout << "\n\nStress test completed:" << std::endl;
        std::cout << "  Downloaded 50 files in " << duration.count() << " ms" << std::endl;
        std::cout << "  Average: " << duration.count() / 50.0 << " ms per file" << std::endl;
    }

    static void runConcurrentTest()
    {
        std::cout << "\n=== Concurrent Download Test ===" << std::endl;

        Config config;
        MemoryCacheManager memory_cache;
        AsyncDownloadManager download_manager(memory_cache, config, 3);

        // Try to download the same file multiple times concurrently
        std::wstring virtual_path = L"/cache/test_files/large.txt";
        std::wstring network_path = L"test_files/large.txt";

        std::atomic<int> in_progress_count(0);
        std::atomic<int> completed_count(0);

        std::cout << "Queueing same file 5 times..." << std::endl;

        for (int i = 0; i < 5; ++i)
        {
            download_manager.queueDownload(virtual_path, network_path, nullptr, CachePolicy::ALWAYS_CACHE,
                                           [&in_progress_count, &completed_count, i](NTSTATUS status, const std::wstring &error)
                                           {
                                               (void)error; // Suppress unused warning
                                               if (status == STATUS_PENDING)
                                               {
                                                   in_progress_count++;
                                                   std::cout << "  Request " << i << ": Already in progress" << std::endl;
                                               }
                                               else if (status == STATUS_SUCCESS)
                                               {
                                                   completed_count++;
                                                   std::cout << "  Request " << i << ": Completed" << std::endl;
                                               }
                                           });

            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        // Wait for completion
        std::this_thread::sleep_for(std::chrono::seconds(2));

        std::cout << "\nResults:" << std::endl;
        std::cout << "  In-progress responses: " << in_progress_count << std::endl;
        std::cout << "  Completed responses: " << completed_count << std::endl;
        std::cout << "  (Should have multiple in-progress and one completed)" << std::endl;
    }

    static void cleanupTestFiles()
    {
        std::cout << "\nCleaning up test files..." << std::endl;
        try
        {
            fs::remove_all("test_files");
            std::cout << "  Cleanup complete" << std::endl;
        }
        catch (const std::exception &e)
        {
            std::cout << "  Cleanup failed: " << e.what() << std::endl;
        }
    }
};

int main(int argc, char **argv)
{
    std::cout << "=== Async Download Manager Test with Metrics ===" << std::endl;

    // Initialize global metrics with custom port to avoid conflicts
    MetricsConfig metrics_config;
    metrics_config.enabled = true;
    metrics_config.bind_address = "127.0.0.1";
    metrics_config.port = 8082;  // Use different port to avoid conflicts
    metrics_config.endpoint_path = "/metrics";
    
    std::cout << "Initializing metrics on port " << metrics_config.port << "..." << std::endl;
    GlobalMetrics::initialize(metrics_config);
    
    if (auto* metrics = GlobalMetrics::instance())
    {
        std::cout << "Metrics server started at: " << metrics->getMetricsUrl() << std::endl;
    }
    else
    {
        std::cout << "Warning: Metrics server failed to initialize" << std::endl;
    }
    
    // Wait for metrics server to start
    std::cout << "Waiting 3 seconds for metrics server to start..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(3));

    try
    {
        // Fetch initial metrics
        TestAsyncDownload::fetchMetrics("BEFORE TESTS");
        
        // Create test files
        TestAsyncDownload::createTestFiles();

        // Run tests with metrics between each
        std::cout << "\n--- Running Basic Test ---" << std::endl;
        TestAsyncDownload::runBasicTest();
        TestAsyncDownload::fetchMetrics("AFTER BASIC TEST");
        
        std::cout << "\n--- Running Stress Test ---" << std::endl;
        TestAsyncDownload::runStressTest();
        TestAsyncDownload::fetchMetrics("AFTER STRESS TEST");
        
        std::cout << "\n--- Running Concurrent Test ---" << std::endl;
        TestAsyncDownload::runConcurrentTest();
        TestAsyncDownload::fetchMetrics("AFTER CONCURRENT TEST");

        // Cleanup
        TestAsyncDownload::cleanupTestFiles();
        
        // Final metrics
        TestAsyncDownload::fetchMetrics("FINAL METRICS");

        std::cout << "\nAll tests completed successfully!" << std::endl;
        std::cout << "Keeping metrics server running for 5 more seconds..." << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(5));
        
        GlobalMetrics::shutdown();
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        TestAsyncDownload::cleanupTestFiles();
        GlobalMetrics::shutdown();
        return 1;
    }
}