#include <ce-win-file-cache/network_client.hpp>
#include <ce-win-file-cache/windows_compat.hpp>
#include <iostream>

#ifndef WINE_CROSS_COMPILE
#pragma comment(lib, "mpr.lib")
#endif

namespace CeWinFileCache
{

NetworkClient::NetworkClient() : is_connected(false)
{
    ZeroMemory(&net_resource, sizeof(net_resource));
}

NetworkClient::~NetworkClient()
{
    disconnect();
}

NTSTATUS NetworkClient::connect(const std::wstring &share_path)
{
    if (is_connected && current_share == share_path)
    {
        return STATUS_SUCCESS;
    }

    if (is_connected)
    {
        disconnect();
    }

    return establishConnection(share_path);
}

NTSTATUS NetworkClient::disconnect()
{
    if (!is_connected)
    {
        return STATUS_SUCCESS;
    }

    cleanupConnection();
    return STATUS_SUCCESS;
}

NTSTATUS NetworkClient::copyFileToLocal(const std::wstring &network_path, const std::wstring &local_path)
{
    // Ensure the local directory exists
    size_t last_slash = local_path.find_last_of(L'\\');
    if (last_slash != std::wstring::npos)
    {
        std::wstring dir_path = local_path.substr(0, last_slash);
        SHCreateDirectoryExW(nullptr, dir_path.c_str(), nullptr);
    }

    if (!CopyFileW(network_path.c_str(), local_path.c_str(), FALSE))
    {
        DWORD error = GetLastError();
        std::wcerr << L"Failed to copy file from " << network_path << L" to " << local_path << L". Error: " << error << std::endl;
        return CeWinFileCache::WineCompat::NtStatusFromWin32(error);
    }

    return STATUS_SUCCESS;
}

NTSTATUS NetworkClient::getFileInfo(const std::wstring &network_path, WIN32_FILE_ATTRIBUTE_DATA *file_data)
{
    if (!GetFileAttributesExW(network_path.c_str(), GetFileExInfoStandard, file_data))
    {
        return CeWinFileCache::WineCompat::GetLastErrorAsNtStatus();
    }

    return STATUS_SUCCESS;
}

bool NetworkClient::fileExists(const std::wstring &network_path)
{
    DWORD attributes = GetFileAttributesW(network_path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES;
}

NTSTATUS NetworkClient::enumerateDirectory(const std::wstring &network_path, std::vector<WIN32_FIND_DATAW> &entries)
{
    entries.clear();

    std::wstring search_pattern = network_path;
    if (search_pattern.back() != L'\\')
    {
        search_pattern += L'\\';
    }
    search_pattern += L"*";

    WIN32_FIND_DATAW find_data;
    HANDLE find_handle = FindFirstFileW(search_pattern.c_str(), &find_data);

    if (find_handle == INVALID_HANDLE_VALUE)
    {
        return CeWinFileCache::WineCompat::GetLastErrorAsNtStatus();
    }

    do
    {
        // Skip . and .. entries
        const wchar_t *dot = L".";
        const wchar_t *dotdot = L"..";
        if (wcscmp(find_data.cFileName, dot) != 0 && wcscmp(find_data.cFileName, dotdot) != 0)
        {
            entries.push_back(find_data);
        }
    } while (FindNextFileW(find_handle, &find_data));

    DWORD error = GetLastError();
    FindClose(find_handle);

    if (error != ERROR_NO_MORE_FILES)
    {
        return CeWinFileCache::WineCompat::NtStatusFromWin32(error);
    }

    return STATUS_SUCCESS;
}

NTSTATUS NetworkClient::establishConnection(const std::wstring &share_path)
{
    // For UNC paths, we typically don't need explicit connection
    // but we can verify access
    DWORD attributes = GetFileAttributesW(share_path.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES)
    {
        // Try to establish a connection if access failed
        // Create a non-const copy for the Windows API
        std::wstring share_path_copy = share_path;
        net_resource.dwType = RESOURCETYPE_DISK;
        net_resource.lpLocalName = nullptr; // No drive mapping
        net_resource.lpRemoteName = share_path_copy.data();
        net_resource.lpProvider = nullptr;

        DWORD result = WNetAddConnection2W(&net_resource, nullptr, nullptr, 0);
        if (result != NO_ERROR && result != ERROR_ALREADY_ASSIGNED)
        {
            std::wcerr << L"Failed to connect to " << share_path << L". Error: " << result << std::endl;
            return CeWinFileCache::WineCompat::NtStatusFromWin32(result);
        }

        // Verify access again
        attributes = GetFileAttributesW(share_path.c_str());
        if (attributes == INVALID_FILE_ATTRIBUTES)
        {
            return CeWinFileCache::WineCompat::GetLastErrorAsNtStatus();
        }
    }

    current_share = share_path;
    is_connected = true;

    return STATUS_SUCCESS;
}

void NetworkClient::cleanupConnection()
{
    if (!current_share.empty())
    {
        // Only disconnect if we explicitly connected
        if (net_resource.lpRemoteName != nullptr)
        {
            WNetCancelConnection2W(current_share.c_str(), 0, FALSE);
        }
        current_share.clear();
    }

    ZeroMemory(&net_resource, sizeof(net_resource));
    is_connected = false;
}

} // namespace CeWinFileCache
