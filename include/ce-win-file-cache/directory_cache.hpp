#pragma once

#include "../types/directory_tree.hpp"
#include "../types/config.hpp"
#include "windows_compat.hpp"
#include <string>
#include <vector>
#include <memory>

namespace CeWinFileCache
{

class DirectoryCache
{
public:
    DirectoryCache();
    ~DirectoryCache() = default;

    // Initialization
    NTSTATUS initialize(const Config& config);
    
    // Core directory operations
    std::vector<DirectoryNode*> getDirectoryContents(const std::wstring& virtual_path);
    DirectoryNode* findNode(const std::wstring& virtual_path);
    
    // Directory tree building
    NTSTATUS buildDirectoryTreeFromConfig(const Config& config);
    NTSTATUS enumerateNetworkDirectory(const std::wstring& network_path, const std::wstring& virtual_path);
    
    // Statistics
    size_t getTotalDirectories() const;
    size_t getTotalFiles() const;
    size_t getTotalNodes() const;
    
    // Testing support
    void addTestFile(const std::wstring& virtual_path, const std::wstring& network_path, UINT64 size = 1024);
    void addTestDirectory(const std::wstring& virtual_path, const std::wstring& network_path);
    void clearTree();

private:
    DirectoryTree directory_tree;
    
    // Platform-specific network enumeration
    NTSTATUS enumerateNetworkDirectoryWindows(const std::wstring& network_path, const std::wstring& virtual_path);
    NTSTATUS enumerateNetworkDirectoryMock(const std::wstring& network_path, const std::wstring& virtual_path);
};

} // namespace CeWinFileCache