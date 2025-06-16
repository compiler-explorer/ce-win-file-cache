#include "include/ce-win-file-cache/directory_cache.hpp"
#include "include/types/config.hpp"
#include <iostream>
#include <iomanip>

using namespace CeWinFileCache;

void printNode(DirectoryNode* node, int depth = 0) {
    if (!node) return;
    
    std::wstring indent(depth * 2, L' ');
    std::wcout << indent << L"- " << node->name << L" (" 
               << (node->isDirectory() ? L"DIR" : L"FILE") 
               << L") -> " << node->network_path << std::endl;
}

void testPathNormalization(DirectoryCache& cache) {
    std::wcout << L"\n=== Testing Path Normalization ===" << std::endl;
    
    std::vector<std::wstring> test_paths = {
        L"",
        L"/",
        L"\\",
        L"\\\\",
        L"/msvc-14.40",
        L"\\msvc-14.40",
        L"msvc-14.40",
        L"/msvc-14.40/bin",
        L"\\msvc-14.40\\bin"
    };
    
    for (const auto& path : test_paths) {
        DirectoryNode* node = cache.findNode(path);
        std::wcout << L"findNode('" << path << L"'): " 
                   << (node ? L"FOUND" : L"NOT FOUND");
        if (node) {
            std::wcout << L" -> full_path: '" << node->full_virtual_path 
                       << L"', name: '" << node->name << L"'";
        }
        std::wcout << std::endl;
    }
}

void printDirectoryContents(DirectoryCache& cache, const std::wstring& path) {
    std::wcout << L"\n=== Directory Contents: '" << path << L"' ===" << std::endl;
    
    auto contents = cache.getDirectoryContents(path);
    std::wcout << L"Found " << contents.size() << L" items" << std::endl;
    
    for (auto* node : contents) {
        printNode(node, 1);
    }
}

int main() {
    std::wcout << L"=== Enhanced DirectoryCache Debug Test ===" << std::endl;
    
    // Create test config
    Config config;
    CompilerConfig msvc_config;
    msvc_config.network_path = L"\\\\127.0.0.1\\efs\\compilers\\msvc\\14.40.33807-14.40.33811.0";
    config.compilers[L"msvc-14.40"] = msvc_config;
    
    CompilerConfig ninja_config;
    ninja_config.network_path = L"\\\\127.0.0.1\\efs\\compilers\\ninja";
    config.compilers[L"ninja"] = ninja_config;
    
    CompilerConfig kits_config;
    kits_config.network_path = L"\\\\127.0.0.1\\efs\\compilers\\windows-kits-10";
    config.compilers[L"windows-kits-10"] = kits_config;
    
    // Initialize DirectoryCache
    DirectoryCache directory_cache;
    NTSTATUS result = directory_cache.initialize(config);
    std::wcout << L"DirectoryCache initialize result: 0x" << std::hex << result << std::dec << std::endl;
    
    // Test statistics
    std::wcout << L"\n=== DirectoryCache Statistics ===" << std::endl;
    std::wcout << L"Total nodes: " << directory_cache.getTotalNodes() << std::endl;
    std::wcout << L"Total directories: " << directory_cache.getTotalDirectories() << std::endl;
    std::wcout << L"Total files: " << directory_cache.getTotalFiles() << std::endl;
    
    // Test path normalization
    testPathNormalization(directory_cache);
    
    // Test directory enumeration
    printDirectoryContents(directory_cache, L"/");
    printDirectoryContents(directory_cache, L"\\");
    printDirectoryContents(directory_cache, L"");
    
    // Test compiler directory enumeration
    printDirectoryContents(directory_cache, L"/msvc-14.40");
    printDirectoryContents(directory_cache, L"\\msvc-14.40");
    
    // Add some test files to see if enumeration works
    std::wcout << L"\n=== Adding Test Files ===" << std::endl;
    directory_cache.addTestFile(L"/msvc-14.40/cl.exe", L"\\\\127.0.0.1\\efs\\compilers\\msvc\\14.40.33807-14.40.33811.0\\cl.exe", 1024000);
    directory_cache.addTestFile(L"/ninja/ninja.exe", L"\\\\127.0.0.1\\efs\\compilers\\ninja\\ninja.exe", 512000);
    
    std::wcout << L"After adding test files:" << std::endl;
    std::wcout << L"Total nodes: " << directory_cache.getTotalNodes() << std::endl;
    std::wcout << L"Total files: " << directory_cache.getTotalFiles() << std::endl;
    
    // Re-test directory enumeration
    printDirectoryContents(directory_cache, L"/");
    printDirectoryContents(directory_cache, L"/msvc-14.40");
    
    // Test HybridFileSystem's normalizePath function directly
    std::wcout << L"\n=== Testing HybridFileSystem normalizePath ===" << std::endl;
    // Note: This would need HybridFileSystem instance to test
    
    return 0;
}