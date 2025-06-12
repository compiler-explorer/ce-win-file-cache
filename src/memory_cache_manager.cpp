#include "../include/ce-win-file-cache/memory_cache_manager.hpp"
#include <fstream>
#include <iostream>
#include <filesystem>
#include <algorithm>
#include <codecvt>
#include <locale>

namespace CeWinFileCache
{

std::vector<uint8_t> MemoryCacheManager::loadNetworkFileToMemory(const std::wstring& network_path)
{
    std::vector<uint8_t> content;
    
    try
    {
#ifdef _WIN32
        std::ifstream file(network_path, std::ios::binary | std::ios::ate);
#else
        // Convert wstring to string for non-Windows platforms
        std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
        std::string narrow_path = converter.to_bytes(network_path);
        std::ifstream file(narrow_path, std::ios::binary | std::ios::ate);
#endif
        if (!file.is_open())
        {
            std::wcerr << L"Failed to open network file: " << network_path << std::endl;
            return content;
        }
        
        std::streamsize size = file.tellg();
        file.seekg(0, std::ios::beg);
        
        content.resize(static_cast<size_t>(size));
        if (!file.read(reinterpret_cast<char*>(content.data()), size))
        {
            std::wcerr << L"Failed to read network file: " << network_path << std::endl;
            content.clear();
        }
    }
    catch (const std::exception& e)
    {
        std::wcerr << L"Exception loading network file: " << network_path << L" - " 
                   << e.what() << std::endl;
        content.clear();
    }
    
    return content;
}

bool MemoryCacheManager::isFileInMemoryCache(const std::wstring& virtual_path)
{
    std::lock_guard<std::mutex> lock(cache_mutex);
    return memory_cache.find(virtual_path) != memory_cache.end();
}

std::optional<std::vector<uint8_t>> MemoryCacheManager::getMemoryCachedFile(const std::wstring& virtual_path)
{
    std::lock_guard<std::mutex> lock(cache_mutex);
    
    auto it = memory_cache.find(virtual_path);
    if (it != memory_cache.end())
    {
        return it->second;
    }
    
    return std::nullopt;
}

void MemoryCacheManager::addFileToMemoryCache(const std::wstring& virtual_path, 
                                             const std::vector<uint8_t>& content)
{
    std::lock_guard<std::mutex> lock(cache_mutex);
    memory_cache[virtual_path] = content;
}

std::vector<uint8_t> MemoryCacheManager::getFileContent(const std::wstring& virtual_path, 
                                                       const Config& config)
{
    auto cached = getMemoryCachedFile(virtual_path);
    if (cached.has_value())
    {
        return cached.value();
    }
    
    std::wstring network_path = resolveVirtualToNetworkPath(virtual_path, config);
    if (network_path.empty())
    {
        std::wcerr << L"Failed to resolve virtual path: " << virtual_path << std::endl;
        return std::vector<uint8_t>();
    }
    
    auto content = loadNetworkFileToMemory(network_path);
    if (!content.empty())
    {
        addFileToMemoryCache(virtual_path, content);
    }
    
    return content;
}

void MemoryCacheManager::clearCache()
{
    std::lock_guard<std::mutex> lock(cache_mutex);
    memory_cache.clear();
}

size_t MemoryCacheManager::getCacheSize() const
{
    std::lock_guard<std::mutex> lock(cache_mutex);
    
    size_t total_size = 0;
    for (const auto& [path, content] : memory_cache)
    {
        total_size += content.size();
    }
    
    return total_size;
}

size_t MemoryCacheManager::getCachedFileCount() const
{
    std::lock_guard<std::mutex> lock(cache_mutex);
    return memory_cache.size();
}

std::wstring MemoryCacheManager::resolveVirtualToNetworkPath(const std::wstring& virtual_path, 
                                                            const Config& config)
{
    if (virtual_path.empty() || virtual_path[0] != L'/')
    {
        return L"";
    }
    
    size_t second_slash = virtual_path.find(L'/', 1);
    if (second_slash == std::wstring::npos)
    {
        return L"";
    }
    
    std::wstring compiler_name = virtual_path.substr(1, second_slash - 1);
    std::wstring relative_path = virtual_path.substr(second_slash + 1);
    
    auto it = config.compilers.find(compiler_name);
    if (it == config.compilers.end())
    {
        return L"";
    }
    
    std::wstring network_path = it->second.network_path;
    if (!network_path.empty() && network_path.back() != L'\\')
    {
        network_path += L'\\';
    }
    
    std::replace(relative_path.begin(), relative_path.end(), L'/', L'\\');
    
    return network_path + relative_path;
}

} // namespace CeWinFileCache