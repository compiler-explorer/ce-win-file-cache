#pragma once

#include <ce-win-file-cache/windows_compat.hpp>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#ifndef NO_WINFSP
#include <sddl.h>
#endif

namespace CeWinFileCache
{

/**
 * Access rights for security descriptors.
 * These can be combined using bitwise OR.
 */
enum class AccessRights : DWORD
{
    None = 0,
    Read = 0x120089,           // FILE_GENERIC_READ
    Write = 0x120116,          // FILE_GENERIC_WRITE
    Execute = 0x1200a0,        // FILE_GENERIC_EXECUTE
    ReadExecute = 0x1200a9,    // FILE_GENERIC_READ | FILE_GENERIC_EXECUTE
    Modify = 0x1201bf,         // Read + Write + Execute + Delete
    FullControl = 0x1f01ff     // FILE_ALL_ACCESS (FA in SDDL)
};

// Bitwise operators for AccessRights
inline AccessRights operator|(AccessRights a, AccessRights b)
{
    return static_cast<AccessRights>(static_cast<DWORD>(a) | static_cast<DWORD>(b));
}

inline AccessRights operator&(AccessRights a, AccessRights b)
{
    return static_cast<AccessRights>(static_cast<DWORD>(a) & static_cast<DWORD>(b));
}

/**
 * Well-known security principals (SIDs).
 */
enum class WellKnownSid
{
    System,                    // Local System
    Administrators,            // Built-in Administrators
    Users,                     // Built-in Users
    AuthenticatedUsers,        // Authenticated Users
    Everyone,                  // World/Everyone
    AllApplicationPackages     // All Application Packages (UWP/AppContainer apps)
};

/**
 * Inheritance flags for directory ACEs.
 */
enum class InheritanceFlags : DWORD
{
    None = 0,
    ObjectInherit = 0x1,      // OI - Files inherit
    ContainerInherit = 0x2,   // CI - Subdirectories inherit
    Both = 0x3                // OICI - Both files and subdirectories inherit
};

/**
 * Represents a single Access Control Entry.
 */
struct AccessControlEntry
{
    WellKnownSid sid;                         // Well-known SID
    AccessRights rights;                      // Access rights mask
    InheritanceFlags inheritance;             // Inheritance flags for directories

    AccessControlEntry(WellKnownSid sid_value, AccessRights access_rights,
                      InheritanceFlags inherit = InheritanceFlags::None)
        : sid(sid_value), rights(access_rights), inheritance(inherit)
    {
    }
};

/**
 * Configuration for building security descriptors.
 */
struct SecurityDescriptorConfig
{
    WellKnownSid owner;                              // Owner SID
    WellKnownSid group;                              // Group SID
    std::vector<AccessControlEntry> file_aces;       // ACEs for files
    std::vector<AccessControlEntry> directory_aces;  // ACEs for directories

    SecurityDescriptorConfig()
        : owner(WellKnownSid::System), group(WellKnownSid::System)
    {
    }
};

/**
 * SecurityDescriptorBuilder creates security descriptors on demand for files and directories.
 *
 * This class provides pre-built security descriptors with configurable permissions to avoid
 * the overhead of calling GetNamedSecurityInfo or creating them dynamically for each file.
 *
 * Usage:
 *   SecurityDescriptorConfig config;
 *   config.owner = WellKnownSid::System;
 *   config.group = WellKnownSid::System;
 *   config.file_aces.push_back({WellKnownSid::System, AccessRights::FullControl});
 *   config.file_aces.push_back({WellKnownSid::Administrators, AccessRights::FullControl});
 *
 *   SecurityDescriptorBuilder builder(config);
 *   PSECURITY_DESCRIPTOR sd;
 *   DWORD size;
 *   builder.getFileSecurityDescriptor(&sd, &size);
 */
class SecurityDescriptorBuilder
{
public:
    /**
     * Construct with custom configuration.
     */
    explicit SecurityDescriptorBuilder(const SecurityDescriptorConfig &config);

    /**
     * Construct with default permissions:
     * - Owner/Group: SYSTEM
     * - Files: SYSTEM (Full), Administrators (Full), Users (Read & Execute), All Application Packages (Read & Execute)
     * - Directories: Same with inheritance
     */
    SecurityDescriptorBuilder();

    ~SecurityDescriptorBuilder();

    // Prevent copying
    SecurityDescriptorBuilder(const SecurityDescriptorBuilder &) = delete;
    SecurityDescriptorBuilder &operator=(const SecurityDescriptorBuilder &) = delete;

    /**
     * Get a security descriptor for a file.
     * Returns a pointer to the internal security descriptor and its size.
     * The descriptor remains valid for the lifetime of this object.
     */
    bool getFileSecurityDescriptor(PSECURITY_DESCRIPTOR *out_descriptor, DWORD *out_size);

    /**
     * Get a security descriptor for a directory.
     * Returns a pointer to the internal security descriptor and its size.
     * The descriptor remains valid for the lifetime of this object.
     */
    bool getDirectorySecurityDescriptor(PSECURITY_DESCRIPTOR *out_descriptor, DWORD *out_size);

    /**
     * Copy a security descriptor to a buffer.
     * Returns true if successful, false if buffer is too small.
     * If buffer_size is 0, only sets required_size.
     */
    bool copySecurityDescriptor(PSECURITY_DESCRIPTOR source_descriptor, PSECURITY_DESCRIPTOR dest_buffer, SIZE_T buffer_size, SIZE_T *required_size);

private:
    void buildFileDescriptor();
    void buildDirectoryDescriptor();
    void freeDescriptors();
    static PSID createWellKnownSid(WellKnownSid sid_type);
    static void freeSid(PSID sid);

    SecurityDescriptorConfig config;

    PSECURITY_DESCRIPTOR file_descriptor;
    DWORD file_descriptor_size;

    PSECURITY_DESCRIPTOR directory_descriptor;
    DWORD directory_descriptor_size;

    std::mutex descriptor_mutex;
    bool initialized;
};

} // namespace CeWinFileCache
