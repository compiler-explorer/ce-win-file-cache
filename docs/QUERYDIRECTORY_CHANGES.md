# QueryDirectory Implementation Changes

## Overview

This document describes changes made to the `ReadDirectory` and `Open` implementations to address Windows Explorer Properties dialog behavior and QueryDirectory operations.

## Problem Identified

Windows Explorer Properties dialog was not showing file size and timestamps for files. ProcMon analysis revealed differences between our filesystem's QueryDirectory behavior and real Windows filesystems:

- **Our filesystem**: `QueryDirectory NO MORE FILES`
- **Real Windows**: `QueryDirectory SUCCESS`

## Changes Made

### 1. ReadDirectory Return Value Changes

**File**: `src/hybrid_filesystem.cpp`
**Lines**: ~951-955

**Before**:
```cpp
if (entries_returned == 0)
{
    Logger::debug(LogCategory::FILESYSTEM, "ReadDirectory() - no entries returned");
    return STATUS_NO_MORE_FILES;
}
```

**After**:
```cpp
if (entries_returned == 0)
{
    Logger::debug(LogCategory::FILESYSTEM, "ReadDirectory() - no entries returned, returning SUCCESS with 0 bytes");
    return STATUS_SUCCESS; // Real Windows filesystem returns SUCCESS, not NO_MORE_FILES
}
```

### 2. handleFileAsDirectoryEntry Method

**File**: `src/hybrid_filesystem.cpp`
**Lines**: ~1586-1691

**Added**: Complete new method `handleFileAsDirectoryEntry()` to handle Windows special behavior where QueryDirectory can be called on file paths directly.

**Key features**:
- Extracts filename from virtual path
- Handles marker-based continuation correctly
- Supports pattern matching
- Returns `STATUS_SUCCESS` with 0 bytes for continuation calls (matching Windows behavior)

**Method signature**:
```cpp
NTSTATUS handleFileAsDirectoryEntry(CacheEntry *entry, PWSTR Pattern, PWSTR Marker, PVOID Buffer, ULONG Length, PULONG PBytesTransferred);
```

### 3. ReadDirectory File Path Detection

**File**: `src/hybrid_filesystem.cpp`
**Lines**: ~804-817

**Added**: Special case handling in `ReadDirectory()` to detect when called on file paths:

```cpp
// Handle special case: ReadDirectory called on a file path
// Windows filesystem allows QueryDirectory on files to get their directory entry
if (!file_desc->entry || !(file_desc->entry->file_attributes & FILE_ATTRIBUTE_DIRECTORY))
{
    Logger::info(LogCategory::FILESYSTEM, "ReadDirectory() called on file: '{}' - returning file as single directory entry",
                 file_desc->entry ? StringUtils::wideToUtf8(file_desc->entry->virtual_path) : "null");

    if (!file_desc->entry)
    {
        *PBytesTransferred = 0;
        return STATUS_OBJECT_NAME_NOT_FOUND;
    }

    // Return this file as a single directory entry (like real Windows filesystem)
    return handleFileAsDirectoryEntry(file_desc->entry, Pattern, Marker, Buffer, Length, PBytesTransferred);
}
```

### 4. Open Method Validation Changes

**File**: `src/hybrid_filesystem.cpp`
**Lines**: ~385-391

**Before**:
```cpp
if (wants_directory && !is_directory)
{
    Logger::error(LogCategory::FILESYSTEM, "Open() - FILE_DIRECTORY_FILE requested for non-directory: '{}'", StringUtils::wideToUtf8(virtual_path));
    return STATUS_NOT_A_DIRECTORY;
}
```

**After**:
```cpp
// Special case: Windows allows FILE_DIRECTORY_FILE on files for QueryDirectory operations
// This is used by Explorer to query a file as if it were a directory entry
if (wants_directory && !is_directory)
{
    Logger::debug(LogCategory::FILESYSTEM, "Open() - FILE_DIRECTORY_FILE requested for file: '{}' - allowing for QueryDirectory", StringUtils::wideToUtf8(virtual_path));
    // Allow this for the special Windows QueryDirectory behavior - don't return error
}
```

### 5. Header File Declaration

**File**: `include/ce-win-file-cache/hybrid_filesystem.hpp`
**Lines**: ~67

**Added**: Method declaration in header:
```cpp
NTSTATUS handleFileAsDirectoryEntry(CacheEntry *entry, PWSTR Pattern, PWSTR Marker, PVOID Buffer, ULONG Length, PULONG PBytesTransferred);
```

## Behavioral Changes

### Before Changes
1. QueryDirectory with 0 results returned `STATUS_NO_MORE_FILES`
2. `Open()` with `FILE_DIRECTORY_FILE` on files returned `STATUS_NOT_A_DIRECTORY`
3. No special handling for QueryDirectory called directly on file paths

### After Changes
1. QueryDirectory with 0 results returns `STATUS_SUCCESS` with 0 bytes transferred
2. `Open()` allows `FILE_DIRECTORY_FILE` on files (logs but doesn't error)
3. QueryDirectory on file paths handled specially via `handleFileAsDirectoryEntry()`

## Expected Impact

These changes align our filesystem behavior more closely with real Windows filesystems:

- **ProcMon logs should show**: `QueryDirectory SUCCESS` instead of `QueryDirectory NO MORE FILES`
- **Explorer Properties dialog**: May now display correct file size and timestamps
- **File operations**: Should work more seamlessly with Windows applications expecting standard filesystem behavior

## Testing

The changes maintain backward compatibility while adding Windows-standard behavior. All existing functionality should continue to work, with improved compatibility for Windows shell operations.

## Notes

- Changes follow Windows filesystem semantics where `QueryDirectory` can be called on file paths
- The `handleFileAsDirectoryEntry` method creates proper directory entry structures for files
- Logging has been enhanced to track these special-case operations
- The implementation handles marker-based directory enumeration continuation correctly