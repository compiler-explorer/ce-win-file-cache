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
    // JSON parsing methods
    static std::optional<Config> parseJsonFile(std::wstring_view file_path);
    static std::optional<Config> parseJsonString(std::string_view json_content);
};

} // namespace CeWinFileCache