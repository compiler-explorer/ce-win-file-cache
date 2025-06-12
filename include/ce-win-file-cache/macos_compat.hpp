#pragma once

// Minimal Windows type definitions for macOS compilation
// This file provides just enough Windows types to compile the code

#ifndef _WIN32

#include <cstdint>
#include <string>

// Basic Windows types
typedef uint32_t DWORD;
typedef int32_t BOOL;
typedef void* HANDLE;
typedef int32_t LONG;
typedef uint32_t ULONG;
typedef int32_t NTSTATUS;
typedef wchar_t* PWSTR;
typedef const wchar_t* PCWSTR;
typedef uint64_t ULONGLONG;
typedef void* PVOID;

// Windows constants
#define INVALID_HANDLE_VALUE ((HANDLE)(intptr_t)-1)
#define MAX_PATH 260
#define FALSE 0
#define TRUE 1

// Status codes
#define STATUS_SUCCESS ((NTSTATUS)0x00000000L)
#define STATUS_UNSUCCESSFUL ((NTSTATUS)0xC0000001L)
#define NT_SUCCESS(Status) ((NTSTATUS)(Status) >= 0)

// Fake security structures
struct SECURITY_ATTRIBUTES {
    DWORD nLength;
    void* lpSecurityDescriptor;
    BOOL bInheritHandle;
};

struct SECURITY_CAPABILITIES {
    void* AppContainerSid;
    void* Capabilities;
    DWORD CapabilityCount;
    DWORD Reserved;
};

// Fake file attributes
#define FILE_ATTRIBUTE_DIRECTORY 0x00000010
#define FILE_ATTRIBUTE_NORMAL    0x00000080

// Function stubs
inline void CloseHandle(HANDLE) {}
inline DWORD GetLastError() { return 0; }

#endif // !_WIN32