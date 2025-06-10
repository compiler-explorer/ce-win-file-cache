#pragma once

#include "../types/config.hpp"
#include <string>
#include <optional>

namespace CeWinFileCache
{

class ConfigParser
{
public:
    static std::optional<Config> parseYamlFile(std::wstring_view file_path);
    static std::optional<Config> parseYamlString(std::string yaml_content);
    
private:
    static CompilerConfig parseCompilerConfig(const std::string& yaml_section);
    static GlobalConfig parseGlobalConfig(const std::string& yaml_section);
    static std::vector<std::wstring> parseStringArray(const std::string& yaml_array);
};

} // namespace CeWinFileCache