#pragma once

#include <ce-win-file-cache/windows_compat.hpp>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace CeWinFileCache
{

enum class NodeType : std::uint8_t
{
    DIRECTORY,
    FILE
};

struct DirectoryNode
{
    std::wstring name;
    std::wstring full_virtual_path; // e.g., "/msvc-14.40/bin/cl.exe"
    std::wstring network_path; // e.g., "\\server\share\msvc\14.40\bin\cl.exe"
    NodeType type;

    // File metadata
    UINT64 file_size = 0;
    FILETIME creation_time;
    FILETIME last_access_time;
    FILETIME last_write_time;
    DWORD file_attributes = 0;

    // Directory structure
    std::unordered_map<std::wstring, std::unique_ptr<DirectoryNode>> children;
    DirectoryNode *parent;

    // Thread safety for children operations
    mutable std::mutex children_mutex;

    // Constructor
    DirectoryNode(std::wstring node_name, NodeType node_type, DirectoryNode *parent_node = nullptr);

    // Helper methods
    bool isDirectory() const;
    bool isFile() const;
    DirectoryNode *findChild(const std::wstring &child_name);
    DirectoryNode *addChild(const std::wstring &child_name, NodeType child_type);
    std::vector<std::wstring> getChildNames() const;
    std::vector<DirectoryNode *> getChildNodes() const;
};

class DirectoryTree
{
    public:
    DirectoryTree();
    ~DirectoryTree() = default;

    // Core operations
    DirectoryNode *findNode(const std::wstring &virtual_path);
    DirectoryNode *createPath(const std::wstring &virtual_path, NodeType type);
    bool addFile(const std::wstring &virtual_path, const std::wstring &network_path, UINT64 size = 0, const FILETIME *creation_time = nullptr);
    bool addDirectory(const std::wstring &virtual_path, const std::wstring &network_path);

    // Directory enumeration
    std::vector<DirectoryNode *> getDirectoryContents(const std::wstring &virtual_path);

    // Statistics
    size_t getTotalNodes() const;
    size_t getTotalDirectories() const;
    size_t getTotalFiles() const;

    // Tree management
    void reset();

    // Thread safety
    void lock();
    void unlock();
    std::lock_guard<std::mutex> getLock();

    private:
    std::unique_ptr<DirectoryNode> root;
    mutable std::mutex tree_mutex;

    // Helper methods
    static std::vector<std::wstring> splitPath(const std::wstring &path);
    DirectoryNode *findOrCreatePath(const std::wstring &virtual_path, bool create_missing);
    static void updateNodeMetadata(DirectoryNode *node, const std::wstring &network_path, UINT64 size, const FILETIME *creation_time);
};

} // namespace CeWinFileCache