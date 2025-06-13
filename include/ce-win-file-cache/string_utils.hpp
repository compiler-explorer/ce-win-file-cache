#pragma once

#include <string>

namespace CeWinFileCache
{

/**
 * Platform-specific string conversion utilities
 * Based on cross-platform string handling patterns
 */
class StringUtils
{
public:
    /**
     * Convert wide string to UTF-8 string
     * @param wide_str The wide string to convert
     * @return UTF-8 encoded string
     */
    static std::string wideToUtf8(const std::wstring &wide_str);

    /**
     * Convert UTF-8 string to wide string
     * @param utf8_str The UTF-8 string to convert
     * @return Wide string
     */
    static std::wstring utf8ToWide(const std::string &utf8_str);

private:
    /**
     * Fallback ASCII-only conversion for testing/development
     */
    static std::string wideToAsciiFallback(const std::wstring &wide_str);
    static std::wstring asciiToWideFallback(const std::string &ascii_str);
};

} // namespace CeWinFileCache