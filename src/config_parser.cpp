#include <ce-win-file-cache/config_parser.hpp>
#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>

namespace CeWinFileCache
{

std::optional<Config> ConfigParser::parseYamlFile(std::wstring_view file_path)
{
    std::ifstream file;
    auto filename = std::filesystem::path(file_path).string();
    file.open(filename, std::ios::in);
    if (!file.is_open())
    {
        return std::nullopt;
    }

    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

    return parseYamlString(content);
}

std::optional<Config> ConfigParser::parseYamlString(std::string yaml_content)
{
    Config config;

    std::istringstream stream(yaml_content.data());
    std::string line;
    std::string current_section;
    std::string current_compiler;

    while (std::getline(stream, line))
    {
        // Remove leading/trailing whitespace
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
            std::regex compiler_regex(R"(^\s+([^:]+):\s*$)");
            std::smatch match;

            if (std::regex_match(line, match, compiler_regex))
            {
                current_compiler = match[1].str();
                config.compilers[std::wstring(current_compiler.begin(), current_compiler.end())] = CompilerConfig{};
                continue;
            }

            if (!current_compiler.empty())
            {
                std::wstring compiler_name(current_compiler.begin(), current_compiler.end());
                auto &compiler_config = config.compilers[compiler_name];

                // Parse compiler config properties
                std::regex property_regex(R"(^\s+([^:]+):\s*(.+)$)");
                if (std::regex_match(line, match, property_regex))
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
            // Parse global config
            std::regex property_regex(R"(^\s+([^:]+):\s*(.+)$)");
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

std::vector<std::wstring> ConfigParser::parseStringArray(const std::string &yaml_array)
{
    std::vector<std::wstring> result;

    // Simple array parsing - supports both inline and multiline arrays
    std::string cleaned = yaml_array;

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