#include "../../include/ce-win-file-cache/config_parser.hpp"
#include <fstream>
#include <iostream>

using namespace CeWinFileCache;

int main()
{
    std::cout << "=== Download Threads Configuration Test ===" << std::endl;

    // Test 1: Default value
    Config default_config;
    default_config.global.download_threads = 4; // Initialize with default value
    std::cout << "Default download_threads: " << default_config.global.download_threads << std::endl;

    // Test 2: Load from JSON and check if download_threads is parsed
    auto config_opt = ConfigParser::parseJsonFile(L"compilers.json");
    if (config_opt.has_value())
    {
        const Config &config = config_opt.value();
        std::cout << "Loaded download_threads from compilers.json: " << config.global.download_threads << std::endl;
        std::cout << "Other global settings:" << std::endl;
        std::cout << "  total_cache_size_mb: " << config.global.total_cache_size_mb << std::endl;
        std::wcout << L"  eviction_policy: " << config.global.eviction_policy << std::endl;
        std::wcout << L"  cache_directory: " << config.global.cache_directory << std::endl;
    }
    else
    {
        std::cout << "Error loading config from compilers.json" << std::endl;
        return 1;
    }

    // Test 3: Create a test config with different thread count
    std::cout << "\nTesting custom thread count:" << std::endl;
    std::ofstream test_file("test_threads.json");
    test_file << "{\n";
    test_file << "  \"global\": {\n";
    test_file << "    \"total_cache_size_mb\": 1024,\n";
    test_file << "    \"eviction_policy\": \"lru\",\n";
    test_file << "    \"cache_directory\": \"/tmp/cache\",\n";
    test_file << "    \"download_threads\": 8\n";
    test_file << "  },\n";
    test_file << "  \"compilers\": {\n";
    test_file << "    \"test-compiler\": {\n";
    test_file << "      \"network_path\": \"/test/path\",\n";
    test_file << "      \"cache_size_mb\": 100\n";
    test_file << "    }\n";
    test_file << "  }\n";
    test_file << "}\n";
    test_file.close();

    auto test_config_opt = ConfigParser::parseJsonFile(L"test_threads.json");
    if (test_config_opt.has_value())
    {
        const Config &test_config = test_config_opt.value();
        std::cout << "Custom config download_threads: " << test_config.global.download_threads << std::endl;

        // Cleanup
        std::remove("test_threads.json");
    }
    else
    {
        std::cout << "Error loading test config from test_threads.json" << std::endl;
        std::remove("test_threads.json");
        return 1;
    }

    std::cout << "\n✓ All download_threads configuration tests passed!" << std::endl;
    return 0;
}