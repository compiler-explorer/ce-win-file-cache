#pragma once

#include "windows_compat.hpp"
#include <string>
#include <vector>

namespace CeWinFileCache
{

class NetworkClient
{
    public:
    NetworkClient();
    ~NetworkClient();

    NTSTATUS connect(const std::wstring &share_path);
    NTSTATUS disconnect();

    // File operations
    NTSTATUS copyFileToLocal(const std::wstring &network_path, const std::wstring &local_path);
    NTSTATUS getFileInfo(const std::wstring &network_path, WIN32_FILE_ATTRIBUTE_DATA *file_data);
    bool fileExists(const std::wstring &network_path);

    // Directory operations
    NTSTATUS enumerateDirectory(const std::wstring &network_path, std::vector<WIN32_FIND_DATAW> &entries);

    private:
    NTSTATUS establishConnection(const std::wstring &share_path);
    void cleanupConnection();

    std::wstring current_share;
    bool is_connected;
    NETRESOURCEW net_resource;
};

} // namespace CeWinFileCache