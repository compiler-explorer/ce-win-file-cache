#include "../include/ce-win-file-cache/directory_cache.hpp"
#include "../include/ce-win-file-cache/logger.hpp"
#include "../include/ce-win-file-cache/string_utils.hpp"
#include <filesystem>
#include <set>

#ifdef NO_WINFSP
#include <fstream>
#include <random>
namespace fs = std::filesystem;
#endif

namespace CeWinFileCache
{

DirectoryCache::DirectoryCache()
{
}

NTSTATUS DirectoryCache::initialize(const Config &config)
{
    return buildDirectoryTreeFromConfig(config);
}

std::vector<DirectoryNode *> DirectoryCache::getDirectoryContents(const std::wstring &virtual_path)
{
    return directory_tree.getDirectoryContents(virtual_path);
}

DirectoryNode *DirectoryCache::findNode(const std::wstring &virtual_path)
{
    std::wstring normalized_path = normalizePath(virtual_path);
    return directory_tree.findNode(normalized_path);
}

NTSTATUS DirectoryCache::buildDirectoryTreeFromConfig(const Config &config)
{
    // Add root directory entry to handle requests for "\" or "/"
    directory_tree.addDirectory(L"/", L"");

    // Build directory tree from all configured compilers
    for (const auto &[compiler_name, compiler_config] : config.compilers)
    {
        std::wstring virtual_root = std::filesystem::relative(compiler_config.network_path, compiler_config.root_path).wstring();
        virtual_root = normalizePath(L"/" + virtual_root);

        // Add compiler root directory
        directory_tree.addDirectory(virtual_root, compiler_config.network_path);

        // Enumerate network directory to build complete tree
        (void)enumerateNetworkDirectory(compiler_config.network_path, virtual_root);
        // Don't fail initialization if some network paths are inaccessible
        // Directory tree will be built on-demand during access
    }

    return STATUS_SUCCESS;
}

NTSTATUS DirectoryCache::enumerateNetworkDirectory(const std::wstring &network_path, const std::wstring &virtual_path)
{
#ifdef NO_WINFSP
    // Mock implementation for testing on macOS/Linux
    return enumerateNetworkDirectoryMock(network_path, virtual_path);
#else
    // Real Windows implementation
    return enumerateNetworkDirectoryWindows(network_path, virtual_path);
#endif
}

#ifndef NO_WINFSP
NTSTATUS DirectoryCache::enumerateNetworkDirectoryWindows(const std::wstring &network_path, const std::wstring &virtual_path)
{
    WIN32_FIND_DATAW find_data;
    std::wstring search_pattern = network_path + L"\\*";

    HANDLE find_handle = FindFirstFileW(search_pattern.c_str(), &find_data);
    if (find_handle == INVALID_HANDLE_VALUE)
    {
        // Network path not accessible - return success to continue
        return STATUS_SUCCESS;
    }

    do
    {
        // Skip . and .. entries
        if (wcscmp(find_data.cFileName, L".") == 0 || wcscmp(find_data.cFileName, L"..") == 0)
        {
            continue;
        }

        std::wstring child_name = find_data.cFileName;
        std::wstring child_virtual_path = normalizePath(virtual_path + L"/" + child_name);
        std::wstring child_network_path = network_path + L"\\" + child_name;

        if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            // Add directory to tree
            directory_tree.addDirectory(child_virtual_path, child_network_path);

            // Recursively enumerate subdirectory with cycle detection
            static thread_local std::set<std::wstring> visited_paths;
            
            // Check for circular references (junction points, symbolic links)
            if (visited_paths.find(child_network_path) == visited_paths.end())
            {
                visited_paths.insert(child_network_path);
                enumerateNetworkDirectory(child_network_path, child_virtual_path);
                visited_paths.erase(child_network_path);
            }
        }
        else
        {
            // Add file to tree
            UINT64 file_size = ((UINT64)find_data.nFileSizeHigh << 32) | find_data.nFileSizeLow;
            directory_tree.addFile(child_virtual_path, 
               child_network_path, 
               file_size, 
               find_data.ftCreationTime, 
               find_data.ftLastAccessTime, 
               find_data.ftLastWriteTime,
               find_data.dwFileAttributes);
        }

    } while (FindNextFileW(find_handle, &find_data));

    FindClose(find_handle);
    return STATUS_SUCCESS;
}
#endif

#ifdef NO_WINFSP
NTSTATUS DirectoryCache::enumerateNetworkDirectoryMock(const std::wstring &network_path, const std::wstring &virtual_path)
{
    // Mock directory structure for testing
    // Convert wstring to string for filesystem operations
    std::string narrow_network_path(network_path.begin(), network_path.end());

    // Check if path exists (for real file testing)
    if (fs::exists(narrow_network_path) && fs::is_directory(narrow_network_path))
    {
        try
        {
            for (const auto &entry : fs::directory_iterator(narrow_network_path))
            {
                std::string child_name = entry.path().filename().string();
                std::wstring wide_child_name(child_name.begin(), child_name.end());
                std::wstring child_virtual_path = virtual_path + L"/" + wide_child_name;
                std::wstring child_network_path = network_path + L"/" + wide_child_name;

                if (entry.is_directory())
                {
                    directory_tree.addDirectory(child_virtual_path, child_network_path);

                    // Recursively enumerate (with depth limit for safety)
                    static thread_local int recursion_depth = 0;
                    if (recursion_depth < 50) // Increased limit for full enumeration
                    {
                        recursion_depth++;
                        enumerateNetworkDirectory(child_network_path, child_virtual_path);
                        recursion_depth--;
                    }
                }
                else if (entry.is_regular_file())
                {
                    UINT64 file_size = fs::file_size(entry);
                    directory_tree.addFile(child_virtual_path, child_network_path, file_size, nullptr);
                }
            }
        }
        catch (const fs::filesystem_error &e)
        {
            Logger::error(LogCategory::DIRECTORY, "Filesystem error enumerating {}: {}", 
                         StringUtils::wideToUtf8(network_path), e.what());
        }
    }
    else
    {
        // Create mock structure for testing when real directories don't exist
        if (virtual_path.find(L"msvc") != std::wstring::npos)
        {
            // Mock MSVC structure
            addTestDirectory(virtual_path + L"/bin", network_path + L"/bin");
            addTestDirectory(virtual_path + L"/include", network_path + L"/include");
            addTestDirectory(virtual_path + L"/lib", network_path + L"/lib");

            addTestFile(virtual_path + L"/bin/cl.exe", network_path + L"/bin/cl.exe", 2048576); // 2MB
            addTestFile(virtual_path + L"/bin/link.exe", network_path + L"/bin/link.exe", 1536000); // 1.5MB
            addTestFile(virtual_path + L"/include/iostream", network_path + L"/include/iostream", 4096);
            addTestFile(virtual_path + L"/include/vector", network_path + L"/include/vector", 8192);
            addTestFile(virtual_path + L"/lib/msvcrt.lib", network_path + L"/lib/msvcrt.lib", 512000); // 512KB
        }
        else if (virtual_path.find(L"ninja") != std::wstring::npos)
        {
            // Mock Ninja structure
            addTestFile(virtual_path + L"/ninja.exe", network_path + L"/ninja.exe", 1024000); // 1MB
        }
    }

    return STATUS_SUCCESS;
}
#endif

size_t DirectoryCache::getTotalDirectories() const
{
    return directory_tree.getTotalDirectories();
}

size_t DirectoryCache::getTotalFiles() const
{
    return directory_tree.getTotalFiles();
}

size_t DirectoryCache::getTotalNodes() const
{
    return directory_tree.getTotalNodes();
}

void DirectoryCache::addTestFile(const std::wstring &virtual_path, const std::wstring &network_path, UINT64 size)
{
    //directory_tree.addFile(virtual_path, network_path, size, nullptr);
}

void DirectoryCache::addTestDirectory(const std::wstring &virtual_path, const std::wstring &network_path)
{
    directory_tree.addDirectory(virtual_path, network_path);
}

void DirectoryCache::clearTree()
{
    directory_tree.reset();
}

std::wstring DirectoryCache::normalizePath(const std::wstring &path)
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

} // namespace CeWinFileCache