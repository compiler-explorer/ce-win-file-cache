#include "../include/types/directory_tree.hpp"
#include <algorithm>
#include <functional>
#include <ranges>
#include <sstream>

namespace CeWinFileCache
{

// DirectoryNode implementations
DirectoryNode::DirectoryNode(std::wstring node_name, NodeType node_type, DirectoryNode *parent_node)
: name(std::move(node_name)), type(node_type), creation_time{}, last_access_time{}, last_write_time{}, parent(parent_node)
{
}

bool DirectoryNode::isDirectory() const
{
    return type == NodeType::DIRECTORY;
}

bool DirectoryNode::isFile() const
{
    return type == NodeType::FILE;
}

DirectoryNode *DirectoryNode::findChild(const std::wstring &child_name)
{
    std::lock_guard<std::mutex> lock(children_mutex);
    auto child_it = children.find(child_name);
    return (child_it != children.end()) ? child_it->second.get() : nullptr;
}

DirectoryNode *DirectoryNode::addChild(const std::wstring &child_name, NodeType child_type)
{
    std::lock_guard<std::mutex> lock(children_mutex);
    auto child = std::make_unique<DirectoryNode>(child_name, child_type, this);
    DirectoryNode *result = child.get();
    children[child_name] = std::move(child);
    return result;
}

std::vector<std::wstring> DirectoryNode::getChildNames() const
{
    std::lock_guard<std::mutex> lock(children_mutex);
    std::vector<std::wstring> names;
    names.reserve(children.size());
    for (const auto &[child_name, child] : children)
    {
        names.push_back(child_name);
    }
    return names;
}

std::vector<DirectoryNode *> DirectoryNode::getChildNodes() const
{
    std::lock_guard<std::mutex> lock(children_mutex);
    std::vector<DirectoryNode *> nodes;
    nodes.reserve(children.size());
    for (const auto &[child_name, child] : children)
    {
        nodes.push_back(child.get());
    }
    return nodes;
}

// DirectoryTree implementations
DirectoryTree::DirectoryTree() : root(std::make_unique<DirectoryNode>(L"", NodeType::DIRECTORY))
{
    root->full_virtual_path = L"/";
}

DirectoryNode *DirectoryTree::findNode(const std::wstring &virtual_path)
{
    std::lock_guard<std::mutex> tree_lock(tree_mutex);
    return findOrCreatePath(virtual_path, false);
}

DirectoryNode *DirectoryTree::createPath(const std::wstring &virtual_path, NodeType /*type*/)
{
    std::lock_guard<std::mutex> tree_lock(tree_mutex);
    return findOrCreatePath(virtual_path, true);
}

bool DirectoryTree::addFile(const std::wstring &virtual_path, const std::wstring &network_path, UINT64 size, const FILETIME *creation_time, DWORD file_attributes)
{
    std::lock_guard<std::mutex> tree_lock(tree_mutex);

    DirectoryNode *node = findOrCreatePath(virtual_path, true);
    if (node == nullptr)
    {
        return false;
    }

    node->type = NodeType::FILE;
    updateNodeMetadata(node, network_path, size, creation_time, file_attributes);

    return true;
}

bool DirectoryTree::addDirectory(const std::wstring &virtual_path, const std::wstring &network_path)
{
    std::lock_guard<std::mutex> tree_lock(tree_mutex);

    DirectoryNode *node = findOrCreatePath(virtual_path, true);
    if (node == nullptr)
    {
        return false;
    }

    node->type = NodeType::DIRECTORY;
    node->network_path = network_path;
    node->file_attributes = FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_OFFLINE;

    return true;
}

std::vector<DirectoryNode *> DirectoryTree::getDirectoryContents(const std::wstring &virtual_path)
{
    std::lock_guard<std::mutex> tree_lock(tree_mutex);

    DirectoryNode *dir_node = findOrCreatePath(virtual_path, false);
    if ((dir_node == nullptr) || !dir_node->isDirectory())
    {
        return {};
    }

    // Use DirectoryNode's thread-safe method to get child nodes
    std::vector<DirectoryNode *> contents = dir_node->getChildNodes();

    // Sort for consistent enumeration order
    std::ranges::sort(contents,
                      [](const DirectoryNode *first, const DirectoryNode *second)
                      {
                          return first->name < second->name;
                      });

    return contents;
}

size_t DirectoryTree::getTotalNodes() const
{
    // Implementation for statistics - could be optimized with counters
    return getTotalDirectories() + getTotalFiles();
}

size_t DirectoryTree::getTotalDirectories() const
{
    std::lock_guard<std::mutex> tree_lock(tree_mutex);

    size_t count = 0;
    std::function<void(const DirectoryNode *)> countDirs = [&](const DirectoryNode *node)
    {
        if (node->isDirectory())
        {
            count++;
        }
        for (const auto &[child_name, child] : node->children)
        {
            countDirs(child.get());
        }
    };

    countDirs(root.get());
    return count;
}

size_t DirectoryTree::getTotalFiles() const
{
    std::lock_guard<std::mutex> tree_lock(tree_mutex);

    size_t count = 0;
    std::function<void(const DirectoryNode *)> countFiles = [&](const DirectoryNode *node)
    {
        if (node->isFile())
        {
            count++;
        }
        for (const auto &[child_name, child] : node->children)
        {
            countFiles(child.get());
        }
    };

    countFiles(root.get());
    return count;
}

void DirectoryTree::reset()
{
    std::lock_guard<std::mutex> tree_lock(tree_mutex);
    root = std::make_unique<DirectoryNode>(L"", NodeType::DIRECTORY);
    root->full_virtual_path = L"/";
}

std::vector<std::wstring> DirectoryTree::splitPath(const std::wstring &path)
{
    std::vector<std::wstring> components;

    if (path.empty() || path == L"/")
    {
        return components;
    }

    std::wstring normalized_path = path;

    // Remove leading slash (path is guaranteed non-empty here)
    if (normalized_path[0] == L'/')
    {
        normalized_path = normalized_path.substr(1);
    }

    // Remove trailing slash
    if (!normalized_path.empty() && normalized_path.back() == L'/')
    {
        normalized_path.pop_back();
    }

    // Split by '/'
    size_t start = 0;
    size_t pos = 0;

    while ((pos = normalized_path.find(L'/', start)) != std::wstring::npos)
    {
        if (pos > start) // Skip empty components
        {
            components.push_back(normalized_path.substr(start, pos - start));
        }
        start = pos + 1;
    }

    // Add final component
    if (start < normalized_path.length())
    {
        components.push_back(normalized_path.substr(start));
    }

    return components;
}

DirectoryNode *DirectoryTree::findOrCreatePath(const std::wstring &virtual_path, bool create_missing)
{
    if (virtual_path.empty() || virtual_path == L"/")
    {
        return root.get();
    }

    auto components = splitPath(virtual_path);
    DirectoryNode *current = root.get();
    std::wstring current_path = L"/";

    for (const auto &component : components)
    {
        DirectoryNode *child = current->findChild(component);

        if (child == nullptr)
        {
            if (!create_missing)
            {
                return nullptr; // Path doesn't exist and we're not creating
            }

            // Create missing node as directory by default
            child = current->addChild(component, NodeType::DIRECTORY);
        }

        // Update full virtual path
        if (current_path != L"/")
        {
            current_path += L"/";
        }
        current_path += component;
        child->full_virtual_path = current_path;

        current = child;
    }

    return current;
}

void DirectoryTree::updateNodeMetadata(DirectoryNode *node, const std::wstring &network_path, UINT64 size, const FILETIME *creation_time, DWORD file_attributes)
{
    if (node == nullptr)
    {
        return;
    }

    node->network_path = network_path;
    node->file_size = size;

    if (creation_time != nullptr)
    {
        node->creation_time = *creation_time;
        node->last_access_time = *creation_time;
        node->last_write_time = *creation_time;
    }
    else
    {
        // Use current time
        FILETIME current_time{};
        GetSystemTimeAsFileTime(&current_time);
        node->creation_time = current_time;
        node->last_access_time = current_time;
        node->last_write_time = current_time;
    }

    // Set file attributes from the actual file system
    node->file_attributes = file_attributes;
}

void DirectoryTree::lock()
{
    tree_mutex.lock();
}

void DirectoryTree::unlock()
{
    tree_mutex.unlock();
}

std::lock_guard<std::mutex> DirectoryTree::getLock()
{
    return std::lock_guard<std::mutex>(tree_mutex);
}

} // namespace CeWinFileCache