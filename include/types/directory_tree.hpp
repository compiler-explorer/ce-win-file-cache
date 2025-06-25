#pragma once

#include <ce-win-file-cache/windows_compat.hpp>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace CeWinFileCache
{

enum class NodeType
{
    DIRECTORY,
    FILE
};

struct DirectoryNode
{
    std::wstring name{};
    std::wstring full_virtual_path{}; // e.g., "/msvc-14.40/bin/cl.exe"
    std::wstring network_path{}; // e.g., "\\server\share\msvc\14.40\bin\cl.exe"
    NodeType type;

    // File metadata
    UINT64 file_size;
    FILETIME creation_time;
    FILETIME last_access_time;
    FILETIME last_write_time;
    DWORD file_attributes;

    // Directory structure
    std::unordered_map<std::wstring, std::unique_ptr<DirectoryNode>> children;
    DirectoryNode *parent;

    DirectoryNode(const std::wstring &node_name, NodeType node_type, DirectoryNode *parent_node = nullptr)
    : name(node_name), type(node_type), file_size(0), creation_time{}, last_access_time{}, last_write_time{},
      file_attributes(0), parent(parent_node)
    {
    }

    // Helper methods
    bool isDirectory() const
    {
        return type == NodeType::DIRECTORY;
    }
    bool isFile() const
    {
        return type == NodeType::FILE;
    }

    DirectoryNode *findChild(const std::wstring &child_name)
    {
        auto it = children.find(child_name);
        return (it != children.end()) ? it->second.get() : nullptr;
    }

    DirectoryNode *addChild(const std::wstring &child_name, NodeType child_type)
    {
        auto child = std::make_unique<DirectoryNode>(child_name, child_type, this);
        DirectoryNode *result = child.get();
        children[child_name] = std::move(child);
        return result;
    }

    // Get all child names for directory enumeration
    std::vector<std::wstring> getChildNames() const
    {
        std::vector<std::wstring> names;
        names.reserve(children.size());
        for (const auto &[child_name, child] : children)
        {
            names.push_back(child_name);
        }
        return names;
    }
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
    void lock()
    {
        tree_mutex.lock();
    }
    void unlock()
    {
        tree_mutex.unlock();
    }
    std::lock_guard<std::mutex> getLock()
    {
        return std::lock_guard<std::mutex>(tree_mutex);
    }

    private:
    std::unique_ptr<DirectoryNode> root{};
    mutable std::mutex tree_mutex{};

    // Helper methods
    std::vector<std::wstring> splitPath(const std::wstring &path);
    DirectoryNode *findOrCreatePath(const std::wstring &virtual_path, bool create_missing);
    void updateNodeMetadata(DirectoryNode *node, const std::wstring &network_path, UINT64 size, const FILETIME *creation_time);
};

} // namespace CeWinFileCache