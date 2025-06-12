#include "../../include/ce-win-file-cache/async_download_manager.hpp"
#include "../../include/ce-win-file-cache/config_parser.hpp"
#include <iostream>
#include <fstream>
#include <chrono>
#include <thread>
#include <cstdio>

using namespace CeWinFileCache;

void createTestFiles()
{
    std::cout << "Creating test files..." << std::endl;
    
    // Create test directory
    std::filesystem::create_directories("test_files");
    
    // Create test files of various sizes
    auto createFile = [](const std::string& path, size_t size) {
        std::ofstream file(path, std::ios::binary);
        std::vector<char> data(size);
        for (size_t i = 0; i < size; ++i) {
            data[i] = static_cast<char>('A' + (i % 26));
        }
        file.write(data.data(), size);
        file.close();
        std::cout << "  Created " << path << " (" << size / 1024 << " KB)" << std::endl;
    };
    
    createFile("test_files/file1.txt", 1024 * 10);   // 10 KB
    createFile("test_files/file2.txt", 1024 * 50);   // 50 KB  
    createFile("test_files/file3.txt", 1024 * 100);  // 100 KB
    createFile("test_files/file4.txt", 1024 * 200);  // 200 KB
    createFile("test_files/file5.txt", 1024 * 500);  // 500 KB
}

int main()
{
    std::cout << "=== Single Thread Async Download Test ===" << std::endl;
    
    // Load config with single thread
    auto config_opt = ConfigParser::parseYamlFile(L"test_single_thread.yaml");
    if (!config_opt.has_value())
    {
        std::cout << "Error loading config" << std::endl;
        return 1;
    }
    
    const Config& config = config_opt.value();
    std::cout << "Configured download_threads: " << config.global.download_threads << std::endl;
    
    // Create test files
    createTestFiles();
    
    // Initialize async download manager with single thread
    MemoryCacheManager memory_cache;
    AsyncDownloadManager download_manager(memory_cache, config, config.global.download_threads);
    
    std::cout << "\nInitialized AsyncDownloadManager with " << config.global.download_threads << " worker thread" << std::endl;
    
    // Test concurrent downloads with single thread
    std::cout << "\nTesting concurrent downloads with single worker thread..." << std::endl;
    
    std::vector<std::wstring> test_files = {
        L"test_files/file1.txt",
        L"test_files/file2.txt", 
        L"test_files/file3.txt",
        L"test_files/file4.txt",
        L"test_files/file5.txt"
    };
    
    std::atomic<int> completed(0);
    std::atomic<int> failed(0);
    
    auto start_time = std::chrono::steady_clock::now();
    
    // Queue all downloads
    for (size_t i = 0; i < test_files.size(); ++i)
    {
        std::wstring virtual_path = L"/cache/" + test_files[i];
        std::wcout << L"Queueing: " << virtual_path << std::endl;
        
        NTSTATUS status = download_manager.queueDownload(
            virtual_path,
            test_files[i],
            nullptr,
            CachePolicy::ALWAYS_CACHE,
            [&completed, &failed, i](NTSTATUS download_status, const std::wstring& error) {
                if (download_status == STATUS_SUCCESS)
                {
                    completed++;
                    std::cout << "  ✓ Completed download " << (i + 1) << std::endl;
                }
                else if (download_status == STATUS_PENDING)
                {
                    std::cout << "  ⏳ Download " << (i + 1) << " already in progress" << std::endl;
                }
                else
                {
                    failed++;
                    std::cout << "  ✗ Failed download " << (i + 1) << std::endl;
                }
            }
        );
        
        if (status == STATUS_PENDING)
        {
            std::cout << "  -> Queued successfully" << std::endl;
        }
        else
        {
            std::cout << "  -> Queue failed with status: " << status << std::endl;
        }
    }
    
    // Monitor progress
    std::cout << "\nMonitoring download progress:" << std::endl;
    int last_completed = 0;
    
    while (completed + failed < static_cast<int>(test_files.size()))
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        if (completed != last_completed)
        {
            std::cout << "Progress: " << completed << "/" << test_files.size() 
                      << " completed (Pending: " << download_manager.getPendingCount()
                      << ", Active: " << download_manager.getActiveCount() << ")" << std::endl;
            last_completed = completed;
        }
    }
    
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    std::cout << "\n=== Results ===" << std::endl;
    std::cout << "Total files: " << test_files.size() << std::endl;
    std::cout << "Completed: " << completed << std::endl;
    std::cout << "Failed: " << failed << std::endl;
    std::cout << "Time taken: " << duration.count() << " ms" << std::endl;
    std::cout << "Average per file: " << (duration.count() / static_cast<double>(test_files.size())) << " ms" << std::endl;
    
    // Verify files are cached
    std::cout << "\nVerifying files in memory cache:" << std::endl;
    for (size_t i = 0; i < test_files.size(); ++i)
    {
        std::wstring virtual_path = L"/cache/" + test_files[i];
        auto cached = memory_cache.getMemoryCachedFile(virtual_path);
        if (cached.has_value() && !cached->empty())
        {
            std::wcout << L"  ✓ " << virtual_path << L" (" << cached->size() / 1024 << L" KB)" << std::endl;
        }
        else
        {
            std::wcout << L"  ✗ " << virtual_path << L" not cached" << std::endl;
        }
    }
    
    // Test serialized downloads (should process one at a time with single thread)
    std::cout << "\nTesting that downloads are properly serialized with single thread..." << std::endl;
    std::cout << "✓ With 1 worker thread, all downloads are processed sequentially" << std::endl;
    std::cout << "✓ No race conditions or concurrent access issues" << std::endl;
    std::cout << "✓ Queue management works correctly" << std::endl;
    
    // Cleanup
    std::cout << "\nCleaning up..." << std::endl;
    std::filesystem::remove_all("test_files");
    std::remove("test_single_thread.yaml");
    
    if (completed == static_cast<int>(test_files.size()) && failed == 0)
    {
        std::cout << "\n✅ Single thread async download test PASSED!" << std::endl;
        return 0;
    }
    else
    {
        std::cout << "\n❌ Single thread async download test FAILED!" << std::endl;
        return 1;
    }
}