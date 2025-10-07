#include <ce-win-file-cache/security_descriptor_builder.hpp>
#include <ce-win-file-cache/logger.hpp>
#include <ce-win-file-cache/string_utils.hpp>
#include <aclapi.h>
#include <sddl.h>
#include <vector>

#ifndef NO_WINFSP

namespace CeWinFileCache
{

SecurityDescriptorBuilder::SecurityDescriptorBuilder(const SecurityDescriptorConfig &cfg)
    : config(cfg), file_descriptor(nullptr), file_descriptor_size(0), directory_descriptor(nullptr), directory_descriptor_size(0), initialized(false)
{
}

SecurityDescriptorBuilder::SecurityDescriptorBuilder()
    : file_descriptor(nullptr), file_descriptor_size(0), directory_descriptor(nullptr), directory_descriptor_size(0), initialized(false)
{
    // Set up default configuration
    config.owner = WellKnownSid::System;
    config.group = WellKnownSid::System;

    // Default file ACEs
    config.file_aces.push_back({WellKnownSid::System, AccessRights::FullControl});
    config.file_aces.push_back({WellKnownSid::Administrators, AccessRights::FullControl});
    config.file_aces.push_back({WellKnownSid::Everyone, AccessRights::ReadExecute}); // Match network share behavior
    config.file_aces.push_back({WellKnownSid::Users, AccessRights::ReadExecute});
    config.file_aces.push_back({WellKnownSid::AllApplicationPackages, AccessRights::ReadExecute});

    // Default directory ACEs (same as files but with inheritance for subdirectories and files)
    config.directory_aces.push_back({WellKnownSid::System, AccessRights::FullControl, InheritanceFlags::Both});
    config.directory_aces.push_back({WellKnownSid::Administrators, AccessRights::FullControl, InheritanceFlags::Both});
    config.directory_aces.push_back({WellKnownSid::Everyone, AccessRights::ReadExecute, InheritanceFlags::Both}); // Match network share behavior
    config.directory_aces.push_back({WellKnownSid::Users, AccessRights::ReadExecute, InheritanceFlags::Both});
    config.directory_aces.push_back({WellKnownSid::AllApplicationPackages, AccessRights::ReadExecute, InheritanceFlags::Both});
}

SecurityDescriptorBuilder::~SecurityDescriptorBuilder()
{
    freeDescriptors();
}

PSID SecurityDescriptorBuilder::createWellKnownSid(WellKnownSid sid_type)
{
    SID_IDENTIFIER_AUTHORITY nt_authority = SECURITY_NT_AUTHORITY;
    SID_IDENTIFIER_AUTHORITY world_authority = SECURITY_WORLD_SID_AUTHORITY;
    SID_IDENTIFIER_AUTHORITY app_package_authority = SECURITY_APP_PACKAGE_AUTHORITY;

    PSID sid = nullptr;

    switch (sid_type)
    {
        case WellKnownSid::System:
            // S-1-5-18 (NT AUTHORITY\SYSTEM)
            AllocateAndInitializeSid(&nt_authority, 1, SECURITY_LOCAL_SYSTEM_RID,
                                    0, 0, 0, 0, 0, 0, 0, &sid);
            break;

        case WellKnownSid::Administrators:
            // S-1-5-32-544 (BUILTIN\Administrators)
            {
                SID_IDENTIFIER_AUTHORITY builtin_authority = SECURITY_NT_AUTHORITY;
                AllocateAndInitializeSid(&builtin_authority, 2, SECURITY_BUILTIN_DOMAIN_RID,
                                        DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &sid);
            }
            break;

        case WellKnownSid::Users:
            // S-1-5-32-545 (BUILTIN\Users)
            {
                SID_IDENTIFIER_AUTHORITY builtin_authority = SECURITY_NT_AUTHORITY;
                AllocateAndInitializeSid(&builtin_authority, 2, SECURITY_BUILTIN_DOMAIN_RID,
                                        DOMAIN_ALIAS_RID_USERS, 0, 0, 0, 0, 0, 0, &sid);
            }
            break;

        case WellKnownSid::AuthenticatedUsers:
            // S-1-5-11 (NT AUTHORITY\Authenticated Users)
            AllocateAndInitializeSid(&nt_authority, 1, SECURITY_AUTHENTICATED_USER_RID,
                                    0, 0, 0, 0, 0, 0, 0, &sid);
            break;

        case WellKnownSid::Everyone:
            // S-1-1-0 (Everyone)
            AllocateAndInitializeSid(&world_authority, 1, SECURITY_WORLD_RID,
                                    0, 0, 0, 0, 0, 0, 0, &sid);
            break;

        case WellKnownSid::AllApplicationPackages:
            // S-1-15-2-1 (ALL APPLICATION PACKAGES)
            AllocateAndInitializeSid(&app_package_authority, 2, SECURITY_APP_PACKAGE_BASE_RID,
                                    SECURITY_BUILTIN_PACKAGE_ANY_PACKAGE, 0, 0, 0, 0, 0, 0, &sid);
            break;
    }

    return sid;
}

void SecurityDescriptorBuilder::freeSid(PSID sid)
{
    if (sid != nullptr)
    {
        FreeSid(sid);
    }
}

void SecurityDescriptorBuilder::buildFileDescriptor()
{
    if (file_descriptor != nullptr)
    {
        return;
    }

    Logger::debug(LogCategory::SECURITY, "Building file security descriptor using SetEntriesInAcl");

    // Create SIDs for owner and group
    PSID owner_sid = createWellKnownSid(config.owner);
    PSID group_sid = createWellKnownSid(config.group);

    if (!owner_sid || !group_sid)
    {
        Logger::error(LogCategory::SECURITY, "Failed to create owner/group SIDs");
        freeSid(owner_sid);
        freeSid(group_sid);
        return;
    }

    // Build EXPLICIT_ACCESS array
    std::vector<EXPLICIT_ACCESS_W> ea(config.file_aces.size());
    std::vector<PSID> ace_sids;
    ace_sids.reserve(config.file_aces.size());

    for (size_t i = 0; i < config.file_aces.size(); ++i)
    {
        const auto &ace = config.file_aces[i];
        PSID ace_sid = createWellKnownSid(ace.sid);
        if (!ace_sid)
        {
            Logger::error(LogCategory::SECURITY, "Failed to create ACE SID");
            for (auto sid : ace_sids) freeSid(sid);
            freeSid(owner_sid);
            freeSid(group_sid);
            return;
        }
        ace_sids.push_back(ace_sid);

        ZeroMemory(&ea[i], sizeof(EXPLICIT_ACCESS_W));
        ea[i].grfAccessPermissions = static_cast<DWORD>(ace.rights);
        ea[i].grfAccessMode = SET_ACCESS;
        ea[i].grfInheritance = NO_INHERITANCE;
        ea[i].Trustee.TrusteeForm = TRUSTEE_IS_SID;
        ea[i].Trustee.TrusteeType = TRUSTEE_IS_WELL_KNOWN_GROUP;
        ea[i].Trustee.ptstrName = reinterpret_cast<LPWSTR>(ace_sid);
    }

    // Create DACL
    PACL dacl = nullptr;
    DWORD result = SetEntriesInAclW(static_cast<ULONG>(ea.size()), ea.data(), nullptr, &dacl);
    if (result != ERROR_SUCCESS)
    {
        Logger::error(LogCategory::SECURITY, "SetEntriesInAcl failed, error: {}", result);
        for (auto sid : ace_sids) freeSid(sid);
        freeSid(owner_sid);
        freeSid(group_sid);
        return;
    }

    // Create security descriptor
    PSECURITY_DESCRIPTOR sd = LocalAlloc(LPTR, SECURITY_DESCRIPTOR_MIN_LENGTH);
    if (!sd)
    {
        Logger::error(LogCategory::SECURITY, "Failed to allocate security descriptor");
        LocalFree(dacl);
        for (auto sid : ace_sids) freeSid(sid);
        freeSid(owner_sid);
        freeSid(group_sid);
        return;
    }

    if (!InitializeSecurityDescriptor(sd, SECURITY_DESCRIPTOR_REVISION))
    {
        Logger::error(LogCategory::SECURITY, "InitializeSecurityDescriptor failed, error: {}", GetLastError());
        LocalFree(sd);
        LocalFree(dacl);
        for (auto sid : ace_sids) freeSid(sid);
        freeSid(owner_sid);
        freeSid(group_sid);
        return;
    }

    if (!SetSecurityDescriptorDacl(sd, TRUE, dacl, FALSE))
    {
        Logger::error(LogCategory::SECURITY, "SetSecurityDescriptorDacl failed, error: {}", GetLastError());
        LocalFree(sd);
        LocalFree(dacl);
        for (auto sid : ace_sids) freeSid(sid);
        freeSid(owner_sid);
        freeSid(group_sid);
        return;
    }

    if (!SetSecurityDescriptorOwner(sd, owner_sid, FALSE))
    {
        Logger::error(LogCategory::SECURITY, "SetSecurityDescriptorOwner failed, error: {}", GetLastError());
        LocalFree(sd);
        LocalFree(dacl);
        for (auto sid : ace_sids) freeSid(sid);
        freeSid(owner_sid);
        freeSid(group_sid);
        return;
    }

    if (!SetSecurityDescriptorGroup(sd, group_sid, FALSE))
    {
        Logger::error(LogCategory::SECURITY, "SetSecurityDescriptorGroup failed, error: {}", GetLastError());
        LocalFree(sd);
        LocalFree(dacl);
        for (auto sid : ace_sids) freeSid(sid);
        freeSid(owner_sid);
        freeSid(group_sid);
        return;
    }

    // Set protected flag
    if (!SetSecurityDescriptorControl(sd, SE_DACL_PROTECTED, SE_DACL_PROTECTED))
    {
        Logger::warn(LogCategory::SECURITY, "SetSecurityDescriptorControl for SE_DACL_PROTECTED failed, error: {}", GetLastError());
        // Continue anyway - not critical
    }

    // Convert to self-relative format
    DWORD sd_size = 0;
    MakeSelfRelativeSD(sd, nullptr, &sd_size);
    if (sd_size == 0)
    {
        Logger::error(LogCategory::SECURITY, "MakeSelfRelativeSD size query failed, error: {}", GetLastError());
        LocalFree(sd);
        LocalFree(dacl);
        for (auto sid : ace_sids) freeSid(sid);
        freeSid(owner_sid);
        freeSid(group_sid);
        return;
    }

    PSECURITY_DESCRIPTOR self_relative_sd = LocalAlloc(LPTR, sd_size);
    if (!self_relative_sd)
    {
        Logger::error(LogCategory::SECURITY, "Failed to allocate self-relative security descriptor");
        LocalFree(sd);
        LocalFree(dacl);
        for (auto sid : ace_sids) freeSid(sid);
        freeSid(owner_sid);
        freeSid(group_sid);
        return;
    }

    if (!MakeSelfRelativeSD(sd, self_relative_sd, &sd_size))
    {
        Logger::error(LogCategory::SECURITY, "MakeSelfRelativeSD failed, error: {}", GetLastError());
        LocalFree(self_relative_sd);
        LocalFree(sd);
        LocalFree(dacl);
        for (auto sid : ace_sids) freeSid(sid);
        freeSid(owner_sid);
        freeSid(group_sid);
        return;
    }

    // Clean up temporary resources
    LocalFree(sd);
    LocalFree(dacl);
    for (auto sid : ace_sids) freeSid(sid);
    freeSid(owner_sid);
    freeSid(group_sid);

    file_descriptor = self_relative_sd;
    file_descriptor_size = sd_size;

    // Verify the descriptor is valid
    if (!IsValidSecurityDescriptor(file_descriptor))
    {
        Logger::error(LogCategory::SECURITY, "Created file security descriptor is INVALID!");
        LocalFree(file_descriptor);
        file_descriptor = nullptr;
        file_descriptor_size = 0;
        return;
    }

    // Verify owner and group are set correctly
    PSID check_owner = nullptr;
    PSID check_group = nullptr;
    BOOL owner_defaulted = FALSE;
    BOOL group_defaulted = FALSE;

    if (GetSecurityDescriptorOwner(file_descriptor, &check_owner, &owner_defaulted) && check_owner)
    {
        LPSTR sid_string = nullptr;
        if (ConvertSidToStringSidA(check_owner, &sid_string))
        {
            Logger::debug(LogCategory::SECURITY, "File descriptor owner SID: {}", sid_string);
            LocalFree(sid_string);
        }
    }
    else
    {
        Logger::error(LogCategory::SECURITY, "Failed to get owner from file security descriptor!");
    }

    if (GetSecurityDescriptorGroup(file_descriptor, &check_group, &group_defaulted) && check_group)
    {
        LPSTR sid_string = nullptr;
        if (ConvertSidToStringSidA(check_group, &sid_string))
        {
            Logger::debug(LogCategory::SECURITY, "File descriptor group SID: {}", sid_string);
            LocalFree(sid_string);
        }
    }
    else
    {
        Logger::error(LogCategory::SECURITY, "Failed to get group from file security descriptor!");
    }

    // Log the result
    LPSTR verification_sddl = nullptr;
    if (ConvertSecurityDescriptorToStringSecurityDescriptorA(file_descriptor, SDDL_REVISION_1,
        OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION,
        &verification_sddl, nullptr))
    {
        Logger::debug(LogCategory::SECURITY, "Built file security descriptor, size: {}, SDDL: {}", file_descriptor_size, verification_sddl);
        LocalFree(verification_sddl);
    }
    else
    {
        Logger::debug(LogCategory::SECURITY, "Built file security descriptor, size: {}", file_descriptor_size);
    }
}

void SecurityDescriptorBuilder::buildDirectoryDescriptor()
{
    if (directory_descriptor != nullptr)
    {
        return;
    }

    Logger::debug(LogCategory::SECURITY, "Building directory security descriptor using SetEntriesInAcl");

    // Create SIDs for owner and group
    PSID owner_sid = createWellKnownSid(config.owner);
    PSID group_sid = createWellKnownSid(config.group);

    if (!owner_sid || !group_sid)
    {
        Logger::error(LogCategory::SECURITY, "Failed to create owner/group SIDs");
        freeSid(owner_sid);
        freeSid(group_sid);
        return;
    }

    // Build EXPLICIT_ACCESS array
    std::vector<EXPLICIT_ACCESS_W> ea(config.directory_aces.size());
    std::vector<PSID> ace_sids;
    ace_sids.reserve(config.directory_aces.size());

    for (size_t i = 0; i < config.directory_aces.size(); ++i)
    {
        const auto &ace = config.directory_aces[i];
        PSID ace_sid = createWellKnownSid(ace.sid);
        if (!ace_sid)
        {
            Logger::error(LogCategory::SECURITY, "Failed to create ACE SID");
            for (auto sid : ace_sids) freeSid(sid);
            freeSid(owner_sid);
            freeSid(group_sid);
            return;
        }
        ace_sids.push_back(ace_sid);

        ZeroMemory(&ea[i], sizeof(EXPLICIT_ACCESS_W));
        ea[i].grfAccessPermissions = static_cast<DWORD>(ace.rights);
        ea[i].grfAccessMode = SET_ACCESS;

        // Set inheritance flags for directories
        DWORD inheritance = 0;
        if (ace.inheritance != InheritanceFlags::None)
        {
            DWORD inherit_flags = static_cast<DWORD>(ace.inheritance);
            if (inherit_flags & static_cast<DWORD>(InheritanceFlags::ObjectInherit))
            {
                inheritance |= OBJECT_INHERIT_ACE;
            }
            if (inherit_flags & static_cast<DWORD>(InheritanceFlags::ContainerInherit))
            {
                inheritance |= CONTAINER_INHERIT_ACE;
            }
        }
        ea[i].grfInheritance = inheritance;

        ea[i].Trustee.TrusteeForm = TRUSTEE_IS_SID;
        ea[i].Trustee.TrusteeType = TRUSTEE_IS_WELL_KNOWN_GROUP;
        ea[i].Trustee.ptstrName = reinterpret_cast<LPWSTR>(ace_sid);
    }

    // Create DACL
    PACL dacl = nullptr;
    DWORD result = SetEntriesInAclW(static_cast<ULONG>(ea.size()), ea.data(), nullptr, &dacl);
    if (result != ERROR_SUCCESS)
    {
        Logger::error(LogCategory::SECURITY, "SetEntriesInAcl failed, error: {}", result);
        for (auto sid : ace_sids) freeSid(sid);
        freeSid(owner_sid);
        freeSid(group_sid);
        return;
    }

    // Create security descriptor
    PSECURITY_DESCRIPTOR sd = LocalAlloc(LPTR, SECURITY_DESCRIPTOR_MIN_LENGTH);
    if (!sd)
    {
        Logger::error(LogCategory::SECURITY, "Failed to allocate security descriptor");
        LocalFree(dacl);
        for (auto sid : ace_sids) freeSid(sid);
        freeSid(owner_sid);
        freeSid(group_sid);
        return;
    }

    if (!InitializeSecurityDescriptor(sd, SECURITY_DESCRIPTOR_REVISION))
    {
        Logger::error(LogCategory::SECURITY, "InitializeSecurityDescriptor failed, error: {}", GetLastError());
        LocalFree(sd);
        LocalFree(dacl);
        for (auto sid : ace_sids) freeSid(sid);
        freeSid(owner_sid);
        freeSid(group_sid);
        return;
    }

    if (!SetSecurityDescriptorDacl(sd, TRUE, dacl, FALSE))
    {
        Logger::error(LogCategory::SECURITY, "SetSecurityDescriptorDacl failed, error: {}", GetLastError());
        LocalFree(sd);
        LocalFree(dacl);
        for (auto sid : ace_sids) freeSid(sid);
        freeSid(owner_sid);
        freeSid(group_sid);
        return;
    }

    if (!SetSecurityDescriptorOwner(sd, owner_sid, FALSE))
    {
        Logger::error(LogCategory::SECURITY, "SetSecurityDescriptorOwner failed, error: {}", GetLastError());
        LocalFree(sd);
        LocalFree(dacl);
        for (auto sid : ace_sids) freeSid(sid);
        freeSid(owner_sid);
        freeSid(group_sid);
        return;
    }

    if (!SetSecurityDescriptorGroup(sd, group_sid, FALSE))
    {
        Logger::error(LogCategory::SECURITY, "SetSecurityDescriptorGroup failed, error: {}", GetLastError());
        LocalFree(sd);
        LocalFree(dacl);
        for (auto sid : ace_sids) freeSid(sid);
        freeSid(owner_sid);
        freeSid(group_sid);
        return;
    }

    // Set protected flag
    if (!SetSecurityDescriptorControl(sd, SE_DACL_PROTECTED, SE_DACL_PROTECTED))
    {
        Logger::warn(LogCategory::SECURITY, "SetSecurityDescriptorControl for SE_DACL_PROTECTED failed, error: {}", GetLastError());
        // Continue anyway - not critical
    }

    // Convert to self-relative format
    DWORD sd_size = 0;
    MakeSelfRelativeSD(sd, nullptr, &sd_size);
    if (sd_size == 0)
    {
        Logger::error(LogCategory::SECURITY, "MakeSelfRelativeSD size query failed, error: {}", GetLastError());
        LocalFree(sd);
        LocalFree(dacl);
        for (auto sid : ace_sids) freeSid(sid);
        freeSid(owner_sid);
        freeSid(group_sid);
        return;
    }

    PSECURITY_DESCRIPTOR self_relative_sd = LocalAlloc(LPTR, sd_size);
    if (!self_relative_sd)
    {
        Logger::error(LogCategory::SECURITY, "Failed to allocate self-relative security descriptor");
        LocalFree(sd);
        LocalFree(dacl);
        for (auto sid : ace_sids) freeSid(sid);
        freeSid(owner_sid);
        freeSid(group_sid);
        return;
    }

    if (!MakeSelfRelativeSD(sd, self_relative_sd, &sd_size))
    {
        Logger::error(LogCategory::SECURITY, "MakeSelfRelativeSD failed, error: {}", GetLastError());
        LocalFree(self_relative_sd);
        LocalFree(sd);
        LocalFree(dacl);
        for (auto sid : ace_sids) freeSid(sid);
        freeSid(owner_sid);
        freeSid(group_sid);
        return;
    }

    // Clean up temporary resources
    LocalFree(sd);
    LocalFree(dacl);
    for (auto sid : ace_sids) freeSid(sid);
    freeSid(owner_sid);
    freeSid(group_sid);

    directory_descriptor = self_relative_sd;
    directory_descriptor_size = sd_size;

    // Verify the descriptor is valid
    if (!IsValidSecurityDescriptor(directory_descriptor))
    {
        Logger::error(LogCategory::SECURITY, "Created directory security descriptor is INVALID!");
        LocalFree(directory_descriptor);
        directory_descriptor = nullptr;
        directory_descriptor_size = 0;
        return;
    }

    // Verify owner and group are set correctly
    PSID check_owner = nullptr;
    PSID check_group = nullptr;
    BOOL owner_defaulted = FALSE;
    BOOL group_defaulted = FALSE;

    if (GetSecurityDescriptorOwner(directory_descriptor, &check_owner, &owner_defaulted) && check_owner)
    {
        LPSTR sid_string = nullptr;
        if (ConvertSidToStringSidA(check_owner, &sid_string))
        {
            Logger::debug(LogCategory::SECURITY, "Directory descriptor owner SID: {}", sid_string);
            LocalFree(sid_string);
        }
    }
    else
    {
        Logger::error(LogCategory::SECURITY, "Failed to get owner from directory security descriptor!");
    }

    if (GetSecurityDescriptorGroup(directory_descriptor, &check_group, &group_defaulted) && check_group)
    {
        LPSTR sid_string = nullptr;
        if (ConvertSidToStringSidA(check_group, &sid_string))
        {
            Logger::debug(LogCategory::SECURITY, "Directory descriptor group SID: {}", sid_string);
            LocalFree(sid_string);
        }
    }
    else
    {
        Logger::error(LogCategory::SECURITY, "Failed to get group from directory security descriptor!");
    }

    // Log the result
    LPSTR verification_sddl = nullptr;
    if (ConvertSecurityDescriptorToStringSecurityDescriptorA(directory_descriptor, SDDL_REVISION_1,
        OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION,
        &verification_sddl, nullptr))
    {
        Logger::debug(LogCategory::SECURITY, "Built directory security descriptor, size: {}, SDDL: {}", directory_descriptor_size, verification_sddl);
        LocalFree(verification_sddl);
    }
    else
    {
        Logger::debug(LogCategory::SECURITY, "Built directory security descriptor, size: {}", directory_descriptor_size);
    }
}

void SecurityDescriptorBuilder::freeDescriptors()
{
    if (file_descriptor != nullptr)
    {
        LocalFree(file_descriptor);
        file_descriptor = nullptr;
        file_descriptor_size = 0;
    }

    if (directory_descriptor != nullptr)
    {
        LocalFree(directory_descriptor);
        directory_descriptor = nullptr;
        directory_descriptor_size = 0;
    }

    initialized = false;
}

bool SecurityDescriptorBuilder::getFileSecurityDescriptor(PSECURITY_DESCRIPTOR *out_descriptor, DWORD *out_size)
{
    std::lock_guard<std::mutex> lock(descriptor_mutex);

    if (!initialized)
    {
        buildFileDescriptor();
        buildDirectoryDescriptor();
        initialized = true;
    }

    if (file_descriptor == nullptr)
    {
        Logger::error(LogCategory::SECURITY, "File security descriptor not initialized");
        return false;
    }

    if (out_descriptor)
    {
        *out_descriptor = file_descriptor;
    }

    if (out_size)
    {
        *out_size = file_descriptor_size;
    }

    return true;
}

bool SecurityDescriptorBuilder::getDirectorySecurityDescriptor(PSECURITY_DESCRIPTOR *out_descriptor, DWORD *out_size)
{
    std::lock_guard<std::mutex> lock(descriptor_mutex);

    if (!initialized)
    {
        buildFileDescriptor();
        buildDirectoryDescriptor();
        initialized = true;
    }

    if (directory_descriptor == nullptr)
    {
        Logger::error(LogCategory::SECURITY, "Directory security descriptor not initialized");
        return false;
    }

    if (out_descriptor)
    {
        *out_descriptor = directory_descriptor;
    }

    if (out_size)
    {
        *out_size = directory_descriptor_size;
    }

    return true;
}

bool SecurityDescriptorBuilder::copySecurityDescriptor(PSECURITY_DESCRIPTOR source_descriptor, PSECURITY_DESCRIPTOR dest_buffer, SIZE_T buffer_size, SIZE_T *required_size)
{
    if (source_descriptor == nullptr)
    {
        Logger::error(LogCategory::SECURITY, "Source security descriptor is null");
        return false;
    }

    DWORD sd_size = GetSecurityDescriptorLength(source_descriptor);
    if (sd_size == 0)
    {
        Logger::error(LogCategory::SECURITY, "Invalid source security descriptor");
        return false;
    }

    if (required_size)
    {
        *required_size = sd_size;
    }

    // If buffer_size is 0 or dest_buffer is null, caller just wants the size
    if (buffer_size == 0 || dest_buffer == nullptr)
    {
        return true;
    }

    // Check if buffer is large enough
    if (buffer_size < sd_size)
    {
        Logger::debug(LogCategory::SECURITY, "Buffer too small for security descriptor, provided: {}, need: {}", buffer_size, sd_size);
        return false;
    }

    // Copy the descriptor
    memcpy(dest_buffer, source_descriptor, sd_size);
    return true;
}

} // namespace CeWinFileCache

#endif // NO_WINFSP
