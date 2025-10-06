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
    UNKNOWN,
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

    PSECURITY_DESCRIPTOR SecDesc = nullptr;

    // Directory structure
    std::unordered_map<std::wstring, std::unique_ptr<DirectoryNode>> children;
    DirectoryNode *parent;

    // Sorted children cache for directory listings
    mutable std::vector<DirectoryNode*> sorted_children_cache;
    mutable bool sorted_cache_valid = false;

    // Thread safety for children operations
    mutable std::mutex children_mutex;

    // Constructor
    DirectoryNode(std::wstring node_name, NodeType node_type, DirectoryNode *parent_node = nullptr);

    // Helper methods
    bool isDirectory() const;
    bool isFile() const;
    DirectoryNode *findChild(std::wstring_view child_name);
    DirectoryNode *addChild(const std::wstring &child_name, NodeType child_type);
    DirectoryNode *addChild(std::wstring_view child_name, NodeType child_type);
    std::vector<std::wstring> getChildNames() const;
    std::vector<DirectoryNode *> getChildNodes() const;

    // Path normalization for Windows
    static std::wstring normalizePath(const std::wstring &path);
    static std::wstring normalizeUNCPath(const std::wstring &path);
};

class DirectoryTree
{
    public:
    DirectoryTree();
    ~DirectoryTree() = default;

    void init(const std::wstring &network_path);

    // Core operations
    DirectoryNode *findNode(const std::wstring &virtual_path);
    DirectoryNode *findNodeNormalized(const std::wstring &normalized_virtual_path);  // Internal: path already normalized
    bool addFile(const std::wstring &virtual_path,
                 const std::wstring &network_path,
                 UINT64 size,
                 FILETIME creation_time,
                 FILETIME last_access_time,
                 FILETIME last_write_time,
                 DWORD file_attributes);
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
    std::lock_guard<std::mutex> getLock();

    private:
    std::unique_ptr<DirectoryNode> root;
    mutable std::mutex tree_mutex;
    std::wstring base_network_path;

    // Helper methods
    static std::vector<std::wstring_view> splitPath(const std::wstring &path);
    DirectoryNode *findOrCreatePath(const std::wstring &virtual_path, NodeType node_type, bool create_missing);
    static void updateNodeMetadata(DirectoryNode *node,
                                   const std::wstring &network_path,
                                   UINT64 size,
                                   FILETIME creation_time,
                                   FILETIME last_access_time,
                                   FILETIME last_write_time,
                                   DWORD file_attributes);
};

} // namespace CeWinFileCache