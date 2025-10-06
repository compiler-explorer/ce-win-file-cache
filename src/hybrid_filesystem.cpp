#include <algorithm>
#include <cassert>
#include <ce-win-file-cache/glob_matcher.hpp>
#include <ce-win-file-cache/hybrid_filesystem.hpp>
#include <ce-win-file-cache/logger.hpp>
#include <ce-win-file-cache/string_utils.hpp>
#include <ce-win-file-cache/windows_compat.hpp>
#include <chrono>
#include <vector>

#ifndef NO_WINFSP
#include <sddl.h>
#endif

#ifndef NO_WINFSP

namespace CeWinFileCache
{

constexpr ULONG ALLOCATION_UNIT = 4096;
constexpr ULONG FULLPATH_SIZE = MAX_PATH + FSP_FSCTL_TRANSACT_PATH_SIZEMAX / sizeof(WCHAR);


HybridFileSystem::HybridFileSystem()
: Fsp::FileSystemBase(), current_cache_size(0), creation_time(0)
{
}

HybridFileSystem::~HybridFileSystem()
{
    // Stop background eviction thread
    shutdown_eviction = true;
    if (memory_eviction_thread.joinable())
    {
        memory_eviction_thread.join();
    }

    // Shutdown global metrics
    GlobalMetrics::shutdown();
}

NTSTATUS HybridFileSystem::Initialize(const Config &new_config)
{
    this->config = new_config;
    Logger::debug(LogCategory::FILESYSTEM, "Initialize() called");

    // Initialize global metrics if enabled
    if (new_config.global.metrics.enabled)
    {
        Logger::info(LogCategory::CONFIG, "Initializing metrics on {}:{}", new_config.global.metrics.bind_address,
                     new_config.global.metrics.port);
        GlobalMetrics::initialize(new_config.global.metrics);
    }

    // Create cache directory if it doesn't exist
    Logger::info(LogCategory::FILESYSTEM, "Creating cache directory: {}", StringUtils::wideToUtf8(new_config.global.cache_directory));
    if (!CreateDirectoryW(new_config.global.cache_directory.c_str(), nullptr))
    {
        DWORD error = GetLastError();
        if (error != ERROR_ALREADY_EXISTS)
        {
            Logger::error(LogCategory::FILESYSTEM, "Failed to create cache directory, error: {}", error);
            return CeWinFileCache::WineCompat::NtStatusFromWin32(error);
        }
        Logger::debug(LogCategory::FILESYSTEM, "Cache directory already exists");
    }
    else
    {
        Logger::debug(LogCategory::FILESYSTEM, "Cache directory created successfully");
    }

    // Get creation time for volume
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    creation_time = ((PLARGE_INTEGER)&ft)->QuadPart;
    Logger::info(LogCategory::FILESYSTEM, "Volume creation time set: {}", creation_time);

    // Initialize directory cache with configured compiler paths
    Logger::info(LogCategory::DIRECTORY, "Initializing directory cache with {} compilers", new_config.compilers.size());
    NTSTATUS result = directory_cache.initialize(new_config);
    if (!NT_SUCCESS(result))
    {
        Logger::error(LogCategory::DIRECTORY, "Directory cache initialization failed: 0x{:x}", result);
        return result;
    }
    Logger::info(LogCategory::DIRECTORY, "Directory cache initialized successfully, total nodes: {}",
                 directory_cache.getTotalNodes());

    // Initialize async download manager with configured number of worker threads
    Logger::info(LogCategory::NETWORK, "Initializing async download manager with {} threads", new_config.global.download_threads);
    download_manager = std::make_unique<AsyncDownloadManager>(memory_cache, new_config, new_config.global.download_threads);

    // Set up memory eviction callback for cache size management
    // Note: Immediate eviction removed - background thread handles eviction during idle periods
    download_manager->setEvictionCallback([this](size_t bytes_needed)
    {
        // Just record activity - background thread will handle eviction when system is idle
        recordActivity();
    });

    // Initialize activity tracking
    last_activity.store(std::chrono::steady_clock::now());

    // Start background memory eviction thread
    Logger::info(LogCategory::CACHE, "Starting background memory eviction thread");
    memory_eviction_thread = std::thread(&HybridFileSystem::memoryEvictionThreadFunc, this);

    // Initialize file access tracker if enabled
    if (new_config.global.file_tracking.enabled)
    {
        Logger::debug(LogCategory::ACCESS, "Initializing file access tracker");
        access_tracker = std::make_unique<FileAccessTracker>();
        access_tracker->initialize(new_config.global.file_tracking.report_directory,
                                   std::chrono::minutes(new_config.global.file_tracking.report_interval_minutes),
                                   new_config.global.file_tracking.top_files_count);
        access_tracker->startReporting();
        Logger::info(LogCategory::ACCESS, "File access tracker started");
    }

    Logger::debug(LogCategory::FILESYSTEM, "Initialize() completed successfully");
    return STATUS_SUCCESS;
}

NTSTATUS HybridFileSystem::SetPaths(const std::unordered_map<std::wstring, std::wstring> &compiler_paths)
{
    Logger::debug(LogCategory::CONFIG, "SetCompilerPaths() called with {} paths", compiler_paths.size());
    std::lock_guard<std::mutex> lock(cache_mutex);

    // Initialize cache entries for each compiler
    for (const auto &[compiler_name, base_path] : compiler_paths)
    {
        Logger::info(LogCategory::CONFIG, "Processing path: {} -> {}", StringUtils::wideToUtf8(compiler_name),
                     StringUtils::wideToUtf8(base_path));
        auto it = this->config.compilers.find(compiler_name);
        if (it == this->config.compilers.end())
        {
            Logger::warn(LogCategory::CONFIG, "Path {} not found in config", StringUtils::wideToUtf8(compiler_name));
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

        Logger::info(LogCategory::CACHE, "Created cache entry: {} -> {}", StringUtils::wideToUtf8(entry->virtual_path),
                     StringUtils::wideToUtf8(entry->network_path));
        cache_entries[entry->virtual_path] = std::move(entry);
    }

    Logger::info(LogCategory::CONFIG, "SetCompilerPaths() completed, total entries: {}", cache_entries.size());
    return STATUS_SUCCESS;
}

NTSTATUS HybridFileSystem::Init(PVOID Host)
{
    Logger::debug(LogCategory::FILESYSTEM, "Init() called - setting up WinFsp host parameters");
    auto *host = static_cast<Fsp::FileSystemHost *>(Host);

    Logger::info(LogCategory::FILESYSTEM, "Setting sector size: {}", ALLOCATION_UNIT);
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

    Logger::debug(LogCategory::FILESYSTEM, "Init() completed successfully");
    return STATUS_SUCCESS;
}

NTSTATUS HybridFileSystem::GetVolumeInfo(VolumeInfo *VolumeInfo)
{
    Logger::debug(LogCategory::FILESYSTEM, "GetVolumeInfo() called");

    // Initialize all fields to prevent buffer overflow errors
    memset(VolumeInfo, 0, sizeof(*VolumeInfo));

    VolumeInfo->TotalSize = config.global.total_cache_size_mb * 1024ULL * 1024ULL;
    VolumeInfo->FreeSize = VolumeInfo->TotalSize - (current_cache_size * 1024ULL * 1024ULL);

    // Set volume label - keep it short to avoid buffer issues
    wcscpy_s(VolumeInfo->VolumeLabel, sizeof(VolumeInfo->VolumeLabel) / sizeof(WCHAR), L"CompilerCache");

    Logger::debug(LogCategory::FILESYSTEM, "Volume info - Total: {} bytes, Free: {} bytes", VolumeInfo->TotalSize,
                  VolumeInfo->FreeSize);

    return STATUS_SUCCESS;
}

NTSTATUS HybridFileSystem::GetSecurityByName(PWSTR FileName, PUINT32 PFileAttributes, PSECURITY_DESCRIPTOR SecurityDescriptor, SIZE_T *PSecurityDescriptorSize)
{
    std::wstring virtual_path(FileName);
    Logger::debug(LogCategory::SECURITY, "GetSecurityByName() called for: '{}'", StringUtils::wideToUtf8(virtual_path));

    // Get or create cache entry
    CacheEntry *entry = getCacheEntry(virtual_path);
    if (!entry)
    {
        Logger::debug(LogCategory::SECURITY, "GetSecurityByName() - entry NOT FOUND for: '{}'", StringUtils::wideToUtf8(virtual_path));
        return STATUS_OBJECT_NAME_NOT_FOUND;
    }

    Logger::debug(LogCategory::SECURITY, "GetSecurityByName() - found entry, attributes: 0x{:x}", entry->file_attributes);

    if (PFileAttributes)
    {
        *PFileAttributes = entry->file_attributes;
        Logger::debug(LogCategory::SECURITY, "GetSecurityByName() - returning file attributes: 0x{:x} for: {}", *PFileAttributes, StringUtils::wideToUtf8(virtual_path));
    }

    if (entry->SecDesc != nullptr)
    {
        if (PSecurityDescriptorSize)
        {
            // Validate security descriptor before using it
            if (!IsValidSecurityDescriptor(entry->SecDesc))
            {
                Logger::error(LogCategory::SECURITY, "GetSecurityByName() - invalid security descriptor detected, using fallback");
                // Fall through to create default descriptor
            }
            else
            {
                DWORD sdSize = GetSecurityDescriptorLength(entry->SecDesc);

                // Sanity check the size - security descriptors shouldn't be huge
                if (sdSize == 0 || sdSize > 65536)
                {
                    Logger::error(LogCategory::SECURITY, "GetSecurityByName() - suspicious security descriptor size: {}, using fallback", sdSize);
                    // Fall through to create default descriptor
                }
                else
                {
                    // If SecurityDescriptor is NULL, caller wants to know the required size
                    if (!SecurityDescriptor)
                    {
                        *PSecurityDescriptorSize = sdSize;
                        Logger::debug(LogCategory::SECURITY, "GetSecurityByName() - returning required size: {}", sdSize);
                        return STATUS_SUCCESS;
                    }

                    // Check if the provided buffer is large enough
                    if (*PSecurityDescriptorSize < sdSize)
                    {
                        SIZE_T provided_size = *PSecurityDescriptorSize;
                        *PSecurityDescriptorSize = sdSize;
                        Logger::debug(LogCategory::SECURITY, "GetSecurityByName() - buffer too small, provided: {}, need: {}", provided_size, sdSize);
                        return STATUS_BUFFER_OVERFLOW;  // WinFsp retries on STATUS_BUFFER_OVERFLOW, not STATUS_BUFFER_TOO_SMALL
                    }

                    // Copy the security descriptor to the output buffer
                    memcpy(SecurityDescriptor, entry->SecDesc, sdSize);
                    *PSecurityDescriptorSize = sdSize;

                    Logger::debug(LogCategory::SECURITY, "GetSecurityByName() - provided real security descriptor, size: {}", sdSize);
                    return STATUS_SUCCESS;
                }
            }
        }
    }

    // Fallback: Create a simple default security descriptor
    if (PSecurityDescriptorSize)
    {
        const char *sddl_string = "O:SYG:SYD:(A;;FA;;;SY)(A;;FA;;;BA)(A;;FR;;;AU)";
        PSECURITY_DESCRIPTOR temp_descriptor = nullptr;

        if (ConvertStringSecurityDescriptorToSecurityDescriptorA(sddl_string, SDDL_REVISION_1, &temp_descriptor, nullptr))
        {
            DWORD sdSize = GetSecurityDescriptorLength(temp_descriptor);

            if (!SecurityDescriptor)
            {
                *PSecurityDescriptorSize = sdSize;
                LocalFree(temp_descriptor);
                Logger::debug(LogCategory::SECURITY, "GetSecurityByName() - returning fallback size: {}", sdSize);
                return STATUS_SUCCESS;
            }

            if (*PSecurityDescriptorSize < sdSize)
            {
                SIZE_T provided_size = *PSecurityDescriptorSize;
                *PSecurityDescriptorSize = sdSize;
                LocalFree(temp_descriptor);
                Logger::debug(LogCategory::SECURITY, "GetSecurityByName() - fallback buffer too small, provided: {}, need: {}", provided_size, sdSize);
                return STATUS_BUFFER_OVERFLOW;  // WinFsp retries on STATUS_BUFFER_OVERFLOW, not STATUS_BUFFER_TOO_SMALL
            }

            memcpy(SecurityDescriptor, temp_descriptor, sdSize);
            *PSecurityDescriptorSize = sdSize;
            LocalFree(temp_descriptor);

            Logger::debug(LogCategory::SECURITY, "GetSecurityByName() - used fallback security descriptor, size: {}", sdSize);
        }
        else
        {
            Logger::error(LogCategory::SECURITY, "GetSecurityByName() - failed to create fallback security descriptor");
            return STATUS_UNSUCCESSFUL;
        }
    }
    else
    {
        throw new std::runtime_error("Unexpected error: Cached entry does not have a security descriptor!");
    }

    Logger::debug(LogCategory::SECURITY, "GetSecurityByName() completed successfully");
    return STATUS_SUCCESS;
}

void HybridFileSystem::copyFileInfo(CacheEntry *source, FileInfo *dest) const
{
    dest->FileAttributes = source->file_attributes;
    dest->ReparseTag = 0; // Missing: Should be set for reparse points (symlinks, mount points)
    dest->FileSize = source->file_size;
    dest->AllocationSize = source->file_attributes & FILE_ATTRIBUTE_DIRECTORY ?
                           ALLOCATION_UNIT :
                           (source->file_size + ALLOCATION_UNIT - 1) / ALLOCATION_UNIT * ALLOCATION_UNIT;
    dest->CreationTime = std::bit_cast<UINT64>(source->creation_time);
    dest->LastAccessTime = std::bit_cast<UINT64>(source->last_access_time);
    dest->LastWriteTime = std::bit_cast<UINT64>(source->last_write_time);
    dest->ChangeTime = dest->LastWriteTime; // Missing: Should track actual metadata change time separately
    dest->IndexNumber = 0; // Missing: Should be unique file identifier (inode equivalent)
    dest->HardLinks = 0; // Missing: Should track actual hard link count from filesystem
    dest->EaSize = 0; // Missing: Should reflect actual Extended Attributes size if EA support is added
}

NTSTATUS HybridFileSystem::GetFileInfoByName(PWSTR FileName, FileInfo *FileInfo)
{
    std::wstring virtual_path(FileName);
    Logger::debug(LogCategory::FILESYSTEM, "GetFileInfoByName() called for: '{}'", StringUtils::wideToUtf8(virtual_path));

    // Get cache entry (same logic as GetSecurityByName)
    CacheEntry *entry = getCacheEntry(virtual_path);
    if (!entry)
    {
        Logger::warn(LogCategory::FILESYSTEM, "GetFileInfoByName() - entry NOT FOUND for: '{}'", StringUtils::wideToUtf8(virtual_path));
        return STATUS_OBJECT_NAME_NOT_FOUND;
    }

    copyFileInfo(entry, FileInfo);

    Logger::debug(LogCategory::FILESYSTEM, "GetFileInfoByName() - File: {}, Size: {}, Attributes: 0x{:x}",
                 StringUtils::wideToUtf8(virtual_path), entry->file_size, entry->file_attributes);
    Logger::debug(LogCategory::FILESYSTEM, "GetFileInfoByName() completed successfully");
    return STATUS_SUCCESS;
}

NTSTATUS HybridFileSystem::Open(PWSTR FileName, UINT32 CreateOptions, UINT32 GrantedAccess, PVOID *PFileNode, PVOID *PFileDesc, OpenFileInfo *OpenFileInfo)
{
    std::wstring virtual_path(FileName);
    Logger::debug(LogCategory::FILESYSTEM, "Open() called for: '{}', CreateOptions: 0x{:x}", StringUtils::wideToUtf8(virtual_path), CreateOptions);

    *PFileNode = nullptr;
    *PFileDesc = nullptr;

    auto start_time = std::chrono::high_resolution_clock::now();

    // Record activity and filesystem operation
    auto now = std::chrono::steady_clock::now();
    last_activity.store(now, std::memory_order_relaxed);
    GlobalMetrics::instance().recordFilesystemOperation("open");

    // Get or create cache entry
    CacheEntry *entry = getCacheEntry(virtual_path);
    if (!entry)
    {
        Logger::warn(LogCategory::FILESYSTEM, "Open() - entry not found for: '{}'", StringUtils::wideToUtf8(virtual_path));
        return STATUS_OBJECT_NAME_NOT_FOUND;
    }
    Logger::debug(LogCategory::FILESYSTEM, "Open() - entry found");

    // Validate CreateOptions compatibility with file type
    bool is_directory = (entry->file_attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
    bool wants_directory = (CreateOptions & FILE_DIRECTORY_FILE) != 0;
    bool wants_non_directory = (CreateOptions & FILE_NON_DIRECTORY_FILE) != 0;

    // Special case: Windows allows FILE_DIRECTORY_FILE on files for QueryDirectory operations
    // This is used by Explorer to query a file as if it were a directory entry
    if (wants_directory && !is_directory)
    {
        Logger::debug(LogCategory::FILESYSTEM, "Open() - FILE_DIRECTORY_FILE requested for file: '{}' - allowing for QueryDirectory", StringUtils::wideToUtf8(virtual_path));
        // Allow this for the special Windows QueryDirectory behavior - don't return error
    }

    if (wants_non_directory && is_directory)
    {
        Logger::error(LogCategory::FILESYSTEM, "Open() - FILE_NON_DIRECTORY_FILE requested for directory: '{}'", StringUtils::wideToUtf8(virtual_path));
        return STATUS_FILE_IS_A_DIRECTORY;
    }

    // Ensure file is available locally
    Logger::debug(LogCategory::FILESYSTEM, "Open() - ensuring file available");
    NTSTATUS result = ensureFileAvailable(entry);
    if (!NT_SUCCESS(result))
    {
        Logger::warn(LogCategory::FILESYSTEM, "Open() - ensureFileAvailable failed: 0x{:x}", result);
        // For async downloads, STATUS_PENDING is returned
        // WinFsp will retry the operation when the download completes
        return result;
    }
    Logger::debug(LogCategory::FILESYSTEM, "Open() - file available");

    // Create file descriptor
    Logger::debug(LogCategory::FILESYSTEM, "Open() - creating file descriptor");
    auto *file_desc = new FileDescriptor();

    DWORD create_flags = FILE_FLAG_BACKUP_SEMANTICS;
    if (CreateOptions & FILE_DELETE_ON_CLOSE)
    {
        create_flags |= FILE_FLAG_DELETE_ON_CLOSE;
    }

    // Handle directories separately from files
    if (entry->file_attributes & FILE_ATTRIBUTE_DIRECTORY)
    {
        Logger::debug(LogCategory::FILESYSTEM, "Open() - handling directory");
        // For directories, we don't need a real file handle
        file_desc->handle = INVALID_HANDLE_VALUE;
    }
    else
    {
        Logger::debug(LogCategory::FILESYSTEM, "Open() - handling file");
        // Check if file is in memory cache first
        if (entry->state == FileState::CACHED && entry->local_path.empty())
        {
            // File is in memory cache - use flag to avoid mutex lock
            Logger::debug(LogCategory::FILESYSTEM, "Open() - checking is_in_memory_cache flag for: {}", StringUtils::wideToUtf8(entry->virtual_path));
            if (entry->is_in_memory_cache.load())
            {
                // Serve ALL files directly from memory cache - no temp files needed!
                // As a filesystem driver, we handle Read() callbacks directly
                file_desc->handle = INVALID_HANDLE_VALUE;

                // Get direct pointer to cached content for fast reads - only ONE mutex lock!
                // This also increments memory_ref_count for eviction protection
                file_desc->cached_content = memory_cache.getMemoryCachedFilePtr(entry);

                Logger::debug(LogCategory::CACHE, "Open() - serving from memory cache (no handle) for: {}, cached_ptr: {}",
                            StringUtils::wideToUtf8(entry->virtual_path), (void*)file_desc->cached_content);
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
        Logger::debug(LogCategory::FILESYSTEM, "Open() - checking handle validity (using is_in_memory_cache flag) for: {}", StringUtils::wideToUtf8(entry->virtual_path));
        if (file_desc->handle == INVALID_HANDLE_VALUE && !(entry->state == FileState::CACHED && entry->local_path.empty() &&
                                                           entry->is_in_memory_cache.load()))
        {
            DWORD lastError = GetLastError();
            std::wstring attempted_path = entry->local_path.empty() ? entry->network_path : entry->local_path;
            Logger::error(LogCategory::FILESYSTEM,
                "Open() - file handle creation failed for '{}', state: {}, attempted_path: '{}', Windows error: {} (0x{:x})",
                StringUtils::wideToUtf8(entry->virtual_path),
                static_cast<int>(entry->state),
                StringUtils::wideToUtf8(attempted_path),
                lastError, lastError);
            delete file_desc;
            return CeWinFileCache::WineCompat::GetLastErrorAsNtStatus();
        }
    }

    file_desc->entry = entry;

    Logger::debug(LogCategory::FILESYSTEM, "Open() - setting up file info");

    // Get file info - handle directories differently
    if (entry->file_attributes & FILE_ATTRIBUTE_DIRECTORY)
    {
        Logger::debug(LogCategory::FILESYSTEM, "Open() - using cached directory info");
        // For directories, use cached metadata from DirectoryNode
        copyFileInfo(entry, &OpenFileInfo->FileInfo);
        // Ensure directories have size 0
        OpenFileInfo->FileInfo.FileSize = 0;
        OpenFileInfo->FileInfo.AllocationSize = ALLOCATION_UNIT;
        Logger::debug(LogCategory::FILESYSTEM, "Open() - Directory: {}, Size: {}, Attributes: 0x{:x}",
                     StringUtils::wideToUtf8(entry->virtual_path), OpenFileInfo->FileInfo.FileSize, OpenFileInfo->FileInfo.FileAttributes);
    }
    else
    {
        Logger::debug(LogCategory::FILESYSTEM, "Open() - using cached file metadata");
        // Use cached metadata from DirectoryNode instead of querying handle
        // This ensures consistent file information for virtual filesystem files
        copyFileInfo(entry, &OpenFileInfo->FileInfo);
        Logger::debug(LogCategory::FILESYSTEM, "Open() - File: {}, Size: {}, Attributes: 0x{:x}",
                     StringUtils::wideToUtf8(entry->virtual_path), OpenFileInfo->FileInfo.FileSize, OpenFileInfo->FileInfo.FileAttributes);

        // Special logging for castguard.h to debug QueryDirectory issue
        if (entry->virtual_path.find(L"castguard.h") != std::wstring::npos)
        {
            Logger::error(LogCategory::FILESYSTEM, "CASTGUARD DEBUG - Open() File: {}, Attributes: 0x{:x}, IsDirectory: {}, node->file_attributes: 0x{:x}",
                         StringUtils::wideToUtf8(entry->virtual_path), OpenFileInfo->FileInfo.FileAttributes,
                         (OpenFileInfo->FileInfo.FileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? "YES" : "NO",
                         entry->file_attributes);
        }

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
            Logger::debug(LogCategory::FILESYSTEM, "Open() - checking is_in_memory_cache flag (access tracker) for: {}",
                          StringUtils::wideToUtf8(entry->virtual_path));
            is_memory_cached = entry->is_in_memory_cache.load();
        }
        else
        {
            Logger::debug(LogCategory::FILESYSTEM, "Open() - skipping memory cache check for directory: {}",
                          StringUtils::wideToUtf8(entry->virtual_path));
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

    Logger::debug(LogCategory::FILESYSTEM, "Open() completed successfully");
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

    std::string path_str = file_desc->entry ? StringUtils::wideToUtf8(file_desc->entry->virtual_path) : "unknown";
    Logger::debug(LogCategory::FILESYSTEM, "Read() called for: '{}', Offset: {}, Length: {}", path_str, Offset, Length);

    // Try to serve from memory cache first for maximum performance - using cached pointer!
    if (file_desc->cached_content != nullptr)
    {
        // File is cached in memory - serve directly from cached pointer (NO mutex lock!)
        const auto &content = *file_desc->cached_content;

        // Validate read parameters
        if (Offset >= content.size())
        {
            *PBytesTransferred = 0;
            return STATUS_END_OF_FILE;
        }

        // Calculate actual bytes to transfer
        ULONG bytes_available = static_cast<ULONG>(content.size() - Offset);
        ULONG bytes_to_read = std::min(Length, bytes_available);

        // Copy data directly from memory - blazing fast memcpy with no locks!
        memcpy(Buffer, content.data() + Offset, bytes_to_read);
        *PBytesTransferred = bytes_to_read;

        // OPTIMIZATION: Only update activity/access time every ~100 reads to reduce overhead
        if (file_desc->entry)
        {
            file_desc->entry->access_count++;

            // Update timestamps only every 128 reads (1% overhead instead of 100%)
            // Use fast bit mask check instead of modulo
            if ((file_desc->entry->access_count & 0x7F) == 0)
            {
                auto now = std::chrono::steady_clock::now();
                file_desc->entry->last_used = now;
                last_activity.store(now, std::memory_order_relaxed);
            }
        }

        // Record filesystem operation (metrics) - keep for monitoring
        GlobalMetrics::instance().recordFilesystemOperation("read");

        return STATUS_SUCCESS;
    }

    // Record activity for non-cached reads (less frequent, more important to track)
    recordActivity();

    // Record filesystem operation
    GlobalMetrics::instance().recordFilesystemOperation("read");

    // Fallback to file handle read (for network-only files or cache miss)
    OVERLAPPED overlapped = {};
    overlapped.Offset = static_cast<DWORD>(Offset);
    overlapped.OffsetHigh = static_cast<DWORD>(Offset >> 32);

    Logger::debug(LogCategory::FILESYSTEM, "Read() - reading from file handle for: '{}'", path_str);
    if (!ReadFile(file_desc->handle, Buffer, Length, PBytesTransferred, &overlapped))
    {
        DWORD error = GetLastError();
        Logger::error(LogCategory::FILESYSTEM, "Read() - ReadFile failed for: '{}', Error: {}", path_str, error);
        return CeWinFileCache::WineCompat::GetLastErrorAsNtStatus();
    }

    Logger::debug(LogCategory::FILESYSTEM, "Read() - success for: '{}', BytesTransferred: {}", path_str, *PBytesTransferred);

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
        Logger::debug(LogCategory::FILESYSTEM, "Read() - checking is_in_memory_cache flag (access tracker) for: {}",
                      StringUtils::wideToUtf8(file_desc->entry->virtual_path));
        bool is_memory_cached = file_desc->entry->is_in_memory_cache.load();

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

    copyFileInfo(entry, FileInfo);

    // Log key values for Properties dialog debugging
    Logger::debug(LogCategory::FILESYSTEM, "GetFileInfo() called - File: {}, Size: {}, Attributes: 0x{:x}, CreationTime: {}",
                 StringUtils::wideToUtf8(entry->virtual_path), entry->file_size, entry->file_attributes, FileInfo->CreationTime);

    return STATUS_SUCCESS;
}

NTSTATUS HybridFileSystem::SetBasicInfo(PVOID FileNode,
                                        PVOID FileDesc,
                                        UINT32 FileAttributes,
                                        UINT64 CreationTime,
                                        UINT64 LastAccessTime,
                                        UINT64 LastWriteTime,
                                        UINT64 ChangeTime,
                                        FileInfo *FileInfo)
{
    Logger::info(LogCategory::FILESYSTEM, "SetBasicInfo() called - FileAttributes: 0x{:x}", FileAttributes);
    auto *file_desc = static_cast<FileDescriptor *>(FileDesc);
    CacheEntry *entry = file_desc->entry;

    // Update cached metadata with new values
    if (FileAttributes != 0xFFFFFFFF) // 0xFFFFFFFF means don't change
    {
        entry->file_attributes = FileAttributes;
        Logger::debug(LogCategory::FILESYSTEM, "SetBasicInfo() - Updated file attributes to: 0x{:x}", FileAttributes);
    }

    if (CreationTime != 0) // 0 means don't change
    {
        entry->creation_time = std::bit_cast<FILETIME>(CreationTime);
        Logger::debug(LogCategory::FILESYSTEM, "SetBasicInfo() - Updated creation time");
    }

    if (LastAccessTime != 0)
    {
        entry->last_access_time = std::bit_cast<FILETIME>(LastAccessTime);
        Logger::debug(LogCategory::FILESYSTEM, "SetBasicInfo() - Updated last access time");
    }

    if (LastWriteTime != 0)
    {
        entry->last_write_time = std::bit_cast<FILETIME>(LastWriteTime);
        Logger::debug(LogCategory::FILESYSTEM, "SetBasicInfo() - Updated last write time");
    }

    // Return current file info after update
    if (FileInfo != nullptr)
    {
        copyFileInfo(entry, FileInfo);
    }

    Logger::info(LogCategory::FILESYSTEM, "SetBasicInfo() completed successfully");
    return STATUS_SUCCESS;
}

NTSTATUS HybridFileSystem::GetSecurity(PVOID FileNode, PVOID FileDesc, PSECURITY_DESCRIPTOR SecurityDescriptor, SIZE_T *PSecurityDescriptorSize)
{
    Logger::debug(LogCategory::FILESYSTEM, "GetSecurity() called for opened file");

    // Use the same security descriptor logic as GetSecurityByName
    // This ensures consistency between the two methods

    const char *sddl_string =
    "O:S-1-5-21-663732323-46111922-2075403870-1001G:S-1-5-21-663732323-46111922-2075403870-1001D:(A;OICI;FA;;;SY)(A;"
    "OICI;FA;;;BA)(A;OICI;FA;;;S-1-5-21-663732323-46111922-2075403870-1001)";

    PSECURITY_DESCRIPTOR temp_descriptor = nullptr;
    if (!ConvertStringSecurityDescriptorToSecurityDescriptorA(sddl_string, SDDL_REVISION_1, &temp_descriptor, nullptr))
    {
        Logger::debug(LogCategory::FILESYSTEM, "GetSecurity() - Failed to convert SDDL string, error: {}", GetLastError());
        return CeWinFileCache::WineCompat::NtStatusFromWin32(GetLastError());
    }

    DWORD descriptor_length = GetSecurityDescriptorLength(temp_descriptor);
    Logger::debug(LogCategory::FILESYSTEM, "GetSecurity() - Security descriptor size: {}", descriptor_length);

    if (SecurityDescriptor == nullptr)
    {
        // Caller is querying the required buffer size
        *PSecurityDescriptorSize = descriptor_length;
        LocalFree(temp_descriptor);
        Logger::debug(LogCategory::FILESYSTEM, "GetSecurity() - Returning required size: {}", descriptor_length);
        return STATUS_SUCCESS;
    }

    if (*PSecurityDescriptorSize < descriptor_length)
    {
        // Buffer too small
        *PSecurityDescriptorSize = descriptor_length;
        LocalFree(temp_descriptor);
        Logger::debug(LogCategory::FILESYSTEM, "GetSecurity() - Buffer too small");
        return STATUS_BUFFER_TOO_SMALL;
    }

    // Copy security descriptor to provided buffer
    memcpy(SecurityDescriptor, temp_descriptor, descriptor_length);
    *PSecurityDescriptorSize = descriptor_length;
    LocalFree(temp_descriptor);

    Logger::debug(LogCategory::FILESYSTEM, "GetSecurity() completed successfully");
    return STATUS_SUCCESS;
}

NTSTATUS HybridFileSystem::SetSecurity(PVOID FileNode, PVOID FileDesc, SECURITY_INFORMATION SecurityInformation, PSECURITY_DESCRIPTOR ModificationDescriptor)
{
    Logger::debug(LogCategory::FILESYSTEM, "SetSecurity() called - SecurityInformation: 0x{:x}", SecurityInformation);

    // For a read-only caching filesystem, we don't actually modify security descriptors
    // But we should accept the call to avoid blocking the Properties dialog
    // Just log what's being requested and return success

    if (SecurityInformation & OWNER_SECURITY_INFORMATION)
    {
        Logger::debug(LogCategory::FILESYSTEM, "SetSecurity() - Owner security information requested");
    }
    if (SecurityInformation & GROUP_SECURITY_INFORMATION)
    {
        Logger::debug(LogCategory::FILESYSTEM, "SetSecurity() - Group security information requested");
    }
    if (SecurityInformation & DACL_SECURITY_INFORMATION)
    {
        Logger::debug(LogCategory::FILESYSTEM, "SetSecurity() - DACL security information requested");
    }
    if (SecurityInformation & SACL_SECURITY_INFORMATION)
    {
        Logger::debug(LogCategory::FILESYSTEM, "SetSecurity() - SACL security information requested");
    }

    // For a caching filesystem, we accept the modification but don't actually change anything
    // This allows the Properties dialog to work without errors
    Logger::debug(LogCategory::FILESYSTEM, "SetSecurity() completed successfully (no-op for caching filesystem)");
    return STATUS_SUCCESS;
}

NTSTATUS HybridFileSystem::ReadDirectory(PVOID FileNode, PVOID FileDesc, PWSTR Pattern, PWSTR Marker, PVOID Buffer, ULONG Length, PULONG PBytesTransferred)
{
    Logger::debug(LogCategory::FILESYSTEM, "ReadDirectory() called - Pattern: '{}', Length: {}, Marker: {}",
                 Pattern ? StringUtils::wideToUtf8(Pattern) : "null",
                 Length,
                 Marker ? StringUtils::wideToUtf8(Marker) : "null");

    // Record activity for idle detection
    recordActivity();

    auto *file_desc = static_cast<FileDescriptor *>(FileDesc);

    // Handle special case: ReadDirectory called on a file path
    // Windows filesystem allows QueryDirectory on files to get their directory entry
    if (!file_desc->entry || !(file_desc->entry->file_attributes & FILE_ATTRIBUTE_DIRECTORY))
    {
        Logger::debug(LogCategory::FILESYSTEM, "ReadDirectory() called on file: '{}' - returning file as single directory entry",
                     file_desc->entry ? StringUtils::wideToUtf8(file_desc->entry->virtual_path) : "null");

        if (!file_desc->entry)
        {
            *PBytesTransferred = 0;
            return STATUS_OBJECT_NAME_NOT_FOUND;
        }

        // Return this file as a single directory entry (like real Windows filesystem)
        return handleFileAsDirectoryEntry(file_desc->entry, Pattern, Marker, Buffer, Length, PBytesTransferred);
    }

    // Get directory path
    std::wstring dir_path = file_desc->entry->virtual_path;
    std::wstring normalized_path = normalizePath(dir_path);

    Logger::debug(LogCategory::FILESYSTEM, "ReadDirectory() - enumerating directory: '{}'", StringUtils::wideToUtf8(normalized_path));

    // Get directory contents
    std::vector<DirectoryNode *> contents = directory_cache.getDirectoryContents(dir_path);
    Logger::debug(LogCategory::FILESYSTEM, "ReadDirectory() - directory '{}' has {} entries", StringUtils::wideToUtf8(dir_path), contents.size());

    // Apply pattern filtering if specified
    if (Pattern && wcslen(Pattern) > 0)
    {
        contents = filterDirectoryContents(contents, Pattern, "ReadDirectory");
    }

    if (contents.empty())
    {
        Logger::debug(LogCategory::FILESYSTEM, "ReadDirectory() - no entries found");
        *PBytesTransferred = 0;
        return STATUS_NO_MORE_FILES;
    }

    // Find starting index based on marker using binary search for O(log n) performance
    size_t start_index = 0;
    if (Marker && wcslen(Marker) > 0)
    {
        std::string marker_str = StringUtils::wideToUtf8(Marker);

        // todo: should use hashmap that should already be cached (its not)

        // Use binary search to find marker position (assumes contents are sorted by name)
        auto it = std::lower_bound(contents.begin(), contents.end(), Marker,
                                   [](const DirectoryNode *node, const std::wstring &marker)
                                   {
                                       return node->name < marker;
                                   });

        if (it != contents.end() && (*it)->name == Marker)
        {
            start_index = std::distance(contents.begin(), it) + 1; // Next entry after marker
            Logger::debug(LogCategory::FILESYSTEM, "ReadDirectory() - found marker '{}' at index {}", marker_str, start_index - 1);
        }
        else
        {
            Logger::debug(LogCategory::FILESYSTEM, "ReadDirectory() - marker '{}' not found, starting from beginning", marker_str);
        }
    }
    else
    {
        Logger::debug(LogCategory::FILESYSTEM, "ReadDirectory() - no marker, starting from beginning");
    }

    // Fill buffer with directory entries
    PUCHAR buffer_ptr = (PUCHAR)Buffer;
    size_t bytes_used = 0;
    size_t entries_returned = 0;

    for (size_t i = start_index; i < contents.size(); i++)
    {
        DirectoryNode *node = contents[i];

        // Calculate size needed for this entry
        size_t name_chars = node->name.length();
        size_t name_bytes = name_chars * sizeof(WCHAR);
        // Size = base structure + filename bytes (matching WinFsp memfs example)
        size_t dir_info_size = sizeof(FSP_FSCTL_DIR_INFO) + name_bytes;

        // Check if entry size exceeds UINT16 maximum
        if (dir_info_size > UINT16_MAX)
        {
            Logger::error(LogCategory::FILESYSTEM, "ReadDirectory() - entry '{}' size {} exceeds UINT16_MAX, skipping",
                         StringUtils::wideToUtf8(node->name), dir_info_size);
            continue;
        }

        // Align to 8-byte boundary for buffer positioning (but Size field stays unaligned)
        size_t entry_size = (dir_info_size + 7) & ~7;

        // Removed verbose per-entry logging for performance

        // Check if entry fits in remaining buffer
        if (bytes_used + entry_size > Length)
        {
            Logger::debug(LogCategory::FILESYSTEM, "ReadDirectory() - buffer full, stopping at index {}", i);
            break;
        }

        // Fill directory info
        FSP_FSCTL_DIR_INFO *dir_info = (FSP_FSCTL_DIR_INFO *)(buffer_ptr + bytes_used);
        dir_info->Size = (UINT16)dir_info_size;  // Size field is unaligned

        // Set file attributes from cached network data
        dir_info->FileInfo.FileAttributes = node->file_attributes;

        // Debug logging for file attributes
        Logger::debug(LogCategory::FILESYSTEM, "ReadDirectory() - entry '{}': attributes=0x{:x}, isDirectory={}, size={}",
                     StringUtils::wideToUtf8(node->name), node->file_attributes,
                     node->isDirectory(), node->file_size);
        dir_info->FileInfo.ReparseTag = 0;
        dir_info->FileInfo.FileSize = node->isDirectory() ? 0 : node->file_size;
        dir_info->FileInfo.AllocationSize =
        node->isDirectory() ? ALLOCATION_UNIT : (dir_info->FileInfo.FileSize + ALLOCATION_UNIT - 1) / ALLOCATION_UNIT * ALLOCATION_UNIT;

        // Set timestamps
        UINT64 creation_ts = std::bit_cast<UINT64>(node->creation_time);
        UINT64 access_ts = std::bit_cast<UINT64>(node->last_access_time);
        UINT64 write_ts = std::bit_cast<UINT64>(node->last_write_time);

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

    *PBytesTransferred = static_cast<ULONG>(bytes_used);

    Logger::debug(LogCategory::FILESYSTEM, "ReadDirectory() - returning {} entries, {} bytes total", entries_returned, bytes_used);

    if (entries_returned == 0)
    {
        Logger::debug(LogCategory::FILESYSTEM, "ReadDirectory() - no entries returned, returning SUCCESS with 0 bytes");
        return STATUS_SUCCESS; // Real Windows filesystem returns SUCCESS, not NO_MORE_FILES
    }
    else if (start_index + entries_returned >= contents.size())
    {
        Logger::debug(LogCategory::FILESYSTEM, "ReadDirectory() - enumeration complete");
        return STATUS_SUCCESS; // All entries have been returned
    }
    else
    {
        Logger::debug(LogCategory::FILESYSTEM, "ReadDirectory() - more entries available");
        return STATUS_SUCCESS; // More entries available
    }
}

NTSTATUS HybridFileSystem::ReadDirectoryEntry(PVOID FileNode, PVOID FileDesc, PWSTR Pattern, PWSTR Marker, PVOID *PContext, DirInfo *DirInfo)
{
    Logger::info(LogCategory::FILESYSTEM, "ReadDirectoryEntry() called - Pattern: '{}', Context: {}, Marker: {}",
                  Pattern ? StringUtils::wideToUtf8(Pattern) : "null",
                  *PContext ? "exists" : "null",
                  Marker ? StringUtils::wideToUtf8(Marker) : "null");
    auto *file_desc = static_cast<FileDescriptor *>(FileDesc);

    if (!file_desc->entry)
    {
        Logger::debug(LogCategory::FILESYSTEM, "ReadDirectoryEntry() - no entry");
        return STATUS_INVALID_PARAMETER;
    }

    Logger::debug(LogCategory::FILESYSTEM, "ReadDirectoryEntry() - path: '{}'", StringUtils::wideToUtf8(file_desc->entry->virtual_path));

    if (*PContext == nullptr)
    {
        Logger::debug(LogCategory::FILESYSTEM, "ReadDirectoryEntry() - starting enumeration");

        // Handle root directory specially - return compiler directories
        // Normalize path for comparison (both \ and / should be treated as root)
        std::wstring normalized_path = normalizePath(file_desc->entry->virtual_path);
        if (normalized_path == L"/")
        {
            Logger::debug(LogCategory::FILESYSTEM, "ReadDirectoryEntry() - enumerating root directory");

            // Use DirectoryCache to get all contents in root directory
            std::vector<DirectoryNode *> root_contents = directory_cache.getDirectoryContents(L"/");

            if (root_contents.empty())
            {
                Logger::debug(LogCategory::FILESYSTEM, "ReadDirectoryEntry() - no contents from DirectoryCache for root");
                return STATUS_NO_MORE_FILES;
            }

            // Apply pattern filtering if specified
            if (Pattern && wcslen(Pattern) > 0)
            {
                root_contents = filterDirectoryContents(root_contents, Pattern, "ReadDirectoryEntry-root");
                if (root_contents.empty())
                {
                    Logger::debug(LogCategory::FILESYSTEM, "ReadDirectoryEntry() - no root entries match pattern");
                    return STATUS_NO_MORE_FILES;
                }
            }

            Logger::debug(LogCategory::FILESYSTEM, "ReadDirectoryEntry() - found {} entries in root directory", root_contents.size());
            for (size_t i = 0; i < root_contents.size(); i++)
            {
                Logger::debug(LogCategory::FILESYSTEM, "ReadDirectoryEntry() - root entry[{}]: '{}'", i,
                              StringUtils::wideToUtf8(root_contents[i]->name));
            }

            // Store the contents vector as context for enumeration
            auto *context = new std::vector<DirectoryNode *>(std::move(root_contents));
            *PContext = context;

            // Return first entry
            auto *first_entry = (*context)[0];
            fillDirInfo(DirInfo, first_entry);

            Logger::debug(LogCategory::FILESYSTEM, "ReadDirectoryEntry() - returning first root entry: '{}' (name: '{}')",
                          StringUtils::wideToUtf8(first_entry->full_virtual_path), StringUtils::wideToUtf8(first_entry->name));
            return STATUS_SUCCESS;
        }
        else
        {
            Logger::debug(LogCategory::FILESYSTEM, "ReadDirectoryEntry() - enumerating non-root directory");

            // For non-root directories, use directory cache
            std::vector<DirectoryNode *> contents = directory_cache.getDirectoryContents(file_desc->entry->virtual_path);

            if (contents.empty())
            {
                Logger::debug(LogCategory::FILESYSTEM, "ReadDirectoryEntry() - no contents from directory cache");
                return STATUS_NO_MORE_FILES;
            }

            // Apply pattern filtering if specified
            if (Pattern && wcslen(Pattern) > 0)
            {
                contents = filterDirectoryContents(contents, Pattern, "ReadDirectoryEntry-nonroot");
                if (contents.empty())
                {
                    Logger::debug(LogCategory::FILESYSTEM, "ReadDirectoryEntry() - no non-root entries match pattern");
                    return STATUS_NO_MORE_FILES;
                }
            }

            // Store the contents vector as context (simplified - in production should be more robust)
            auto *context_data = new std::vector<DirectoryNode *>(std::move(contents));
            *PContext = context_data;

            // Return first entry
            auto *first_entry = (*context_data)[0];
            fillDirInfo(DirInfo, first_entry);

            Logger::debug(LogCategory::FILESYSTEM, "ReadDirectoryEntry() - returning first non-root entry: '{}'",
                          StringUtils::wideToUtf8(first_entry->full_virtual_path));
            return STATUS_SUCCESS;
        }
    }
    else
    {
        Logger::debug(LogCategory::FILESYSTEM, "ReadDirectoryEntry() - continuing enumeration");

        // Continue enumeration using stored context
        auto *context_data = static_cast<std::vector<DirectoryNode *> *>(*PContext);

        Logger::debug(LogCategory::FILESYSTEM, "ReadDirectoryEntry() - context has {} entries", context_data->size());
        for (size_t i = 0; i < context_data->size() && i < 5; i++) // Show first 5 entries
        {
            Logger::debug(LogCategory::FILESYSTEM, "ReadDirectoryEntry() - context entry[{}]: '{}'", i,
                          StringUtils::wideToUtf8((*context_data)[i]->name));
        }

        // Find current position based on marker
        size_t current_index = 0;
        bool found_marker = false;

        if (Marker && wcslen(Marker) > 0)
        {
            std::string marker_str = StringUtils::wideToUtf8(Marker);
            Logger::debug(LogCategory::FILESYSTEM, "ReadDirectoryEntry() - searching for marker: '{}'", marker_str);

            // Find marker in the list - marker is the name of the last returned entry
            for (size_t i = 0; i < context_data->size(); i++)
            {
                if ((*context_data)[i]->name == Marker)
                {
                    current_index = i + 1; // Next entry after the marker
                    found_marker = true;
                    Logger::debug(LogCategory::FILESYSTEM, "ReadDirectoryEntry() - found marker at index {}, next index: {}", i, current_index);
                    break;
                }
            }

            if (!found_marker)
            {
                Logger::debug(LogCategory::FILESYSTEM, "ReadDirectoryEntry() - marker '{}' not found, starting from index 1", marker_str);
                current_index = 1; // Fallback to second entry
            }
        }
        else
        {
            Logger::debug(LogCategory::FILESYSTEM, "ReadDirectoryEntry() - no marker, continuing from index 1");
            current_index = 1; // Next after first
        }

        if (current_index >= context_data->size())
        {
            // End of enumeration
            Logger::debug(LogCategory::FILESYSTEM, "ReadDirectoryEntry() - end of enumeration at index {} (total: {})", current_index,
                          context_data->size());
            delete context_data;
            *PContext = nullptr;
            return STATUS_NO_MORE_FILES;
        }

        // Return next entry
        auto *next_entry = (*context_data)[current_index];
        fillDirInfo(DirInfo, next_entry);

        Logger::debug(LogCategory::FILESYSTEM, "ReadDirectoryEntry() - returning entry: '{}' (full path: '{}', index {})",
                      StringUtils::wideToUtf8(next_entry->name), StringUtils::wideToUtf8(next_entry->full_virtual_path),
                      current_index);
        return STATUS_SUCCESS;
    }
}

// NTSTATUS HybridFileSystem::GetEa(PVOID FileNode, PVOID FileDesc, PFILE_FULL_EA_INFORMATION Ea, ULONG EaLength, PULONG PBytesTransferred)
// {
//     // todo: Implement extended attributes support
//     // Extended attributes (EA) could be:
//     // 1. Stored in CacheEntry alongside other metadata
//     // 2. Passed through to network source files using GetFileInformationByHandleEx
//     // 3. Cached separately with TTL expiration
//     // For now, return not supported to avoid breaking applications
//     Logger::debug(LogCategory::FILESYSTEM, "GetEa() called - not implemented, returning STATUS_NOT_SUPPORTED");
//     return STATUS_NOT_SUPPORTED;
// }

// NTSTATUS HybridFileSystem::SetEa(PVOID FileNode, PVOID FileDesc, PFILE_FULL_EA_INFORMATION Ea, ULONG EaLength, FileInfo *FileInfo)
// {
//     // todo: Implement extended attributes support
//     // For a caching filesystem, SetEa options:
//     // 1. Store EA in cache only (local to virtual filesystem)
//     // 2. Forward to network file if supported by remote filesystem
//     // 3. Hybrid approach: cache locally and sync to network periodically
//     // Need to consider: EA size limits, persistence, and synchronization
//     Logger::debug(LogCategory::FILESYSTEM, "SetEa() called - not implemented, returning STATUS_NOT_SUPPORTED");
//     return STATUS_NOT_SUPPORTED;
// }

// Private methods

CacheEntry *HybridFileSystem::getCacheEntry(const std::wstring &virtual_path)
{
    Logger::debug(LogCategory::CACHE, "getCacheEntry() called for: '{}'", StringUtils::wideToUtf8(virtual_path));

    auto normalized = this->normalizePath(virtual_path);

    // Fast path: check if entry exists in cache
    {
        std::lock_guard<std::mutex> lock(cache_mutex);
        auto it = cache_entries.find(normalized);
        if (it != cache_entries.end())
        {
            Logger::debug(LogCategory::CACHE, "getCacheEntry() - found existing entry for: '{}'", StringUtils::wideToUtf8(virtual_path));
            Logger::debug(LogCategory::CACHE, "getCacheEntry() - entry state: {}, attributes: 0x{:x}",
                          static_cast<int>(it->second->state), it->second->file_attributes);
            return it->second.get();
        }
    }
    // Lock released - allows concurrent cache access during directory tree lookup

    // Slow path: lookup in directory tree (no lock held)
    Logger::debug(LogCategory::CACHE, "getCacheEntry() - checking DirectoryCache for: '{}'", StringUtils::wideToUtf8(virtual_path));
    DirectoryNode *node = directory_cache.findNode(normalized);
    if (!node)
    {
        return nullptr;
    }

    Logger::debug(LogCategory::CACHE, "getCacheEntry() - found node in DirectoryCache, creating dynamic entry");

    // Reacquire lock to create and insert cache entry
    std::lock_guard<std::mutex> lock(cache_mutex);

    // Double-check: another thread might have created the entry while we were unlocked
    auto it = cache_entries.find(normalized);
    if (it != cache_entries.end())
    {
        return it->second.get();
    }

    // Create entry and add to cache
    return createDynamicCacheEntry(node);
}

CacheEntry *HybridFileSystem::createDynamicCacheEntry(DirectoryNode *node)
{
    Logger::debug(LogCategory::FILESYSTEM, "createDynamicCacheEntry() called for: '{}', attributes: 0x{:x}",
                 StringUtils::wideToUtf8(node->full_virtual_path), node->file_attributes);

    assert(!cache_entries.contains(node->full_virtual_path));

    // Create cache entry from DirectoryNode
    auto entry = std::make_unique<CacheEntry>();
    entry->virtual_path = node->full_virtual_path;
    entry->network_path = node->network_path;
    entry->file_attributes = node->file_attributes; // Use actual file attributes from DirectoryNode
    entry->file_size = node->file_size;
    entry->creation_time = node->creation_time;
    entry->last_access_time = node->last_access_time;
    entry->last_write_time = node->last_write_time;
    entry->SecDesc = node->SecDesc;

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

    Logger::debug(LogCategory::FILESYSTEM, "createDynamicCacheEntry() - created entry for: '{}' -> '{}', policy: {}",
                 StringUtils::wideToUtf8(entry->virtual_path), StringUtils::wideToUtf8(entry->network_path),
                 static_cast<int>(entry->policy));

    // Store in cache_entries for future fast access
    if (!config.global.case_sensitive)
    {
        std::wstring lowered = entry->virtual_path;
        StringUtils::toLower(lowered);
        cache_entries[lowered] = std::move(entry);
        return cache_entries[lowered].get();
    }

    cache_entries[node->full_virtual_path] = std::move(entry);
    return cache_entries[node->full_virtual_path].get();
}

NTSTATUS HybridFileSystem::ensureFileAvailable(CacheEntry *entry)
{
    // Skip directories - they don't need to be cached
    if (entry->file_attributes & FILE_ATTRIBUTE_DIRECTORY)
    {
        return STATUS_SUCCESS;
    }

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

        NTSTATUS status = download_manager->queueDownload(
        entry->virtual_path, entry->network_path, entry, entry->policy,
        [this](NTSTATUS download_status, const std::wstring error, CacheEntry *entry)
        {
            if (download_status == STATUS_SUCCESS)
            {
                // Download completed successfully
                // The AsyncDownloadManager already updated the cache entry
                Logger::debug(LogCategory::NETWORK, "Download completed: {}", StringUtils::wideToUtf8(entry->virtual_path));
            }
            else if (download_status == STATUS_PENDING)
            {
                // Already downloading
                Logger::debug(LogCategory::NETWORK, "Already downloading: {}", StringUtils::wideToUtf8(entry->virtual_path));
            }
            else
            {
                // Download failed
                entry->state = FileState::NETWORK_ONLY;
                entry->local_path = entry->network_path;
                Logger::error(LogCategory::NETWORK, "Download failed for {}: {}",
                              StringUtils::wideToUtf8(entry->virtual_path), StringUtils::wideToUtf8(error));
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

std::vector<DirectoryNode *> HybridFileSystem::filterDirectoryContents(const std::vector<DirectoryNode *> &contents, PWSTR pattern, const char *context)
{
    std::wstring pattern_str = pattern;
    Logger::debug(LogCategory::FILESYSTEM, "{} - filtering with pattern: '{}'", context, StringUtils::wideToUtf8(pattern_str));

    std::vector<DirectoryNode *> filtered_contents;
    for (DirectoryNode *node : contents)
    {
        if (matchesPattern(node->name, pattern_str))
        {
            filtered_contents.push_back(node);
            Logger::debug(LogCategory::FILESYSTEM, "{} - '{}' matches pattern '{}'",
                          context, StringUtils::wideToUtf8(node->name), StringUtils::wideToUtf8(pattern_str));
        }
    }

    Logger::debug(LogCategory::FILESYSTEM, "{} - after pattern filtering: {} entries", context, filtered_contents.size());
    return filtered_contents;
}

CachePolicy HybridFileSystem::determineCachePolicy(const std::wstring &virtual_path)
{
    // Match against configured compiler paths using longest-prefix matching
    // This handles both simple paths like "/msvc-14.40/bin/cl.exe"
    // and multi-level paths like "/compilers/msvc/14.40.33807-14.40.33811.0/bin/cl.exe"
    if (virtual_path.empty() || virtual_path[0] != L'/')
    {
        return CachePolicy::NEVER_CACHE;
    }

    std::wstring path_without_leading_slash = virtual_path.substr(1);
    std::wstring best_match_compiler;
    size_t best_match_length = 0;

    // Find the longest matching compiler path
    for (const auto &[compiler_name, compiler_config] : config.compilers)
    {
        // Check if virtual path starts with this compiler path
        if (path_without_leading_slash.starts_with(compiler_name))
        {
            // Ensure it's a proper path boundary (next char is / or end of string)
            size_t compiler_name_len = compiler_name.length();
            if (compiler_name_len > best_match_length &&
                (path_without_leading_slash.length() == compiler_name_len ||
                 path_without_leading_slash[compiler_name_len] == L'/'))
            {
                best_match_compiler = compiler_name;
                best_match_length = compiler_name_len;
            }
        }
    }

    // No matching compiler found
    if (best_match_compiler.empty())
    {
        return CachePolicy::NEVER_CACHE;
    }

    const auto &compiler_config = config.compilers.at(best_match_compiler);

    // Get relative path within the compiler directory
    std::wstring relative_path;
    if (path_without_leading_slash.length() > best_match_length)
    {
        relative_path = path_without_leading_slash.substr(best_match_length + 1); // +1 to skip the /
    }

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

    // Create persistent cache file path using cache directory
    std::wstring cache_dir = config.global.cache_directory;

    // Ensure cache directory exists
    CreateDirectoryW(cache_dir.c_str(), nullptr);

    // Create simple hash from virtual path for filename
    std::hash<std::wstring> hasher;
    size_t path_hash = hasher(entry->virtual_path);
    std::wstring cache_filename = L"cache_" + std::to_wstring(path_hash) + L".tmp";
    std::wstring cache_file_path = cache_dir + L"\\" + cache_filename;

    // Check if cache file already exists and is valid
    WIN32_FILE_ATTRIBUTE_DATA file_attr;
    if (GetFileAttributesExW(cache_file_path.c_str(), GetFileExInfoStandard, &file_attr))
    {
        // File exists - check if size matches
        LARGE_INTEGER file_size;
        file_size.LowPart = file_attr.nFileSizeLow;
        file_size.HighPart = file_attr.nFileSizeHigh;

        if (static_cast<size_t>(file_size.QuadPart) == content.size())
        {
            Logger::debug(LogCategory::CACHE, "createTemporaryFileForMemoryCached() - reusing existing cache file: {}",
                         StringUtils::wideToUtf8(cache_file_path));
            return cache_file_path;
        }
    }

    // Write memory content to persistent cache file
    HANDLE cache_file = CreateFileW(cache_file_path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);

    if (cache_file == INVALID_HANDLE_VALUE)
    {
        Logger::warn(LogCategory::CACHE, "createTemporaryFileForMemoryCached() - failed to create cache file: {}",
                    StringUtils::wideToUtf8(cache_file_path));
        return L"";
    }

    DWORD bytes_written;
    BOOL write_result = WriteFile(cache_file, content.data(), static_cast<DWORD>(content.size()), &bytes_written, nullptr);

    CloseHandle(cache_file);

    if (!write_result || bytes_written != content.size())
    {
        DeleteFileW(cache_file_path.c_str()); // Clean up on failure
        return L"";
    }

    Logger::debug(LogCategory::CACHE, "createTemporaryFileForMemoryCached() - created cache file: {} ({} bytes)",
                 StringUtils::wideToUtf8(cache_file_path), content.size());

    return cache_file_path;
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

    if (!loaded_config.global.case_sensitive)
    {
        StringUtils::toLower(normalized);
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

    // Size includes the structure + filename (FileNameBuf already counted in sizeof)
    size_t entry_size = sizeof(FSP_FSCTL_DIR_INFO) + name_size_bytes;

    // Check for overflow before casting to UINT16
    if (entry_size > UINT16_MAX)
    {
        Logger::error(LogCategory::FILESYSTEM, "fillDirInfo() - entry '{}' size {} exceeds UINT16_MAX",
                     StringUtils::wideToUtf8(name), entry_size);
        entry_size = UINT16_MAX;
    }

    dir_info->Size = (UINT16)entry_size;

    Logger::info(LogCategory::FILESYSTEM, "fillDirInfo() - filling entry for: '{}', attributes: 0x{:x}, size: {}", StringUtils::wideToUtf8(name), node->file_attributes, dir_info->Size);

    // Set file attributes from cached network data
    dir_info->FileInfo.FileAttributes = node->file_attributes;

    // Debug logging for file attributes
    Logger::debug(LogCategory::FILESYSTEM, "fillDirInfo() - entry '{}': attributes=0x{:x}, isDirectory={}, file_size={}",
                 StringUtils::wideToUtf8(name), node->file_attributes,
                 node->isDirectory(), node->file_size);
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

    // Copy filename with null termination
    memcpy(dir_info->FileNameBuf, name.c_str(), name_size_bytes);
    WCHAR *filename_end = (WCHAR *)((char *)dir_info->FileNameBuf + name_size_bytes);
    *filename_end = L'\0';

    Logger::debug(LogCategory::FILESYSTEM, "fillDirInfo() - completed for: '{}', filename set in buffer", StringUtils::wideToUtf8(name));
}

NTSTATUS HybridFileSystem::evictIfNeeded()
{
    // todo: implement LRU eviction
    return STATUS_SUCCESS;
}

size_t HybridFileSystem::performMemoryEviction(size_t bytes_needed)
{
    Logger::info(LogCategory::CACHE, "=== Memory Eviction Starting ===");
    Logger::info(LogCategory::CACHE, "Target: evict {} bytes ({} MB)",
                 bytes_needed, bytes_needed / (1024 * 1024));

    std::lock_guard<std::mutex> lock(cache_mutex);

    // Log current cache state
    size_t current_size = memory_cache.getCacheSize();
    size_t cache_entry_count = 0;
    size_t always_cache_count = 0;
    size_t on_demand_count = 0;
    size_t active_readers = 0;

    for (const auto &[path, entry] : cache_entries)
    {
        if (entry->is_in_memory_cache.load())
        {
            cache_entry_count++;
            if (entry->policy == CachePolicy::ALWAYS_CACHE)
                always_cache_count++;
            else if (entry->policy == CachePolicy::ON_DEMAND)
                on_demand_count++;
            if (entry->memory_ref_count.load() > 0)
                active_readers++;
        }
    }

    Logger::info(LogCategory::CACHE, "Current cache: {} MB, {} files ({} ALWAYS_CACHE, {} ON_DEMAND, {} with active readers)",
                 current_size / (1024 * 1024), cache_entry_count, always_cache_count, on_demand_count, active_readers);

    // Collect candidates for eviction - only ON_DEMAND files
    // Sorted by last_used (oldest first) for LRU policy
    std::vector<std::pair<std::chrono::steady_clock::time_point, std::wstring>> candidates;

    for (const auto &[path, entry] : cache_entries)
    {
        // Skip NEVER_CACHE files (shouldn't be in memory cache anyway)
        if (entry->policy == CachePolicy::NEVER_CACHE)
        {
            continue;
        }

        // Never evict ALWAYS_CACHE files (critical compiler binaries)
        if (entry->policy == CachePolicy::ALWAYS_CACHE)
        {
            continue;
        }

        // Never evict files currently being downloaded
        if (entry->is_downloading.load())
        {
            continue;
        }

        // Never evict files with active readers (reference counting protection)
        if (entry->memory_ref_count.load() > 0)
        {
            continue;
        }

        // Only evict files actually in memory cache
        if (!entry->is_in_memory_cache.load())
        {
            continue;
        }

        // This is an ON_DEMAND file safe to evict
        candidates.emplace_back(entry->last_used, path);
    }

    // Sort by last access time (oldest first) - LRU policy
    std::sort(candidates.begin(), candidates.end());

    Logger::info(LogCategory::CACHE, "Found {} evictable ON_DEMAND files", candidates.size());

    // If no ON_DEMAND files available, collect ALWAYS_CACHE files as emergency fallback
    if (candidates.empty())
    {
        Logger::warn(LogCategory::CACHE, "No ON_DEMAND files to evict - entering emergency mode");
        Logger::warn(LogCategory::CACHE, "Will evict ALWAYS_CACHE files (excluding active readers and downloads)");

        for (const auto &[path, entry] : cache_entries)
        {
            if (entry->policy != CachePolicy::ALWAYS_CACHE)
                continue;

            if (entry->is_downloading.load())
                continue;

            if (entry->memory_ref_count.load() > 0)
                continue;

            if (!entry->is_in_memory_cache.load())
                continue;

            candidates.emplace_back(entry->last_used, path);
        }

        std::sort(candidates.begin(), candidates.end());
        Logger::warn(LogCategory::CACHE, "Emergency mode: found {} ALWAYS_CACHE files to evict", candidates.size());
    }

    // Evict files until we have enough space
    size_t bytes_evicted = 0;
    size_t files_evicted = 0;

    for (const auto &[last_used, path] : candidates)
    {
        if (bytes_evicted >= bytes_needed)
        {
            break;
        }

        auto it = cache_entries.find(path);
        if (it != cache_entries.end() && it->second->is_in_memory_cache.load())
        {
            size_t file_size = it->second->file_size;

            Logger::info(LogCategory::CACHE, "  [{}] Evicting: {} ({} KB, last used {} sec ago)",
                         files_evicted + 1,
                         StringUtils::wideToUtf8(path),
                         file_size / 1024,
                         std::chrono::duration_cast<std::chrono::seconds>(
                             std::chrono::steady_clock::now() - last_used).count());

            // Remove from memory cache
            memory_cache.removeFileFromMemoryCache(path);

            // Update entry state
            it->second->is_in_memory_cache.store(false);

            files_evicted++;
            bytes_evicted += file_size;
        }
    }

    size_t new_size = memory_cache.getCacheSize();
    Logger::info(LogCategory::CACHE, "=== Eviction Complete ===");
    Logger::info(LogCategory::CACHE, "Evicted {} files, {} bytes ({} MB)",
                 files_evicted, bytes_evicted, bytes_evicted / (1024 * 1024));
    Logger::info(LogCategory::CACHE, "Cache size: {} MB -> {} MB",
                 current_size / (1024 * 1024), new_size / (1024 * 1024));

    // Record failed eviction metric if nothing was evicted
    if (files_evicted == 0)
    {
        GlobalMetrics::instance().recordCacheEvictionFailed();
    }

    return bytes_evicted;
}

void HybridFileSystem::recordActivity()
{
    last_activity.store(std::chrono::steady_clock::now());
}

bool HybridFileSystem::isSystemIdle(std::chrono::seconds threshold) const
{
    auto now = std::chrono::steady_clock::now();
    auto idle_time = std::chrono::duration_cast<std::chrono::seconds>(now - last_activity.load());
    return idle_time >= threshold;
}

void HybridFileSystem::memoryEvictionThreadFunc()
{
    Logger::info(LogCategory::CACHE, "Memory eviction background thread started");

    while (!shutdown_eviction)
    {
        // Check every 10 seconds
        std::this_thread::sleep_for(std::chrono::seconds(10));

        if (shutdown_eviction)
        {
            break;
        }

        // Only evict if system has been idle for at least 5 seconds
        if (!isSystemIdle(std::chrono::seconds(5)))
        {
            Logger::debug(LogCategory::CACHE, "System active, skipping eviction check");
            continue;
        }

        // System is idle - check if eviction needed
        size_t max_cache_size = config.global.total_cache_size_mb * 1024ULL * 1024ULL;
        size_t current_size = memory_cache.getCacheSize();

        if (current_size > max_cache_size * 0.9)
        {
            Logger::info(LogCategory::CACHE,
                        "System idle and memory cache at {}% capacity ({} MB / {} MB), performing background eviction",
                        (current_size * 100) / max_cache_size,
                        current_size / (1024 * 1024),
                        max_cache_size / (1024 * 1024));

            size_t bytes_to_evict = current_size - (max_cache_size * 0.8);
            performMemoryEviction(bytes_to_evict);
        }
    }

    Logger::info(LogCategory::CACHE, "Memory eviction background thread stopped");
}

void HybridFileSystem::updateAccessTime(CacheEntry *entry)
{
    entry->last_used = std::chrono::steady_clock::now();
    entry->access_count++;
}

NTSTATUS HybridFileSystem::handleFileAsDirectoryEntry(CacheEntry *entry, PWSTR Pattern, PWSTR Marker, PVOID Buffer, ULONG Length, PULONG PBytesTransferred)
{
    Logger::debug(LogCategory::FILESYSTEM, "handleFileAsDirectoryEntry() called for file: '{}'",
                 entry ? StringUtils::wideToUtf8(entry->virtual_path) : "null");

    if (!entry)
    {
        Logger::error(LogCategory::FILESYSTEM, "handleFileAsDirectoryEntry() - null entry");
        *PBytesTransferred = 0;
        return STATUS_NO_MORE_FILES;
    }

    // Extract filename from virtual path
    std::wstring filename;
    size_t lastSlash = entry->virtual_path.find_last_of(L"/\\");
    if (lastSlash != std::wstring::npos && lastSlash + 1 < entry->virtual_path.length())
    {
        filename = entry->virtual_path.substr(lastSlash + 1);
    }
    else
    {
        filename = entry->virtual_path;
    }

    // If Marker is provided and matches our filename, we've already returned this entry
    if (Marker && *Marker != L'\0')
    {
        std::wstring marker_str(Marker);
        if (filename == marker_str)
        {
            Logger::debug(LogCategory::FILESYSTEM, "handleFileAsDirectoryEntry() - marker matches filename, returning SUCCESS with 0 bytes");
            *PBytesTransferred = 0;
            return STATUS_SUCCESS; // Real Windows filesystem returns SUCCESS, not NO_MORE_FILES
        }
    }

    // Check if pattern matches (if provided)
    if (Pattern && *Pattern != L'\0')
    {
        std::wstring pattern_str(Pattern);
        if (!matchesPattern(filename, pattern_str))
        {
            Logger::debug(LogCategory::FILESYSTEM, "handleFileAsDirectoryEntry() - file '{}' doesn't match pattern '{}', returning SUCCESS with 0 bytes",
                         StringUtils::wideToUtf8(filename), StringUtils::wideToUtf8(pattern_str));
            *PBytesTransferred = 0;
            return STATUS_SUCCESS; // Real Windows filesystem returns SUCCESS, not NO_MORE_FILES
        }
    }

    // Calculate required buffer size
    size_t name_length_chars = filename.length();
    size_t name_size_bytes = name_length_chars * sizeof(WCHAR);
    size_t required_size = sizeof(FSP_FSCTL_DIR_INFO) + name_size_bytes;

    // Check for overflow before using
    if (required_size > UINT16_MAX)
    {
        Logger::error(LogCategory::FILESYSTEM, "handleFileAsDirectoryEntry() - entry '{}' size {} exceeds UINT16_MAX",
                     StringUtils::wideToUtf8(filename), required_size);
        *PBytesTransferred = 0;
        return STATUS_INVALID_PARAMETER;
    }

    if (Length < required_size)
    {
        Logger::error(LogCategory::FILESYSTEM, "handleFileAsDirectoryEntry() - buffer too small: {} < {}", Length, required_size);
        *PBytesTransferred = 0;
        return STATUS_BUFFER_TOO_SMALL;
    }

    // Fill directory entry for this file
    DirInfo *dir_info = static_cast<DirInfo *>(Buffer);
    memset(dir_info, 0, sizeof(DirInfo));

    dir_info->Size = (UINT16)required_size;
    dir_info->FileInfo.FileAttributes = entry->file_attributes;
    dir_info->FileInfo.ReparseTag = 0;
    dir_info->FileInfo.FileSize = entry->file_size;
    dir_info->FileInfo.AllocationSize = (entry->file_size + ALLOCATION_UNIT - 1) / ALLOCATION_UNIT * ALLOCATION_UNIT;

    // Convert FILETIME to UINT64 timestamps
    UINT64 creation_timestamp = ((PLARGE_INTEGER)&entry->creation_time)->QuadPart;
    UINT64 access_timestamp = ((PLARGE_INTEGER)&entry->last_access_time)->QuadPart;
    UINT64 write_timestamp = ((PLARGE_INTEGER)&entry->last_write_time)->QuadPart;

    // Use default creation time if entry times are not set
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

    // Copy filename with null termination
    memcpy(dir_info->FileNameBuf, filename.c_str(), name_size_bytes);
    WCHAR *filename_end = (WCHAR *)((char *)dir_info->FileNameBuf + name_size_bytes);
    *filename_end = L'\0';

    *PBytesTransferred = (ULONG)required_size;

    Logger::info(LogCategory::FILESYSTEM, "handleFileAsDirectoryEntry() - returning file '{}' as directory entry, size: {}, attributes: 0x{:x}",
                StringUtils::wideToUtf8(filename), entry->file_size, entry->file_attributes);

    return STATUS_SUCCESS;
}

NTSTATUS HybridFileSystem::GetStreamInfo(PVOID FileNode, PVOID FileDesc, PVOID Buffer, ULONG Length, PULONG PBytesTransferred)
{
    Logger::debug(LogCategory::FILESYSTEM, "GetStreamInfo() called");

    // For a caching filesystem that doesn't support alternate data streams,
    // we should return STATUS_NOT_IMPLEMENTED or STATUS_INVALID_DEVICE_REQUEST
    // This tells the caller that this filesystem doesn't support stream enumeration
    *PBytesTransferred = 0;

    Logger::debug(LogCategory::FILESYSTEM, "GetStreamInfo() - not implemented for caching filesystem");
    return STATUS_NOT_IMPLEMENTED;
}

} // namespace CeWinFileCache

#endif
