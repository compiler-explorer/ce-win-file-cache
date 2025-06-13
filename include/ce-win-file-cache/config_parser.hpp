#pragma once

#include "../types/config.hpp"
#include <optional>
#include <string>
#include <string_view>

namespace CeWinFileCache
{

class ConfigParser
{
    public:
    // JSON parsing methods (preferred)
    static std::optional<Config> parseJsonFile(std::wstring_view file_path);
    static std::optional<Config> parseJsonString(std::string_view json_content);
    
    // YAML parsing methods (legacy support)
    static std::optional<Config> parseYamlFile(std::wstring_view file_path);
    static std::optional<Config> parseYamlString(std::string_view yaml_content);

    private:
    static std::vector<std::wstring> parseStringArray(std::string_view yaml_array);
};

} // namespace CeWinFileCache