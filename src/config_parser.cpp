#include <ce-win-file-cache/config_parser.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>

namespace CeWinFileCache
{

std::optional<Config> ConfigParser::parseJsonFile(std::wstring_view file_path)
{
    std::ifstream file;
    auto filename = std::filesystem::path(file_path).string();
    file.open(filename, std::ios::in);
    if (!file.is_open())
    {
        std::wcerr << L"Failed to open config file: " << file_path << std::endl;
        return std::nullopt;
    }

    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

    return parseJsonString(content);
}

std::optional<Config> ConfigParser::parseJsonString(std::string_view json_content)
{
    try
    {
        nlohmann::json j = nlohmann::json::parse(json_content);
        Config config;

        // Parse compilers section
        if (j.contains("compilers") && j["compilers"].is_object())
        {
            for (const auto& [compiler_name, compiler_config] : j["compilers"].items())
            {
                std::wstring wide_name(compiler_name.begin(), compiler_name.end());
                CompilerConfig cc;

                // Parse network_path
                if (compiler_config.contains("network_path") && compiler_config["network_path"].is_string())
                {
                    std::string network_path = compiler_config["network_path"];
                    cc.network_path = std::wstring(network_path.begin(), network_path.end());
                }

                // Parse cache_size_mb
                if (compiler_config.contains("cache_size_mb") && compiler_config["cache_size_mb"].is_number())
                {
                    cc.cache_size_mb = compiler_config["cache_size_mb"];
                }

                // Parse cache_always patterns
                if (compiler_config.contains("cache_always") && compiler_config["cache_always"].is_array())
                {
                    for (const auto& pattern : compiler_config["cache_always"])
                    {
                        if (pattern.is_string())
                        {
                            std::string str_pattern = pattern;
                            cc.cache_always_patterns.emplace_back(str_pattern.begin(), str_pattern.end());
                        }
                    }
                }

                // Parse prefetch_patterns
                if (compiler_config.contains("prefetch_patterns") && compiler_config["prefetch_patterns"].is_array())
                {
                    for (const auto& pattern : compiler_config["prefetch_patterns"])
                    {
                        if (pattern.is_string())
                        {
                            std::string str_pattern = pattern;
                            cc.prefetch_patterns.emplace_back(str_pattern.begin(), str_pattern.end());
                        }
                    }
                }

                config.compilers[wide_name] = std::move(cc);
            }
        }

        // Parse global section
        if (j.contains("global") && j["global"].is_object())
        {
            const auto& global = j["global"];

            // Parse total_cache_size_mb
            if (global.contains("total_cache_size_mb") && global["total_cache_size_mb"].is_number())
            {
                config.global.total_cache_size_mb = global["total_cache_size_mb"];
            }

            // Parse eviction_policy
            if (global.contains("eviction_policy") && global["eviction_policy"].is_string())
            {
                std::string policy = global["eviction_policy"];
                config.global.eviction_policy = std::wstring(policy.begin(), policy.end());
            }

            // Parse cache_directory
            if (global.contains("cache_directory") && global["cache_directory"].is_string())
            {
                std::string cache_dir = global["cache_directory"];
                config.global.cache_directory = std::wstring(cache_dir.begin(), cache_dir.end());
            }

            // Parse download_threads
            if (global.contains("download_threads") && global["download_threads"].is_number())
            {
                config.global.download_threads = global["download_threads"];
            }

            // Parse metrics section
            if (global.contains("metrics") && global["metrics"].is_object())
            {
                const auto& metrics = global["metrics"];

                if (metrics.contains("enabled") && metrics["enabled"].is_boolean())
                {
                    config.global.metrics.enabled = metrics["enabled"];
                }

                if (metrics.contains("bind_address") && metrics["bind_address"].is_string())
                {
                    config.global.metrics.bind_address = metrics["bind_address"];
                }

                if (metrics.contains("port") && metrics["port"].is_number())
                {
                    config.global.metrics.port = metrics["port"];
                }

                if (metrics.contains("endpoint_path") && metrics["endpoint_path"].is_string())
                {
                    config.global.metrics.endpoint_path = metrics["endpoint_path"];
                }
            }
        }

        // Set defaults if not specified
        if (config.global.eviction_policy.empty())
        {
            config.global.eviction_policy = L"lru";
        }
        if (config.global.cache_directory.empty())
        {
            config.global.cache_directory = L"C:\\CompilerCache";
        }
        if (config.global.total_cache_size_mb == 0)
        {
            config.global.total_cache_size_mb = 8192; // 8GB default
        }

        return config;
    }
    catch (const nlohmann::json::exception& e)
    {
        std::cerr << "JSON parsing error: " << e.what() << std::endl;
        return std::nullopt;
    }
    catch (const std::exception& e)
    {
        std::cerr << "Configuration parsing error: " << e.what() << std::endl;
        return std::nullopt;
    }
}

} // namespace CeWinFileCache