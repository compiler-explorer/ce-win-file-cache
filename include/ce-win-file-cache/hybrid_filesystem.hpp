#pragma once

#include "../types/cache_entry.hpp"
#include "../types/config.hpp"
#include "windows_compat.hpp"
#include "memory_cache_manager.hpp"
#include <memory>
#include <mutex>
#include <unordered_map>

#ifndef NO_WINFSP
#include <winfsp/winfsp.hpp>

namespace CeWinFileCache
{

class HybridFileSystem : public Fsp::FileSystemBase
{
    public:
    HybridFileSystem();
    ~HybridFileSystem();

    NTSTATUS Initialize(const Config &config);
    NTSTATUS SetCompilerPaths(const std::unordered_map<std::wstring, std::wstring> &compiler_paths);

    protected:
    // WinFsp callbacks
    NTSTATUS Init(PVOID Host) override;
    NTSTATUS GetVolumeInfo(VolumeInfo *VolumeInfo) override;
    NTSTATUS GetSecurityByName(PWSTR FileName, PUINT32 PFileAttributes, PSECURITY_DESCRIPTOR SecurityDescriptor, SIZE_T *PSecurityDescriptorSize) override;
    NTSTATUS Open(PWSTR FileName, UINT32 CreateOptions, UINT32 GrantedAccess, PVOID *PFileNode, PVOID *PFileDesc, OpenFileInfo *OpenFileInfo) override;
    VOID Close(PVOID FileNode, PVOID FileDesc) override;
    NTSTATUS Read(PVOID FileNode, PVOID FileDesc, PVOID Buffer, UINT64 Offset, ULONG Length, PULONG PBytesTransferred) override;
    NTSTATUS GetFileInfo(PVOID FileNode, PVOID FileDesc, FileInfo *FileInfo) override;
    NTSTATUS ReadDirectory(PVOID FileNode, PVOID FileDesc, PWSTR Pattern, PWSTR Marker, PVOID Buffer, ULONG Length, PULONG PBytesTransferred) override;
    NTSTATUS ReadDirectoryEntry(PVOID FileNode, PVOID FileDesc, PWSTR Pattern, PWSTR Marker, PVOID *PContext, DirInfo *DirInfo) override;

    private:
    // Internal methods
    std::wstring resolveVirtualPath(const std::wstring &virtual_path);
    CacheEntry *getCacheEntry(const std::wstring &virtual_path);
    NTSTATUS ensureFileAvailable(CacheEntry *entry);
    NTSTATUS fetchFromNetwork(CacheEntry *entry);
    bool matchesPattern(const std::wstring &path, const std::wstring &pattern);
    CachePolicy determineCachePolicy(const std::wstring &virtual_path);

    // Cache management
    NTSTATUS evictIfNeeded();
    void updateAccessTime(CacheEntry *entry);

    Config config_;
    std::unordered_map<std::wstring, std::unique_ptr<CacheEntry>> cache_entries_;
    std::mutex cache_mutex_;
    size_t current_cache_size_;
    UINT64 creation_time_;
    
    // In-memory cache for fast file access
    MemoryCacheManager memory_cache_;
};

struct FileDescriptor
{
    HANDLE handle;
    CacheEntry *entry;
    PVOID dir_buffer;

    FileDescriptor() : handle(INVALID_HANDLE_VALUE), entry(nullptr), dir_buffer(nullptr)
    {
    }
    ~FileDescriptor()
    {
        if (handle != INVALID_HANDLE_VALUE)
        {
            CloseHandle(handle);
        }
        if (dir_buffer)
        {
            Fsp::FileSystemBase::DeleteDirectoryBuffer(&dir_buffer);
        }
    }
};

} // namespace CeWinFileCache

#endif
