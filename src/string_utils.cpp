#include <ce-win-file-cache/string_utils.hpp>
#include <iostream>

#ifdef _WIN32
#include <windows.h>
#else
#include <locale>
#include <clocale>
#include <cwchar>
#include <cstdlib>
#include <cstring>
#endif

namespace CeWinFileCache
{

std::string StringUtils::wideToUtf8(const std::wstring &wide_str)
{
    if (wide_str.empty())
    {
        return std::string();
    }

#ifdef _WIN32
    // Windows: Use WideCharToMultiByte
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, wide_str.c_str(), static_cast<int>(wide_str.size()), 
                                         nullptr, 0, nullptr, nullptr);
    if (size_needed <= 0)
    {
        // Fallback to ASCII conversion
        return wideToAsciiFallback(wide_str);
    }

    std::string result(size_needed, 0);
    int result_size = WideCharToMultiByte(CP_UTF8, 0, wide_str.c_str(), static_cast<int>(wide_str.size()), 
                                         &result[0], size_needed, nullptr, nullptr);
    if (result_size <= 0)
    {
        // Fallback to ASCII conversion
        return wideToAsciiFallback(wide_str);
    }

    return result;

#else
    // Unix/Linux: Use wcstombs with UTF-8 locale
    
    // Set locale to UTF-8 if not already set
    const char* current_locale = setlocale(LC_CTYPE, nullptr);
    bool locale_set = false;
    if (current_locale == nullptr || strstr(current_locale, "UTF-8") == nullptr)
    {
        if (setlocale(LC_CTYPE, "C.UTF-8") != nullptr || 
            setlocale(LC_CTYPE, "en_US.UTF-8") != nullptr ||
            setlocale(LC_CTYPE, "C.utf8") != nullptr)
        {
            locale_set = true;
        }
    }

    // Calculate required buffer size
    size_t size_needed = wcstombs(nullptr, wide_str.c_str(), 0);
    if (size_needed == static_cast<size_t>(-1))
    {
        // Conversion failed, use ASCII fallback
        return wideToAsciiFallback(wide_str);
    }

    // Perform the conversion
    std::string result(size_needed, 0);
    size_t result_size = wcstombs(&result[0], wide_str.c_str(), size_needed);
    if (result_size == static_cast<size_t>(-1))
    {
        // Conversion failed, use ASCII fallback
        return wideToAsciiFallback(wide_str);
    }

    // Restore previous locale if we changed it
    if (locale_set && current_locale != nullptr)
    {
        setlocale(LC_CTYPE, current_locale);
    }

    return result;
#endif
}

std::wstring StringUtils::utf8ToWide(const std::string &utf8_str)
{
    if (utf8_str.empty())
    {
        return std::wstring();
    }

#ifdef _WIN32
    // Windows: Use MultiByteToWideChar
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, utf8_str.c_str(), static_cast<int>(utf8_str.size()), 
                                         nullptr, 0);
    if (size_needed <= 0)
    {
        // Fallback to ASCII conversion
        return asciiToWideFallback(utf8_str);
    }

    std::wstring result(size_needed, 0);
    int result_size = MultiByteToWideChar(CP_UTF8, 0, utf8_str.c_str(), static_cast<int>(utf8_str.size()), 
                                         &result[0], size_needed);
    if (result_size <= 0)
    {
        // Fallback to ASCII conversion
        return asciiToWideFallback(utf8_str);
    }

    return result;

#else
    // Unix/Linux: Use mbstowcs with UTF-8 locale
    
    // Set locale to UTF-8 if not already set
    const char* current_locale = setlocale(LC_CTYPE, nullptr);
    bool locale_set = false;
    if (current_locale == nullptr || strstr(current_locale, "UTF-8") == nullptr)
    {
        if (setlocale(LC_CTYPE, "C.UTF-8") != nullptr || 
            setlocale(LC_CTYPE, "en_US.UTF-8") != nullptr ||
            setlocale(LC_CTYPE, "C.utf8") != nullptr)
        {
            locale_set = true;
        }
    }

    // Calculate required buffer size
    size_t size_needed = mbstowcs(nullptr, utf8_str.c_str(), 0);
    if (size_needed == static_cast<size_t>(-1))
    {
        // Conversion failed, use ASCII fallback
        return asciiToWideFallback(utf8_str);
    }

    // Perform the conversion
    std::wstring result(size_needed, 0);
    size_t result_size = mbstowcs(&result[0], utf8_str.c_str(), size_needed);
    if (result_size == static_cast<size_t>(-1))
    {
        // Conversion failed, use ASCII fallback
        return asciiToWideFallback(utf8_str);
    }

    // Restore previous locale if we changed it
    if (locale_set && current_locale != nullptr)
    {
        setlocale(LC_CTYPE, current_locale);
    }

    return result;
#endif
}

std::string StringUtils::wideToAsciiFallback(const std::wstring &wide_str)
{
    std::string result;
    result.reserve(wide_str.size());
    
    for (wchar_t wc : wide_str)
    {
        if (wc <= 127)  // ASCII range
        {
            result += static_cast<char>(wc);
        }
        else
        {
            result += '?';  // Replace non-ASCII with placeholder
        }
    }
    
    return result;
}

std::wstring StringUtils::asciiToWideFallback(const std::string &ascii_str)
{
    std::wstring result;
    result.reserve(ascii_str.size());
    
    for (char c : ascii_str)
    {
        result += static_cast<wchar_t>(static_cast<unsigned char>(c));
    }
    
    return result;
}

unsigned long StringUtils::parseULong(wchar_t* arg)
{
    return wcstoul(static_cast<wchar_t*>(arg), nullptr, 0);
}

const wchar_t* StringUtils::getNextArg(wchar_t* argv[], int& index, int argc)
{
    if (index + 1 < argc)
    {
        return argv[++index];
    }
    return nullptr;
}

} // namespace CeWinFileCache