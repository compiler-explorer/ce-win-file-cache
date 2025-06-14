// Test program for DirectoryCache functionality
#include "../../include/ce-win-file-cache/config_parser.hpp"
#include "../../include/ce-win-file-cache/directory_cache.hpp"
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>

using namespace CeWinFileCache;

void exportDirectoryTreeToFile(DirectoryCache &cache, const std::wstring &path, std::ofstream &file, int depth = 0)
{
    auto contents = cache.getDirectoryContents(path);

    // Debug output for first level only
    if (depth == 0)
    {
        std::wcout << L"Export function called with " << contents.size() << L" items" << std::endl;
    }

    for (const auto *node : contents)
    {
        // Skip . and .. entries (these shouldn't be in our cache but check just in case)
        if (node->name == L"." || node->name == L"..")
        {
            continue;
        }

        // Print indentation
        for (int i = 0; i < depth; i++)
        {
            file << "  ";
        }

        // Convert wide string to narrow string for file output
        std::string narrow_name(node->name.begin(), node->name.end());

        // Print node info
        if (node->isDirectory())
        {
            file << "DIR  " << narrow_name << "/" << std::endl;
            // Recursively print subdirectories (NO depth limit for complete tree)
            exportDirectoryTreeToFile(cache, node->full_virtual_path, file, depth + 1);
        }
        else
        {
            file << "FILE " << narrow_name;
            if (node->file_size > 0)
            {
                if (node->file_size >= 1024 * 1024)
                {
                    file << " (" << (node->file_size / (1024 * 1024)) << " MB)";
                }
                else if (node->file_size >= 1024)
                {
                    file << " (" << (node->file_size / 1024) << " KB)";
                }
                else
                {
                    file << " (" << node->file_size << " bytes)";
                }
            }
            file << std::endl;
        }
    }
}

void printDirectoryTree(DirectoryCache &cache, const std::wstring &path, int depth = 0)
{
    auto contents = cache.getDirectoryContents(path);

    for (const auto *node : contents)
    {
        // Print indentation
        for (int i = 0; i < depth; i++)
        {
            std::wcout << L"  ";
        }

        // Print node info
        if (node->isDirectory())
        {
            std::wcout << L"📁 " << node->name << L"/" << std::endl;
            // Recursively print subdirectories (with depth limit)
            if (depth < 5)
            {
                printDirectoryTree(cache, node->full_virtual_path, depth + 1);
            }
        }
        else
        {
            std::wcout << L"📄 " << node->name;
            if (node->file_size > 0)
            {
                if (node->file_size >= 1024 * 1024)
                {
                    std::wcout << L" (" << (node->file_size / (1024 * 1024)) << L" MB)";
                }
                else if (node->file_size >= 1024)
                {
                    std::wcout << L" (" << (node->file_size / 1024) << L" KB)";
                }
                else
                {
                    std::wcout << L" (" << node->file_size << L" bytes)";
                }
            }
            std::wcout << std::endl;
        }
    }
}

int testDirectoryCache()
{
    std::wcout << L"=== Directory Cache Test ===" << std::endl;

    // Create directory cache
    DirectoryCache cache;

    // Create mock config
    Config config;

    CompilerConfig msvc_config;
    msvc_config.network_path = L"/mock/msvc/14.40";
    msvc_config.cache_size_mb = 512;
    config.compilers[L"msvc-14.40"] = msvc_config;

    CompilerConfig ninja_config;
    ninja_config.network_path = L"/mock/ninja";
    ninja_config.cache_size_mb = 100;
    config.compilers[L"ninja"] = ninja_config;

    // Initialize cache with mock data
    std::wcout << L"\n1. Initializing directory cache..." << std::endl;
    auto result = cache.initialize(config);
    if (result != STATUS_SUCCESS)
    {
        std::wcout << L"Failed to initialize directory cache" << std::endl;
        return 1;
    }

    // Show statistics
    std::wcout << L"\n2. Directory cache statistics:" << std::endl;
    std::wcout << L"  Total directories: " << cache.getTotalDirectories() << std::endl;
    std::wcout << L"  Total files: " << cache.getTotalFiles() << std::endl;
    std::wcout << L"  Total nodes: " << cache.getTotalNodes() << std::endl;

    // Test root directory listing
    std::wcout << L"\n3. Root directory contents:" << std::endl;
    auto root_contents = cache.getDirectoryContents(L"/");
    for (const auto *node : root_contents)
    {
        std::wcout << L"  " << (node->isDirectory() ? L"📁" : L"📄") << L" " << node->name << std::endl;
    }

    // Test specific directory listings
    std::wcout << L"\n4. MSVC directory structure:" << std::endl;
    printDirectoryTree(cache, L"/msvc-14.40");

    std::wcout << L"\n5. Ninja directory structure:" << std::endl;
    printDirectoryTree(cache, L"/ninja");

    // Test node finding
    std::wcout << L"\n6. Testing node lookup:" << std::endl;

    std::vector<std::wstring> test_paths = { L"/msvc-14.40", L"/msvc-14.40/bin", L"/msvc-14.40/bin/cl.exe",
                                             L"/ninja/ninja.exe", L"/nonexistent" };

    for (const auto &path : test_paths)
    {
        const auto *node = cache.findNode(path);
        std::wcout << L"  " << path << L": ";
        if (node)
        {
            std::wcout << (node->isDirectory() ? L"Directory" : L"File");
            if (node->isFile() && node->file_size > 0)
            {
                std::wcout << L" (" << node->file_size << L" bytes)";
            }
            std::wcout << std::endl;
        }
        else
        {
            std::wcout << L"Not found" << std::endl;
        }
    }

    // Test directory enumeration performance
    std::wcout << L"\n7. Performance test - directory enumeration:" << std::endl;

    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 1000; i++)
    {
        auto contents = cache.getDirectoryContents(L"/msvc-14.40/bin");
        (void)contents; // Suppress unused warning
    }
    auto end = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    std::wcout << L"  1000 directory enumerations: " << duration << L" μs" << std::endl;
    std::wcout << L"  Average per enumeration: " << (duration / 1000.0) << L" μs" << std::endl;

    std::wcout << L"\nDirectory cache test completed successfully!" << std::endl;
    return 0;
}

int testRealFilesystem()
{
    std::wcout << L"=== Real Filesystem Performance Test ===" << std::endl;

    DirectoryCache cache;
    Config config;

    // Configure for real filesystem enumeration
    CompilerConfig root_config;
    root_config.network_path = L"/Users/patrickquist/Documents"; // Start from Documents
    root_config.cache_size_mb = 1024;
    config.compilers[L"docs"] = root_config;

    std::wcout << L"Starting enumeration of Documents directory..." << std::endl;
    std::wcout << L"This will enumerate /Users/patrickquist/Documents recursively..." << std::endl;

    auto start = std::chrono::high_resolution_clock::now();

    // Initialize - this will trigger the enumeration
    auto result = cache.initialize(config);

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    if (result != STATUS_SUCCESS)
    {
        std::wcout << L"Failed to initialize directory cache" << std::endl;
        return 1;
    }

    // Show results
    std::wcout << L"\n=== Enumeration Results ===" << std::endl;
    std::wcout << L"Time taken: " << duration << L" ms (" << (duration / 1000.0) << L" seconds)" << std::endl;
    std::wcout << L"Total directories: " << cache.getTotalDirectories() << std::endl;
    std::wcout << L"Total files: " << cache.getTotalFiles() << std::endl;
    std::wcout << L"Total nodes: " << cache.getTotalNodes() << std::endl;

    // Performance metrics
    size_t total_nodes = cache.getTotalNodes();
    double nodes_per_second = total_nodes / (duration / 1000.0);
    std::wcout << L"Performance: " << std::fixed << std::setprecision(1) << nodes_per_second << L" nodes/second" << std::endl;

    // Memory usage estimate (rough)
    size_t estimated_memory_kb = total_nodes * 200 / 1024; // ~200 bytes per node estimate
    std::wcout << L"Estimated memory usage: ~" << estimated_memory_kb << L" KB" << std::endl;

    // Test a few quick lookups to show performance
    std::wcout << L"\n=== Quick Lookup Test ===" << std::endl;
    std::vector<std::wstring> test_paths = { L"/docs", L"/docs/ce-win-file-cache" };

    for (const auto &path : test_paths)
    {
        const auto *node = cache.findNode(path);
        std::wcout << L"  " << path << L": ";
        if (node)
        {
            std::wcout << (node->isDirectory() ? L"Directory" : L"File") << std::endl;
        }
        else
        {
            std::wcout << L"Not found" << std::endl;
        }
    }

    // Show sample directory contents instead of full export
    std::wcout << L"\n=== Sample Directory Contents ===" << std::endl;
    std::wcout << L"Getting root contents for /docs..." << std::endl;
    auto root_contents = cache.getDirectoryContents(L"/docs");
    std::wcout << L"Root level contains " << root_contents.size() << L" items" << std::endl;

    if (!root_contents.empty())
    {
        std::wcout << L"First few items:" << std::endl;
        size_t count = 0;
        for (const auto *node : root_contents)
        {
            if (count >= 5)
                break; // Show first 5 items only
            std::wcout << L"  - " << node->name;
            if (node->isDirectory())
                std::wcout << L" (directory)";
            else
                std::wcout << L" (file)";
            std::wcout << std::endl;
            count++;
        }

        if (root_contents.size() > 5)
        {
            std::wcout << L"  ... and " << (root_contents.size() - 5) << L" more items" << std::endl;
        }
    }

    std::wcout << L"\nDirectory cache test completed successfully!" << std::endl;

    // Write full directory tree to file
    std::wcout << L"\nWriting full directory tree to file..." << std::endl;
    std::ofstream tree_file("directory_tree_cache.txt");
    if (tree_file.is_open())
    {
        tree_file << "=== Complete Cached Directory Tree ===" << std::endl;
        tree_file << "Enumeration source: /Users/patrickquist/Documents" << std::endl;
        tree_file << "Time taken: " << duration << " ms (" << (duration / 1000.0) << " seconds)" << std::endl;
        tree_file << "Total directories: " << cache.getTotalDirectories() << std::endl;
        tree_file << "Total files: " << cache.getTotalFiles() << std::endl;
        tree_file << "Total nodes: " << cache.getTotalNodes() << std::endl;
        tree_file << "Performance: " << std::fixed << std::setprecision(1) << (total_nodes / (duration / 1000.0))
                  << " nodes/second" << std::endl;
        tree_file << "Estimated memory usage: ~" << (total_nodes * 200 / 1024) << " KB" << std::endl;
        tree_file << "" << std::endl;
        tree_file << "=== FULL DIRECTORY TREE ===" << std::endl;
        tree_file << "" << std::endl;

        auto export_start = std::chrono::high_resolution_clock::now();
        std::wcout << L"Starting tree export for /docs..." << std::endl;

        // First check if we can get root contents
        auto test_contents = cache.getDirectoryContents(L"/docs");
        std::wcout << L"Root has " << test_contents.size() << L" items before export" << std::endl;

        exportDirectoryTreeToFile(cache, L"/docs", tree_file, 0);
        auto export_end = std::chrono::high_resolution_clock::now();
        auto export_duration = std::chrono::duration_cast<std::chrono::milliseconds>(export_end - export_start).count();

        tree_file.close();

        std::wcout << L"Full directory tree exported to 'directory_tree_cache.txt'" << std::endl;
        std::wcout << L"Export time: " << export_duration << L" ms" << std::endl;
    }
    else
    {
        std::wcout << L"Failed to open file for export" << std::endl;
    }

    return 0;
}

int main(int argc, char **argv)
{
    if (argc > 1)
    {
        std::string arg = argv[1];
        if (arg == "--help")
        {
            std::cout << "Directory cache test program\n"
                      << "Usage:\n"
                      << "  " << argv[0] << "           Run basic directory cache test\n"
                      << "  " << argv[0] << " --real    Enumerate real filesystem and export tree (WARNING: May be slow!)\n"
                      << "  " << argv[0] << " --help    Show this help\n";
            return 0;
        }
        else if (arg == "--real")
        {
            return testRealFilesystem();
        }
    }

    return testDirectoryCache();
}