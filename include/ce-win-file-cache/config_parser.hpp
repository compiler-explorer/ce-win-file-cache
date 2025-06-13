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
    static std::optional<Config> parseYamlFile(std::wstring_view file_path);
    static std::optional<Config> parseYamlString(std::string_view yaml_content);

    private:
    static std::vector<std::wstring> parseStringArray(std::string_view yaml_array);
};

} // namespace CeWinFileCache