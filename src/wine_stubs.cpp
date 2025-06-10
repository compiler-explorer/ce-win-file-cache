#ifdef WINE_CROSS_COMPILE

#include "../include/compiler_cache/windows_compat.hpp"
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <memory>
#include <string>
#include <vector>

// C++ implementation of CommandLineToArgvW for Wine
extern "C" WCHAR **WINAPI CommandLineToArgvW(LPCWSTR lpCmdLine, int *pNumArgs)
{
    if (!lpCmdLine || !pNumArgs)
    {
        *pNumArgs = 0;
        return nullptr;
    }

    std::wstring cmdLine(lpCmdLine);
    std::vector<std::wstring> args;

    // Simple tokenization - split on spaces
    // This is a basic implementation for Wine compatibility
    if (!cmdLine.empty())
    {
        size_t start = 0;
        size_t end = 0;

        while ((end = cmdLine.find(L' ', start)) != std::wstring::npos)
        {
            if (end != start)
            {
                args.push_back(cmdLine.substr(start, end - start));
            }
            start = end + 1;
        }

        // Add the last token
        if (start < cmdLine.length())
        {
            args.push_back(cmdLine.substr(start));
        }

        // If no spaces found, use the whole string
        if (args.empty())
        {
            args.push_back(cmdLine);
        }
    }

    *pNumArgs = static_cast<int>(args.size());

    if (args.empty())
    {
        return nullptr;
    }

    // Allocate array of WCHAR* pointers
    WCHAR **argv = static_cast<WCHAR **>(LocalAlloc(LMEM_FIXED, sizeof(WCHAR *) * args.size()));
    if (!argv)
    {
        *pNumArgs = 0;
        return nullptr;
    }

    // Allocate and copy each argument string
    for (size_t i = 0; i < args.size(); ++i)
    {
        size_t len = args[i].length() + 1;
        argv[i] = static_cast<WCHAR *>(LocalAlloc(LMEM_FIXED, len * sizeof(WCHAR)));
        if (!argv[i])
        {
            // Clean up previously allocated strings
            for (size_t j = 0; j < i; ++j)
            {
                LocalFree(argv[j]);
            }
            LocalFree(argv);
            *pNumArgs = 0;
            return nullptr;
        }

        wcscpy(argv[i], args[i].c_str());
    }

    return argv;
}

// Stub implementation of SHCreateDirectoryExW for Wine
extern "C" HRESULT WINAPI SHCreateDirectoryExW(HWND hwnd, LPCWSTR pszPath, const SECURITY_ATTRIBUTES *psa)
{
    // Simple implementation using CreateDirectoryW
    if (CreateDirectoryW(pszPath, (SECURITY_ATTRIBUTES *)psa))
    {
        return S_OK;
    }
    return HRESULT_FROM_WIN32(GetLastError());
}

// Stub implementations for Wine network functions
// These should be provided by Wine's mpr.dll but may not link properly
extern "C" DWORD WINAPI WNetAddConnection2W(LPNETRESOURCEW lpNetResource, LPCWSTR lpPassword, LPCWSTR lpUserName, DWORD dwFlags)
{
    // Minimal stub for Wine compatibility
    if (!lpNetResource || !lpNetResource->lpRemoteName)
    {
        return ERROR_INVALID_PARAMETER;
    }

    // Accept UNC paths
    if (wcslen(lpNetResource->lpRemoteName) >= 2 && lpNetResource->lpRemoteName[0] == L'\\' && lpNetResource->lpRemoteName[1] == L'\\')
    {
        return NO_ERROR;
    }

    return ERROR_BAD_NET_NAME;
}

extern "C" DWORD WINAPI WNetCancelConnection2W(LPCWSTR lpName, DWORD dwFlags, BOOL fForce)
{
    // Minimal stub for Wine compatibility
    if (!lpName)
    {
        return ERROR_INVALID_PARAMETER;
    }

    return NO_ERROR;
}

#endif // WINE_CROSS_COMPILE