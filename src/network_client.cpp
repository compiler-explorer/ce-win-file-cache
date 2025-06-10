#include "../include/compiler_cache/network_client.hpp"
#include <Windows.h>
#include <Winnetwk.h>
#include <iostream>

#pragma comment(lib, "mpr.lib")

namespace CeWinFileCache
{

NetworkClient::NetworkClient()
    : is_connected_(false)
{
    ZeroMemory(&net_resource_, sizeof(net_resource_));
}

NetworkClient::~NetworkClient()
{
    disconnect();
}

NTSTATUS NetworkClient::connect(const std::wstring& share_path)
{
    if (is_connected_ && current_share_ == share_path)
    {
        return STATUS_SUCCESS;
    }
    
    if (is_connected_)
    {
        disconnect();
    }
    
    return establishConnection(share_path);
}

NTSTATUS NetworkClient::disconnect()
{
    if (!is_connected_)
    {
        return STATUS_SUCCESS;
    }
    
    cleanupConnection();
    return STATUS_SUCCESS;
}

NTSTATUS NetworkClient::copyFileToLocal(const std::wstring& network_path, const std::wstring& local_path)
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
        std::wcerr << L"Failed to copy file from " << network_path 
                   << L" to " << local_path 
                   << L". Error: " << error << std::endl;
        return NTSTATUS_FROM_WIN32(error);
    }
    
    return STATUS_SUCCESS;
}

NTSTATUS NetworkClient::getFileInfo(const std::wstring& network_path, WIN32_FILE_ATTRIBUTE_DATA* file_data)
{
    if (!GetFileAttributesExW(network_path.c_str(), GetFileExInfoStandard, file_data))
    {
        return NTSTATUS_FROM_WIN32(GetLastError());
    }
    
    return STATUS_SUCCESS;
}

bool NetworkClient::fileExists(const std::wstring& network_path)
{
    DWORD attributes = GetFileAttributesW(network_path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES;
}

NTSTATUS NetworkClient::enumerateDirectory(const std::wstring& network_path, 
                                          std::vector<WIN32_FIND_DATAW>& entries)
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
        return NTSTATUS_FROM_WIN32(GetLastError());
    }
    
    do
    {
        // Skip . and .. entries
        if (wcscmp(find_data.cFileName, L".") != 0 && 
            wcscmp(find_data.cFileName, L"..") != 0)
        {
            entries.push_back(find_data);
        }
    } while (FindNextFileW(find_handle, &find_data));
    
    DWORD error = GetLastError();
    FindClose(find_handle);
    
    if (error != ERROR_NO_MORE_FILES)
    {
        return NTSTATUS_FROM_WIN32(error);
    }
    
    return STATUS_SUCCESS;
}

NTSTATUS NetworkClient::establishConnection(const std::wstring& share_path)
{
    // For UNC paths, we typically don't need explicit connection
    // but we can verify access
    DWORD attributes = GetFileAttributesW(share_path.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES)
    {
        DWORD error = GetLastError();
        
        // Try to establish a connection if access failed
        net_resource_.dwType = RESOURCETYPE_DISK;
        net_resource_.lpLocalName = nullptr; // No drive mapping
        net_resource_.lpRemoteName = const_cast<LPWSTR>(share_path.c_str());
        net_resource_.lpProvider = nullptr;
        
        DWORD result = WNetAddConnection2W(&net_resource_, nullptr, nullptr, 0);
        if (result != NO_ERROR && result != ERROR_ALREADY_ASSIGNED)
        {
            std::wcerr << L"Failed to connect to " << share_path 
                       << L". Error: " << result << std::endl;
            return NTSTATUS_FROM_WIN32(result);
        }
        
        // Verify access again
        attributes = GetFileAttributesW(share_path.c_str());
        if (attributes == INVALID_FILE_ATTRIBUTES)
        {
            return NTSTATUS_FROM_WIN32(GetLastError());
        }
    }
    
    current_share_ = share_path;
    is_connected_ = true;
    
    return STATUS_SUCCESS;
}

void NetworkClient::cleanupConnection()
{
    if (!current_share_.empty())
    {
        // Only disconnect if we explicitly connected
        if (net_resource_.lpRemoteName != nullptr)
        {
            WNetCancelConnection2W(current_share_.c_str(), 0, FALSE);
        }
        current_share_.clear();
    }
    
    ZeroMemory(&net_resource_, sizeof(net_resource_));
    is_connected_ = false;
}

} // namespace CeWinFileCache