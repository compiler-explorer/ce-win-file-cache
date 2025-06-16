#include "include/ce-win-file-cache/directory_cache.hpp"
#include "include/types/config.hpp"
#include <iostream>

using namespace CeWinFileCache;

int main() {
    std::wcout << L"=== DirectoryCache Debug Test ===" << std::endl;
    
    // Create test config similar to what's being used
    Config config;
    CompilerConfig msvc_config;
    msvc_config.network_path = L"\\\\127.0.0.1\\efs\\compilers\\msvc\\14.40.33807-14.40.33811.0";
    config.compilers[L"msvc-14.40"] = msvc_config;
    
    CompilerConfig ninja_config;
    ninja_config.network_path = L"\\\\127.0.0.1\\efs\\compilers\\ninja";
    config.compilers[L"ninja"] = ninja_config;
    
    // Initialize DirectoryCache
    DirectoryCache directory_cache;
    NTSTATUS result = directory_cache.initialize(config);
    std::wcout << L"DirectoryCache initialize result: 0x" << std::hex << result << std::endl;
    
    // Test what paths were actually created
    std::wcout << L"\nTesting paths that should exist:" << std::endl;
    
    std::vector<std::wstring> test_paths = {
        L"\\",           // Root (what getCacheEntry is looking for)
        L"/",            // Root with forward slash
        L"\\msvc-14.40", // Compiler root (backslash)
        L"/msvc-14.40",  // Compiler root (forward slash)
        L"\\ninja",      // Ninja (backslash)  
        L"/ninja"        // Ninja (forward slash)
    };
    
    for (const auto& path : test_paths) {
        DirectoryNode* node = directory_cache.findNode(path);
        std::wcout << L"findNode('" << path << L"'): " << (node ? L"FOUND" : L"NOT FOUND") << std::endl;
        if (node) {
            std::wcout << L"  -> network_path: " << node->network_path << std::endl;
            std::wcout << L"  -> type: " << (node->isDirectory() ? L"DIRECTORY" : L"FILE") << std::endl;
        }
    }
    
    // Check statistics
    std::wcout << L"\nDirectoryCache statistics:" << std::endl;
    std::wcout << L"Total nodes: " << directory_cache.getTotalNodes() << std::endl;
    std::wcout << L"Total directories: " << directory_cache.getTotalDirectories() << std::endl;
    std::wcout << L"Total files: " << directory_cache.getTotalFiles() << std::endl;
    
    return 0;
}