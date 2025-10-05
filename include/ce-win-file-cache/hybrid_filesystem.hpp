#pragma once

#include "../types/cache_entry.hpp"
#include "../types/config.hpp"
#include "async_download_manager.hpp"
#include "directory_cache.hpp"
#include "file_access_tracker.hpp"
#include "memory_cache_manager.hpp"
#include "metrics_collector.hpp"
#include "windows_compat.hpp"
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

    NTSTATUS Initialize(const Config &new_config);
    NTSTATUS SetPaths(const std::unordered_map<std::wstring, std::wstring> &compiler_paths);

    protected:
    // WinFsp callbacks
    NTSTATUS Init(PVOID Host) override;
    NTSTATUS GetVolumeInfo(VolumeInfo *VolumeInfo) override;
    NTSTATUS GetSecurityByName(PWSTR FileName, PUINT32 PFileAttributes, PSECURITY_DESCRIPTOR SecurityDescriptor, SIZE_T *PSecurityDescriptorSize) override;
    NTSTATUS Open(PWSTR FileName, UINT32 CreateOptions, UINT32 GrantedAccess, PVOID *PFileNode, PVOID *PFileDesc, OpenFileInfo *OpenFileInfo) override;
    VOID Close(PVOID FileNode, PVOID FileDesc) override;
    NTSTATUS Read(PVOID FileNode, PVOID FileDesc, PVOID Buffer, UINT64 Offset, ULONG Length, PULONG PBytesTransferred) override;
    NTSTATUS GetFileInfo(PVOID FileNode, PVOID FileDesc, FileInfo *FileInfo) override;
    NTSTATUS SetBasicInfo(PVOID FileNode, PVOID FileDesc, UINT32 FileAttributes, UINT64 CreationTime, UINT64 LastAccessTime, UINT64 LastWriteTime, UINT64 ChangeTime, FileInfo *FileInfo) override;
    NTSTATUS GetSecurity(PVOID FileNode, PVOID FileDesc, PSECURITY_DESCRIPTOR SecurityDescriptor, SIZE_T *PSecurityDescriptorSize) override;
    NTSTATUS SetSecurity(PVOID FileNode, PVOID FileDesc, SECURITY_INFORMATION SecurityInformation, PSECURITY_DESCRIPTOR ModificationDescriptor) override;
    NTSTATUS ReadDirectory(PVOID FileNode, PVOID FileDesc, PWSTR Pattern, PWSTR Marker, PVOID Buffer, ULONG Length, PULONG PBytesTransferred) override;
    NTSTATUS ReadDirectoryEntry(PVOID FileNode, PVOID FileDesc, PWSTR Pattern, PWSTR Marker, PVOID *PContext, DirInfo *DirInfo) override;
    NTSTATUS GetStreamInfo(PVOID FileNode, PVOID FileDesc, PVOID Buffer, ULONG Length, PULONG PBytesTransferred) override;
    // NTSTATUS GetEa(PVOID FileNode, PVOID FileDesc, PFILE_FULL_EA_INFORMATION Ea, ULONG EaLength, PULONG PBytesTransferred) override;
    // NTSTATUS SetEa(PVOID FileNode, PVOID FileDesc, PFILE_FULL_EA_INFORMATION Ea, ULONG EaLength, FileInfo *FileInfo) override;

    private:
    // Internal methods
    CacheEntry *getCacheEntry(const std::wstring &virtual_path);
    CacheEntry *createDynamicCacheEntry(DirectoryNode *node);
    NTSTATUS ensureFileAvailable(CacheEntry *entry);
    NTSTATUS fetchFromNetwork(CacheEntry *entry);
    bool matchesPattern(const std::wstring &path, const std::wstring &pattern);
    CachePolicy determineCachePolicy(const std::wstring &virtual_path);
    std::wstring createTemporaryFileForMemoryCached(CacheEntry *entry);

    NTSTATUS GetFileInfoByName(PWSTR FileName, FileInfo *FileInfo);
    void copyFileInfo(CacheEntry *source, FileInfo *dest) const;

    // Path normalization for Windows (handle both / and \ as separators)
    std::wstring normalizePath(const std::wstring &path);

    // Directory tree management
    void fillDirInfo(DirInfo *dir_info, DirectoryNode *node);
    std::vector<DirectoryNode *> filterDirectoryContents(const std::vector<DirectoryNode *> &contents, PWSTR pattern, const char *context);
    NTSTATUS handleFileAsDirectoryEntry(CacheEntry *entry, PWSTR Pattern, PWSTR Marker, PVOID Buffer, ULONG Length, PULONG PBytesTransferred);

    // Cache management
    NTSTATUS evictIfNeeded();
    void updateAccessTime(CacheEntry *entry);

    // Memory cache eviction with policy-aware LRU
    size_t performMemoryEviction(size_t bytes_needed);

    // Activity tracking for idle detection
    void recordActivity();
    bool isSystemIdle(std::chrono::seconds threshold = std::chrono::seconds(5)) const;

    // Background eviction thread
    void memoryEvictionThreadFunc();

    Config config;
    std::unordered_map<std::wstring, std::unique_ptr<CacheEntry>> cache_entries;
    std::mutex cache_mutex;
    size_t current_cache_size;
    uint64_t creation_time;

    // Activity tracking
    std::atomic<std::chrono::steady_clock::time_point> last_activity;

    // Background eviction thread
    std::thread memory_eviction_thread;
    std::atomic<bool> shutdown_eviction{false};

// Test access - allow tests to access private members
#ifdef ENABLE_TEST_ACCESS
public:
    CachePolicy testDetermineCachePolicy(const std::wstring &path) { return determineCachePolicy(path); }
    void testSetConfig(const Config &test_config) { config = test_config; }
private:
#endif

    // In-memory cache for fast file access
    MemoryCacheManager memory_cache;

    // Always-resident directory tree (never evicted)
    DirectoryCache directory_cache;

    // Async download manager for non-blocking file fetching
    std::unique_ptr<AsyncDownloadManager> download_manager;

    // File access tracking for usage reports
    std::unique_ptr<FileAccessTracker> access_tracker;
};

struct FileDescriptor
{
    HANDLE handle;
    CacheEntry *entry;
    PVOID dir_buffer;
    const std::vector<uint8_t> *cached_content;  // Direct pointer to memory cached content for fast reads

    FileDescriptor() : handle(INVALID_HANDLE_VALUE), entry(nullptr), dir_buffer(nullptr), cached_content(nullptr)
    {
    }
    ~FileDescriptor()
    {
        // Decrement memory reference count if we were using cached content
        if (cached_content && entry)
        {
            entry->memory_ref_count--;
        }

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
