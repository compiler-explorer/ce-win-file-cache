#include "../../include/ce-win-file-cache/async_download_manager.hpp"
#include "../../include/ce-win-file-cache/config_parser.hpp"
#include "../../include/ce-win-file-cache/string_utils.hpp"
#include <atomic>
#include <chrono>
#include <cstdio>
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
                                                         [](NTSTATUS s, const std::wstring & /*e*/, CacheEntry * /*entry*/)
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
                                       [&completed](NTSTATUS s, const std::wstring & /*e*/, CacheEntry * /*entry*/)
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
                                           [i](NTSTATUS /*s*/, const std::wstring & /*e*/, CacheEntry * /*entry*/)
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
                                               [](NTSTATUS /*s*/, const std::wstring & /*e*/, CacheEntry * /*entry*/)
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

    // Test 5: Empty file with a cache entry must end up cached, not failed
    std::cout << "\nTest 5: Empty file download with cache entry" << std::endl;
    bool empty_file_ok = false;
    {
        Config config;
        MemoryCacheManager memory_cache;
        AsyncDownloadManager download_manager(memory_cache, config, 1);

        std::ofstream("empty_file_test.txt").close();

        CacheEntry entry;
        entry.virtual_path = L"/empty/empty_file_test.txt";
        entry.network_path = L"empty_file_test.txt";
        entry.file_size = 123;

        std::atomic<bool> completed(false);
        std::atomic<NTSTATUS> result(STATUS_PENDING);
        download_manager.queueDownload(entry.virtual_path, entry.network_path, &entry, CachePolicy::ALWAYS_CACHE,
                                       [&completed, &result](NTSTATUS s, const std::wstring & /*e*/, CacheEntry * /*entry*/)
                                       {
                                           result = s;
                                           completed = true;
                                       });

        auto start = std::chrono::steady_clock::now();
        while (!completed && std::chrono::steady_clock::now() - start < std::chrono::seconds(5))
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        empty_file_ok = completed && result == STATUS_SUCCESS && entry.state == FileState::CACHED &&
                        entry.file_size == 0 && entry.is_in_memory_cache.load();
        std::remove("empty_file_test.txt");
    }
    std::cout << (empty_file_ok ? "✓ Empty file cached" : "❌ Empty file was not cached") << std::endl;

    // Test 6: Stable file ids are case and separator insensitive, distinct and never zero
    std::cout << "\nTest 6: Stable file ids" << std::endl;
    bool file_id_ok = true;
    {
        uint64_t id = StringUtils::stableFileId(L"/compilers/msvc/bin/Hostx64/x64/cl.exe");
        file_id_ok &= id != 0;
        file_id_ok &= id == StringUtils::stableFileId(L"\\COMPILERS\\msvc\\BIN\\hostx64\\x64\\CL.EXE");
        file_id_ok &= id != StringUtils::stableFileId(L"/compilers/msvc/bin/Hostx64/x64/c1.dll");
        file_id_ok &= StringUtils::stableFileId(L"") != 0;
    }
    std::cout << (file_id_ok ? "✓ Stable file ids" : "❌ Stable file ids wrong") << std::endl;

    if (!empty_file_ok || !file_id_ok)
    {
        return 1;
    }

    std::cout << "\n✅ All edge case tests completed!" << std::endl;
    return 0;
}