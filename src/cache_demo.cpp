// Comprehensive cache demonstration program
#include "../include/ce-win-file-cache/memory_cache_manager.hpp"
#include "../include/ce-win-file-cache/config_parser.hpp"
#include <iostream>
#include <chrono>
#include <fstream>
#include <filesystem>
#include <iomanip>

using namespace CeWinFileCache;
namespace fs = std::filesystem;

// Helper to create test files
void createTestFiles() {
    fs::create_directories("test_network/msvc-14.40/bin");
    fs::create_directories("test_network/msvc-14.40/include");
    fs::create_directories("test_network/ninja");
    
    // Create a "large" executable file (1MB)
    std::ofstream exe("test_network/msvc-14.40/bin/cl.exe", std::ios::binary);
    std::vector<char> exe_data(1024 * 1024, 'X');
    exe.write(exe_data.data(), exe_data.size());
    exe.close();
    
    // Create a header file (50KB)
    std::ofstream header("test_network/msvc-14.40/include/iostream", std::ios::binary);
    std::vector<char> header_data(50 * 1024, 'H');
    header.write(header_data.data(), header_data.size());
    header.close();
    
    // Create a small tool (10KB)
    std::ofstream tool("test_network/ninja/ninja.exe", std::ios::binary);
    std::vector<char> tool_data(10 * 1024, 'N');
    tool.write(tool_data.data(), tool_data.size());
    tool.close();
}

// Format bytes in human-readable form
std::string formatBytes(size_t bytes) {
    const char* units[] = {"B", "KB", "MB", "GB"};
    int unit = 0;
    double size = static_cast<double>(bytes);
    
    while (size >= 1024 && unit < 3) {
        size /= 1024;
        unit++;
    }
    
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << size << " " << units[unit];
    return oss.str();
}

// Demo with timing comparisons
void runCacheDemo() {
    std::cout << "\n=== CACHE DEMONSTRATION ===\n" << std::endl;
    
    // Create test files
    std::cout << "Creating test files..." << std::endl;
    createTestFiles();
    
    // Create cache manager and config
    MemoryCacheManager cache;
    Config config;
    
    // Configure mock paths
    CompilerConfig msvc_config;
    msvc_config.network_path = L"test_network/msvc-14.40";
    config.compilers[L"msvc-14.40"] = msvc_config;
    
    CompilerConfig ninja_config;
    ninja_config.network_path = L"test_network/ninja";
    config.compilers[L"ninja"] = ninja_config;
    
    // Test files to load
    std::vector<std::wstring> test_files = {
        L"/msvc-14.40/bin/cl.exe",
        L"/msvc-14.40/include/iostream",
        L"/ninja/ninja.exe"
    };
    
    std::cout << "\n1. INITIAL LOAD (Cache Miss - Loading from 'Network')\n" << std::endl;
    std::cout << std::setw(40) << std::left << "File" 
              << std::setw(12) << "Size" 
              << std::setw(12) << "Load Time" 
              << "Status" << std::endl;
    std::cout << std::string(80, '-') << std::endl;
    
    size_t total_network_time = 0;
    
    for (const auto& path : test_files) {
        auto start = std::chrono::high_resolution_clock::now();
        auto content = cache.getFileContent(path, config);
        auto end = std::chrono::high_resolution_clock::now();
        
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
        total_network_time += duration;
        
        std::wcout << std::setw(40) << std::left << path;
        std::cout << std::setw(12) << formatBytes(content.size())
                  << std::setw(12) << (std::to_string(duration) + " μs")
                  << (content.empty() ? "FAILED" : "Cached") << std::endl;
    }
    
    std::cout << "\nTotal network load time: " << total_network_time << " μs" << std::endl;
    std::cout << "Cache status: " << cache.getCachedFileCount() << " files, " 
              << formatBytes(cache.getCacheSize()) << " total" << std::endl;
    
    // Simulate some work
    std::cout << "\n[Simulating application work...]\n" << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    std::cout << "2. SUBSEQUENT ACCESS (Cache Hit - Loading from Memory)\n" << std::endl;
    std::cout << std::setw(40) << std::left << "File" 
              << std::setw(12) << "Size" 
              << std::setw(12) << "Load Time" 
              << "Speedup" << std::endl;
    std::cout << std::string(80, '-') << std::endl;
    
    size_t total_cache_time = 0;
    size_t file_idx = 0;
    std::vector<size_t> network_times = {0, 0, 0}; // Store original times
    
    // First, get network times for comparison
    for (const auto& path : test_files) {
        // Clear this file from cache
        cache.clearCache();
        
        auto start = std::chrono::high_resolution_clock::now();
        cache.getFileContent(path, config);
        auto end = std::chrono::high_resolution_clock::now();
        
        network_times[file_idx++] = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    }
    
    // Now test cache hits
    file_idx = 0;
    for (const auto& path : test_files) {
        auto start = std::chrono::high_resolution_clock::now();
        auto content = cache.getMemoryCachedFile(path);
        auto end = std::chrono::high_resolution_clock::now();
        
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
        total_cache_time += duration;
        
        double speedup = network_times[file_idx] > 0 ? 
            static_cast<double>(network_times[file_idx]) / duration : 0;
        
        std::wcout << std::setw(40) << std::left << path;
        std::cout << std::setw(12) << formatBytes(content.value_or(std::vector<uint8_t>()).size())
                  << std::setw(12) << (std::to_string(duration) + " μs")
                  << std::fixed << std::setprecision(1) << speedup << "x faster" << std::endl;
        
        file_idx++;
    }
    
    std::cout << "\nTotal cache hit time: " << total_cache_time << " μs" << std::endl;
    std::cout << "Overall speedup: " << std::fixed << std::setprecision(1) 
              << (total_network_time > 0 ? static_cast<double>(total_network_time) / total_cache_time : 0) 
              << "x faster" << std::endl;
    
    // Demonstrate cache operations
    std::cout << "\n3. CACHE OPERATIONS DEMO\n" << std::endl;
    
    std::cout << "Current cache contents:" << std::endl;
    std::cout << "  Files: " << cache.getCachedFileCount() << std::endl;
    std::cout << "  Size: " << formatBytes(cache.getCacheSize()) << std::endl;
    
    std::cout << "\nChecking if files are cached:" << std::endl;
    for (const auto& path : test_files) {
        std::wcout << "  " << path << ": " 
                   << (cache.isFileInMemoryCache(path) ? "YES" : "NO") << std::endl;
    }
    
    std::cout << "\nClearing cache..." << std::endl;
    cache.clearCache();
    std::cout << "Cache after clear: " << cache.getCachedFileCount() << " files" << std::endl;
    
    // Clean up test files
    std::cout << "\nCleaning up test files..." << std::endl;
    fs::remove_all("test_network");
    
    std::cout << "\n=== DEMO COMPLETE ===\n" << std::endl;
}

int main(int argc, char** argv) {
    if (argc > 1 && std::string(argv[1]) == "--help") {
        std::cout << "Cache demonstration program\n"
                  << "Shows cache performance with simulated network files\n";
        return 0;
    }
    
    runCacheDemo();
    return 0;
}