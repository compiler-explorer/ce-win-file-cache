#pragma once

// Dummy ntifs.h implementation for Wine compilation
// This provides NT internal filesystem definitions

#ifndef _NTIFS_H_
#define _NTIFS_H_

#ifdef WINE_CROSS_COMPILE

#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

// NT Status codes (some may be missing in Wine)
#ifndef STATUS_SUCCESS
#define STATUS_SUCCESS                  ((NTSTATUS)0x00000000L)
#endif

#ifndef STATUS_UNSUCCESSFUL
#define STATUS_UNSUCCESSFUL             ((NTSTATUS)0xC0000001L)
#endif

#ifndef STATUS_NOT_IMPLEMENTED
#define STATUS_NOT_IMPLEMENTED          ((NTSTATUS)0xC0000002L)
#endif

#ifndef STATUS_INVALID_INFO_CLASS
#define STATUS_INVALID_INFO_CLASS       ((NTSTATUS)0xC0000003L)
#endif

#ifndef STATUS_ACCESS_DENIED
#define STATUS_ACCESS_DENIED            ((NTSTATUS)0xC0000022L)
#endif

#ifndef STATUS_BUFFER_TOO_SMALL
#define STATUS_BUFFER_TOO_SMALL         ((NTSTATUS)0xC0000023L)
#endif

#ifndef STATUS_OBJECT_NAME_NOT_FOUND
#define STATUS_OBJECT_NAME_NOT_FOUND    ((NTSTATUS)0xC0000034L)
#endif

#ifndef STATUS_OBJECT_NAME_INVALID
#define STATUS_OBJECT_NAME_INVALID      ((NTSTATUS)0xC0000033L)
#endif

#ifndef STATUS_OBJECT_PATH_NOT_FOUND
#define STATUS_OBJECT_PATH_NOT_FOUND    ((NTSTATUS)0xC000003AL)
#endif

#ifndef STATUS_SHARING_VIOLATION
#define STATUS_SHARING_VIOLATION        ((NTSTATUS)0xC0000043L)
#endif

#ifndef STATUS_DELETE_PENDING
#define STATUS_DELETE_PENDING           ((NTSTATUS)0xC0000056L)
#endif

#ifndef STATUS_NO_MORE_FILES
#define STATUS_NO_MORE_FILES            ((NTSTATUS)0x80000006L)
#endif

#ifndef STATUS_PENDING
#define STATUS_PENDING                  ((NTSTATUS)0x00000103L)
#endif

#ifndef STATUS_DISK_FULL
#define STATUS_DISK_FULL                ((NTSTATUS)0xC000007FL)
#endif

#ifndef STATUS_INVALID_HANDLE
#define STATUS_INVALID_HANDLE           ((NTSTATUS)0xC0000008L)
#endif

#ifndef STATUS_INVALID_DEVICE_REQUEST
#define STATUS_INVALID_DEVICE_REQUEST   ((NTSTATUS)0xC0000010L)
#endif

#ifndef STATUS_END_OF_FILE
#define STATUS_END_OF_FILE              ((NTSTATUS)0xC0000011L)
#endif

#ifndef STATUS_NO_SUCH_FILE
#define STATUS_NO_SUCH_FILE             ((NTSTATUS)0xC000000FL)
#endif

#ifndef STATUS_INVALID_PARAMETER
#define STATUS_INVALID_PARAMETER        ((NTSTATUS)0xC000000DL)
#endif

#ifndef STATUS_INSUFFICIENT_RESOURCES
#define STATUS_INSUFFICIENT_RESOURCES   ((NTSTATUS)0xC000009AL)
#endif

#ifndef STATUS_FILE_IS_A_DIRECTORY
#define STATUS_FILE_IS_A_DIRECTORY      ((NTSTATUS)0xC00000BAL)
#endif

#ifndef STATUS_NOT_A_DIRECTORY
#define STATUS_NOT_A_DIRECTORY          ((NTSTATUS)0xC0000103L)
#endif

#ifndef STATUS_DIRECTORY_NOT_EMPTY
#define STATUS_DIRECTORY_NOT_EMPTY      ((NTSTATUS)0xC0000101L)
#endif

#ifndef STATUS_UNEXPECTED_IO_ERROR
#define STATUS_UNEXPECTED_IO_ERROR      ((NTSTATUS)0xC00000E9L)
#endif

// File information classes
typedef enum _FILE_INFORMATION_CLASS {
    FileDirectoryInformation = 1,
    FileFullDirectoryInformation,
    FileBothDirectoryInformation,
    FileBasicInformation,
    FileStandardInformation,
    FileInternalInformation,
    FileEaInformation,
    FileAccessInformation,
    FileNameInformation,
    FileRenameInformation,
    FileLinkInformation,
    FileNamesInformation,
    FileDispositionInformation,
    FilePositionInformation,
    FileFullEaInformation,
    FileModeInformation,
    FileAlignmentInformation,
    FileAllInformation,
    FileAllocationInformation,
    FileEndOfFileInformation,
    FileAlternateNameInformation,
    FileStreamInformation,
    FilePipeInformation,
    FilePipeLocalInformation,
    FilePipeRemoteInformation,
    FileMailslotQueryInformation,
    FileMailslotSetInformation,
    FileCompressionInformation,
    FileObjectIdInformation,
    FileCompletionInformation,
    FileMoveClusterInformation,
    FileQuotaInformation,
    FileReparsePointInformation,
    FileNetworkOpenInformation,
    FileAttributeTagInformation,
    FileTrackingInformation,
    FileIdBothDirectoryInformation,
    FileIdFullDirectoryInformation,
    FileValidDataLengthInformation,
    FileShortNameInformation,
    FileIoCompletionNotificationInformation,
    FileIoStatusBlockRangeInformation,
    FileIoPriorityHintInformation,
    FileSfioReserveInformation,
    FileSfioVolumeInformation,
    FileHardLinkInformation,
    FileProcessIdsUsingFileInformation,
    FileNormalizedNameInformation,
    FileNetworkPhysicalNameInformation,
    FileIdGlobalTxDirectoryInformation,
    FileIsRemoteDeviceInformation,
    FileUnusedInformation,
    FileNumaNodeInformation,
    FileStandardLinkInformation,
    FileRemoteProtocolInformation,
    FileRenameInformationBypassAccessCheck,
    FileLinkInformationBypassAccessCheck,
    FileVolumeNameInformation,
    FileIdInformation,
    FileIdExtdDirectoryInformation,
    FileReplaceCompletionInformation,
    FileHardLinkFullIdInformation,
    FileIdExtdBothDirectoryInformation,
    FileMaximumInformation
} FILE_INFORMATION_CLASS, *PFILE_INFORMATION_CLASS;

// Volume information classes
typedef enum _FS_INFORMATION_CLASS {
    FileFsVolumeInformation = 1,
    FileFsLabelInformation,
    FileFsSizeInformation,
    FileFsDeviceInformation,
    FileFsAttributeInformation,
    FileFsControlInformation,
    FileFsFullSizeInformation,
    FileFsObjectIdInformation,
    FileFsDriverPathInformation,
    FileFsVolumeFlagsInformation,
    FileFsSectorSizeInformation,
    FileFsDataCopyInformation,
    FileFsMetadataSizeInformation,
    FileFsFullSizeInformationEx,
    FileFsMaximumInformation
} FS_INFORMATION_CLASS, *PFS_INFORMATION_CLASS;

#ifdef __cplusplus
}
#endif

#endif // WINE_CROSS_COMPILE

#endif // _NTIFS_H_