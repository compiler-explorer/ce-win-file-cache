#include "../include/ce-win-file-cache/async_download_manager.hpp"
#include "../include/ce-win-file-cache/memory_cache_manager.hpp"
#include "../include/ce-win-file-cache/config_parser.hpp"
#include <iostream>
#include <fstream>
#include <chrono>
#include <thread>
#include <cstdio>

using namespace CeWinFileCache;

// Simulate a filesystem operation that triggers async download
void simulateFileSystemOperation()
{
    Config config;
    MemoryCacheManager memory_cache;
    AsyncDownloadManager download_manager(memory_cache, config, 2);
    
    // Create a mock cache entry
    CacheEntry cache_entry;
    cache_entry.virtual_path = L"/compiler/bin/cl.exe";
    cache_entry.network_path = L"test_files/large.txt";  // Simulating a compiler binary
    cache_entry.policy = CachePolicy::ALWAYS_CACHE;
    cache_entry.state = FileState::NETWORK_ONLY;
    
    std::cout << "\n=== Simulating File System Async Download ===" << std::endl;
    std::cout << "User opens: " << std::string(cache_entry.virtual_path.begin(), cache_entry.virtual_path.end()) << std::endl;
    
    // Simulate ensureFileAvailable logic
    if (cache_entry.state != FileState::CACHED)
    {
        std::cout << "File not cached, initiating async download..." << std::endl;
        
        NTSTATUS status = download_manager.queueDownload(
            cache_entry.virtual_path,
            cache_entry.network_path,
            &cache_entry,
            cache_entry.policy,
            [&cache_entry](NTSTATUS download_status, const std::wstring& error) {
                if (download_status == STATUS_SUCCESS)
                {
                    std::wcout << L"✓ Download completed: " << cache_entry.virtual_path << std::endl;
                    std::cout << "  File is now ready for use!" << std::endl;
                }
                else if (download_status == STATUS_PENDING)
                {
                    std::cout << "  Download already in progress..." << std::endl;
                }
                else
                {
                    std::wcerr << L"✗ Download failed: " << error << std::endl;
                }
            }
        );
        
        if (status == STATUS_PENDING)
        {
            std::cout << "Returned STATUS_PENDING to filesystem" << std::endl;
            std::cout << "WinFsp will retry the operation when download completes\n" << std::endl;
        }
    }
    
    // Simulate other filesystem operations continuing while download happens
    std::cout << "Meanwhile, other filesystem operations continue..." << std::endl;
    for (int i = 0; i < 5; ++i)
    {
        std::cout << "  Processing other request " << (i + 1) << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    
    // Wait for download to complete
    std::cout << "\nWaiting for download to complete..." << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // Check if file is now in cache
    if (memory_cache.isFileInMemoryCache(cache_entry.virtual_path))
    {
        auto content = memory_cache.getMemoryCachedFile(cache_entry.virtual_path);
        if (content.has_value())
        {
            std::cout << "\n✓ File successfully cached in memory (" 
                      << content->size() / 1024 << " KB)" << std::endl;
        }
    }
}

int main()
{
    std::cout << "=== Async File System Integration Test ===" << std::endl;
    
    // Create test file first
    std::cout << "Creating test file..." << std::endl;
    std::ofstream file("test_files/large.txt", std::ios::binary);
    std::vector<char> data(1024 * 1024);  // 1 MB
    for (size_t i = 0; i < data.size(); ++i)
    {
        data[i] = static_cast<char>('A' + (i % 26));
    }
    file.write(data.data(), data.size());
    file.close();
    
    // Run simulation
    simulateFileSystemOperation();
    
    // Cleanup
    std::remove("test_files/large.txt");
    
    std::cout << "\nTest completed successfully!" << std::endl;
    return 0;
}