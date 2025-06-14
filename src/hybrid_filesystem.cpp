#include <algorithm>
#include <ce-win-file-cache/glob_matcher.hpp>
#include <ce-win-file-cache/hybrid_filesystem.hpp>
#include <ce-win-file-cache/windows_compat.hpp>
#include <chrono>
#include <iostream>
#include <iomanip>
#include <sddl.h>
#include <vector>

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
        std::wcout << L"[FS] Initializing metrics on " << std::wstring(new_config.global.metrics.bind_address.begin(), new_config.global.metrics.bind_address.end())
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
    std::wcout << L"[FS] Directory cache initialized successfully" << std::endl;

    // Initialize async download manager with configured number of worker threads
    std::wcout << L"[FS] Initializing async download manager with " << new_config.global.download_threads << L" threads" << std::endl;
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
    
    std::wcout << L"[FS] Volume info - Total: " << VolumeInfo->TotalSize 
               << L" bytes, Free: " << VolumeInfo->FreeSize << L" bytes" << std::endl;

    return STATUS_SUCCESS;
}

NTSTATUS HybridFileSystem::GetSecurityByName(PWSTR FileName, PUINT32 PFileAttributes, PSECURITY_DESCRIPTOR SecurityDescriptor, SIZE_T *PSecurityDescriptorSize)
{
    std::wstring virtual_path(FileName);
    std::wcout << L"[FS] GetSecurityByName() called for: '" << virtual_path << L"'" << std::endl;

    // Get or create cache entry
    CacheEntry *entry = getCacheEntry(virtual_path);
    if (!entry)
    {
        std::wcout << L"[FS] GetSecurityByName() - entry not found for: '" << virtual_path << L"'" << std::endl;
        return STATUS_OBJECT_NAME_NOT_FOUND;
    }

    std::wcout << L"[FS] GetSecurityByName() - found entry, attributes: 0x" << std::hex << entry->file_attributes << std::endl;

    if (PFileAttributes)
    {
        *PFileAttributes = entry->file_attributes;
        std::wcout << L"[FS] GetSecurityByName() - returning file attributes: 0x" << std::hex << *PFileAttributes << std::endl;
    }

    if (PSecurityDescriptorSize)
    {
        std::wcout << L"[FS] GetSecurityByName() - security descriptor requested" << std::endl;
        
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
            sddl = L"O:" + cachedUserSid + L"G:" + cachedUserSid + 
                   L"D:(A;OICI;FA;;;SY)(A;OICI;FA;;;BA)(A;OICI;FA;;;" + cachedUserSid + L")";
        }
        else
        {
            // File: Match real user-created file pattern (164 bytes)  
            sddl = L"O:" + cachedUserSid + L"G:" + cachedUserSid + 
                   L"D:(A;;FA;;;SY)(A;;FA;;;BA)(A;;FA;;;" + cachedUserSid + L")";
        }
        
        PSECURITY_DESCRIPTOR pSD = nullptr;
        if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(
            sddl.c_str(), SDDL_REVISION_1, &pSD, nullptr))
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
        
        std::wcout << L"[FS] GetSecurityByName() - provided real security descriptor, size: " << sdSize 
                   << L", SDDL: " << sddl << std::endl;
        
        LocalFree(pSD);
    }

    std::wcout << L"[FS] GetSecurityByName() completed successfully" << std::endl;
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
        if (file_desc->handle == INVALID_HANDLE_VALUE && !(entry->state == FileState::CACHED && entry->local_path.empty() &&
                                                           memory_cache.isFileInMemoryCache(entry->virtual_path)))
        {
            std::wcout << L"[FS] Open() - file handle creation failed" << std::endl;
            delete file_desc;
            return CeWinFileCache::WineCompat::GetLastErrorAsNtStatus();
        }
    }

    file_desc->entry = entry;
    *PFileNode = entry;
    *PFileDesc = file_desc;

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
        std::wcout << L"[FS] Open() - getting file info from handle" << std::endl;
        BY_HANDLE_FILE_INFORMATION file_info;
        if (!GetFileInformationByHandle(file_desc->handle, &file_info))
        {
            std::wcout << L"[FS] Open() - GetFileInformationByHandle failed" << std::endl;
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
        bool is_memory_cached = memory_cache.isFileInMemoryCache(entry->virtual_path);

        access_tracker->recordAccess(entry->virtual_path, entry->network_path, entry->file_size, entry->state,
                                     is_cache_hit, is_memory_cached,
                                     duration * 1000.0, // Convert to milliseconds
                                     entry->policy == CachePolicy::ALWAYS_CACHE ? L"always_cache" :
                                     entry->policy == CachePolicy::ON_DEMAND    ? L"on_demand" :
                                                                                  L"never_cache");
    }

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
    std::wcout << L"[FS] ReadDirectory() called" << std::endl;
    auto *file_desc = static_cast<FileDescriptor *>(FileDesc);
    return BufferedReadDirectory(&file_desc->dir_buffer, FileNode, FileDesc, Pattern, Marker, Buffer, Length, PBytesTransferred);
}

NTSTATUS HybridFileSystem::ReadDirectoryEntry(PVOID FileNode, PVOID FileDesc, PWSTR Pattern, PWSTR Marker, PVOID *PContext, DirInfo *DirInfo)
{
    std::wcout << L"[FS] ReadDirectoryEntry() called" << std::endl;
    auto *file_desc = static_cast<FileDescriptor *>(FileDesc);

    if (!file_desc->entry)
    {
        std::wcout << L"[FS] ReadDirectoryEntry() - no entry" << std::endl;
        return STATUS_INVALID_PARAMETER;
    }

    std::wcout << L"[FS] ReadDirectoryEntry() - path: '" << file_desc->entry->virtual_path << L"'" << std::endl;

    if (*PContext == nullptr)
    {
        std::wcout << L"[FS] ReadDirectoryEntry() - starting enumeration" << std::endl;
        
        // Handle root directory specially - return compiler directories
        if (file_desc->entry->virtual_path == L"\\" || file_desc->entry->virtual_path == L"")
        {
            std::wcout << L"[FS] ReadDirectoryEntry() - enumerating root directory" << std::endl;
            
            // Create directory enumeration context
            struct DirEnumContext {
                std::vector<std::wstring> entries;
                size_t next_index;
            };
            
            auto *context = new DirEnumContext();
            context->next_index = 0;
            
            std::lock_guard<std::mutex> lock(cache_mutex);
            for (const auto& [path, entry] : cache_entries)
            {
                // Skip the root entry and only include top-level directories
                if (path != L"\\" && path.find(L'\\', 1) == std::wstring::npos && !path.empty() && 
                    (entry->file_attributes & FILE_ATTRIBUTE_DIRECTORY))
                {
                    std::wstring dir_name = path.substr(1); // Remove leading backslash
                    context->entries.push_back(dir_name);
                    std::wcout << L"[FS] ReadDirectoryEntry() - adding directory: '" << dir_name << L"'" << std::endl;
                }
            }
            
            if (context->entries.empty())
            {
                std::wcout << L"[FS] ReadDirectoryEntry() - no directories found" << std::endl;
                delete context;
                return STATUS_NO_MORE_FILES;
            }
            
            *PContext = context;
            
            // Return first directory
            const std::wstring &dir_name = context->entries[0];
            DirInfo->Size = (UINT16)(sizeof(FSP_FSCTL_DIR_INFO) + dir_name.length() * sizeof(WCHAR));
            DirInfo->FileInfo.FileAttributes = FILE_ATTRIBUTE_DIRECTORY;
            DirInfo->FileInfo.ReparseTag = 0;
            DirInfo->FileInfo.FileSize = 0;
            DirInfo->FileInfo.AllocationSize = ALLOCATION_UNIT;
            DirInfo->FileInfo.CreationTime = creation_time;
            DirInfo->FileInfo.LastAccessTime = creation_time;
            DirInfo->FileInfo.LastWriteTime = creation_time;
            DirInfo->FileInfo.ChangeTime = creation_time;
            DirInfo->FileInfo.IndexNumber = 0;
            DirInfo->FileInfo.HardLinks = 0;
            memcpy(DirInfo->FileNameBuf, dir_name.c_str(), dir_name.length() * sizeof(WCHAR));
            
            context->next_index = 1;
            std::wcout << L"[FS] ReadDirectoryEntry() - returning first dir: '" << dir_name << L"'" << std::endl;
            return STATUS_SUCCESS;
        }
        else
        {
            // For non-root directories, use directory cache
            std::vector<DirectoryNode *> contents = directory_cache.getDirectoryContents(file_desc->entry->virtual_path);

            if (contents.empty())
            {
                std::wcout << L"[FS] ReadDirectoryEntry() - no contents from directory cache" << std::endl;
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
    }
    else
    {
        std::wcout << L"[FS] ReadDirectoryEntry() - continuing enumeration" << std::endl;
        
        // Handle root directory continuation
        if (file_desc->entry->virtual_path == L"\\" || file_desc->entry->virtual_path == L"")
        {
            struct DirEnumContext {
                std::vector<std::wstring> entries;
                size_t next_index;
            };
            
            auto *context = static_cast<DirEnumContext *>(*PContext);
            
            // Get the next entry from our stored index
            if (context->next_index >= context->entries.size())
            {
                std::wcout << L"[FS] ReadDirectoryEntry() - end of enumeration" << std::endl;
                delete context;
                *PContext = nullptr;
                return STATUS_NO_MORE_FILES;
            }
            
            // Return next directory
            const std::wstring &dir_name = context->entries[context->next_index];
            DirInfo->Size = (UINT16)(sizeof(FSP_FSCTL_DIR_INFO) + dir_name.length() * sizeof(WCHAR));
            DirInfo->FileInfo.FileAttributes = FILE_ATTRIBUTE_DIRECTORY;
            DirInfo->FileInfo.ReparseTag = 0;
            DirInfo->FileInfo.FileSize = 0;
            DirInfo->FileInfo.AllocationSize = ALLOCATION_UNIT;
            DirInfo->FileInfo.CreationTime = creation_time;
            DirInfo->FileInfo.LastAccessTime = creation_time;
            DirInfo->FileInfo.LastWriteTime = creation_time;
            DirInfo->FileInfo.ChangeTime = creation_time;
            DirInfo->FileInfo.IndexNumber = 0;
            DirInfo->FileInfo.HardLinks = 0;
            memcpy(DirInfo->FileNameBuf, dir_name.c_str(), dir_name.length() * sizeof(WCHAR));
            
            std::wcout << L"[FS] ReadDirectoryEntry() - returning dir: '" << dir_name << L"' (index " << context->next_index << L")" << std::endl;
            context->next_index++;
            return STATUS_SUCCESS;
        }
        else
        {
            // Continue enumeration for non-root directories
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
}

// Private methods

std::wstring HybridFileSystem::resolveVirtualPath(const std::wstring &virtual_path)
{
    // todo: implement proper path resolution for different compilers
    return virtual_path;
}

CacheEntry *HybridFileSystem::getCacheEntry(const std::wstring &virtual_path)
{
    std::wcout << L"[FS] getCacheEntry() called for: '" << virtual_path << L"'" << std::endl;
    std::lock_guard<std::mutex> lock(cache_mutex);

    auto it = cache_entries.find(virtual_path);
    if (it != cache_entries.end())
    {
        std::wcout << L"[FS] getCacheEntry() - found existing entry for: '" << virtual_path << L"'" << std::endl;
        std::wcout << L"[FS] getCacheEntry() - entry state: " << static_cast<int>(it->second->state) 
                   << L", attributes: 0x" << std::hex << it->second->file_attributes << std::endl;
        return it->second.get();
    }

    std::wcout << L"[FS] getCacheEntry() - creating new virtual entry for: '" << virtual_path << L"'" << std::endl;
    // Create virtual entry for files that don't exist yet
    // In practice, this would check if the file exists on the network
    auto entry = std::make_unique<CacheEntry>();
    entry->virtual_path = virtual_path;
    entry->state = FileState::VIRTUAL;
    entry->policy = determineCachePolicy(virtual_path);

    // Set default attributes based on path
    if (virtual_path == L"\\" || virtual_path.back() == L'\\')
    {
        entry->file_attributes = FILE_ATTRIBUTE_DIRECTORY;
        std::wcout << L"[FS] getCacheEntry() - treating as directory" << std::endl;
    }
    else
    {
        entry->file_attributes = FILE_ATTRIBUTE_NORMAL;
        std::wcout << L"[FS] getCacheEntry() - treating as file" << std::endl;
    }

    // todo: determine network path based on virtual path and compiler config

    CacheEntry *result = entry.get();
    cache_entries[virtual_path] = std::move(entry);
    std::wcout << L"[FS] getCacheEntry() - created entry with attributes: 0x" << std::hex << result->file_attributes << std::endl;
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

    // Fill DirInfo structure properly based on WinFsp FSP_FSCTL_DIR_INFO
    const std::wstring &name = node->name;
    dir_info->Size = (UINT16)(sizeof(FSP_FSCTL_DIR_INFO) + name.length() * sizeof(WCHAR));
    
    // Set file attributes based on node type
    dir_info->FileInfo.FileAttributes = node->isDirectory() ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_NORMAL;
    dir_info->FileInfo.ReparseTag = 0;
    dir_info->FileInfo.FileSize = node->isDirectory() ? 0 : node->file_size;
    dir_info->FileInfo.AllocationSize = node->isDirectory() ? ALLOCATION_UNIT : 
        (dir_info->FileInfo.FileSize + ALLOCATION_UNIT - 1) / ALLOCATION_UNIT * ALLOCATION_UNIT;
    
    // Convert FILETIME to UINT64 timestamps
    UINT64 creation_timestamp = ((PLARGE_INTEGER)&node->creation_time)->QuadPart;
    UINT64 access_timestamp = ((PLARGE_INTEGER)&node->last_access_time)->QuadPart;
    UINT64 write_timestamp = ((PLARGE_INTEGER)&node->last_write_time)->QuadPart;
    
    // Use default creation time if node times are not set
    if (creation_timestamp == 0) creation_timestamp = creation_time;
    if (access_timestamp == 0) access_timestamp = creation_time;
    if (write_timestamp == 0) write_timestamp = creation_time;
    
    dir_info->FileInfo.CreationTime = creation_timestamp;
    dir_info->FileInfo.LastAccessTime = access_timestamp;
    dir_info->FileInfo.LastWriteTime = write_timestamp;
    dir_info->FileInfo.ChangeTime = write_timestamp;
    dir_info->FileInfo.IndexNumber = 0;
    dir_info->FileInfo.HardLinks = 0;
    
    // Clear padding
    memset(dir_info->Padding, 0, sizeof(dir_info->Padding));
    
    // Copy filename to FileNameBuf
    memcpy(dir_info->FileNameBuf, name.c_str(), dir_info->Size - sizeof(FSP_FSCTL_DIR_INFO));
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
