#pragma once

// Wine compatibility header for cross-compilation
#ifdef WINE_CROSS_COMPILE

// Include Wine stub headers first  
#include "../wine_stubs/devioctl.h"

// Prevent Windows min/max macro conflicts with C++ STL
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

// Define Windows architecture macros for WinFsp compatibility
#ifdef __x86_64__
#ifndef _AMD64_
#define _AMD64_
#endif
#elif defined(__i386__)
#ifndef _X86_
#define _X86_
#endif
#elif defined(__aarch64__)
#ifndef _ARM64_
#define _ARM64_
#endif
#endif

#include <windows.h>
#include <winnetwk.h>
#include <strsafe.h>

// Undefine conflicting macros if they still exist
#ifdef min
#undef min
#endif
#ifdef max  
#undef max
#endif

// Wine doesn't always have all the latest Windows API definitions
// Add any missing definitions here

#ifndef STATUS_SUCCESS
#define STATUS_SUCCESS ((NTSTATUS)0x00000000L)
#endif

#ifndef STATUS_UNSUCCESSFUL
#define STATUS_UNSUCCESSFUL ((NTSTATUS)0xC0000001L)
#endif

#ifndef STATUS_INVALID_PARAMETER
#define STATUS_INVALID_PARAMETER ((NTSTATUS)0xC000000DL)
#endif

#ifndef STATUS_OBJECT_NAME_NOT_FOUND
#define STATUS_OBJECT_NAME_NOT_FOUND ((NTSTATUS)0xC0000034L)
#endif

#ifndef STATUS_OBJECT_NAME_INVALID
#define STATUS_OBJECT_NAME_INVALID ((NTSTATUS)0xC0000033L)
#endif

#ifndef STATUS_BUFFER_TOO_SMALL
#define STATUS_BUFFER_TOO_SMALL ((NTSTATUS)0xC0000023L)
#endif

#ifndef STATUS_NO_MORE_FILES
#define STATUS_NO_MORE_FILES ((NTSTATUS)0x80000006L)
#endif

#ifndef STATUS_PENDING
#define STATUS_PENDING ((NTSTATUS)0x00000103L)
#endif

#ifndef STATUS_DISK_FULL
#define STATUS_DISK_FULL ((NTSTATUS)0xC000007FL)
#endif

#ifndef STATUS_INVALID_HANDLE
#define STATUS_INVALID_HANDLE ((NTSTATUS)0xC0000008L)
#endif

#ifndef STATUS_UNEXPECTED_IO_ERROR
#define STATUS_UNEXPECTED_IO_ERROR ((NTSTATUS)0xC00000E9L)
#endif

#ifndef STATUS_INVALID_DEVICE_REQUEST
#define STATUS_INVALID_DEVICE_REQUEST ((NTSTATUS)0xC0000010L)
#endif

// Helper macros for Wine compatibility
#ifndef NT_SUCCESS
#define NT_SUCCESS(Status) (((NTSTATUS)(Status)) >= 0)
#endif

#ifndef NTSTATUS_FROM_WIN32
#define NTSTATUS_FROM_WIN32(x) \
    ((NTSTATUS)(x) <= 0 ? ((NTSTATUS)(x)) : ((NTSTATUS) (((x) & 0x0000FFFF) | (FACILITY_WIN32 << 16) | ERROR_SEVERITY_ERROR)))
#endif

// Wine may not have SHCreateDirectoryEx
#ifndef SHCreateDirectoryExW
HRESULT WINAPI SHCreateDirectoryExW(HWND hwnd, LPCWSTR pszPath, const SECURITY_ATTRIBUTES *psa);
#endif

// Wine-specific workarounds
namespace CeWinFileCache 
{
namespace WineCompat 
{

// Helper function to work around potential Wine limitations
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
            return NTSTATUS_FROM_WIN32(error);
    }
}

} // namespace WineCompat
} // namespace CeWinFileCache

#endif // WINE_CROSS_COMPILE