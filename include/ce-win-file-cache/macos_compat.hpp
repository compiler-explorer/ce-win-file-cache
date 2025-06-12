#pragma once

// Minimal Windows type definitions for macOS compilation
// This file provides just enough Windows types to compile the code

#ifndef _WIN32

#include <cstdint>
#include <string>

// Basic Windows types
typedef uint32_t DWORD;
typedef int32_t BOOL;
typedef void *HANDLE;
typedef int32_t LONG;
typedef uint32_t ULONG;
typedef int32_t NTSTATUS;
typedef wchar_t *PWSTR;
typedef const wchar_t *PCWSTR;
typedef uint64_t ULONGLONG;
typedef uint64_t UINT64;
typedef void *PVOID;

// File time structure
struct FILETIME
{
    DWORD dwLowDateTime;
    DWORD dwHighDateTime;
};

// Windows constants
#define INVALID_HANDLE_VALUE ((HANDLE)(intptr_t)-1)
#define MAX_PATH 260
#define FALSE 0
#define TRUE 1

// Status codes
#define STATUS_SUCCESS ((NTSTATUS)0x00000000L)
#define STATUS_UNSUCCESSFUL ((NTSTATUS)0xC0000001L)
#define STATUS_PENDING ((NTSTATUS)0x00000103L)
#define STATUS_OBJECT_NAME_NOT_FOUND ((NTSTATUS)0xC0000034L)
#define STATUS_ACCESS_DENIED ((NTSTATUS)0xC0000022L)
#define STATUS_BUFFER_TOO_SMALL ((NTSTATUS)0xC0000023L)
#define STATUS_NO_MORE_FILES ((NTSTATUS)0x80000006L)
#define STATUS_DISK_FULL ((NTSTATUS)0xC000007FL)
#define STATUS_INVALID_HANDLE ((NTSTATUS)0xC0000008L)
#define STATUS_IO_PENDING ((NTSTATUS)0x00000103L)
#define STATUS_CANCELLED ((NTSTATUS)0xC0000120L)
#define NT_SUCCESS(Status) ((NTSTATUS)(Status) >= 0)

// Fake security structures
struct SECURITY_ATTRIBUTES
{
    DWORD nLength;
    void *lpSecurityDescriptor;
    BOOL bInheritHandle;
};

struct SECURITY_CAPABILITIES
{
    void *AppContainerSid;
    void *Capabilities;
    DWORD CapabilityCount;
    DWORD Reserved;
};

// Fake file attributes
#define FILE_ATTRIBUTE_DIRECTORY 0x00000010
#define FILE_ATTRIBUTE_NORMAL 0x00000080

// Function stubs
inline void CloseHandle(HANDLE)
{
}
inline DWORD GetLastError()
{
    return 0;
}
inline void GetSystemTimeAsFileTime(FILETIME *lpSystemTimeAsFileTime)
{
    // Mock implementation - set to zero
    lpSystemTimeAsFileTime->dwLowDateTime = 0;
    lpSystemTimeAsFileTime->dwHighDateTime = 0;
}

#endif // !_WIN32