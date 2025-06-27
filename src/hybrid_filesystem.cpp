#include <algorithm>
#include <ce-win-file-cache/glob_matcher.hpp>
#include <ce-win-file-cache/hybrid_filesystem.hpp>
#include <ce-win-file-cache/logger.hpp>
#include <ce-win-file-cache/string_utils.hpp>
#include <ce-win-file-cache/windows_compat.hpp>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <vector>

#ifndef NO_WINFSP
#include <sddl.h>
#endif

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
    std::wcout << L"[FS] Initialize() called" << std::endl;
    this->config = new_config;

    // Initialize global metrics if enabled
    if (new_config.global.metrics.enabled)
    {
        std::wcout
        << L"[FS] Initializing metrics on "
        << std::wstring(new_config.global.metrics.bind_address.begin(), new_config.global.metrics.bind_address.end())
        << L":" << new_config.global.metrics.port << std::endl;
        GlobalMetrics::initialize(new_config.global.metrics);
    }

    // Create cache directory if it doesn't exist
    std::wcout << L"[FS] Creating cache directory: " << new_config.global.cache_directory << std::endl;
    if (!CreateDirectoryW(new_config.global.cache_directory.c_str(), nullptr))
    {
        DWORD error = GetLastError();
        if (error != ERROR_ALREADY_EXISTS)
        {
            std::wcerr << L"[FS] ERROR: Failed to create cache directory, error: " << error << std::endl;
            return CeWinFileCache::WineCompat::NtStatusFromWin32(error);
        }
        std::wcout << L"[FS] Cache directory already exists" << std::endl;
    }
    else
    {
        std::wcout << L"[FS] Cache directory created successfully" << std::endl;
    }

    // Get creation time for volume
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    creation_time = ((PLARGE_INTEGER)&ft)->QuadPart;
    std::wcout << L"[FS] Volume creation time set: " << creation_time << std::endl;

    // Initialize directory cache with configured compiler paths
    std::wcout << L"[FS] Initializing directory cache with " << new_config.compilers.size() << L" compilers" << std::endl;
    NTSTATUS result = directory_cache.initialize(new_config);
    if (!NT_SUCCESS(result))
    {
        std::wcerr << L"[FS] ERROR: Directory cache initialization failed: 0x" << std::hex << result << std::endl;
        return result;
    }
    std::wcout << L"[FS] Directory cache initialized successfully, total nodes: " << directory_cache.getTotalNodes() << std::endl;

    // Initialize async download manager with configured number of worker threads
    std::wcout << L"[FS] Initializing async download manager with " << new_config.global.download_threads << L" threads"
               << std::endl;
    download_manager = std::make_unique<AsyncDownloadManager>(memory_cache, new_config, new_config.global.download_threads);

    // Initialize file access tracker if enabled
    if (new_config.global.file_tracking.enabled)
    {
        std::wcout << L"[FS] Initializing file access tracker" << std::endl;
        access_tracker = std::make_unique<FileAccessTracker>();
        access_tracker->initialize(new_config.global.file_tracking.report_directory,
                                   std::chrono::minutes(new_config.global.file_tracking.report_interval_minutes),
                                   new_config.global.file_tracking.top_files_count);
        access_tracker->startReporting();
        std::wcout << L"[FS] File access tracker started" << std::endl;
    }

    std::wcout << L"[FS] Initialize() completed successfully" << std::endl;
    return STATUS_SUCCESS;
}

NTSTATUS HybridFileSystem::SetCompilerPaths(const std::unordered_map<std::wstring, std::wstring> &compiler_paths)
{
    std::wcout << L"[FS] SetCompilerPaths() called with " << compiler_paths.size() << L" paths" << std::endl;
    std::lock_guard<std::mutex> lock(cache_mutex);

    // Initialize cache entries for each compiler
    for (const auto &[compiler_name, base_path] : compiler_paths)
    {
        std::wcout << L"[FS] Processing compiler: " << compiler_name << L" -> " << base_path << std::endl;
        auto it = config.compilers.find(compiler_name);
        if (it == config.compilers.end())
        {
            std::wcout << L"[FS] WARNING: Compiler " << compiler_name << L" not found in config" << std::endl;
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

        std::wcout << L"[FS] Created cache entry: " << entry->virtual_path << L" -> " << entry->network_path << std::endl;
        cache_entries[entry->virtual_path] = std::move(entry);
    }

    std::wcout << L"[FS] SetCompilerPaths() completed, total entries: " << cache_entries.size() << std::endl;
    return STATUS_SUCCESS;
}

NTSTATUS HybridFileSystem::Init(PVOID Host)
{
    std::wcout << L"[FS] Init() called - setting up WinFsp host parameters" << std::endl;
    auto *host = static_cast<Fsp::FileSystemHost *>(Host);

    std::wcout << L"[FS] Setting sector size: " << ALLOCATION_UNIT << std::endl;
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

    std::wcout << L"[FS] Init() completed successfully" << std::endl;
    return STATUS_SUCCESS;
}

NTSTATUS HybridFileSystem::GetVolumeInfo(VolumeInfo *VolumeInfo)
{
    std::wcout << L"[FS] GetVolumeInfo() called" << std::endl;
    VolumeInfo->TotalSize = config.global.total_cache_size_mb * 1024ULL * 1024ULL;
    VolumeInfo->FreeSize = VolumeInfo->TotalSize - (current_cache_size * 1024ULL * 1024ULL);

    std::wcout << L"[FS] Volume info - Total: " << VolumeInfo->TotalSize << L" bytes, Free: " << VolumeInfo->FreeSize
               << L" bytes" << std::endl;

    return STATUS_SUCCESS;
}

NTSTATUS HybridFileSystem::GetSecurityByName(PWSTR FileName, PUINT32 PFileAttributes, PSECURITY_DESCRIPTOR SecurityDescriptor, SIZE_T *PSecurityDescriptorSize)
{
    std::wstring virtual_path(FileName);
    Logger::info("[FS] GetSecurityByName() called for: '{}'", StringUtils::wideToUtf8(virtual_path));

    // Get or create cache entry
    CacheEntry *entry = getCacheEntry(virtual_path);
    if (!entry)
    {
        Logger::info("[FS] GetSecurityByName() - entry NOT FOUND for: '{}'", StringUtils::wideToUtf8(virtual_path));
        return STATUS_OBJECT_NAME_NOT_FOUND;
    }

    Logger::info("[FS] GetSecurityByName() - found entry, attributes: 0x{:x}", entry->file_attributes);

    if (PFileAttributes)
    {
        *PFileAttributes = entry->file_attributes;
        Logger::info("[FS] GetSecurityByName() - returning file attributes: 0x{:x}", *PFileAttributes);
    }

    if (PSecurityDescriptorSize)
    {
        Logger::info("[FS] GetSecurityByName() - security descriptor requested");

        // Get current user SID to create real 164-byte security descriptors
        static std::wstring cachedUserSid;
        if (cachedUserSid.empty())
        {
            HANDLE hToken;
            if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken))
            {
                DWORD dwLength = 0;
                GetTokenInformation(hToken, TokenUser, nullptr, 0, &dwLength);
                if (GetLastError() == ERROR_INSUFFICIENT_BUFFER)
                {
                    std::vector<BYTE> buffer(dwLength);
                    PTOKEN_USER pTokenUser = (PTOKEN_USER)buffer.data();
                    if (GetTokenInformation(hToken, TokenUser, pTokenUser, dwLength, &dwLength))
                    {
                        LPWSTR pStringSid;
                        if (ConvertSidToStringSidW(pTokenUser->User.Sid, &pStringSid))
                        {
                            cachedUserSid = pStringSid;
                            LocalFree(pStringSid);
                        }
                    }
                }
                CloseHandle(hToken);
            }

            // Fallback if we can't get user SID
            if (cachedUserSid.empty())
            {
                cachedUserSid = L"S-1-5-32-545"; // Users group
            }
        }

        // Create real Windows SDDL that matches 164-byte user-created objects
        std::wstring sddl;
        if (entry->file_attributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            // Directory: Match real user-created directory pattern (164 bytes)
            sddl = L"O:" + cachedUserSid + L"G:" + cachedUserSid + L"D:(A;OICI;FA;;;SY)(A;OICI;FA;;;BA)(A;OICI;FA;;;" +
                   cachedUserSid + L")";
        }
        else
        {
            // File: Match real user-created file pattern (164 bytes)
            sddl = L"O:" + cachedUserSid + L"G:" + cachedUserSid + L"D:(A;;FA;;;SY)(A;;FA;;;BA)(A;;FA;;;" + cachedUserSid + L")";
        }

        PSECURITY_DESCRIPTOR pSD = nullptr;
        if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(sddl.c_str(), SDDL_REVISION_1, &pSD, nullptr))
        {
            DWORD err = GetLastError();
            std::wcout << L"[FS] GetSecurityByName() - ConvertStringSecurityDescriptor failed: " << err << std::endl;
            return STATUS_UNSUCCESSFUL;
        }

        // Get the size of the security descriptor
        DWORD sdSize = GetSecurityDescriptorLength(pSD);

        // If SecurityDescriptor is NULL, caller wants to know the required size
        if (!SecurityDescriptor)
        {
            *PSecurityDescriptorSize = sdSize;
            std::wcout << L"[FS] GetSecurityByName() - returning required size: " << sdSize << std::endl;
            LocalFree(pSD);
            return STATUS_SUCCESS;
        }

        // Check if the provided buffer is large enough
        if (*PSecurityDescriptorSize < sdSize)
        {
            *PSecurityDescriptorSize = sdSize;
            std::wcout << L"[FS] GetSecurityByName() - buffer too small, need: " << sdSize << std::endl;
            LocalFree(pSD);
            return STATUS_BUFFER_TOO_SMALL;
        }

        // Copy the security descriptor to the output buffer
        memcpy(SecurityDescriptor, pSD, sdSize);
        *PSecurityDescriptorSize = sdSize;

        Logger::info("[FS] GetSecurityByName() - provided real security descriptor, size: {}", sdSize);

        LocalFree(pSD);
    }

    Logger::info("[FS] GetSecurityByName() completed successfully");
    return STATUS_SUCCESS;
}

NTSTATUS HybridFileSystem::GetFileInfoByName(PWSTR FileName, FileInfo *FileInfo)
{
    std::wstring virtual_path(FileName);
    Logger::info("[FS] GetFileInfoByName() called for: '{}'", StringUtils::wideToUtf8(virtual_path));

    // Get cache entry (same logic as GetSecurityByName)
    CacheEntry *entry = getCacheEntry(virtual_path);
    if (!entry)
    {
        Logger::info("[FS] GetFileInfoByName() - entry NOT FOUND for: '{}'", StringUtils::wideToUtf8(virtual_path));
        return STATUS_OBJECT_NAME_NOT_FOUND;
    }

    // Fill FileInfo with entry metadata (same as GetFileInfo for opened files)
    FileInfo->FileAttributes = entry->file_attributes;
    FileInfo->ReparseTag = 0;
    FileInfo->FileSize = entry->file_size;
    FileInfo->AllocationSize = (entry->file_size + ALLOCATION_UNIT - 1) / ALLOCATION_UNIT * ALLOCATION_UNIT;
    FileInfo->CreationTime = ((PLARGE_INTEGER)&entry->creation_time)->QuadPart;
    FileInfo->LastAccessTime = ((PLARGE_INTEGER)&entry->last_access_time)->QuadPart;
    FileInfo->LastWriteTime = ((PLARGE_INTEGER)&entry->last_write_time)->QuadPart;
    FileInfo->ChangeTime = FileInfo->LastWriteTime;
    FileInfo->IndexNumber = 0;
    FileInfo->HardLinks = 0;

    Logger::info("[FS] GetFileInfoByName() - File: {}, Size: {}, Attributes: 0x{:x}", 
                 StringUtils::wideToUtf8(virtual_path), entry->file_size, entry->file_attributes);
    Logger::info("[FS] GetFileInfoByName() completed successfully");
    return STATUS_SUCCESS;
}

NTSTATUS HybridFileSystem::Open(PWSTR FileName, UINT32 CreateOptions, UINT32 GrantedAccess, PVOID *PFileNode, PVOID *PFileDesc, OpenFileInfo *OpenFileInfo)
{
    std::wstring virtual_path(FileName);
    std::wcout << L"[FS] Open() called for: '" << virtual_path << L"'" << std::endl;

    auto start_time = std::chrono::high_resolution_clock::now();

    // Record filesystem operation
    GlobalMetrics::instance().recordFilesystemOperation("open");

    // Get or create cache entry
    CacheEntry *entry = getCacheEntry(virtual_path);
    if (!entry)
    {
        std::wcout << L"[FS] Open() - entry not found for: '" << virtual_path << L"'" << std::endl;
        return STATUS_OBJECT_NAME_NOT_FOUND;
    }
    std::wcout << L"[FS] Open() - entry found" << std::endl;

    // Ensure file is available locally
    std::wcout << L"[FS] Open() - ensuring file available" << std::endl;
    NTSTATUS result = ensureFileAvailable(entry);
    if (!NT_SUCCESS(result))
    {
        std::wcout << L"[FS] Open() - ensureFileAvailable failed: 0x" << std::hex << result << std::endl;
        // For async downloads, STATUS_PENDING is returned
        // WinFsp will retry the operation when the download completes
        return result;
    }
    std::wcout << L"[FS] Open() - file available" << std::endl;

    // Create file descriptor
    std::wcout << L"[FS] Open() - creating file descriptor" << std::endl;
    auto *file_desc = new FileDescriptor();

    DWORD create_flags = FILE_FLAG_BACKUP_SEMANTICS;
    if (CreateOptions & FILE_DELETE_ON_CLOSE)
    {
        create_flags |= FILE_FLAG_DELETE_ON_CLOSE;
    }

    // Handle directories separately from files
    if (entry->file_attributes & FILE_ATTRIBUTE_DIRECTORY)
    {
        std::wcout << L"[FS] Open() - handling directory" << std::endl;
        // For directories, we don't need a real file handle
        file_desc->handle = INVALID_HANDLE_VALUE;
    }
    else
    {
        std::wcout << L"[FS] Open() - handling file" << std::endl;
        // Check if file is in memory cache first
        if (entry->state == FileState::CACHED && entry->local_path.empty())
        {
            // File is in memory cache - check if we have it
            Logger::debug("[FS] Open() - calling isFileInMemoryCache for: {}", StringUtils::wideToUtf8(entry->virtual_path));
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

        // For files, check if handle creation failed
        Logger::debug("[FS] Open() - calling isFileInMemoryCache (error check) for: {}", StringUtils::wideToUtf8(entry->virtual_path));
        if (file_desc->handle == INVALID_HANDLE_VALUE && !(entry->state == FileState::CACHED && entry->local_path.empty() &&
                                                           memory_cache.isFileInMemoryCache(entry->virtual_path)))
        {
            std::wcout << L"[FS] Open() - file handle creation failed" << std::endl;
            delete file_desc;
            return CeWinFileCache::WineCompat::GetLastErrorAsNtStatus();
        }
    }

    file_desc->entry = entry;

    std::wcout << L"[FS] Open() - setting up file info" << std::endl;

    // Get file info - handle directories differently
    if (entry->file_attributes & FILE_ATTRIBUTE_DIRECTORY)
    {
        std::wcout << L"[FS] Open() - using cached directory info" << std::endl;
        // For directories, use cached/default info since we don't have a real handle
        OpenFileInfo->FileInfo.FileAttributes = entry->file_attributes;
        OpenFileInfo->FileInfo.ReparseTag = 0;
        OpenFileInfo->FileInfo.FileSize = 0; // Directories have size 0
        OpenFileInfo->FileInfo.AllocationSize = ALLOCATION_UNIT;
        OpenFileInfo->FileInfo.CreationTime = creation_time;
        OpenFileInfo->FileInfo.LastAccessTime = creation_time;
        OpenFileInfo->FileInfo.LastWriteTime = creation_time;
        OpenFileInfo->FileInfo.ChangeTime = creation_time;
        OpenFileInfo->FileInfo.IndexNumber = 0;
        OpenFileInfo->FileInfo.HardLinks = 0;

        // Update cache entry metadata with default values
        entry->file_size = 0;
        GetSystemTimeAsFileTime(&entry->creation_time);
        entry->last_access_time = entry->creation_time;
        entry->last_write_time = entry->creation_time;
    }
    else
    {
        std::wcout << L"[FS] Open() - using cached file metadata" << std::endl;
        // Use cached metadata from DirectoryNode instead of querying handle
        // This ensures consistent file information for virtual filesystem files
        
        // Fill OpenFileInfo with cached metadata
        OpenFileInfo->FileInfo.FileAttributes = entry->file_attributes;
        OpenFileInfo->FileInfo.ReparseTag = 0;
        OpenFileInfo->FileInfo.FileSize = entry->file_size;
        OpenFileInfo->FileInfo.AllocationSize =
        (entry->file_size + ALLOCATION_UNIT - 1) / ALLOCATION_UNIT * ALLOCATION_UNIT;
        OpenFileInfo->FileInfo.CreationTime = ((PLARGE_INTEGER)&entry->creation_time)->QuadPart;
        OpenFileInfo->FileInfo.LastAccessTime = ((PLARGE_INTEGER)&entry->last_access_time)->QuadPart;
        OpenFileInfo->FileInfo.LastWriteTime = ((PLARGE_INTEGER)&entry->last_write_time)->QuadPart;
        OpenFileInfo->FileInfo.ChangeTime = OpenFileInfo->FileInfo.LastWriteTime;
        OpenFileInfo->FileInfo.IndexNumber = 0;
        OpenFileInfo->FileInfo.HardLinks = 0;
    }


    updateAccessTime(entry);

    // Record file open duration
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration<double>(end_time - start_time).count();
    GlobalMetrics::instance().recordFileOpenDuration(duration);

    // Track file access
    if (access_tracker)
    {
        bool is_cache_hit = (result == STATUS_SUCCESS && entry->state == FileState::CACHED);
        bool is_memory_cached = false;
        
        // Only check memory cache for files, not directories
        if (!(entry->file_attributes & FILE_ATTRIBUTE_DIRECTORY))
        {
            Logger::debug("[FS] Open() - calling isFileInMemoryCache (access tracker) for: {}", StringUtils::wideToUtf8(entry->virtual_path));
            is_memory_cached = memory_cache.isFileInMemoryCache(entry->virtual_path);
        }
        else
        {
            Logger::debug("[FS] Open() - skipping isFileInMemoryCache for directory: {}", StringUtils::wideToUtf8(entry->virtual_path));
        }

        access_tracker->recordAccess(entry->virtual_path, entry->network_path, entry->file_size, entry->state,
                                     is_cache_hit, is_memory_cached,
                                     duration * 1000.0, // Convert to milliseconds
                                     entry->policy == CachePolicy::ALWAYS_CACHE ? L"always_cache" :
                                     entry->policy == CachePolicy::ON_DEMAND    ? L"on_demand" :
                                                                                  L"never_cache");
    }

    *PFileDesc = file_desc;
    *PFileNode = nullptr;

    std::wcout << L"[FS] Open() completed successfully" << std::endl;
    return STATUS_SUCCESS;
}

VOID HybridFileSystem::Close(PVOID FileNode, PVOID FileDesc)
{
    auto *file_desc = static_cast<FileDescriptor *>(FileDesc);
    delete file_desc;
}

NTSTATUS HybridFileSystem::Read(PVOID FileNode, PVOID FileDesc, PVOID Buffer, UINT64 Offset, ULONG Length, PULONG PBytesTransferred)
{
    auto start_time = std::chrono::high_resolution_clock::now();
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

    // Track read operation
    if (access_tracker && file_desc->entry)
    {
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration<double>(end_time - start_time).count();

        bool is_cache_hit = (file_desc->entry->state == FileState::CACHED);
        Logger::debug("[FS] Read() - calling isFileInMemoryCache (access tracker) for: {}", StringUtils::wideToUtf8(file_desc->entry->virtual_path));
        bool is_memory_cached = memory_cache.isFileInMemoryCache(file_desc->entry->virtual_path);

        access_tracker->recordAccess(file_desc->entry->virtual_path, file_desc->entry->network_path,
                                     file_desc->entry->file_size, file_desc->entry->state, is_cache_hit, is_memory_cached,
                                     duration * 1000.0, // Convert to milliseconds
                                     file_desc->entry->policy == CachePolicy::ALWAYS_CACHE ? L"always_cache" :
                                     file_desc->entry->policy == CachePolicy::ON_DEMAND    ? L"on_demand" :
                                                                                             L"never_cache");
    }

    return STATUS_SUCCESS;
}

NTSTATUS HybridFileSystem::GetFileInfo(PVOID FileNode, PVOID FileDesc, FileInfo *FileInfo)
{
    auto *file_desc = static_cast<FileDescriptor *>(FileDesc);

    // Use cached metadata from DirectoryNode instead of querying handle
    // This ensures consistent file information for virtual filesystem files
    CacheEntry *entry = file_desc->entry;
    
    FileInfo->FileAttributes = entry->file_attributes;
    FileInfo->ReparseTag = 0;
    FileInfo->FileSize = entry->file_size;
    FileInfo->AllocationSize = (entry->file_size + ALLOCATION_UNIT - 1) / ALLOCATION_UNIT * ALLOCATION_UNIT;
    FileInfo->CreationTime = ((PLARGE_INTEGER)&entry->creation_time)->QuadPart;
    FileInfo->LastAccessTime = ((PLARGE_INTEGER)&entry->last_access_time)->QuadPart;
    FileInfo->LastWriteTime = ((PLARGE_INTEGER)&entry->last_write_time)->QuadPart;
    FileInfo->ChangeTime = FileInfo->LastWriteTime;
    FileInfo->IndexNumber = 0;
    FileInfo->HardLinks = 0;

    // Log key values for Properties dialog debugging
    Logger::info("[FS] GetFileInfo() - File: {}, Size: {}, Attributes: 0x{:x}, CreationTime: {}", 
                 StringUtils::wideToUtf8(entry->virtual_path), entry->file_size, entry->file_attributes, FileInfo->CreationTime);

    return STATUS_SUCCESS;
}

NTSTATUS HybridFileSystem::SetBasicInfo(PVOID FileNode, PVOID FileDesc, UINT32 FileAttributes, UINT64 CreationTime, UINT64 LastAccessTime, UINT64 LastWriteTime, UINT64 ChangeTime, FileInfo *FileInfo)
{
    Logger::info("[FS] SetBasicInfo() called - FileAttributes: 0x{:x}", FileAttributes);
    auto *file_desc = static_cast<FileDescriptor *>(FileDesc);
    CacheEntry *entry = file_desc->entry;

    // Update cached metadata with new values
    if (FileAttributes != 0xFFFFFFFF) // 0xFFFFFFFF means don't change
    {
        entry->file_attributes = FileAttributes;
        Logger::debug("[FS] SetBasicInfo() - Updated file attributes to: 0x{:x}", FileAttributes);
    }

    if (CreationTime != 0) // 0 means don't change
    {
        entry->creation_time = *((FILETIME*)&CreationTime);
        Logger::debug("[FS] SetBasicInfo() - Updated creation time");
    }

    if (LastAccessTime != 0)
    {
        entry->last_access_time = *((FILETIME*)&LastAccessTime);
        Logger::debug("[FS] SetBasicInfo() - Updated last access time");
    }

    if (LastWriteTime != 0)
    {
        entry->last_write_time = *((FILETIME*)&LastWriteTime);
        Logger::debug("[FS] SetBasicInfo() - Updated last write time");
    }

    // Return current file info after update
    if (FileInfo != nullptr)
    {
        FileInfo->FileAttributes = entry->file_attributes;
        FileInfo->ReparseTag = 0;
        FileInfo->FileSize = entry->file_size;
        FileInfo->AllocationSize = (entry->file_size + ALLOCATION_UNIT - 1) / ALLOCATION_UNIT * ALLOCATION_UNIT;
        FileInfo->CreationTime = ((PLARGE_INTEGER)&entry->creation_time)->QuadPart;
        FileInfo->LastAccessTime = ((PLARGE_INTEGER)&entry->last_access_time)->QuadPart;
        FileInfo->LastWriteTime = ((PLARGE_INTEGER)&entry->last_write_time)->QuadPart;
        FileInfo->ChangeTime = FileInfo->LastWriteTime;
        FileInfo->IndexNumber = 0;
        FileInfo->HardLinks = 0;
    }

    Logger::info("[FS] SetBasicInfo() completed successfully");
    return STATUS_SUCCESS;
}

NTSTATUS HybridFileSystem::GetSecurity(PVOID FileNode, PVOID FileDesc, PSECURITY_DESCRIPTOR SecurityDescriptor, SIZE_T *PSecurityDescriptorSize)
{
    Logger::debug("[FS] GetSecurity() called for opened file");
    auto *file_desc = static_cast<FileDescriptor *>(FileDesc);
    CacheEntry *entry = file_desc->entry;

    // Use the same security descriptor logic as GetSecurityByName
    // This ensures consistency between the two methods
    
    const char *sddl_string = "O:S-1-5-21-663732323-46111922-2075403870-1001G:S-1-5-21-663732323-46111922-2075403870-1001D:(A;OICI;FA;;;SY)(A;OICI;FA;;;BA)(A;OICI;FA;;;S-1-5-21-663732323-46111922-2075403870-1001)";
    
    PSECURITY_DESCRIPTOR temp_descriptor = nullptr;
    if (!ConvertStringSecurityDescriptorToSecurityDescriptorA(sddl_string, SDDL_REVISION_1, &temp_descriptor, nullptr))
    {
        Logger::debug("[FS] GetSecurity() - Failed to convert SDDL string, error: {}", GetLastError());
        return CeWinFileCache::WineCompat::NtStatusFromWin32(GetLastError());
    }

    DWORD descriptor_length = GetSecurityDescriptorLength(temp_descriptor);
    Logger::debug("[FS] GetSecurity() - Security descriptor size: {}", descriptor_length);

    if (SecurityDescriptor == nullptr)
    {
        // Caller is querying the required buffer size
        *PSecurityDescriptorSize = descriptor_length;
        LocalFree(temp_descriptor);
        Logger::debug("[FS] GetSecurity() - Returning required size: {}", descriptor_length);
        return STATUS_SUCCESS;
    }

    if (*PSecurityDescriptorSize < descriptor_length)
    {
        // Buffer too small
        *PSecurityDescriptorSize = descriptor_length;
        LocalFree(temp_descriptor);
        Logger::debug("[FS] GetSecurity() - Buffer too small");
        return STATUS_BUFFER_TOO_SMALL;
    }

    // Copy security descriptor to provided buffer
    memcpy(SecurityDescriptor, temp_descriptor, descriptor_length);
    *PSecurityDescriptorSize = descriptor_length;
    LocalFree(temp_descriptor);

    Logger::debug("[FS] GetSecurity() completed successfully");
    return STATUS_SUCCESS;
}

NTSTATUS HybridFileSystem::SetSecurity(PVOID FileNode, PVOID FileDesc, SECURITY_INFORMATION SecurityInformation, PSECURITY_DESCRIPTOR ModificationDescriptor)
{
    Logger::debug("[FS] SetSecurity() called - SecurityInformation: 0x{:x}", SecurityInformation);
    
    // For a read-only caching filesystem, we don't actually modify security descriptors
    // But we should accept the call to avoid blocking the Properties dialog
    // Just log what's being requested and return success
    
    if (SecurityInformation & OWNER_SECURITY_INFORMATION)
    {
        Logger::debug("[FS] SetSecurity() - Owner security information requested");
    }
    if (SecurityInformation & GROUP_SECURITY_INFORMATION)
    {
        Logger::debug("[FS] SetSecurity() - Group security information requested");
    }
    if (SecurityInformation & DACL_SECURITY_INFORMATION)
    {
        Logger::debug("[FS] SetSecurity() - DACL security information requested");
    }
    if (SecurityInformation & SACL_SECURITY_INFORMATION)
    {
        Logger::debug("[FS] SetSecurity() - SACL security information requested");
    }
    
    // For a caching filesystem, we accept the modification but don't actually change anything
    // This allows the Properties dialog to work without errors
    Logger::debug("[FS] SetSecurity() completed successfully (no-op for caching filesystem)");
    return STATUS_SUCCESS;
}

NTSTATUS HybridFileSystem::ReadDirectory(PVOID FileNode, PVOID FileDesc, PWSTR Pattern, PWSTR Marker, PVOID Buffer, ULONG Length, PULONG PBytesTransferred)
{
    Logger::debug("[FS] ReadDirectory() called - Length: {}, Marker: {}", Length, Marker ? StringUtils::wideToUtf8(Marker) : "null");
    auto *file_desc = static_cast<FileDescriptor *>(FileDesc);

    // Instead of using BufferedReadDirectory, implement directory enumeration directly
    // This gives us more control over the marker handling

    // Get directory path
    std::wstring dir_path = file_desc->entry->virtual_path;
    std::wstring normalized_path = normalizePath(dir_path);

    Logger::debug("[FS] ReadDirectory() - enumerating directory: '{}'", StringUtils::wideToUtf8(normalized_path));

    // Get directory contents
    std::vector<DirectoryNode *> contents;
    if (normalized_path == L"/")
    {
        contents = directory_cache.getDirectoryContents(L"/");
        Logger::debug("[FS] ReadDirectory() - root directory has {} entries", contents.size());
    }
    else
    {
        contents = directory_cache.getDirectoryContents(dir_path);
        Logger::debug("[FS] ReadDirectory() - directory '{}' has {} entries", StringUtils::wideToUtf8(dir_path),
                      contents.size());
    }

    if (contents.empty())
    {
        Logger::debug("[FS] ReadDirectory() - no entries found");
        *PBytesTransferred = 0;
        return STATUS_NO_MORE_FILES;
    }

    // Find starting index based on marker using binary search for O(log n) performance
    size_t start_index = 0;
    if (Marker && wcslen(Marker) > 0)
    {
        std::string marker_str = StringUtils::wideToUtf8(Marker);
        
        // Use binary search to find marker position (assumes contents are sorted by name)
        auto it = std::lower_bound(contents.begin(), contents.end(), Marker,
            [](const DirectoryNode* node, const std::wstring& marker) {
                return node->name < marker;
            });
        
        if (it != contents.end() && (*it)->name == Marker)
        {
            start_index = std::distance(contents.begin(), it) + 1; // Next entry after marker
            Logger::debug("[FS] ReadDirectory() - found marker '{}' at index {}", marker_str, start_index - 1);
        }
        else
        {
            Logger::debug("[FS] ReadDirectory() - marker '{}' not found, starting from beginning", marker_str);
        }
    }
    else
    {
        Logger::debug("[FS] ReadDirectory() - no marker, starting from beginning");
    }

    // Fill buffer with directory entries
    PUCHAR buffer_ptr = (PUCHAR)Buffer;
    ULONG bytes_used = 0;
    size_t entries_returned = 0;

    for (size_t i = start_index; i < contents.size(); i++)
    {
        DirectoryNode *node = contents[i];

        // Calculate size needed for this entry
        size_t name_chars = node->name.length();
        size_t name_bytes = name_chars * sizeof(WCHAR);
        ULONG entry_size = sizeof(FSP_FSCTL_DIR_INFO) + name_bytes + sizeof(WCHAR); // +WCHAR for null terminator

        // Align to 8-byte boundary
        entry_size = (entry_size + 7) & ~7;

        // Removed verbose per-entry logging for performance

        // Check if entry fits in remaining buffer
        if (bytes_used + entry_size > Length)
        {
            Logger::debug("[FS] ReadDirectory() - buffer full, stopping at index {}", i);
            break;
        }

        // Fill directory info
        FSP_FSCTL_DIR_INFO *dir_info = (FSP_FSCTL_DIR_INFO *)(buffer_ptr + bytes_used);
        dir_info->Size = (UINT16)entry_size;

        // Set file attributes
        dir_info->FileInfo.FileAttributes = node->isDirectory() ? (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_OFFLINE) : FILE_ATTRIBUTE_NORMAL;
        dir_info->FileInfo.ReparseTag = 0;
        dir_info->FileInfo.FileSize = node->isDirectory() ? 0 : node->file_size;
        dir_info->FileInfo.AllocationSize =
        node->isDirectory() ? ALLOCATION_UNIT : (dir_info->FileInfo.FileSize + ALLOCATION_UNIT - 1) / ALLOCATION_UNIT * ALLOCATION_UNIT;

        // Set timestamps
        UINT64 creation_ts = ((PLARGE_INTEGER)&node->creation_time)->QuadPart;
        UINT64 access_ts = ((PLARGE_INTEGER)&node->last_access_time)->QuadPart;
        UINT64 write_ts = ((PLARGE_INTEGER)&node->last_write_time)->QuadPart;

        if (creation_ts == 0)
            creation_ts = creation_time;
        if (access_ts == 0)
            access_ts = creation_time;
        if (write_ts == 0)
            write_ts = creation_time;

        dir_info->FileInfo.CreationTime = creation_ts;
        dir_info->FileInfo.LastAccessTime = access_ts;
        dir_info->FileInfo.LastWriteTime = write_ts;
        dir_info->FileInfo.ChangeTime = write_ts;
        dir_info->FileInfo.IndexNumber = 0;
        dir_info->FileInfo.HardLinks = 0;

        // Clear padding
        memset(dir_info->Padding, 0, sizeof(dir_info->Padding));

        // Copy filename with null termination
        memcpy(dir_info->FileNameBuf, node->name.c_str(), name_bytes);
        WCHAR *filename_end = (WCHAR *)((char *)dir_info->FileNameBuf + name_bytes);
        *filename_end = L'\0';

        bytes_used += entry_size;
        entries_returned++;

        // Removed verbose per-entry logging for performance
    }

    *PBytesTransferred = bytes_used;

    Logger::debug("[FS] ReadDirectory() - returning {} entries, {} bytes total", entries_returned, bytes_used);

    if (start_index + entries_returned >= contents.size())
    {
        Logger::debug("[FS] ReadDirectory() - enumeration complete");
        return STATUS_SUCCESS; // End of directory
    }
    else
    {
        Logger::debug("[FS] ReadDirectory() - more entries available");
        return STATUS_SUCCESS; // More entries available
    }
}

NTSTATUS HybridFileSystem::ReadDirectoryEntry(PVOID FileNode, PVOID FileDesc, PWSTR Pattern, PWSTR Marker, PVOID *PContext, DirInfo *DirInfo)
{
    Logger::debug("[FS] ReadDirectoryEntry() called - Context: {}, Marker: {}", *PContext ? "exists" : "null",
                  Marker ? StringUtils::wideToUtf8(Marker) : "null");
    auto *file_desc = static_cast<FileDescriptor *>(FileDesc);

    if (!file_desc->entry)
    {
        Logger::debug("[FS] ReadDirectoryEntry() - no entry");
        return STATUS_INVALID_PARAMETER;
    }

    Logger::debug("[FS] ReadDirectoryEntry() - path: '{}'", StringUtils::wideToUtf8(file_desc->entry->virtual_path));

    if (*PContext == nullptr)
    {
        Logger::debug("[FS] ReadDirectoryEntry() - starting enumeration");

        // Handle root directory specially - return compiler directories
        // Normalize path for comparison (both \ and / should be treated as root)
        std::wstring normalized_path = normalizePath(file_desc->entry->virtual_path);
        if (normalized_path == L"/")
        {
            Logger::debug("[FS] ReadDirectoryEntry() - enumerating root directory");

            // Use DirectoryCache to get all contents in root directory
            std::vector<DirectoryNode *> root_contents = directory_cache.getDirectoryContents(L"/");

            if (root_contents.empty())
            {
                Logger::debug("[FS] ReadDirectoryEntry() - no contents from DirectoryCache for root");
                return STATUS_NO_MORE_FILES;
            }

            Logger::debug("[FS] ReadDirectoryEntry() - found {} entries in root directory", root_contents.size());
            for (size_t i = 0; i < root_contents.size(); i++)
            {
                Logger::debug("[FS] ReadDirectoryEntry() - root entry[{}]: '{}'", i,
                              StringUtils::wideToUtf8(root_contents[i]->name));
            }

            // Store the contents vector as context for enumeration
            auto *context = new std::vector<DirectoryNode *>(std::move(root_contents));
            *PContext = context;

            // Return first entry
            auto *first_entry = (*context)[0];
            fillDirInfo(DirInfo, first_entry);

            Logger::debug("[FS] ReadDirectoryEntry() - returning first root entry: '{}' (name: '{}')",
                          StringUtils::wideToUtf8(first_entry->full_virtual_path), StringUtils::wideToUtf8(first_entry->name));
            return STATUS_SUCCESS;
        }
        else
        {
            Logger::debug("[FS] ReadDirectoryEntry() - enumerating non-root directory");

            // For non-root directories, use directory cache
            std::vector<DirectoryNode *> contents = directory_cache.getDirectoryContents(file_desc->entry->virtual_path);

            if (contents.empty())
            {
                Logger::debug("[FS] ReadDirectoryEntry() - no contents from directory cache");
                return STATUS_NO_MORE_FILES;
            }

            // Store the contents vector as context (simplified - in production should be more robust)
            auto *context_data = new std::vector<DirectoryNode *>(std::move(contents));
            *PContext = context_data;

            // Return first entry
            auto *first_entry = (*context_data)[0];
            fillDirInfo(DirInfo, first_entry);

            Logger::debug("[FS] ReadDirectoryEntry() - returning first non-root entry: '{}'",
                          StringUtils::wideToUtf8(first_entry->full_virtual_path));
            return STATUS_SUCCESS;
        }
    }
    else
    {
        Logger::debug("[FS] ReadDirectoryEntry() - continuing enumeration");

        // Continue enumeration using stored context
        auto *context_data = static_cast<std::vector<DirectoryNode *> *>(*PContext);

        Logger::debug("[FS] ReadDirectoryEntry() - context has {} entries", context_data->size());
        for (size_t i = 0; i < context_data->size() && i < 5; i++) // Show first 5 entries
        {
            Logger::debug("[FS] ReadDirectoryEntry() - context entry[{}]: '{}'", i,
                          StringUtils::wideToUtf8((*context_data)[i]->name));
        }

        // Find current position based on marker
        size_t current_index = 0;
        bool found_marker = false;

        if (Marker && wcslen(Marker) > 0)
        {
            std::string marker_str = StringUtils::wideToUtf8(Marker);
            Logger::debug("[FS] ReadDirectoryEntry() - searching for marker: '{}'", marker_str);

            // Find marker in the list - marker is the name of the last returned entry
            for (size_t i = 0; i < context_data->size(); i++)
            {
                if ((*context_data)[i]->name == Marker)
                {
                    current_index = i + 1; // Next entry after the marker
                    found_marker = true;
                    Logger::debug("[FS] ReadDirectoryEntry() - found marker at index {}, next index: {}", i, current_index);
                    break;
                }
            }

            if (!found_marker)
            {
                Logger::debug("[FS] ReadDirectoryEntry() - marker '{}' not found, starting from index 1", marker_str);
                current_index = 1; // Fallback to second entry
            }
        }
        else
        {
            Logger::debug("[FS] ReadDirectoryEntry() - no marker, continuing from index 1");
            current_index = 1; // Next after first
        }

        if (current_index >= context_data->size())
        {
            // End of enumeration
            Logger::debug("[FS] ReadDirectoryEntry() - end of enumeration at index {} (total: {})", current_index,
                          context_data->size());
            delete context_data;
            *PContext = nullptr;
            return STATUS_NO_MORE_FILES;
        }

        // Return next entry
        auto *next_entry = (*context_data)[current_index];
        fillDirInfo(DirInfo, next_entry);

        Logger::debug("[FS] ReadDirectoryEntry() - returning entry: '{}' (full path: '{}', index {})",
                      StringUtils::wideToUtf8(next_entry->name), StringUtils::wideToUtf8(next_entry->full_virtual_path),
                      current_index);
        return STATUS_SUCCESS;
    }
}

// Private methods

std::wstring HybridFileSystem::resolveVirtualPath(const std::wstring &virtual_path)
{
    std::wstring resolved = L"\\\\127.0.0.1\\efs";
    resolved.append(virtual_path);
    return resolved;
}

CacheEntry *HybridFileSystem::getCacheEntry(const std::wstring &virtual_path)
{
    std::wcout << L"[FS] getCacheEntry() called for: '" << virtual_path << L"'" << std::endl;
    std::lock_guard<std::mutex> lock(cache_mutex);

    // 1. Check existing cache_entries first (fast path)
    auto it = cache_entries.find(virtual_path);
    if (it != cache_entries.end())
    {
        std::wcout << L"[FS] getCacheEntry() - found existing entry for: '" << virtual_path << L"'" << std::endl;
        std::wcout << L"[FS] getCacheEntry() - entry state: " << static_cast<int>(it->second->state)
                   << L", attributes: 0x" << std::hex << it->second->file_attributes << std::endl;
        return it->second.get();
    }

    // 2. Check DirectoryCache for path existence
    std::wcout << L"[FS] getCacheEntry() - checking DirectoryCache for: '" << virtual_path << L"'" << std::endl;
    DirectoryNode *node = directory_cache.findNode(virtual_path);
    if (node)
    {
        std::wcout << L"[FS] getCacheEntry() - found node in DirectoryCache, creating dynamic entry" << std::endl;
        // 3. Create dynamic cache entry from DirectoryNode
        return createDynamicCacheEntry(node);
    }

    return nullptr;
}

CacheEntry *HybridFileSystem::createDynamicCacheEntry(DirectoryNode *node)
{
    Logger::info("[FS] createDynamicCacheEntry() called for: '{}', attributes: 0x{:x}", 
                 StringUtils::wideToUtf8(node->full_virtual_path), node->file_attributes);

    // Create cache entry from DirectoryNode
    auto entry = std::make_unique<CacheEntry>();
    entry->virtual_path = node->full_virtual_path;
    entry->network_path = node->network_path;
    entry->file_attributes = node->file_attributes; // Use actual file attributes from DirectoryNode
    entry->file_size = node->file_size;
    entry->creation_time = node->creation_time;
    entry->last_access_time = node->last_access_time;
    entry->last_write_time = node->last_write_time;

    // Determine caching policy based on the file path
    entry->policy = determineCachePolicy(node->full_virtual_path);

    // Set initial state based on policy
    if (entry->policy == CachePolicy::NEVER_CACHE)
    {
        entry->state = FileState::NETWORK_ONLY;
        entry->local_path = entry->network_path;
    }
    else
    {
        // For files that should be cached, start as VIRTUAL and let ensureFileAvailable handle caching
        entry->state = FileState::VIRTUAL;
    }

    Logger::info("[FS] createDynamicCacheEntry() - created entry for: '{}' -> '{}', policy: {}", 
                 StringUtils::wideToUtf8(entry->virtual_path), StringUtils::wideToUtf8(entry->network_path), static_cast<int>(entry->policy));

    // Store in cache_entries for future fast access
    CacheEntry *result = entry.get();
    cache_entries[node->full_virtual_path] = std::move(entry);

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
        entry->is_downloading.store(true);

        NTSTATUS status =
        download_manager->queueDownload(entry->virtual_path, entry->network_path, entry, entry->policy,
                                        [this](NTSTATUS download_status, const std::wstring error, CacheEntry *entry)
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

std::wstring HybridFileSystem::normalizePath(const std::wstring &path)
{
    if (path.empty())
    {
        return L"/";
    }

    std::wstring normalized = path;

    // Convert backslashes to forward slashes for consistent storage
    for (auto &ch : normalized)
    {
        if (ch == L'\\')
        {
            ch = L'/';
        }
    }

    // Ensure path starts with /
    if (normalized[0] != L'/')
    {
        normalized = L"/" + normalized;
    }

    // Handle root path specially
    if (normalized == L"/")
    {
        return L"/";
    }

    // Remove trailing slash (except for root)
    if (normalized.length() > 1 && normalized.back() == L'/')
    {
        normalized.pop_back();
    }

    return normalized;
}

void HybridFileSystem::fillDirInfo(DirInfo *dir_info, DirectoryNode *node)
{
    if (!dir_info || !node)
    {
        return;
    }

    // Fill DirInfo structure properly based on WinFsp FSP_FSCTL_DIR_INFO
    const std::wstring &name = node->name;
    size_t name_length_chars = name.length();
    size_t name_size_bytes = name_length_chars * sizeof(WCHAR);

    // Size includes the structure + filename + null terminator
    dir_info->Size = (UINT16)(sizeof(FSP_FSCTL_DIR_INFO) + name_size_bytes + sizeof(WCHAR));

    Logger::debug("[FS] fillDirInfo() - filling entry for: '{}', size: {}", StringUtils::wideToUtf8(name), dir_info->Size);

    // Set file attributes based on node type
    dir_info->FileInfo.FileAttributes = node->isDirectory() ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_NORMAL;
    dir_info->FileInfo.ReparseTag = 0;
    dir_info->FileInfo.FileSize = node->isDirectory() ? 0 : node->file_size;
    dir_info->FileInfo.AllocationSize =
    node->isDirectory() ? ALLOCATION_UNIT : (dir_info->FileInfo.FileSize + ALLOCATION_UNIT - 1) / ALLOCATION_UNIT * ALLOCATION_UNIT;

    // Convert FILETIME to UINT64 timestamps
    UINT64 creation_timestamp = ((PLARGE_INTEGER)&node->creation_time)->QuadPart;
    UINT64 access_timestamp = ((PLARGE_INTEGER)&node->last_access_time)->QuadPart;
    UINT64 write_timestamp = ((PLARGE_INTEGER)&node->last_write_time)->QuadPart;

    // Use default creation time if node times are not set
    if (creation_timestamp == 0)
        creation_timestamp = creation_time;
    if (access_timestamp == 0)
        access_timestamp = creation_time;
    if (write_timestamp == 0)
        write_timestamp = creation_time;

    dir_info->FileInfo.CreationTime = creation_timestamp;
    dir_info->FileInfo.LastAccessTime = access_timestamp;
    dir_info->FileInfo.LastWriteTime = write_timestamp;
    dir_info->FileInfo.ChangeTime = write_timestamp;
    dir_info->FileInfo.IndexNumber = 0;
    dir_info->FileInfo.HardLinks = 0;

    // Clear padding
    memset(dir_info->Padding, 0, sizeof(dir_info->Padding));

    // Copy filename to FileNameBuf with proper null termination
    memcpy(dir_info->FileNameBuf, name.c_str(), name_size_bytes);
    // Ensure null termination
    WCHAR *filename_end = (WCHAR *)((char *)dir_info->FileNameBuf + name_size_bytes);
    *filename_end = L'\0';

    Logger::debug("[FS] fillDirInfo() - completed for: '{}', filename set in buffer", StringUtils::wideToUtf8(name));
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
