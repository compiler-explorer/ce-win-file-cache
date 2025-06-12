#include "include/ce-win-file-cache/memory_cache_manager.hpp"
#include <iostream>
#include <filesystem>

using namespace CeWinFileCache;

int main() {
    MemoryCacheManager cache;
    
    std::vector<std::wstring> files = {
        L"/Users/patrickquist/Documents/terraform_1.11.4_darwin_arm64/LICENSE.txt",
        L"/Users/patrickquist/Documents/terraform_1.11.4_darwin_arm64/terraform"
    };
    
    for (const auto& path : files) {
        std::string narrow_path(path.begin(), path.end());
        auto disk_size = std::filesystem::file_size(narrow_path);
        
        auto content = cache.loadNetworkFileToMemory(path);
        cache.addFileToMemoryCache(path, content);
        
        std::cout << "File: " << narrow_path << std::endl;
        std::cout << "  Disk size: " << disk_size << " bytes" << std::endl;
        std::cout << "  Loaded: " << content.size() << " bytes" << std::endl;
        std::cout << "  Match: " << (disk_size == content.size() ? "YES" : "NO") << std::endl;
        std::cout << std::endl;
    }
    
    std::cout << "Total cache size: " << cache.getCacheSize() << " bytes" << std::endl;
    std::cout << "Expected total: " << (88937248 + 4922) << " bytes" << std::endl;
    
    return 0;
}