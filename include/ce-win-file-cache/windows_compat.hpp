#pragma once

// Unified Windows compatibility header
// Handles Wine cross-compilation and native Windows compilation

#ifdef NO_WINFSP
// macOS/Linux compatibility mode
#include "macos_compat.hpp"
#elif defined(WINE_CROSS_COMPILE)
#include "wine_compat.hpp"
#else
// Native Windows includes
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include "alfaheader.h"
#include <ntstatus.h>
#include <shellapi.h>
#include <shlobj.h>
#include <strsafe.h>
#include <winnetwk.h>
#include <winternl.h>

// Define PNTSTATUS if not already defined
#ifndef PNTSTATUS
typedef NTSTATUS *PNTSTATUS;
#endif

// Native Windows compatibility namespace
namespace CeWinFileCache
{
namespace WineCompat
{

// Helper function to convert Win32 errors to NTSTATUS for native Windows
inline NTSTATUS NtStatusFromWin32(DWORD error)
{
    if (error == ERROR_SUCCESS)
        return STATUS_SUCCESS;

    // Map common Win32 errors to NTSTATUS
    switch (error)
    {
    case ERROR_FILE_NOT_FOUND:
    case ERROR_PATH_NOT_FOUND:
        return STATUS_OBJECT_NAME_NOT_FOUND;
    case ERROR_INVALID_NAME:
        return STATUS_OBJECT_NAME_INVALID;
    case ERROR_ACCESS_DENIED:
        return STATUS_ACCESS_DENIED;
    case ERROR_INSUFFICIENT_BUFFER:
        return STATUS_BUFFER_TOO_SMALL;
    case ERROR_NO_MORE_FILES:
        return STATUS_NO_MORE_FILES;
    case ERROR_DISK_FULL:
        return STATUS_DISK_FULL;
    case ERROR_INVALID_HANDLE:
        return STATUS_INVALID_HANDLE;
    default:
        return HRESULT_FROM_WIN32(error);
    }
}

// Helper for converting GetLastError() to NTSTATUS on native Windows
inline NTSTATUS GetLastErrorAsNtStatus()
{
    return NtStatusFromWin32(GetLastError());
}

} // namespace WineCompat
} // namespace CeWinFileCache

#endif