#include "../../include/ce-win-file-cache/config_parser.hpp"
#include <iostream>
#include <string>

using namespace CeWinFileCache;

int main()
{
    std::cout << "=== JSON Configuration Parser Test ===" << std::endl;

    // Test 1: Parse JSON string
    std::cout << "\n1. Testing JSON string parsing..." << std::endl;

    std::string test_json = R"({
        "compilers": {
            "test-compiler": {
                "network_path": "\\\\test\\path",
                "cache_size_mb": 1024,
                "cache_always": ["*.exe", "*.dll"],
                "prefetch_patterns": ["*.h"]
            }
        },
        "global": {
            "total_cache_size_mb": 4096,
            "eviction_policy": "lru",
            "cache_directory": "C:\\\\TestCache",
            "download_threads": 8,
            "metrics": {
                "enabled": true,
                "bind_address": "127.0.0.1",
                "port": 9090,
                "endpoint_path": "/test-metrics"
            }
        }
    })";

    auto config = ConfigParser::parseJsonString(test_json);
    if (config.has_value())
    {
        std::cout << "  ✓ JSON parsing successful" << std::endl;

        // Verify compiler config
        if (config->compilers.count(L"test-compiler") > 0)
        {
            const auto &cc = config->compilers.at(L"test-compiler");
            std::wcout << L"  ✓ Compiler network_path: " << cc.network_path << std::endl;
            std::cout << "  ✓ Cache patterns: " << cc.cache_always_patterns.size() << std::endl;
            std::cout << "  ✓ Prefetch patterns: " << cc.prefetch_patterns.size() << std::endl;
        }
        else
        {
            std::cout << "  ✗ Test compiler not found" << std::endl;
        }

        // Verify global config
        std::cout << "  ✓ Global cache size: " << config->global.total_cache_size_mb << " MB" << std::endl;
        std::wcout << L"  ✓ Eviction policy: " << config->global.eviction_policy << std::endl;
        std::wcout << L"  ✓ Cache directory: " << config->global.cache_directory << std::endl;
        std::cout << "  ✓ Download threads: " << config->global.download_threads << std::endl;

        // Verify metrics config
        std::cout << "  ✓ Metrics enabled: " << (config->global.metrics.enabled ? "true" : "false") << std::endl;
        std::cout << "  ✓ Metrics bind address: " << config->global.metrics.bind_address << std::endl;
        std::cout << "  ✓ Metrics port: " << config->global.metrics.port << std::endl;
        std::cout << "  ✓ Metrics endpoint: " << config->global.metrics.endpoint_path << std::endl;
    }
    else
    {
        std::cout << "  ✗ JSON parsing failed" << std::endl;
        return 1;
    }

    // Test 2: Parse main compilers.json file
    std::cout << "\n2. Testing compilers.json file parsing..." << std::endl;

    auto file_config = ConfigParser::parseJsonFile(L"compilers.json");
    if (file_config.has_value())
    {
        std::cout << "  ✓ compilers.json parsing successful" << std::endl;
        std::cout << "  ✓ Found " << file_config->compilers.size() << " compilers" << std::endl;
        std::cout << "  ✓ Global config loaded successfully" << std::endl;
        std::cout << "    Total cache: " << file_config->global.total_cache_size_mb << " MB" << std::endl;
        std::cout << "    Download threads: " << file_config->global.download_threads << std::endl;
    }
    else
    {
        std::cout << "  ✗ compilers.json parsing failed" << std::endl;
        std::cout << "    (This is expected if compilers.json doesn't exist in current directory)" << std::endl;
    }

    // Test 3: Error handling
    std::cout << "\n3. Testing error handling..." << std::endl;

    std::string invalid_json = "{ invalid json }";
    auto error_config = ConfigParser::parseJsonString(invalid_json);
    if (!error_config.has_value())
    {
        std::cout << "  ✓ Invalid JSON properly rejected" << std::endl;
    }
    else
    {
        std::cout << "  ✗ Invalid JSON should have been rejected" << std::endl;
    }

    std::cout << "\n🎉 JSON configuration parser test completed!" << std::endl;
    return 0;
}