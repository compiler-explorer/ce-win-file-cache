#include <ce-win-file-cache/config_parser.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>
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

std::optional<Config> ConfigParser::parseYamlFile(std::wstring_view file_path)
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

    return parseYamlString(content);
}

std::optional<Config> ConfigParser::parseYamlString(std::string_view yaml_content)
{
    Config config;

    std::istringstream stream{std::string(yaml_content)};
    std::string line;

    int line_count = 0;
    std::string current_section;
    std::string current_compiler;

    while (std::getline(stream, line))
    {
        line_count++;

        // Keep original line for pattern matching
        std::string original_line = line;

        // Remove leading/trailing whitespace for empty line check
        line.erase(0, line.find_first_not_of(" \t"));
        line.erase(line.find_last_not_of(" \t") + 1);

        // Skip empty lines and comments
        if (line.empty() || line[0] == '#')
        {
            continue;
        }

        // Check for top-level sections
        if (line == "compilers:")
        {
            current_section = "compilers";
            continue;
        }
        else if (line == "global:")
        {
            current_section = "global";
            continue;
        }

        if (current_section == "compilers")
        {
            // Parse compiler entries
            std::regex compiler_regex(R"(^  ([^:]+):\s*$)");
            std::smatch match;

            if (std::regex_match(original_line, match, compiler_regex))
            {
                std::string potential_compiler = match[1].str();
                // Skip config sections like cache_always, prefetch_patterns, etc.
                if (potential_compiler.find(' ') == std::string::npos && potential_compiler != "cache_always" &&
                    potential_compiler != "prefetch_patterns")
                {
                    current_compiler = potential_compiler;
                    config.compilers[std::wstring(current_compiler.begin(), current_compiler.end())] = CompilerConfig{};
                }
                continue;
            }

            if (!current_compiler.empty())
            {
                std::wstring compiler_name(current_compiler.begin(), current_compiler.end());
                auto &compiler_config = config.compilers[compiler_name];

                // Parse compiler config properties
                std::regex property_regex(R"(^    ([^:]+):\s*(.+)$)");
                if (std::regex_match(original_line, match, property_regex))
                {
                    std::string key = match[1].str();
                    std::string value = match[2].str();

                    if (key == "network_path")
                    {
                        // Remove quotes if present
                        if (value.front() == '"' && value.back() == '"')
                        {
                            value = value.substr(1, value.length() - 2);
                        }
                        compiler_config.network_path = std::wstring(value.begin(), value.end());
                    }
                    else if (key == "cache_size_mb")
                    {
                        compiler_config.cache_size_mb = std::stoull(value);
                    }
                    else if (key == "cache_always")
                    {
                        // Simple array parsing - todo: improve
                        compiler_config.cache_always_patterns = parseStringArray(value);
                    }
                    else if (key == "prefetch_patterns")
                    {
                        compiler_config.prefetch_patterns = parseStringArray(value);
                    }
                }
            }
        }
        else if (current_section == "global")
        {
            // Check for nested metrics section
            if (line == "metrics:")
            {
                current_section = "global.metrics";
                continue;
            }

            // Parse global config
            std::regex property_regex(R"(^\s*([^:]+):\s*(.+)$)");
            std::smatch match;

            if (std::regex_match(line, match, property_regex))
            {
                std::string key = match[1].str();
                std::string value = match[2].str();

                if (key == "total_cache_size_mb")
                {
                    config.global.total_cache_size_mb = std::stoull(value);
                }
                else if (key == "eviction_policy")
                {
                    if (value.front() == '"' && value.back() == '"')
                    {
                        value = value.substr(1, value.length() - 2);
                    }
                    config.global.eviction_policy = std::wstring(value.begin(), value.end());
                }
                else if (key == "cache_directory")
                {
                    if (value.front() == '"' && value.back() == '"')
                    {
                        value = value.substr(1, value.length() - 2);
                    }
                    config.global.cache_directory = std::wstring(value.begin(), value.end());
                }
                else if (key == "download_threads")
                {
                    config.global.download_threads = std::stoull(value);
                }
            }
        }
        else if (current_section == "global.metrics")
        {
            // Parse metrics configuration values
            std::regex metrics_regex(R"(^\s*([^:]+):\s*(.+)$)");
            std::smatch match;
            if (std::regex_match(line, match, metrics_regex))
            {
                std::string key = match[1].str();
                std::string value = match[2].str();

                if (key == "enabled")
                {
                    config.global.metrics.enabled = (value == "true" || value == "1");
                }
                else if (key == "bind_address")
                {
                    if (value.front() == '"' && value.back() == '"')
                    {
                        value = value.substr(1, value.length() - 2);
                    }
                    config.global.metrics.bind_address = value;
                }
                else if (key == "port")
                {
                    config.global.metrics.port = std::stoi(value);
                }
                else if (key == "endpoint_path")
                {
                    if (value.front() == '"' && value.back() == '"')
                    {
                        value = value.substr(1, value.length() - 2);
                    }
                    config.global.metrics.endpoint_path = value;
                }
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

std::vector<std::wstring> ConfigParser::parseStringArray(std::string_view yaml_array)
{
    std::vector<std::wstring> result;

    // Simple array parsing - supports both inline and multiline arrays
    std::string cleaned(yaml_array);

    // Remove square brackets if present
    if (cleaned.front() == '[' && cleaned.back() == ']')
    {
        cleaned = cleaned.substr(1, cleaned.length() - 2);
    }

    std::istringstream stream(cleaned);
    std::string item;

    while (std::getline(stream, item, ','))
    {
        // Remove leading/trailing whitespace and quotes
        item.erase(0, item.find_first_not_of(" \t\""));
        item.erase(item.find_last_not_of(" \t\"") + 1);

        if (!item.empty())
        {
            result.emplace_back(item.begin(), item.end());
        }
    }

    return result;
}

} // namespace CeWinFileCache