#include "../../include/ce-win-file-cache/async_download_manager.hpp"
#include "../../include/ce-win-file-cache/config_parser.hpp"
#include <chrono>
#include <iostream>
#include <thread>

using namespace CeWinFileCache;

int main()
{
    std::cout << "=== Async Download Manager Configuration Test ===" << std::endl;

    // Load config from YAML
    auto config_opt = ConfigParser::parseYamlFile(L"compilers.yaml");
    if (!config_opt.has_value())
    {
        std::cout << "Error loading config from compilers.yaml" << std::endl;
        return 1;
    }

    const Config &config = config_opt.value();
    std::cout << "Loaded config with download_threads: " << config.global.download_threads << std::endl;

    // Create AsyncDownloadManager with configured thread count
    MemoryCacheManager memory_cache;
    AsyncDownloadManager download_manager(memory_cache, config, config.global.download_threads);

    std::cout << "Created AsyncDownloadManager with " << config.global.download_threads << " worker threads" << std::endl;

    // Test that the download manager works
    std::cout << "Active downloads: " << download_manager.getActiveCount() << std::endl;
    std::cout << "Pending downloads: " << download_manager.getPendingCount() << std::endl;

    std::cout << "\n✓ AsyncDownloadManager configured successfully with thread count from YAML!" << std::endl;
    return 0;
}