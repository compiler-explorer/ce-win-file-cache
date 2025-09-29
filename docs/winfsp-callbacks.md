# WinFSP Callback Entry Points

This document describes the WinFSP filesystem callback entry points implemented in our `HybridFileSystem` class and their purposes according to the official WinFSP documentation.

## Overview

Our `HybridFileSystem` class inherits from `Fsp::FileSystemBase` and implements various callback methods that are called by the WinFSP kernel driver when file system operations are requested. These callbacks form the interface between the Windows kernel and our user-mode filesystem implementation.

**Source Location**: `include/ce-win-file-cache/hybrid_filesystem.hpp:21`

## Core Callback Functions

### 1. Init
**Function Signature**: `NTSTATUS Init(PVOID Host) override`
**Source Location**: `hybrid_filesystem.hpp:32`

**Purpose**: Called during filesystem initialization to set up the file system instance.
- Performs initial setup and configuration
- Called once when the filesystem is mounted
- Must return success for the filesystem to become operational

### 2. GetVolumeInfo
**Function Signature**: `NTSTATUS GetVolumeInfo(VolumeInfo *VolumeInfo) override`
**Source Location**: `hybrid_filesystem.hpp:33`

**Purpose**: Query volume information and characteristics.
- Returns total and free space on the volume
- Provides volume-level metadata like volume label
- Called when applications query disk space or volume properties

### 3. GetSecurityByName
**Function Signature**: `NTSTATUS GetSecurityByName(PWSTR FileName, PUINT32 PFileAttributes, PSECURITY_DESCRIPTOR SecurityDescriptor, SIZE_T *PSecurityDescriptorSize) override`
**Source Location**: `hybrid_filesystem.hpp:34`

**Purpose**: Retrieve essential file metadata before opening a file.
- Gets file attributes (directory, hidden, read-only, etc.)
- Retrieves security descriptor for access control
- Performs initial access checks
- Called before file operations to determine if access should be granted
- **Critical for security**: This is where access permissions are enforced

### 4. Open
**Function Signature**: `NTSTATUS Open(PWSTR FileName, UINT32 CreateOptions, UINT32 GrantedAccess, PVOID *PFileNode, PVOID *PFileDesc, OpenFileInfo *OpenFileInfo) override`
**Source Location**: `hybrid_filesystem.hpp:36`

**Purpose**: Open existing files and directories.
- Creates file context and file descriptor
- Opens underlying file handle (cache or network)
- Returns file information to the kernel
- **Must never create new files** - only opens existing ones
- Sets up the file descriptor that will be used for subsequent operations

### 5. Close
**Function Signature**: `VOID Close(PVOID FileNode, PVOID FileDesc) override`
**Source Location**: `hybrid_filesystem.hpp:37`

**Purpose**: Final cleanup when a file handle is closed.
- Closes underlying file handle
- Deletes directory buffer (if applicable)
- Frees file context memory
- Called when the last handle to a file is closed

### 6. Read
**Function Signature**: `NTSTATUS Read(PVOID FileNode, PVOID FileDesc, PVOID Buffer, UINT64 Offset, ULONG Length, PULONG PBytesTransferred) override`
**Source Location**: `hybrid_filesystem.hpp:38`

**Purpose**: Read file contents from storage.
- Reads specified bytes from file at given offset
- Transfers data to provided buffer
- Updates bytes transferred count
- Core operation for file data access

### 7. GetFileInfo
**Function Signature**: `NTSTATUS GetFileInfo(PVOID FileNode, PVOID FileDesc, FileInfo *FileInfo) override`
**Source Location**: `hybrid_filesystem.hpp:39`

**Purpose**: Retrieve current file metadata for an open file.
- Returns file attributes, sizes, and timestamps
- Provides up-to-date file information
- Called when applications query file properties

### 8. SetBasicInfo
**Function Signature**: `NTSTATUS SetBasicInfo(PVOID FileNode, PVOID FileDesc, UINT32 FileAttributes, UINT64 CreationTime, UINT64 LastAccessTime, UINT64 LastWriteTime, UINT64 ChangeTime, FileInfo *FileInfo) override`
**Source Location**: `hybrid_filesystem.hpp:40`

**Purpose**: Update file metadata and attributes.
- Modifies file attributes (hidden, read-only, etc.)
- Updates file timestamps (creation, access, write times)
- Called when applications change file properties

### 9. GetSecurity
**Function Signature**: `NTSTATUS GetSecurity(PVOID FileNode, PVOID FileDesc, PSECURITY_DESCRIPTOR SecurityDescriptor, SIZE_T *PSecurityDescriptorSize) override`
**Source Location**: `hybrid_filesystem.hpp:41`

**Purpose**: Retrieve security descriptor for an open file.
- Returns file access permissions and ownership information
- Manages Access Control Lists (ACLs)
- Called when applications query file security settings

### 10. SetSecurity
**Function Signature**: `NTSTATUS SetSecurity(PVOID FileNode, PVOID FileDesc, SECURITY_INFORMATION SecurityInformation, PSECURITY_DESCRIPTOR ModificationDescriptor) override`
**Source Location**: `hybrid_filesystem.hpp:42`

**Purpose**: Modify file security descriptor and permissions.
- Updates file access permissions
- Changes file ownership information
- Modifies Access Control Lists (ACLs)
- Called when applications change file security settings

### 11. ReadDirectory
**Function Signature**: `NTSTATUS ReadDirectory(PVOID FileNode, PVOID FileDesc, PWSTR Pattern, PWSTR Marker, PVOID Buffer, ULONG Length, PULONG PBytesTransferred) override`
**Source Location**: `hybrid_filesystem.hpp:43`

**Purpose**: List directory contents in a buffered manner.
- Retrieves directory entries matching a pattern
- Supports directory iteration with markers for large directories
- Buffers directory information for efficient enumeration
- Called when applications list folder contents

### 12. ReadDirectoryEntry
**Function Signature**: `NTSTATUS ReadDirectoryEntry(PVOID FileNode, PVOID FileDesc, PWSTR Pattern, PWSTR Marker, PVOID *PContext, DirInfo *DirInfo) override`
**Source Location**: `hybrid_filesystem.hpp:44`

**Purpose**: Read individual directory entries one at a time.
- Provides entry-by-entry directory enumeration
- Supports pattern matching for directory listings
- Alternative to buffered ReadDirectory for simpler implementations
- Called during directory traversal operations

## Implementation Notes

### Minimum Required Callbacks
According to WinFSP documentation, a functional filesystem must implement at minimum:
- `GetSecurityByName` - for file access and security checks
- `Open` - for opening files
- `Close` - for cleanup

### Security and Access Control
Our implementation handles Windows security through:
- **GetSecurityByName**: Initial access checks before file operations
- **GetSecurity/SetSecurity**: Runtime security descriptor management
- Uses Windows security descriptors stored in `CacheEntry::SecDesc`

### File Descriptor Management
The `FileDescriptor` struct (`hybrid_filesystem.hpp:87`) manages per-file state:
- `HANDLE handle` - Windows file handle for actual I/O
- `CacheEntry *entry` - Cache metadata for the file
- `PVOID dir_buffer` - Directory enumeration buffer

### Error Handling
All callbacks return `NTSTATUS` codes following Windows conventions:
- `STATUS_SUCCESS` (0) for successful operations
- `STATUS_OBJECT_NAME_NOT_FOUND` for missing files
- `STATUS_ACCESS_DENIED` for permission failures
- `STATUS_INSUFFICIENT_RESOURCES` for memory/resource issues

## Integration with Cache System

Our callbacks integrate tightly with the hybrid caching system:
- **GetSecurityByName/Open**: Check cache entries and fetch from network if needed
- **Read**: May read from memory cache, disk cache, or network
- **Directory operations**: Use `DirectoryCache` for fast directory enumeration
- **Access tracking**: File operations are logged via `FileAccessTracker`

This callback architecture allows our filesystem to present a unified view of cached and network files to Windows applications while maintaining performance through intelligent caching strategies.