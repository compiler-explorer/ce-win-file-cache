#include <ce-win-file-cache/glob_matcher.hpp>
#include <ce-win-file-cache/hybrid_filesystem.hpp>
#include <ce-win-file-cache/windows_compat.hpp>
#include <algorithm>
#include <chrono>
#include <iostream>

#ifndef NO_WINFSP

namespace CeWinFileCache
{

constexpr ULONG ALLOCATION_UNIT = 4096;
constexpr ULONG FULLPATH_SIZE = MAX_PATH + FSP_FSCTL_TRANSACT_PATH_SIZEMAX / sizeof(WCHAR);


HybridFileSystem::HybridFileSystem() : Fsp::FileSystemBase(), current_cache_size(0), creation_time(0)
{
}

HybridFileSystem::~HybridFileSystem()
{
    // Shutdown global metrics
    GlobalMetrics::shutdown();
}

NTSTATUS HybridFileSystem::Initialize(const Config &new_config)
{
    this->config = new_config;

    // Initialize global metrics if enabled
    if (new_config.global.metrics.enabled)
    {
        GlobalMetrics::initialize(new_config.global.metrics);
    }

    // Create cache directory if it doesn't exist
    if (!CreateDirectoryW(new_config.global.cache_directory.c_str(), nullptr))
    {
        DWORD error = GetLastError();
        if (error != ERROR_ALREADY_EXISTS)
        {
            return CeWinFileCache::WineCompat::NtStatusFromWin32(error);
        }
    }

    // Get creation time for volume
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    creation_time = ((PLARGE_INTEGER)&ft)->QuadPart;

    // Initialize directory cache with configured compiler paths
    NTSTATUS result = directory_cache.initialize(new_config);
    if (!NT_SUCCESS(result))
    {
        return result;
    }

    // Initialize async download manager with configured number of worker threads
    download_manager = std::make_unique<AsyncDownloadManager>(memory_cache, new_config, new_config.global.download_threads);

    return STATUS_SUCCESS;
}

NTSTATUS HybridFileSystem::SetCompilerPaths(const std::unordered_map<std::wstring, std::wstring> &compiler_paths)
{
    std::lock_guard<std::mutex> lock(cache_mutex);

    // Initialize cache entries for each compiler
    for (const auto &[compiler_name, base_path] : compiler_paths)
    {
        auto it = config.compilers.find(compiler_name);
        if (it == config.compilers.end())
        {
            continue;
        }

        const auto &compiler_config = it->second;

        // For now, create a simple entry for the root directory
        auto entry = std::make_unique<CacheEntry>();
        entry->virtual_path = L"\\" + compiler_name;
        entry->network_path = compiler_config.network_path;
        entry->state = FileState::VIRTUAL;
        entry->policy = CachePolicy::ON_DEMAND;
        entry->file_attributes = FILE_ATTRIBUTE_DIRECTORY;

        cache_entries[entry->virtual_path] = std::move(entry);
    }

    return STATUS_SUCCESS;
}

NTSTATUS HybridFileSystem::Init(PVOID Host)
{
    auto *host = static_cast<Fsp::FileSystemHost *>(Host);

    host->SetSectorSize(ALLOCATION_UNIT);
    host->SetSectorsPerAllocationUnit(1);
    host->SetFileInfoTimeout(1000);
    host->SetCaseSensitiveSearch(FALSE);
    host->SetCasePreservedNames(TRUE);
    host->SetUnicodeOnDisk(TRUE);
    host->SetPersistentAcls(TRUE);
    host->SetPostCleanupWhenModifiedOnly(TRUE);
    host->SetPassQueryDirectoryPattern(TRUE);
    host->SetVolumeCreationTime(creation_time);
    host->SetVolumeSerialNumber(0x12345678);
    host->SetFlushAndPurgeOnCleanup(TRUE);

    return STATUS_SUCCESS;
}

NTSTATUS HybridFileSystem::GetVolumeInfo(VolumeInfo *VolumeInfo)
{
    VolumeInfo->TotalSize = config.global.total_cache_size_mb * 1024ULL * 1024ULL;
    VolumeInfo->FreeSize = VolumeInfo->TotalSize - (current_cache_size * 1024ULL * 1024ULL);

    return STATUS_SUCCESS;
}

NTSTATUS HybridFileSystem::GetSecurityByName(PWSTR FileName, PUINT32 PFileAttributes, PSECURITY_DESCRIPTOR SecurityDescriptor, SIZE_T *PSecurityDescriptorSize)
{
    std::wstring virtual_path(FileName);

    // Get or create cache entry
    CacheEntry *entry = getCacheEntry(virtual_path);
    if (!entry)
    {
        return STATUS_OBJECT_NAME_NOT_FOUND;
    }

    if (PFileAttributes)
    {
        *PFileAttributes = entry->file_attributes;
    }

    if (PSecurityDescriptorSize)
    {
        // For now, use a default security descriptor
        if (*PSecurityDescriptorSize >= sizeof(SECURITY_DESCRIPTOR))
        {
            // Create a simple security descriptor
            InitializeSecurityDescriptor(SecurityDescriptor, SECURITY_DESCRIPTOR_REVISION);
            SetSecurityDescriptorDacl(SecurityDescriptor, TRUE, nullptr, FALSE);
            *PSecurityDescriptorSize = sizeof(SECURITY_DESCRIPTOR);
        }
        else
        {
            *PSecurityDescriptorSize = sizeof(SECURITY_DESCRIPTOR);
            return STATUS_BUFFER_TOO_SMALL;
        }
    }

    return STATUS_SUCCESS;
}

NTSTATUS HybridFileSystem::Open(PWSTR FileName, UINT32 CreateOptions, UINT32 GrantedAccess, PVOID *PFileNode, PVOID *PFileDesc, OpenFileInfo *OpenFileInfo)
{
    auto start_time = std::chrono::high_resolution_clock::now();
    std::wstring virtual_path(FileName);

    // Record filesystem operation
    GlobalMetrics::instance().recordFilesystemOperation("open");

    // Get or create cache entry
    CacheEntry *entry = getCacheEntry(virtual_path);
    if (!entry)
    {
        return STATUS_OBJECT_NAME_NOT_FOUND;
    }

    // Ensure file is available locally
    NTSTATUS result = ensureFileAvailable(entry);
    if (!NT_SUCCESS(result))
    {
        // For async downloads, STATUS_PENDING is returned
        // WinFsp will retry the operation when the download completes
        return result;
    }

    // Create file descriptor
    auto *file_desc = new FileDescriptor();

    DWORD create_flags = FILE_FLAG_BACKUP_SEMANTICS;
    if (CreateOptions & FILE_DELETE_ON_CLOSE)
    {
        create_flags |= FILE_FLAG_DELETE_ON_CLOSE;
    }

    // Check if file is in memory cache first
    if (entry->state == FileState::CACHED && entry->local_path.empty())
    {
        // File is in memory cache - check if we have it
        if (memory_cache.isFileInMemoryCache(entry->virtual_path))
        {
            // File is in memory - create a temporary file if we need a handle for compatibility
            std::wstring temp_path = createTemporaryFileForMemoryCached(entry);

            if (!temp_path.empty())
            {
                file_desc->handle =
                CreateFileW(temp_path.c_str(), GrantedAccess, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                            nullptr, OPEN_EXISTING, create_flags | FILE_FLAG_DELETE_ON_CLOSE, nullptr);
                entry->local_path = temp_path; // Track temp file for cleanup
            }
            else
            {
                // If temp file creation fails, use INVALID_HANDLE_VALUE
                // Read method will still work with memory cache
                file_desc->handle = INVALID_HANDLE_VALUE;
            }
        }
        else
        {
            // Memory cache miss - fallback to network
            file_desc->handle = CreateFileW(entry->network_path.c_str(), GrantedAccess,
                                            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                                            OPEN_EXISTING, create_flags, nullptr);
        }
    }
    else
    {
        // File not in memory cache - use local_path or network_path
        std::wstring full_path = entry->local_path.empty() ? entry->network_path : entry->local_path;
        file_desc->handle = CreateFileW(full_path.c_str(), GrantedAccess, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                        nullptr, OPEN_EXISTING, create_flags, nullptr);
    }

    // For memory-cached files, we allow INVALID_HANDLE_VALUE since Read() can serve from memory
    if (file_desc->handle == INVALID_HANDLE_VALUE && !(entry->state == FileState::CACHED && entry->local_path.empty() &&
                                                       memory_cache.isFileInMemoryCache(entry->virtual_path)))
    {
        delete file_desc;
        return CeWinFileCache::WineCompat::GetLastErrorAsNtStatus();
    }

    file_desc->entry = entry;
    *PFileDesc = file_desc;

    // Get file info
    BY_HANDLE_FILE_INFORMATION file_info;
    if (!GetFileInformationByHandle(file_desc->handle, &file_info))
    {
        delete file_desc;
        return CeWinFileCache::WineCompat::GetLastErrorAsNtStatus();
    }

    // Fill OpenFileInfo
    OpenFileInfo->FileInfo.FileAttributes = file_info.dwFileAttributes;
    OpenFileInfo->FileInfo.ReparseTag = 0;
    OpenFileInfo->FileInfo.FileSize = ((UINT64)file_info.nFileSizeHigh << 32) | (UINT64)file_info.nFileSizeLow;
    OpenFileInfo->FileInfo.AllocationSize = (OpenFileInfo->FileInfo.FileSize + ALLOCATION_UNIT - 1) / ALLOCATION_UNIT * ALLOCATION_UNIT;
    OpenFileInfo->FileInfo.CreationTime = ((PLARGE_INTEGER)&file_info.ftCreationTime)->QuadPart;
    OpenFileInfo->FileInfo.LastAccessTime = ((PLARGE_INTEGER)&file_info.ftLastAccessTime)->QuadPart;
    OpenFileInfo->FileInfo.LastWriteTime = ((PLARGE_INTEGER)&file_info.ftLastWriteTime)->QuadPart;
    OpenFileInfo->FileInfo.ChangeTime = OpenFileInfo->FileInfo.LastWriteTime;
    OpenFileInfo->FileInfo.IndexNumber = 0;
    OpenFileInfo->FileInfo.HardLinks = 0;

    // Update cache entry metadata
    entry->file_attributes = file_info.dwFileAttributes;
    entry->file_size = OpenFileInfo->FileInfo.FileSize;
    entry->creation_time = file_info.ftCreationTime;
    entry->last_access_time = file_info.ftLastAccessTime;
    entry->last_write_time = file_info.ftLastWriteTime;

    updateAccessTime(entry);

    // Record file open duration
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration<double>(end_time - start_time).count();
    GlobalMetrics::instance().recordFileOpenDuration(duration);

    return STATUS_SUCCESS;
}

VOID HybridFileSystem::Close(PVOID FileNode, PVOID FileDesc)
{
    auto *file_desc = static_cast<FileDescriptor *>(FileDesc);
    delete file_desc;
}

NTSTATUS HybridFileSystem::Read(PVOID FileNode, PVOID FileDesc, PVOID Buffer, UINT64 Offset, ULONG Length, PULONG PBytesTransferred)
{
    auto *file_desc = static_cast<FileDescriptor *>(FileDesc);

    // Record filesystem operation
    GlobalMetrics::instance().recordFilesystemOperation("read");

    // Try to serve from memory cache first for maximum performance
    if (file_desc->entry && file_desc->entry->state == FileState::CACHED && file_desc->entry->local_path.empty())
    {
        // File is cached in memory - serve directly from memory cache
        auto cached = memory_cache.getMemoryCachedFile(file_desc->entry->virtual_path);

        if (cached.has_value())
        {
            const auto &content = cached.value();

            // Validate read parameters
            if (Offset >= content.size())
            {
                *PBytesTransferred = 0;
                return STATUS_END_OF_FILE;
            }

            // Calculate actual bytes to transfer
            ULONG bytes_available = static_cast<ULONG>(content.size() - Offset);
            ULONG bytes_to_read = std::min(Length, bytes_available);

            // Copy data directly from memory
            memcpy(Buffer, content.data() + Offset, bytes_to_read);
            *PBytesTransferred = bytes_to_read;

            // Update access statistics
            updateAccessTime(file_desc->entry);

            return STATUS_SUCCESS;
        }
    }

    // Fallback to file handle read (for network-only files or cache miss)
    OVERLAPPED overlapped = {};
    overlapped.Offset = static_cast<DWORD>(Offset);
    overlapped.OffsetHigh = static_cast<DWORD>(Offset >> 32);

    if (!ReadFile(file_desc->handle, Buffer, Length, PBytesTransferred, &overlapped))
    {
        return CeWinFileCache::WineCompat::GetLastErrorAsNtStatus();
    }

    // Update access statistics
    if (file_desc->entry)
    {
        updateAccessTime(file_desc->entry);
    }

    return STATUS_SUCCESS;
}

NTSTATUS HybridFileSystem::GetFileInfo(PVOID FileNode, PVOID FileDesc, FileInfo *FileInfo)
{
    auto *file_desc = static_cast<FileDescriptor *>(FileDesc);

    BY_HANDLE_FILE_INFORMATION file_info;
    if (!GetFileInformationByHandle(file_desc->handle, &file_info))
    {
        return CeWinFileCache::WineCompat::GetLastErrorAsNtStatus();
    }

    FileInfo->FileAttributes = file_info.dwFileAttributes;
    FileInfo->ReparseTag = 0;
    FileInfo->FileSize = ((UINT64)file_info.nFileSizeHigh << 32) | (UINT64)file_info.nFileSizeLow;
    FileInfo->AllocationSize = (FileInfo->FileSize + ALLOCATION_UNIT - 1) / ALLOCATION_UNIT * ALLOCATION_UNIT;
    FileInfo->CreationTime = ((PLARGE_INTEGER)&file_info.ftCreationTime)->QuadPart;
    FileInfo->LastAccessTime = ((PLARGE_INTEGER)&file_info.ftLastAccessTime)->QuadPart;
    FileInfo->LastWriteTime = ((PLARGE_INTEGER)&file_info.ftLastWriteTime)->QuadPart;
    FileInfo->ChangeTime = FileInfo->LastWriteTime;
    FileInfo->IndexNumber = 0;
    FileInfo->HardLinks = 0;

    return STATUS_SUCCESS;
}

NTSTATUS HybridFileSystem::ReadDirectory(PVOID FileNode, PVOID FileDesc, PWSTR Pattern, PWSTR Marker, PVOID Buffer, ULONG Length, PULONG PBytesTransferred)
{
    auto *file_desc = static_cast<FileDescriptor *>(FileDesc);
    return BufferedReadDirectory(&file_desc->dir_buffer, FileNode, FileDesc, Pattern, Marker, Buffer, Length, PBytesTransferred);
}

NTSTATUS HybridFileSystem::ReadDirectoryEntry(PVOID FileNode, PVOID FileDesc, PWSTR Pattern, PWSTR Marker, PVOID *PContext, DirInfo *DirInfo)
{
    auto *file_desc = static_cast<FileDescriptor *>(FileDesc);

    if (!file_desc->entry)
    {
        return STATUS_INVALID_PARAMETER;
    }

    if (*PContext == nullptr)
    {
        // Start enumeration - get all directory contents
        std::vector<DirectoryNode *> contents = directory_cache.getDirectoryContents(file_desc->entry->virtual_path);

        if (contents.empty())
        {
            return STATUS_NO_MORE_FILES;
        }

        // Store the contents vector as context (simplified - in production should be more robust)
        auto *context_data = new std::vector<DirectoryNode *>(std::move(contents));
        *PContext = context_data;

        // Return first entry
        auto *first_entry = (*context_data)[0];
        fillDirInfo(DirInfo, first_entry);

        return STATUS_SUCCESS;
    }
    else
    {
        // Continue enumeration
        auto *context_data = static_cast<std::vector<DirectoryNode *> *>(*PContext);

        // Find current position
        size_t current_index = 0;
        if (Marker && wcslen(Marker) > 0)
        {
            // Find marker in the list
            for (size_t i = 0; i < context_data->size(); i++)
            {
                if ((*context_data)[i]->name == Marker)
                {
                    current_index = i + 1;
                    break;
                }
            }
        }
        else
        {
            current_index = 1; // Next after first
        }

        if (current_index >= context_data->size())
        {
            // End of enumeration
            delete context_data;
            *PContext = nullptr;
            return STATUS_NO_MORE_FILES;
        }

        // Return next entry
        auto *next_entry = (*context_data)[current_index];
        fillDirInfo(DirInfo, next_entry);

        return STATUS_SUCCESS;
    }
}

// Private methods

std::wstring HybridFileSystem::resolveVirtualPath(const std::wstring &virtual_path)
{
    // todo: implement proper path resolution for different compilers
    return virtual_path;
}

CacheEntry *HybridFileSystem::getCacheEntry(const std::wstring &virtual_path)
{
    std::lock_guard<std::mutex> lock(cache_mutex);

    auto it = cache_entries.find(virtual_path);
    if (it != cache_entries.end())
    {
        return it->second.get();
    }

    // Create virtual entry for files that don't exist yet
    // In practice, this would check if the file exists on the network
    auto entry = std::make_unique<CacheEntry>();
    entry->virtual_path = virtual_path;
    entry->state = FileState::VIRTUAL;
    entry->policy = determineCachePolicy(virtual_path);

    // todo: determine network path based on virtual path and compiler config

    CacheEntry *result = entry.get();
    cache_entries[virtual_path] = std::move(entry);
    return result;
}

NTSTATUS HybridFileSystem::ensureFileAvailable(CacheEntry *entry)
{
    if (entry->state == FileState::CACHED)
    {
        // File is already cached in memory
        return STATUS_SUCCESS;
    }

    if (entry->state == FileState::FETCHING)
    {
        // File is currently being fetched by async download manager
        return STATUS_PENDING;
    }

    // Check if file is already being downloaded
    if (download_manager && download_manager->isDownloadInProgress(entry->virtual_path))
    {
        entry->state = FileState::FETCHING;
        return STATUS_PENDING;
    }

    // For NEVER_CACHE policy, don't use async download
    if (entry->policy == CachePolicy::NEVER_CACHE)
    {
        entry->local_path = entry->network_path;
        entry->state = FileState::NETWORK_ONLY;
        return STATUS_SUCCESS;
    }

    // Queue async download for ALWAYS_CACHE and ON_DEMAND policies
    if (download_manager && !entry->network_path.empty())
    {
        entry->state = FileState::FETCHING;

        NTSTATUS status =
        download_manager->queueDownload(entry->virtual_path, entry->network_path, entry, entry->policy,
                                        [this, entry](NTSTATUS download_status, const std::wstring &error)
                                        {
                                            if (download_status == STATUS_SUCCESS)
                                            {
                                                // Download completed successfully
                                                // The AsyncDownloadManager already updated the cache entry
                                                std::wcout << L"Download completed: " << entry->virtual_path << std::endl;
                                            }
                                            else if (download_status == STATUS_PENDING)
                                            {
                                                // Already downloading
                                                std::wcout << L"Already downloading: " << entry->virtual_path << std::endl;
                                            }
                                            else
                                            {
                                                // Download failed
                                                entry->state = FileState::NETWORK_ONLY;
                                                entry->local_path = entry->network_path;
                                                std::wcerr << L"Download failed for " << entry->virtual_path << L": "
                                                           << error << std::endl;
                                            }
                                        });

        return status;
    }

    // Fallback to synchronous fetch if async download manager not available
    return fetchFromNetwork(entry);
}

NTSTATUS HybridFileSystem::fetchFromNetwork(CacheEntry *entry)
{
    if (entry->network_path.empty())
    {
        return STATUS_OBJECT_NAME_NOT_FOUND;
    }

    entry->state = FileState::FETCHING;

    // Load file into memory cache for fast access
    if (entry->policy == CachePolicy::ALWAYS_CACHE || entry->policy == CachePolicy::ON_DEMAND)
    {
        // Load file content into memory cache
        auto content = memory_cache.getFileContent(entry->virtual_path, config);

        if (!content.empty())
        {
            // File successfully loaded into memory cache
            entry->file_size = content.size();
            entry->state = FileState::CACHED;

            // Update file metadata
            entry->last_used = std::chrono::steady_clock::now();
            entry->access_count++;

            // No local_path needed - file is served directly from memory
            entry->local_path.clear();
        }
        else
        {
            // Fallback to network-only access if memory caching fails
            entry->local_path = entry->network_path;
            entry->state = FileState::NETWORK_ONLY;
        }
    }
    else
    {
        // NEVER_CACHE policy - always access from network
        entry->local_path = entry->network_path;
        entry->state = FileState::NETWORK_ONLY;
    }

    return STATUS_SUCCESS;
}

bool HybridFileSystem::matchesPattern(const std::wstring &path, const std::wstring &pattern)
{
    return GlobMatcher::matches(path, pattern);
}

CachePolicy HybridFileSystem::determineCachePolicy(const std::wstring &virtual_path)
{
    // Extract compiler name from virtual path (e.g., "/msvc-14.40/bin/cl.exe" -> "msvc-14.40")
    if (virtual_path.empty() || virtual_path[0] != L'/')
    {
        return CachePolicy::NEVER_CACHE;
    }

    size_t second_slash = virtual_path.find(L'/', 1);
    if (second_slash == std::wstring::npos)
    {
        return CachePolicy::NEVER_CACHE;
    }

    std::wstring compiler_name = virtual_path.substr(1, second_slash - 1);
    std::wstring relative_path = virtual_path.substr(second_slash + 1);

    // Find compiler config
    auto compiler_it = config.compilers.find(compiler_name);
    if (compiler_it == config.compilers.end())
    {
        return CachePolicy::NEVER_CACHE;
    }

    const auto &compiler_config = compiler_it->second;

    // Check cache_always_patterns for immediate caching
    for (const auto &pattern : compiler_config.cache_always_patterns)
    {
        if (matchesPattern(relative_path, pattern))
        {
            return CachePolicy::ALWAYS_CACHE;
        }
    }

    // Default policy for compiler files is on-demand caching
    // This provides good performance while managing memory usage
    return CachePolicy::ON_DEMAND;
}

std::wstring HybridFileSystem::createTemporaryFileForMemoryCached(CacheEntry *entry)
{
    // Get cached content from memory
    auto cached = memory_cache.getMemoryCachedFile(entry->virtual_path);
    if (!cached.has_value())
    {
        return L""; // No content in cache
    }

    const auto &content = cached.value();

    // Create temporary file path
    wchar_t temp_path[MAX_PATH];
    wchar_t temp_dir[MAX_PATH];

    if (!GetTempPathW(MAX_PATH, temp_dir))
    {
        return L"";
    }

    if (!GetTempFileNameW(temp_dir, L"CWF", 0, temp_path))
    {
        return L"";
    }

    // Write memory content to temporary file
    HANDLE temp_file = CreateFileW(temp_path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_TEMPORARY, nullptr);

    if (temp_file == INVALID_HANDLE_VALUE)
    {
        return L"";
    }

    DWORD bytes_written;
    BOOL write_result = WriteFile(temp_file, content.data(), static_cast<DWORD>(content.size()), &bytes_written, nullptr);

    CloseHandle(temp_file);

    if (!write_result || bytes_written != content.size())
    {
        DeleteFileW(temp_path); // Clean up on failure
        return L"";
    }

    return std::wstring(temp_path);
}


void HybridFileSystem::fillDirInfo(DirInfo *dir_info, DirectoryNode *node)
{
    if (!dir_info || !node)
    {
        return;
    }

    // TODO: Implement proper DirInfo filling once WinFsp structure compatibility is resolved
    // For now, just zero-initialize to get the build working
    memset(dir_info, 0, sizeof(*dir_info));
    
    // Directory listing functionality will be implemented once structure layout is fixed
    // This allows the main caching functionality to work while directory enumeration is pending
}

NTSTATUS HybridFileSystem::evictIfNeeded()
{
    // todo: implement LRU eviction
    return STATUS_SUCCESS;
}

void HybridFileSystem::updateAccessTime(CacheEntry *entry)
{
    entry->last_used = std::chrono::steady_clock::now();
    entry->access_count++;
}

} // namespace CeWinFileCache

#endif
