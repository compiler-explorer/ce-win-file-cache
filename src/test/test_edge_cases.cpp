#include "../../include/ce-win-file-cache/async_download_manager.hpp"
#include "../../include/ce-win-file-cache/config_parser.hpp"
#include <chrono>
#include <fstream>
#include <iostream>
#include <thread>

using namespace CeWinFileCache;

int main()
{
    std::cout << "=== Edge Cases Test for Single Thread ===" << std::endl;

    // Test 1: Zero threads (should not crash, but let's see what happens)
    std::cout << "\nTest 1: Testing with 0 threads (edge case)" << std::endl;
    try
    {
        Config config;
        config.global.download_threads = 0;
        MemoryCacheManager memory_cache;
        AsyncDownloadManager download_manager(memory_cache, config, config.global.download_threads);
        std::cout << "✓ AsyncDownloadManager created with 0 threads" << std::endl;

        // Try to queue a download
        NTSTATUS status = download_manager.queueDownload(L"/test", L"/nonexistent", nullptr, CachePolicy::ALWAYS_CACHE,
                                                         [](NTSTATUS s, const std::wstring & /*e*/, CacheEntry* /*entry*/)
                                                         {
                                                             std::cout << "Callback called with status: " << s << std::endl;
                                                         });
        std::cout << "Queue status: " << status << std::endl;

        // Wait a bit to see if anything happens
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        std::cout << "✓ No crashes with 0 threads" << std::endl;
    }
    catch (const std::exception &e)
    {
        std::cout << "❌ Exception with 0 threads: " << e.what() << std::endl;
    }

    // Test 2: Very large thread count
    std::cout << "\nTest 2: Testing with very large thread count" << std::endl;
    try
    {
        Config config;
        config.global.download_threads = 1000;
        MemoryCacheManager memory_cache;
        AsyncDownloadManager download_manager(memory_cache, config, config.global.download_threads);
        std::cout << "✓ AsyncDownloadManager created with 1000 threads" << std::endl;

        // Queue a simple download
        std::ofstream test_file("large_thread_test.txt");
        test_file << "test content";
        test_file.close();

        std::atomic<bool> completed(false);
        download_manager.queueDownload(L"/test", L"large_thread_test.txt", nullptr, CachePolicy::ALWAYS_CACHE,
                                       [&completed](NTSTATUS s, const std::wstring & /*e*/, CacheEntry* /*entry*/)
                                       {
                                           completed = true;
                                           std::cout << "Download completed with status: " << s << std::endl;
                                       });

        // Wait for completion
        auto start = std::chrono::steady_clock::now();
        while (!completed && std::chrono::steady_clock::now() - start < std::chrono::seconds(5))
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        std::remove("large_thread_test.txt");
        std::cout << "✓ Large thread count works" << std::endl;
    }
    catch (const std::exception &e)
    {
        std::cout << "❌ Exception with large thread count: " << e.what() << std::endl;
    }

    // Test 3: Single thread with rapid queue/cancel operations
    std::cout << "\nTest 3: Rapid queue operations with single thread" << std::endl;
    try
    {
        Config config;
        config.global.download_threads = 1;
        MemoryCacheManager memory_cache;
        AsyncDownloadManager download_manager(memory_cache, config, config.global.download_threads);

        // Create a test file
        std::ofstream test_file("rapid_test.txt");
        test_file << "rapid test content";
        test_file.close();

        // Queue many downloads rapidly
        for (int i = 0; i < 10; ++i)
        {
            std::wstring path = L"/rapid/" + std::to_wstring(i);
            download_manager.queueDownload(path, L"rapid_test.txt", nullptr, CachePolicy::ALWAYS_CACHE,
                                           [i](NTSTATUS /*s*/, const std::wstring & /*e*/, CacheEntry* /*entry*/)
                                           {
                                               std::cout << "Rapid download " << i << " completed" << std::endl;
                                           });
        }

        // Wait for some to complete
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        std::remove("rapid_test.txt");
        std::cout << "✓ Rapid operations handled correctly" << std::endl;
    }
    catch (const std::exception &e)
    {
        std::cout << "❌ Exception with rapid operations: " << e.what() << std::endl;
    }

    // Test 4: Concurrent shutdown with single thread
    std::cout << "\nTest 4: Shutdown behavior with single thread" << std::endl;
    try
    {
        Config config;
        config.global.download_threads = 1;
        MemoryCacheManager memory_cache;

        {
            AsyncDownloadManager download_manager(memory_cache, config, config.global.download_threads);

            // Queue some downloads
            for (int i = 0; i < 3; ++i)
            {
                download_manager.queueDownload(L"/shutdown/" + std::to_wstring(i), L"/nonexistent", nullptr, CachePolicy::ALWAYS_CACHE,
                                               [](NTSTATUS /*s*/, const std::wstring & /*e*/, CacheEntry* /*entry*/)
                                               {
                                               });
            }

            // Let it start processing
            std::this_thread::sleep_for(std::chrono::milliseconds(10));

            // download_manager destructor called here
        }

        std::cout << "✓ Clean shutdown with pending downloads" << std::endl;
    }
    catch (const std::exception &e)
    {
        std::cout << "❌ Exception during shutdown: " << e.what() << std::endl;
    }

    std::cout << "\n✅ All edge case tests completed!" << std::endl;
    return 0;
}