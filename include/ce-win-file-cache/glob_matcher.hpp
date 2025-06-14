#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace CeWinFileCache
{

/**
 * GlobMatcher provides cross-platform glob pattern matching functionality.
 *
 * Supported patterns:
 * - * : Match any sequence of characters (except path separators)
 * - ** : Match any sequence of directories (recursive)
 * - ? : Match exactly one character (except path separators)
 *
 * Platform behavior:
 * - Windows: Case-insensitive matching, supports both / and \ separators
 * - Unix: Case-sensitive matching, / separator only
 *
 * Examples:
 * - "*.exe" matches "test.exe" but not "test.exe.backup"
 * - "bin/ *.dll" matches "bin/library.dll" but not "bin/sub/library.dll"
 * - "include/ **\/ *.h" matches "include/stdio.h" and "include/sys/types.h"
 */
class GlobMatcher
{
    public:
    /**
     * Test if a file path matches a glob pattern.
     *
     * @param path The file path to test (e.g., "bin/cl.exe")
     * @param pattern The glob pattern (e.g., "bin/ *.exe")
     * @return true if the path matches the pattern
     */
    static bool matches(std::wstring_view path, std::wstring_view pattern);

    /**
     * Test if a file path matches any pattern in a list.
     *
     * @param path The file path to test
     * @param patterns Vector of glob patterns to test against
     * @return true if the path matches any pattern
     */
    static bool matchesAny(std::wstring_view path, const std::vector<std::wstring> &patterns);

    private:
    /**
     * Internal recursive matching function.
     *
     * @param path Current path segment being matched
     * @param pattern Current pattern segment being matched
     * @return true if the segments match
     */
    static bool matchesRecursive(std::wstring_view path, std::wstring_view pattern);

    /**
     * Check if a character is a path separator (/ or \).
     *
     * @param c Character to test
     * @return true if c is a path separator
     */
    static bool isPathSeparator(wchar_t c);

    /**
     * Normalize path separators for cross-platform matching.
     * Converts all backslashes to forward slashes.
     *
     * @param path Path to normalize
     * @return Normalized path string
     */
    static std::wstring normalizePath(std::wstring_view path);

    /**
     * Compare characters with platform-appropriate case sensitivity.
     *
     * @param a First character
     * @param b Second character
     * @return true if characters match (case-insensitive on Windows)
     */
    static bool charsEqual(wchar_t a, wchar_t b);
};

} // namespace CeWinFileCache