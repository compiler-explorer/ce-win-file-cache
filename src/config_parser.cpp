#include <ce-win-file-cache/config_parser.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <ce-win-file-cache/string_utils.hpp>

namespace CeWinFileCache
{

std::optional<Config> ConfigParser::parseJsonFile(std::wstring_view file_path)
{
    std::wcout << L"[CONFIG] Attempting to open config file: " << file_path << std::endl;

    std::ifstream file;
    auto filename = std::filesystem::path(file_path).string();
    std::wcout << L"[CONFIG] Converted to filesystem path: " << std::wstring(filename.begin(), filename.end()) << std::endl;

    file.open(filename, std::ios::in);
    if (!file.is_open())
    {
        std::wcerr << L"[CONFIG ERROR] Failed to open config file: " << file_path << std::endl;
        std::wcerr << L"[CONFIG ERROR] Current working directory: " << std::filesystem::current_path().wstring() << std::endl;

        // Check if file exists
        if (std::filesystem::exists(filename))
        {
            std::wcerr << L"[CONFIG ERROR] File exists but cannot be opened (permission issue?)" << std::endl;
        }
        else
        {
            std::wcerr << L"[CONFIG ERROR] File does not exist" << std::endl;
        }

        return std::nullopt;
    }

    std::wcout << L"[CONFIG] File opened successfully, reading content..." << std::endl;
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    std::wcout << L"[CONFIG] Read " << content.size() << L" bytes from config file" << std::endl;

    return parseJsonString(content);
}

std::optional<Config> ConfigParser::parseJsonString(std::string_view json_content)
{
    std::wcout << L"[CONFIG] Parsing JSON content (" << json_content.size() << L" bytes)..." << std::endl;

    try
    {
        nlohmann::json j = nlohmann::json::parse(json_content);
        std::wcout << L"[CONFIG] JSON parsing successful" << std::endl;
        Config config;

        // Parse compilers section
        if (j.contains("compilers") && j["compilers"].is_object())
        {
            for (const auto &[compiler_name, compiler_config] : j["compilers"].items())
            {
                std::wstring wide_name = StringUtils::utf8ToWide(compiler_name);
                CompilerConfig cc;

                // Parse network_path
                if (compiler_config.contains("network_path") && compiler_config["network_path"].is_string())
                {
                    cc.network_path = StringUtils::utf8ToWide(compiler_config["network_path"]);
                }

                if (compiler_config.contains("root_path") && compiler_config["root_path"].is_string())
                {
                    cc.root_path = StringUtils::utf8ToWide(compiler_config["root_path"]);
                }

                // Parse cache_size_mb
                if (compiler_config.contains("cache_size_mb") && compiler_config["cache_size_mb"].is_number())
                {
                    cc.cache_size_mb = compiler_config["cache_size_mb"];
                }

                // Parse cache_always patterns
                if (compiler_config.contains("cache_always") && compiler_config["cache_always"].is_array())
                {
                    for (const auto &pattern : compiler_config["cache_always"])
                    {
                        if (pattern.is_string())
                        {
                            std::string str_pattern = pattern;
                            cc.cache_always_patterns.emplace_back(StringUtils::utf8ToWide(str_pattern));
                        }
                    }
                }

                // Parse prefetch_patterns
                if (compiler_config.contains("prefetch_patterns") && compiler_config["prefetch_patterns"].is_array())
                {
                    for (const auto &pattern : compiler_config["prefetch_patterns"])
                    {
                        if (pattern.is_string())
                        {
                            std::string str_pattern = pattern;
                            cc.prefetch_patterns.emplace_back(StringUtils::utf8ToWide(str_pattern));
                        }
                    }
                }

                config.compilers[wide_name] = std::move(cc);
            }
        }

        // Parse global section
        if (j.contains("global") && j["global"].is_object())
        {
            const auto &global = j["global"];

            // Parse total_cache_size_mb
            if (global.contains("total_cache_size_mb") && global["total_cache_size_mb"].is_number())
            {
                config.global.total_cache_size_mb = global["total_cache_size_mb"];
            }

            // Parse eviction_policy
            if (global.contains("eviction_policy") && global["eviction_policy"].is_string())
            {
                std::string policy = global["eviction_policy"];
                config.global.eviction_policy = StringUtils::utf8ToWide(policy);
            }

            // Parse cache_directory
            if (global.contains("cache_directory") && global["cache_directory"].is_string())
            {
                std::string cache_dir = global["cache_directory"];
                config.global.cache_directory = StringUtils::utf8ToWide(cache_dir);
            }

            // Parse download_threads
            if (global.contains("download_threads") && global["download_threads"].is_number())
            {
                config.global.download_threads = global["download_threads"];
            }

            // Parse metrics section
            if (global.contains("metrics") && global["metrics"].is_object())
            {
                const auto &metrics = global["metrics"];

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
            
            // Parse file tracking configuration
            if (global.contains("file_tracking") && global["file_tracking"].is_object())
            {
                const auto &tracking = global["file_tracking"];

                if (tracking.contains("enabled") && tracking["enabled"].is_boolean())
                {
                    config.global.file_tracking.enabled = tracking["enabled"];
                }

                if (tracking.contains("report_directory") && tracking["report_directory"].is_string())
                {
                    std::string report_dir = tracking["report_directory"];
                    config.global.file_tracking.report_directory = std::wstring(report_dir.begin(), report_dir.end());
                }

                if (tracking.contains("report_interval_minutes") && tracking["report_interval_minutes"].is_number())
                {
                    config.global.file_tracking.report_interval_minutes = tracking["report_interval_minutes"];
                }

                if (tracking.contains("top_files_count") && tracking["top_files_count"].is_number())
                {
                    config.global.file_tracking.top_files_count = tracking["top_files_count"];
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

        std::wcout << L"[CONFIG] Configuration parsing completed successfully" << std::endl;
        std::wcout << L"[CONFIG] Found " << config.compilers.size() << L" compilers" << std::endl;
        return config;
    }
    catch (const nlohmann::json::exception &e)
    {
        std::wcerr << L"[CONFIG ERROR] JSON parsing error: " << std::wstring(e.what(), e.what() + strlen(e.what())) << std::endl;
        return std::nullopt;
    }
    catch (const std::exception &e)
    {
        std::wcerr << L"[CONFIG ERROR] Configuration parsing error: "
                   << std::wstring(e.what(), e.what() + strlen(e.what())) << std::endl;
        return std::nullopt;
    }
}

} // namespace CeWinFileCache