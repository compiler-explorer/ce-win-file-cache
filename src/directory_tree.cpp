#include "../include/types/directory_tree.hpp"
#include <algorithm>
#include <cassert>
#include <filesystem>
#include <functional>
#include <optional>
#include <ranges>
#include <sstream>
#include "../include/types/config.hpp"
#include "../include/ce-win-file-cache/string_utils.hpp"

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
    std::wstring name = child_name;
    if (!loaded_config.global.case_sensitive)
    {
        StringUtils::toLower(name);
    }

    std::lock_guard<std::mutex> lock(children_mutex);
    auto child_it = children.find(name);
    return (child_it != children.end()) ? child_it->second.get() : nullptr;
}

DirectoryNode *DirectoryNode::addChild(const std::wstring &child_name, NodeType child_type)
{
    std::wstring name = child_name;
    if (!loaded_config.global.case_sensitive)
    {
        StringUtils::toLower(name);
    }

    std::lock_guard<std::mutex> lock(children_mutex);
    auto child = std::make_unique<DirectoryNode>(child_name, child_type, this);

    children[name] = std::move(child);
    return children[name].get();
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

std::wstring DirectoryNode::normalizePath(const std::wstring &path)
{
    if (path.empty())
    {
        return L"/";
    }

    std::wstring normalized = path;

    // Convert backslashes to forward slashes for consistent storage
    for (auto &ch : normalized)
    {
        if (ch == L'\\')
        {
            ch = L'/';
        }
    }

    // Ensure path starts with /
    if (normalized[0] != L'/')
    {
        normalized = L"/" + normalized;
    }

    // Handle root path specially
    if (normalized == L"/" || normalized == L"\\")
    {
        return L"/";
    }

    // Remove trailing slash (except for root)
    if (normalized.length() > 1 && normalized.back() == L'/')
    {
        normalized.pop_back();
    }

    return normalized;
}

std::wstring DirectoryNode::normalizeUNCPath(const std::wstring &path)
{
    assert(!path.empty());

    std::wstring normalized = path;

    // Convert backslashes to forward slashes for consistent storage
    for (auto &ch : normalized)
    {
        if (ch == L'/')
        {
            ch = L'\\';
        }
    }

    // Remove trailing slash (except for root)
    if (normalized.length() > 1 && normalized.back() == L'\\')
    {
        normalized.pop_back();
    }

    return normalized;
}

// DirectoryTree implementations
DirectoryTree::DirectoryTree() : root(std::make_unique<DirectoryNode>(L"", NodeType::DIRECTORY))
{
    this->reset();
}

void DirectoryTree::init(const std::wstring &network_path)
{
    if (this->base_network_path.empty())
    {
        this->base_network_path = network_path;
    }
    else
    {
        assert(network_path == this->base_network_path);
    }
}

DirectoryNode *DirectoryTree::findNode(const std::wstring &virtual_path)
{
    std::wstring path = virtual_path;
    if (!loaded_config.global.case_sensitive)
    {
        StringUtils::toLower(path);
    }

    std::lock_guard<std::mutex> tree_lock(tree_mutex);
    return findOrCreatePath(path, NodeType::UNKNOWN, false);
}

bool DirectoryTree::addFile(const std::wstring &virtual_path,
                            const std::wstring &network_path,
                            UINT64 size,
                            FILETIME creation_time,
                            FILETIME last_access_time,
                            FILETIME last_write_time,
                            DWORD file_attributes)
{
    std::lock_guard<std::mutex> tree_lock(tree_mutex);

    DirectoryNode *node = findOrCreatePath(virtual_path, NodeType::FILE, true);
    if (node == nullptr)
    {
        return false;
    }

    updateNodeMetadata(node, network_path, size, creation_time, last_access_time, last_write_time, file_attributes);

    return true;
}

bool DirectoryTree::addDirectory(const std::wstring &virtual_path, const std::wstring &network_path)
{
    std::lock_guard<std::mutex> tree_lock(tree_mutex);

    DirectoryNode *node = findOrCreatePath(virtual_path, NodeType::DIRECTORY, true);
    if (node == nullptr)
    {
        return false;
    }

    updateNodeMetadata(node, network_path, 0, { 0, 0 }, { 0, 0 }, { 0, 0 }, FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_OFFLINE);

    return true;
}

std::vector<DirectoryNode *> DirectoryTree::getDirectoryContents(const std::wstring &virtual_path)
{
    std::lock_guard<std::mutex> tree_lock(tree_mutex);

    DirectoryNode *dir_node = findOrCreatePath(virtual_path, NodeType::DIRECTORY, false);
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
    root->file_attributes = FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_OFFLINE;
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

DirectoryNode *DirectoryTree::findOrCreatePath(const std::wstring &virtual_path, NodeType node_type, bool create_missing)
{
    if (virtual_path.empty() || virtual_path == L"/")
    {
        if (root->SecDesc == nullptr)
        {
            std::filesystem::path fs_path = std::filesystem::path(base_network_path);
            const auto combined = fs_path.generic_wstring();
            root->network_path = DirectoryNode::normalizeUNCPath(combined);

            root->full_virtual_path = L"/";

            PSECURITY_DESCRIPTOR LocalSecDesc = nullptr;
            DWORD ret = GetNamedSecurityInfo(root->network_path.c_str(), SE_FILE_OBJECT,
                                             OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION,
                                             nullptr, nullptr, nullptr, nullptr, &LocalSecDesc);
            if (ret == ERROR_SUCCESS)
            {
                size_t len = GetSecurityDescriptorLength(LocalSecDesc);
                root->SecDesc = malloc(len);
                if (root->SecDesc != nullptr)
                {
                    memcpy(root->SecDesc, LocalSecDesc, len);
                }
                LocalFree(LocalSecDesc);
            }
        }
        return root.get();
    }

    auto components = splitPath(virtual_path);
    DirectoryNode *current = root.get();
    std::wstring current_path = L"";
    current_path.reserve(virtual_path.length());

    size_t len = components.size() - 1;
    int idx = 0;
    for (const auto &component : components)
    {
        DirectoryNode *child = current->findChild(component);

        if (!current_path.empty())
        {
            current_path += L"/";
        }
        current_path += component;

        if (child == nullptr)
        {
            if (!create_missing)
            {
                return nullptr; // Path doesn't exist and we're not creating
            }

            if ((idx < len) || (node_type == NodeType::DIRECTORY))
            {
                child = current->addChild(component, NodeType::DIRECTORY);
                child->file_attributes = FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_OFFLINE;
            }
            else
            {
                child = current->addChild(component, node_type);
                child->file_attributes = FILE_ATTRIBUTE_NORMAL;
            }

            std::filesystem::path fs_path = std::filesystem::path(base_network_path) / current_path;
            const auto combined = fs_path.generic_wstring();
            child->network_path = DirectoryNode::normalizeUNCPath(combined);

            child->full_virtual_path = L"/" + current_path;

            PSECURITY_DESCRIPTOR LocalSecDesc = nullptr;
            DWORD ret = GetNamedSecurityInfo(child->network_path.c_str(), SE_FILE_OBJECT,
                                             OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION,
                                             nullptr, nullptr, nullptr, nullptr, &LocalSecDesc);
            if (ret == ERROR_SUCCESS)
            {
                size_t desc_len = GetSecurityDescriptorLength(LocalSecDesc);
                child->SecDesc = malloc(desc_len);
                if (child->SecDesc != nullptr)
                {
                    memcpy(child->SecDesc, LocalSecDesc, desc_len);
                }
                LocalFree(LocalSecDesc);
            }
        }

        current = child;
        ++idx;
    }

    return current;
}

void DirectoryTree::updateNodeMetadata(DirectoryNode *node,
                                       const std::wstring &network_path,
                                       UINT64 size,
                                       FILETIME creation_time,
                                       FILETIME last_access_time,
                                       FILETIME last_write_time,
                                       DWORD file_attributes)
{
    if (node == nullptr)
    {
        return;
    }

    node->network_path = network_path;
    node->file_size = size;

    node->creation_time = creation_time;
    node->last_access_time = last_access_time;
    node->last_write_time = last_write_time;

    node->file_attributes = file_attributes;
}

std::lock_guard<std::mutex> DirectoryTree::getLock()
{
    return std::lock_guard<std::mutex>(tree_mutex);
}

} // namespace CeWinFileCache