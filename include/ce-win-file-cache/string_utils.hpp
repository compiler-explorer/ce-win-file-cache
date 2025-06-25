#pragma once

#include <string>
#include <string_view>

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

    /**
     * Parse command-line argument as unsigned long
     * @param arg Single command-line argument string
     * @return Parsed unsigned long value
     */
    static unsigned long parseULong(wchar_t *arg);

    /**
     * Get next command-line argument safely
     * @param argv Command-line arguments array
     * @param index Current argument index (will be incremented)
     * @param argc Total argument count
     * @return Next argument as wide string, or nullptr if no more arguments
     */
    static const wchar_t *getNextArg(wchar_t *argv[], int &index, int argc);

    private:
    /**
     * Fallback ASCII-only conversion for testing/development
     */
    static std::string wideToAsciiFallback(const std::wstring &wide_str);
    static std::wstring asciiToWideFallback(const std::string &ascii_str);
};

} // namespace CeWinFileCache