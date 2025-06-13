#include <ce-win-file-cache/glob_matcher.hpp>
#include <algorithm>
#include <cctype>

#ifdef _WIN32
#include <windows.h>
#endif

namespace CeWinFileCache
{

bool GlobMatcher::matches(std::wstring_view path, std::wstring_view pattern)
{
    // Normalize paths for cross-platform matching
    std::wstring normalized_path = normalizePath(path);
    std::wstring normalized_pattern = normalizePath(pattern);
    
    return matchesRecursive(normalized_path, normalized_pattern);
}

bool GlobMatcher::matchesAny(std::wstring_view path, const std::vector<std::wstring> &patterns)
{
    for (const auto &pattern : patterns)
    {
        if (matches(path, pattern))
        {
            return true;
        }
    }
    return false;
}

bool GlobMatcher::matchesRecursive(std::wstring_view path, std::wstring_view pattern)
{
    size_t path_pos = 0;
    size_t pattern_pos = 0;
    
    while (pattern_pos < pattern.length())
    {
        wchar_t pattern_char = pattern[pattern_pos];
        
        if (pattern_char == L'*')
        {
            // Check for ** (recursive wildcard)
            if (pattern_pos + 1 < pattern.length() && pattern[pattern_pos + 1] == L'*')
            {
                // Handle ** pattern (matches zero or more directories)
                pattern_pos += 2;
                
                // Skip optional path separator after **
                if (pattern_pos < pattern.length() && isPathSeparator(pattern[pattern_pos]))
                {
                    pattern_pos++;
                }
                
                // If ** is at the end, it matches everything remaining
                if (pattern_pos >= pattern.length())
                {
                    return true;
                }
                
                // Try matching the rest of the pattern at every possible position
                for (size_t i = path_pos; i <= path.length(); i++)
                {
                    if (matchesRecursive(path.substr(i), pattern.substr(pattern_pos)))
                    {
                        return true;
                    }
                    
                    // Advance to next directory boundary for ** matching
                    if (i < path.length() && isPathSeparator(path[i]))
                    {
                        continue;
                    }
                    
                    // Find next path separator
                    while (i < path.length() && !isPathSeparator(path[i]))
                    {
                        i++;
                    }
                }
                
                return false;
            }
            else
            {
                // Handle single * pattern (matches zero or more chars, not crossing directories)
                pattern_pos++;
                
                // If * is at the end, match everything until path separator or end
                if (pattern_pos >= pattern.length())
                {
                    // Make sure we don't cross directory boundaries
                    for (size_t i = path_pos; i < path.length(); i++)
                    {
                        if (isPathSeparator(path[i]))
                        {
                            return false;
                        }
                    }
                    return true;
                }
                
                // Try matching the rest of the pattern at every possible position
                wchar_t next_pattern_char = pattern[pattern_pos];
                for (size_t i = path_pos; i <= path.length(); i++)
                {
                    // Don't let * cross directory boundaries
                    if (i < path.length() && isPathSeparator(path[i]))
                    {
                        break;
                    }
                    
                    if (i < path.length() && charsEqual(path[i], next_pattern_char))
                    {
                        if (matchesRecursive(path.substr(i), pattern.substr(pattern_pos)))
                        {
                            return true;
                        }
                    }
                    else if (next_pattern_char == L'?' || next_pattern_char == L'*')
                    {
                        if (matchesRecursive(path.substr(i), pattern.substr(pattern_pos)))
                        {
                            return true;
                        }
                    }
                }
                
                return false;
            }
        }
        else if (pattern_char == L'?')
        {
            // ? matches exactly one character (not path separator)
            if (path_pos >= path.length() || isPathSeparator(path[path_pos]))
            {
                return false;
            }
            path_pos++;
            pattern_pos++;
        }
        else
        {
            // Regular character matching
            if (path_pos >= path.length() || !charsEqual(path[path_pos], pattern_char))
            {
                return false;
            }
            path_pos++;
            pattern_pos++;
        }
    }
    
    // Pattern consumed, check if path is also consumed
    return path_pos == path.length();
}

bool GlobMatcher::isPathSeparator(wchar_t c)
{
    return c == L'/' || c == L'\\';
}

std::wstring GlobMatcher::normalizePath(std::wstring_view path)
{
    std::wstring result;
    result.reserve(path.length());
    
    for (wchar_t c : path)
    {
        if (c == L'\\')
        {
            result += L'/';
        }
        else
        {
            result += c;
        }
    }
    
    return result;
}

bool GlobMatcher::charsEqual(wchar_t a, wchar_t b)
{
#ifdef _WIN32
    // Case-insensitive comparison on Windows
    return towlower(a) == towlower(b);
#else
    // Case-sensitive comparison on Unix
    return a == b;
#endif
}

} // namespace CeWinFileCache