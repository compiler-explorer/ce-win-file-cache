# Windows Filesystem Driver Development Guide

## Overview

This document outlines the main functions and operations required to implement a Windows filesystem driver, and examines how WinFSP (Windows File System Proxy) abstracts these kernel-level operations into a user-mode API.

## Windows Filesystem Driver Architecture

### Traditional Kernel Driver Model

Windows filesystem drivers operate at the kernel level and handle I/O Request Packets (IRPs) sent by the Windows I/O Manager. These drivers must implement specific dispatch routines for various IRP major function codes.

**References:**
- [IRP Major Function Codes - Microsoft Learn](https://learn.microsoft.com/en-us/windows-hardware/drivers/kernel/irp-major-function-codes)
- [File system and filter overview - Microsoft Learn](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/_ifsk/)

### Driver Entry Point

Every Windows driver must implement a `DriverEntry` function that:
- Creates device objects using `IoCreateDevice()` or `IoCreateDeviceSecure()`
- Initializes the IRP dispatch table by setting `DriverObject->MajorFunction[i]` entries
- Sets up Fast I/O dispatch table (required for filesystem drivers)
- Registers unload routines and callback functions

## Core IRP Handlers and Operations

### Essential IRP Major Function Codes

| IRP Code | Function | Description |
|----------|----------|-------------|
| `IRP_MJ_CREATE` | File/Directory Creation | Handles file open, create, and access operations |
| `IRP_MJ_READ` | Read Operations | Reads data from files |
| `IRP_MJ_WRITE` | Write Operations | Writes data to files |
| `IRP_MJ_CLOSE` | File Close | Handles file handle closure |
| `IRP_MJ_CLEANUP` | Cleanup Operations | Cleanup when last handle to file object is closed |
| `IRP_MJ_QUERY_INFORMATION` | File Information | Retrieves file metadata and attributes |
| `IRP_MJ_SET_INFORMATION` | Set File Information | Sets file metadata, renames, deletes |
| `IRP_MJ_DIRECTORY_CONTROL` | Directory Operations | Directory enumeration and control |
| `IRP_MJ_QUERY_VOLUME_INFORMATION` | Volume Information | Retrieves volume metadata |
| `IRP_MJ_SET_VOLUME_INFORMATION` | Set Volume Information | Sets volume metadata |
| `IRP_MJ_FILE_SYSTEM_CONTROL` | Filesystem Control | Mount, dismount, and other FS control operations |
| `IRP_MJ_DEVICE_CONTROL` | Device Control | Device-specific I/O control requests |
| `IRP_MJ_QUERY_SECURITY` | Security Information | Retrieves file security descriptors |
| `IRP_MJ_SET_SECURITY` | Set Security | Sets file security descriptors |

### Fast I/O Operations

Fast I/O provides a faster path for synchronous operations, bypassing IRP generation:
- `FastIoCheckIfPossible` - Determines if fast I/O is possible
- `FastIoRead` - Fast read operations
- `FastIoWrite` - Fast write operations
- `FastIoQueryBasicInfo` - Fast file attribute queries
- `FastIoQueryStandardInfo` - Fast file size/allocation queries
- `FastIoLock` - Fast file locking
- `FastIoUnlockSingle` - Fast unlock operations

**References:**
- [Handling IRPs - Microsoft Learn](https://learn.microsoft.com/en-us/windows-hardware/drivers/kernel/handling-irps)
- [Different Ways of Handling IRPs - Microsoft Learn](https://learn.microsoft.com/en-us/windows-hardware/drivers/kernel/different-ways-of-handling-irps-cheat-sheet)

## Detailed Operation Implementation

### 1. File Creation (`IRP_MJ_CREATE`)

**Purpose**: Handle file open, create, and directory open operations.

**Key Responsibilities**:
- Parse the file path and resolve directory traversal
- Check file existence and permissions
- Create new files if requested with appropriate flags
- Open existing files with proper access rights
- Handle directory opens for enumeration
- Set up file context structures
- Return appropriate status codes

**Implementation Considerations**:
- All access checks must be performed
- Handle various create dispositions (CREATE_NEW, OPEN_EXISTING, etc.)
- Support reparse points and symbolic links
- Manage file sharing modes
- Handle case sensitivity requirements

### 2. Read Operations (`IRP_MJ_READ`)

**Purpose**: Read data from files.

**Key Responsibilities**:
- Validate read parameters (offset, length)
- Retrieve data from underlying storage
- Handle partial reads and EOF conditions
- Support both synchronous and asynchronous operations
- Update file access timestamps
- Handle memory-mapped file reads

### 3. Write Operations (`IRP_MJ_WRITE`)

**Purpose**: Write data to files.

**Key Responsibilities**:
- Validate write parameters and permissions
- Handle file extension if writing beyond EOF
- Update file modification timestamps
- Support both synchronous and asynchronous operations
- Implement write-through and write-behind caching
- Handle disk space limitations

### 4. Directory Enumeration (`IRP_MJ_DIRECTORY_CONTROL`)

**Purpose**: List directory contents and manage directory operations.

**Key Responsibilities**:
- Return file listings with proper formatting
- Support various query patterns (wildcards, specific files)
- Handle multiple enumeration information classes
- Implement proper directory scanning state management
- Support both single and bulk directory queries

**References:**
- [About File System Filter Drivers - Microsoft Learn](https://learn.microsoft.com/en-us/windows-hardware/drivers/ifs/about-file-system-filter-drivers)

### 5. File Information Queries (`IRP_MJ_QUERY_INFORMATION`)

**Purpose**: Retrieve file metadata and attributes.

**Key Responsibilities**:
- Return file sizes, timestamps, and attributes
- Support multiple information classes (Basic, Standard, etc.)
- Handle file ID queries
- Provide network open information
- Support alternate data stream information

### 6. File Information Setting (`IRP_MJ_SET_INFORMATION`)

**Purpose**: Modify file metadata and perform file operations.

**Key Responsibilities**:
- Handle file renames and moves
- Set file attributes and timestamps
- Implement file deletion (mark for delete)
- Support file truncation/extension
- Handle end-of-file setting

### 7. Volume Operations

**Purpose**: Manage volume-level information and operations.

**Key Responsibilities**:
- Return volume size and free space information
- Provide volume labels and filesystem information
- Handle volume mount/dismount operations
- Support volume-level security operations

### 8. Extended Attributes (`IRP_MJ_QUERY_EA`)

**Purpose**: Retrieve extended attributes (EAs) associated with files and directories.

**Extended Attributes Overview**:
Extended Attributes are name-value pairs that can be associated with files and directories to store additional metadata beyond standard file attributes. They are similar to POSIX extended attributes on Unix systems or NTFS alternate data streams, but with a specific structured format.

**Key Responsibilities**:
- Parse EA query requests with specific EA names or wildcard patterns
- Validate EA name format and length constraints
- Retrieve EA values from underlying storage or metadata store
- Format EA data according to `FILE_FULL_EA_INFORMATION` structure
- Handle buffer size calculations and overflow conditions
- Support both single EA queries and bulk EA enumeration
- Implement proper access control for EA operations

**EA Data Structure**:
Extended attributes are returned in `FILE_FULL_EA_INFORMATION` format:
```c
typedef struct _FILE_FULL_EA_INFORMATION {
    ULONG NextEntryOffset;     // Offset to next EA entry (0 if last)
    UCHAR Flags;               // EA flags (usually 0)
    UCHAR EaNameLength;        // Length of EA name (not including null terminator)
    USHORT EaValueLength;      // Length of EA value
    CHAR EaName[1];           // Variable-length EA name (null-terminated)
    // EA value follows immediately after EaName
} FILE_FULL_EA_INFORMATION;
```

**Implementation Considerations**:
- **EA Name Validation**: EA names must be valid, case-insensitive, and typically limited to 255 characters
- **Buffer Management**: Calculate exact buffer sizes needed for EA data and handle `STATUS_BUFFER_OVERFLOW` gracefully
- **Query Types**: Support three query patterns:
  - **Single EA**: Query specific EA by name
  - **EA List**: Query multiple specific EAs by providing an EA name list
  - **All EAs**: Enumerate all EAs associated with the file
- **Performance**: Cache frequently accessed EAs to avoid repeated storage access
- **Storage Integration**: Determine how EAs are stored (file metadata, separate EA file, database, etc.)
- **Compatibility**: Handle systems that don't support EAs gracefully

**Common EA Use Cases**:
- **Application Metadata**: Applications storing custom file properties
- **Content Type Information**: MIME types or content classification
- **Backup/Archive Attributes**: Additional metadata for backup software
- **Security Tags**: Custom security labels or classifications
- **Cross-Platform Compatibility**: Preserving Unix extended attributes

**Typical Extended Attribute Names**:

| EA Name | Purpose | Value Type | Example Value |
|---------|---------|------------|---------------|
| `user.mime_type` | MIME content type | String | `text/plain`, `image/jpeg` |
| `user.charset` | Character encoding | String | `utf-8`, `iso-8859-1` |
| `user.comment` | User comment/description | String | `Personal notes about file` |
| `user.author` | Document author | String | `John Doe` |
| `user.title` | Document title | String | `Meeting Notes 2024` |
| `user.subject` | Document subject | String | `Project Planning` |
| `user.keywords` | Search keywords | String | `important,project,2024` |
| `user.classification` | Security classification | String | `confidential`, `public` |
| `user.checksum` | File integrity checksum | String | `sha256:abc123...` |
| `user.origin_url` | Download source URL | String | `https://example.com/file.pdf` |
| `user.download_date` | When file was downloaded | String | `2024-01-15T10:30:00Z` |
| `system.backup_time` | Last backup timestamp | Binary | 8-byte FILETIME |
| `system.archive_id` | Archive system identifier | String | `ARCH-2024-001234` |
| `security.label` | Security label | String | `SECRET//NOFORN` |
| `security.clearance` | Required clearance level | String | `TOP_SECRET` |
| `app.microsoft.office.version` | Office version info | String | `16.0.12345.67890` |
| `app.adobe.xmp` | Adobe XMP metadata | Binary | XMP metadata blob |
| `app.custom.workflow_state` | Application workflow state | String | `pending_review` |
| `posix.uid` | Unix user ID | Binary | 4-byte integer |
| `posix.gid` | Unix group ID | Binary | 4-byte integer |
| `posix.mode` | Unix file permissions | Binary | 4-byte integer |
| `posix.acl_access` | POSIX ACL data | Binary | ACL structure |
| `darwin.quarantine` | macOS quarantine info | String | Quarantine metadata |
| `selinux.context` | SELinux security context | String | `user_u:object_r:user_home_t:s0` |

**EA Naming Conventions**:
- **user.*** - User-defined attributes, applications can create freely
- **system.*** - System-level attributes, typically managed by OS/filesystem
- **security.*** - Security-related attributes, may require special permissions
- **app.vendor.*** - Application-specific attributes with vendor namespace
- **posix.*** - POSIX-compatible attributes for Unix interoperability
- **darwin.*** - macOS-specific attributes
- **selinux.*** - SELinux security attributes

**Implementation Notes**:
- EA names are case-insensitive on Windows but case-sensitive on Unix
- Maximum EA name length is typically 255 characters
- EA values can be binary or text data
- Some EAs may require special permissions to read/write
- Applications should use proper namespacing to avoid conflicts

**Error Handling**:
- `STATUS_SUCCESS` - EA query completed successfully
- `STATUS_NO_EAS_ON_FILE` - File has no extended attributes
- `STATUS_BUFFER_OVERFLOW` - Buffer too small, return required size
- `STATUS_INVALID_EA_NAME` - Invalid EA name format
- `STATUS_EA_CORRUPT_ERROR` - EA data is corrupted
- `STATUS_ACCESS_DENIED` - Insufficient permissions to read EAs

**Performance Optimization**:
- Implement EA caching for frequently accessed files
- Use efficient storage format for EA data
- Minimize storage I/O for EA operations
- Consider EA data locality with file data

**WinFSP Integration**:
In WinFSP, this functionality is exposed through the `GetEa` callback in the `FSP_FILE_SYSTEM_INTERFACE`. The WinFSP framework handles the complex buffer management and formatting, allowing user-mode implementations to focus on the actual EA retrieval logic.

**Example Implementation Pattern**:
```c
NTSTATUS GetEa(FSP_FILE_SYSTEM *FileSystem,
               PVOID FileNode,
               PFILE_FULL_EA_INFORMATION Ea,
               ULONG EaLength,
               PULONG PBytesTransferred)
{
    // 1. Validate parameters and file node
    // 2. Parse EA query (specific names vs. enumerate all)
    // 3. Retrieve EA data from storage/metadata store
    // 4. Format data into FILE_FULL_EA_INFORMATION structure
    // 5. Handle buffer overflow cases
    // 6. Return appropriate status code
}
```

## WinFSP Implementation and User-Mode API

### WinFSP Architecture

WinFSP abstracts the complexity of kernel driver development by providing:
1. **Kernel Driver (FSD)**: Handles all IRP processing and kernel interactions
2. **User-Mode DLL**: Provides high-level API for user applications
3. **Interface Layer**: Translates between kernel IRPs and user-mode callbacks

**References:**
- [WinFSP Official Website](https://winfsp.dev/)
- [WinFSP GitHub Repository](https://github.com/winfsp/winfsp)
- [WinFSP API Documentation](https://github.com/winfsp/winfsp/blob/master/doc/WinFsp-API-winfsp.h.md)

### IRP to WinFSP Function Mapping

The following table shows how Windows IRP major function codes map to WinFSP callback functions:

| IRP Major Function Code | WinFSP Callback Function | Purpose |
|------------------------|---------------------------|---------|
| `IRP_MJ_CREATE` | `Open` | File/directory creation and opening |
| `IRP_MJ_CLOSE` | `Close` | Handle closure |
| `IRP_MJ_CLEANUP` | `Cleanup` | Last handle cleanup |
| `IRP_MJ_READ` | `Read` | File read operations |
| `IRP_MJ_WRITE` | `Write` | File write operations |
| `IRP_MJ_QUERY_INFORMATION` | `GetFileInfo` | Retrieve file metadata |
| `IRP_MJ_SET_INFORMATION` | `SetBasicInfo`, `SetFileSize`, `CanDelete`, `Rename` | Set file metadata, resize, delete, rename |
| `IRP_MJ_DIRECTORY_CONTROL` | `ReadDirectory` | Directory enumeration |
| `IRP_MJ_QUERY_VOLUME_INFORMATION` | `GetVolumeInfo` | Volume metadata queries |
| `IRP_MJ_SET_VOLUME_INFORMATION` | `SetVolumeLabel` | Set volume label |
| `IRP_MJ_FLUSH_BUFFERS` | `Flush` | Force data to storage |
| `IRP_MJ_QUERY_SECURITY` | `GetSecurity` | Retrieve security descriptors |
| `IRP_MJ_SET_SECURITY` | `SetSecurity` | Set security descriptors |
| `IRP_MJ_QUERY_EA` | `GetEa` | Get extended attributes |
| `IRP_MJ_SET_EA` | `SetEa` | Set extended attributes |
| `IRP_MJ_CREATE_NAMED_PIPE` | Not supported | Named pipe creation |
| `IRP_MJ_QUERY_QUOTA` | Not supported | Quota queries |
| `IRP_MJ_SET_QUOTA` | Not supported | Quota setting |
| `IRP_MJ_FILE_SYSTEM_CONTROL` | `Control` | File system control operations |
| `IRP_MJ_DEVICE_CONTROL` | `Control` | Device control operations |
| Fast I/O Operations | `GetFileInfo`, `Read`, `Write` | Optimized synchronous operations |

**Notes:**
- Some complex IRP operations are split into multiple WinFSP callbacks for clarity
- WinFSP automatically handles many edge cases and parameter validation
- Not all Windows filesystem features are exposed through WinFSP (e.g., quotas, named pipes)
- Fast I/O operations are automatically mapped to their corresponding WinFSP callbacks

### FSP_FILE_SYSTEM_INTERFACE

WinFSP defines a comprehensive interface structure that user-mode filesystems must implement:

```c
typedef struct _FSP_FILE_SYSTEM_INTERFACE
{
    // Core file operations
    NTSTATUS (*Open)(FSP_FILE_SYSTEM *FileSystem,
                     PWSTR FileName,
                     UINT32 CreateOptions,
                     UINT32 GrantedAccess,
                     PVOID *PFileNode,
                     FSP_FSCTL_FILE_INFO *FileInfo);

    NTSTATUS (*Close)(FSP_FILE_SYSTEM *FileSystem,
                      PVOID FileNode);

    NTSTATUS (*Read)(FSP_FILE_SYSTEM *FileSystem,
                     PVOID FileNode,
                     PVOID Buffer,
                     UINT64 Offset,
                     ULONG Length,
                     PULONG PBytesTransferred);

    NTSTATUS (*Write)(FSP_FILE_SYSTEM *FileSystem,
                      PVOID FileNode,
                      PVOID Buffer,
                      UINT64 Offset,
                      ULONG Length,
                      BOOLEAN WriteToEndOfFile,
                      BOOLEAN ConstrainedIo,
                      PULONG PBytesTransferred,
                      FSP_FSCTL_FILE_INFO *FileInfo);

    // Directory operations
    NTSTATUS (*ReadDirectory)(FSP_FILE_SYSTEM *FileSystem,
                              PVOID FileNode,
                              PWSTR Pattern,
                              PWSTR Marker,
                              PVOID Buffer,
                              ULONG BufferLength,
                              PULONG PBytesTransferred);

    // File information operations
    NTSTATUS (*GetFileInfo)(FSP_FILE_SYSTEM *FileSystem,
                            PVOID FileNode,
                            FSP_FSCTL_FILE_INFO *FileInfo);

    NTSTATUS (*SetBasicInfo)(FSP_FILE_SYSTEM *FileSystem,
                             PVOID FileNode,
                             UINT32 FileAttributes,
                             UINT64 CreationTime,
                             UINT64 LastAccessTime,
                             UINT64 LastWriteTime,
                             UINT64 ChangeTime,
                             FSP_FSCTL_FILE_INFO *FileInfo);

    // Volume operations
    NTSTATUS (*GetVolumeInfo)(FSP_FILE_SYSTEM *FileSystem,
                              FSP_FSCTL_VOLUME_INFO *VolumeInfo);

    // Security operations
    NTSTATUS (*GetSecurity)(FSP_FILE_SYSTEM *FileSystem,
                            PVOID FileNode,
                            PSECURITY_DESCRIPTOR SecurityDescriptor,
                            SIZE_T *PSecurityDescriptorSize);

    NTSTATUS (*SetSecurity)(FSP_FILE_SYSTEM *FileSystem,
                            PVOID FileNode,
                            SECURITY_INFORMATION SecurityInformation,
                            PSECURITY_DESCRIPTOR ModificationDescriptor);

} FSP_FILE_SYSTEM_INTERFACE;
```

### How WinFSP Bridges Kernel and User Mode

1. **IRP Reception**: The WinFSP kernel driver receives IRPs from the Windows I/O Manager
2. **Parameter Extraction**: The driver extracts relevant parameters from the IRP and stack location
3. **User-Mode Dispatch**: Parameters are marshaled and sent to the user-mode process via shared memory or other IPC
4. **Callback Invocation**: The user-mode DLL invokes the appropriate callback in the `FSP_FILE_SYSTEM_INTERFACE`
5. **Result Processing**: The callback result is marshaled back to kernel mode
6. **IRP Completion**: The kernel driver completes the original IRP with the appropriate status

### WinFSP API Benefits

- **Simplified Development**: No kernel programming knowledge required
- **Memory Safety**: User-mode execution eliminates many crash scenarios
- **Debugging**: Standard user-mode debugging tools can be used
- **Multiple APIs**: Supports native C, FUSE, and .NET APIs
- **Cross-Platform**: Similar interface to FUSE on Unix systems

### User Implementation Requirements

To create a filesystem with WinFSP:

1. **Implement Interface**: Provide implementations for required operations in `FSP_FILE_SYSTEM_INTERFACE`
2. **Create Filesystem Object**: Use `FspFileSystemCreate()` to instantiate the filesystem
3. **Start Dispatcher**: Call `FspFileSystemStartDispatcher()` to begin processing requests
4. **Handle Callbacks**: Respond to filesystem operations through the implemented callbacks

### Example Implementation Pattern

```c
// User filesystem structure
typedef struct {
    FSP_FILE_SYSTEM *FileSystem;
    // User-specific data
} MY_FILE_SYSTEM;

// Implement required callbacks
static NTSTATUS MyOpen(FSP_FILE_SYSTEM *FileSystem, /* ... */) {
    // Handle file open logic
    return STATUS_SUCCESS;
}

static NTSTATUS MyRead(FSP_FILE_SYSTEM *FileSystem, /* ... */) {
    // Handle file read logic
    return STATUS_SUCCESS;
}

// Interface definition
static FSP_FILE_SYSTEM_INTERFACE MyInterface = {
    .Open = MyOpen,
    .Read = MyRead,
    // ... other operations
};

// Main initialization
int main() {
    MY_FILE_SYSTEM MyFileSystem;

    // Create filesystem object
    FspFileSystemCreate(DevicePath, &VolumeParams, &MyInterface,
                        &MyFileSystem.FileSystem);

    // Start processing requests
    FspFileSystemStartDispatcher(MyFileSystem.FileSystem, 0);

    return 0;
}
```

## Comparison: Kernel vs WinFSP Development

| Aspect | Kernel Driver | WinFSP User-Mode |
|--------|---------------|------------------|
| **Complexity** | High - IRP handling, kernel APIs | Low - Simple callback interface |
| **Safety** | Crashes affect system | Crashes isolated to process |
| **Debugging** | Kernel debugger required | Standard debugging tools |
| **Development Time** | Months | Days to weeks |
| **Performance** | Maximum performance | Slight overhead from user/kernel transitions |
| **Flexibility** | Full control over operations | Limited to WinFSP capabilities |
| **Maintenance** | Complex kernel compatibility | Simplified user-mode updates |

## Additional Resources

### Microsoft Documentation
- [IRP Major Function Codes](https://learn.microsoft.com/en-us/windows-hardware/drivers/kernel/irp-major-function-codes)
- [File System and Filter Overview](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/_ifsk/)
- [About File System Filter Drivers](https://learn.microsoft.com/en-us/windows-hardware/drivers/ifs/about-file-system-filter-drivers)
- [Handling IRPs](https://learn.microsoft.com/en-us/windows-hardware/drivers/kernel/handling-irps)
- [IRP_MJ_FILE_SYSTEM_CONTROL](https://learn.microsoft.com/en-us/windows-hardware/drivers/ifs/irp-mj-file-system-control)

### WinFSP Resources
- [WinFSP Official Documentation](https://winfsp.dev/)
- [WinFSP GitHub Repository](https://github.com/winfsp/winfsp)
- [Known File Systems using WinFSP](https://github.com/winfsp/winfsp/wiki/Known-File-Systems)

## Conclusion

While traditional Windows filesystem driver development requires extensive kernel programming knowledge and careful IRP handling across dozens of operation types, WinFSP dramatically simplifies this process by providing a user-mode abstraction layer. The WinFSP approach allows developers to focus on filesystem logic rather than kernel-level details, while still providing comprehensive filesystem functionality through its well-designed callback interface.

For new filesystem development, WinFSP represents a significant advantage in terms of development speed, safety, and maintainability, making it the preferred approach unless maximum performance or specialized kernel features are absolutely required.