// Minimal test program for memory cache manager
// This can be compiled independently without Windows dependencies

#include "../include/ce-win-file-cache/memory_cache_manager.hpp"
#include "../include/ce-win-file-cache/config_parser.hpp"
#include <iostream>
#include <chrono>
#include <locale>
#include <codecvt>

using namespace CeWinFileCache;

// Test function for cache operations
int testCacheOperations(const Config& config)
{
    std::wcout << L"=== Cache Operations Test ===" << std::endl;
    
    // Create cache manager
    MemoryCacheManager cache_manager;
    
    // Test files (use mock paths for testing)
    std::vector<std::wstring> test_files = {
        L"/msvc-14.40/bin/Hostx64/x64/cl.exe",
        L"/msvc-14.40/include/iostream",
        L"/ninja/ninja.exe"
    };
    
    std::wcout << L"\n1. Testing cache miss and network loading..." << std::endl;
    for (const auto& virtual_path : test_files)
    {
        std::wcout << L"  Loading: " << virtual_path << std::endl;
        
        // Check cache (should miss)
        if (cache_manager.isFileInMemoryCache(virtual_path))
        {
            std::wcout << L"    ERROR: File unexpectedly in cache" << std::endl;
            return 1;
        }
        
        // For testing, we'll simulate loading
        std::wcout << L"    Simulating load (file may not exist on this system)" << std::endl;
        
        // Create mock content
        std::vector<uint8_t> mock_content;
        std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
        std::string path_str = converter.to_bytes(virtual_path);
        for (char c : path_str) {
            mock_content.push_back(static_cast<uint8_t>(c));
        }
        
        // Add to cache
        cache_manager.addFileToMemoryCache(virtual_path, mock_content);
        std::wcout << L"    Added " << mock_content.size() << L" bytes to cache" << std::endl;
    }
    
    std::wcout << L"\n2. Testing cache hits..." << std::endl;
    for (const auto& virtual_path : test_files)
    {
        // Check if in cache
        if (!cache_manager.isFileInMemoryCache(virtual_path))
        {
            std::wcout << L"  ERROR: " << virtual_path << L" not in cache" << std::endl;
            return 1;
        }
        
        std::wcout << L"  Reading from cache: " << virtual_path << std::endl;
        
        auto start = std::chrono::high_resolution_clock::now();
        auto cached = cache_manager.getMemoryCachedFile(virtual_path);
        auto end = std::chrono::high_resolution_clock::now();
        
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
        
        if (!cached.has_value())
        {
            std::wcout << L"    ERROR: Failed to retrieve cached file" << std::endl;
            return 1;
        }
        
        std::wcout << L"    Retrieved " << cached->size() << L" bytes in " << duration << L" microseconds" << std::endl;
    }
    
    std::wcout << L"\n3. Cache statistics..." << std::endl;
    std::wcout << L"  Total cached files: " << cache_manager.getCachedFileCount() << std::endl;
    std::wcout << L"  Total cache size: " << cache_manager.getCacheSize() << L" bytes" << std::endl;
    
    std::wcout << L"\n4. Testing cache clear..." << std::endl;
    cache_manager.clearCache();
    std::wcout << L"  Cache cleared. Files in cache: " << cache_manager.getCachedFileCount() << std::endl;
    
    std::wcout << L"\nCache operations test completed!" << std::endl;
    return 0;
}

int main(int argc, char** argv)
{
    // Basic command line parsing
    bool test_cache = false;
    
    for (int i = 1; i < argc; i++)
    {
        std::string arg(argv[i]);
        if (arg == "--test-cache" || arg == "--test")
        {
            test_cache = true;
        }
        else if (arg == "--help" || arg == "-h")
        {
            std::cout << "Usage: " << argv[0] << " [OPTIONS]\n"
                      << "Options:\n"
                      << "  --test-cache, --test    Run cache tests\n"
                      << "  --help, -h             Show this help\n";
            return 0;
        }
    }
    
    if (!test_cache)
    {
        std::cout << "Use --test-cache to run tests\n";
        return 0;
    }
    
    // Create a mock config for testing
    Config config;
    config.global.cache_directory = L"./cache";
    config.global.total_cache_size_mb = 1024;
    config.global.eviction_policy = L"lru";
    
    // Add mock compiler configs
    CompilerConfig msvc_config;
    msvc_config.network_path = L"/mock/path/msvc";
    msvc_config.cache_size_mb = 512;
    config.compilers[L"msvc-14.40"] = msvc_config;
    
    CompilerConfig ninja_config;
    ninja_config.network_path = L"/mock/path/ninja";
    ninja_config.cache_size_mb = 100;
    config.compilers[L"ninja"] = ninja_config;
    
    return testCacheOperations(config);
}