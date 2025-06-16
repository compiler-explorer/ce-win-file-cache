#include "include/ce-win-file-cache/hybrid_filesystem.hpp"
#include "include/ce-win-file-cache/directory_cache.hpp"
#include "include/types/config.hpp"
#include <iostream>
#include <cassert>

using namespace CeWinFileCache;

void testDirectoryCacheIntegration()
{
    std::wcout << L"=== DirectoryCache Integration Test ===" << std::endl;
    
    // Create test configuration
    Config config;
    config.global.cache_directory = L"./test_cache";
    config.global.total_cache_size_mb = 1024;
    
    // Add test compiler
    CompilerConfig msvc_config;
    msvc_config.network_path = L"./test_network_share/msvc-14.40";
    msvc_config.cache_size_mb = 512;
    msvc_config.cache_always_patterns = {L"*.exe", L"*.dll"};
    config.compilers[L"msvc-14.40"] = msvc_config;
    
    std::wcout << L"1. Testing DirectoryCache standalone..." << std::endl;
    
    // Test DirectoryCache directly
    DirectoryCache directory_cache;
    NTSTATUS init_result = directory_cache.initialize(config);
    assert(NT_SUCCESS(init_result));
    
    // Add some test files to the directory cache
    directory_cache.addTestDirectory(L"/msvc-14.40", L"./test_network_share/msvc-14.40");
    directory_cache.addTestDirectory(L"/msvc-14.40/bin", L"./test_network_share/msvc-14.40/bin");
    directory_cache.addTestFile(L"/msvc-14.40/bin/cl.exe", L"./test_network_share/msvc-14.40/bin/cl.exe", 2048576);
    directory_cache.addTestFile(L"/msvc-14.40/bin/link.exe", L"./test_network_share/msvc-14.40/bin/link.exe", 1536000);
    directory_cache.addTestDirectory(L"/msvc-14.40/include", L"./test_network_share/msvc-14.40/include");
    directory_cache.addTestFile(L"/msvc-14.40/include/iostream", L"./test_network_share/msvc-14.40/include/iostream", 4096);
    
    std::wcout << L"   Added " << directory_cache.getTotalFiles() << L" test files" << std::endl;
    std::wcout << L"   Added " << directory_cache.getTotalDirectories() << L" test directories" << std::endl;
    
    // Test that we can find nodes
    DirectoryNode *cl_exe = directory_cache.findNode(L"/msvc-14.40/bin/cl.exe");
    assert(cl_exe != nullptr);
    assert(cl_exe->isFile());
    assert(cl_exe->file_size == 2048576);
    std::wcout << L"   ✓ Found cl.exe in DirectoryCache" << std::endl;
    
    DirectoryNode *iostream_h = directory_cache.findNode(L"/msvc-14.40/include/iostream");
    assert(iostream_h != nullptr);
    assert(iostream_h->isFile());
    assert(iostream_h->file_size == 4096);
    std::wcout << L"   ✓ Found iostream header in DirectoryCache" << std::endl;
    
    // Test directory enumeration
    std::vector<DirectoryNode*> bin_contents = directory_cache.getDirectoryContents(L"/msvc-14.40/bin");
    assert(bin_contents.size() >= 2); // Should contain cl.exe and link.exe
    std::wcout << L"   ✓ Directory enumeration works, found " << bin_contents.size() << L" items in bin/" << std::endl;
    
#ifndef NO_WINFSP
    std::wcout << L"2. Testing HybridFileSystem integration..." << std::endl;
    
    // Test HybridFileSystem integration
    HybridFileSystem filesystem;
    NTSTATUS fs_init = filesystem.Initialize(config);
    assert(NT_SUCCESS(fs_init));
    
    std::wcout << L"   ✓ HybridFileSystem initialized successfully" << std::endl;
    
    // Test that getCacheEntry now finds files from DirectoryCache
    // This should trigger our new integration code path
    
    // First, verify a file that should NOT be in initial cache_entries
    CacheEntry *entry1 = filesystem.getCacheEntry(L"/msvc-14.40/bin/cl.exe");
    assert(entry1 != nullptr);
    assert(entry1->virtual_path == L"/msvc-14.40/bin/cl.exe");
    assert(entry1->network_path == L"./test_network_share/msvc-14.40/bin/cl.exe");
    assert(entry1->file_size == 2048576);
    assert(entry1->policy == CachePolicy::ALWAYS_CACHE); // *.exe should match cache_always_patterns
    std::wcout << L"   ✓ getCacheEntry() found cl.exe via DirectoryCache integration" << std::endl;
    std::wcout << L"     Virtual path: " << entry1->virtual_path << std::endl;
    std::wcout << L"     Network path: " << entry1->network_path << std::endl;
    std::wcout << L"     File size: " << entry1->file_size << L" bytes" << std::endl;
    std::wcout << L"     Cache policy: " << static_cast<int>(entry1->policy) << std::endl;
    
    // Test another file with different policy
    CacheEntry *entry2 = filesystem.getCacheEntry(L"/msvc-14.40/include/iostream");
    assert(entry2 != nullptr);
    assert(entry2->virtual_path == L"/msvc-14.40/include/iostream");
    assert(entry2->network_path == L"./test_network_share/msvc-14.40/include/iostream");
    assert(entry2->file_size == 4096);
    assert(entry2->policy == CachePolicy::ON_DEMAND); // Headers not in cache_always_patterns
    std::wcout << L"   ✓ getCacheEntry() found iostream header via DirectoryCache integration" << std::endl;
    std::wcout << L"     Cache policy: " << static_cast<int>(entry2->policy) << std::endl;
    
    // Test that subsequent calls return the same cached entry (fast path)
    CacheEntry *entry1_again = filesystem.getCacheEntry(L"/msvc-14.40/bin/cl.exe");
    assert(entry1_again == entry1); // Should be the exact same pointer
    std::wcout << L"   ✓ Subsequent getCacheEntry() calls use fast path (same pointer)" << std::endl;
    
    // Test a file that doesn't exist in DirectoryCache
    CacheEntry *entry_missing = filesystem.getCacheEntry(L"/msvc-14.40/nonexistent/file.txt");
    assert(entry_missing != nullptr); // Should still create virtual entry
    assert(entry_missing->state == FileState::VIRTUAL);
    std::wcout << L"   ✓ Non-existent files still create virtual entries (fallback works)" << std::endl;
    
    std::wcout << L"3. Testing createDynamicCacheEntry() functionality..." << std::endl;
    
    // Manually test createDynamicCacheEntry with a fresh node
    DirectoryNode *fresh_node = directory_cache.findNode(L"/msvc-14.40/bin/link.exe");
    assert(fresh_node != nullptr);
    
    CacheEntry *dynamic_entry = filesystem.createDynamicCacheEntry(fresh_node);
    assert(dynamic_entry != nullptr);
    assert(dynamic_entry->virtual_path == L"/msvc-14.40/bin/link.exe");
    assert(dynamic_entry->network_path == L"./test_network_share/msvc-14.40/bin/link.exe");
    assert(dynamic_entry->file_size == 1536000);
    assert(dynamic_entry->policy == CachePolicy::ALWAYS_CACHE); // *.exe pattern
    assert(dynamic_entry->file_attributes == FILE_ATTRIBUTE_NORMAL);
    std::wcout << L"   ✓ createDynamicCacheEntry() correctly converts DirectoryNode to CacheEntry" << std::endl;
    
#else
    std::wcout << L"2. Skipping HybridFileSystem tests (NO_WINFSP build)" << std::endl;
#endif
    
    std::wcout << L"=== Integration Test Results ===" << std::endl;
    std::wcout << L"✓ DirectoryCache initialization: PASS" << std::endl;
    std::wcout << L"✓ Node finding: PASS" << std::endl;
    std::wcout << L"✓ Directory enumeration: PASS" << std::endl;
#ifndef NO_WINFSP
    std::wcout << L"✓ HybridFileSystem integration: PASS" << std::endl;
    std::wcout << L"✓ Dynamic cache entry creation: PASS" << std::endl;
    std::wcout << L"✓ Policy determination: PASS" << std::endl;
    std::wcout << L"✓ Fast path caching: PASS" << std::endl;
    std::wcout << L"✓ Fallback for missing files: PASS" << std::endl;
#endif
    
    std::wcout << L"🎉 ALL TESTS PASSED! DirectoryCache integration is working correctly." << std::endl;
}

int main()
{
    try {
        testDirectoryCacheIntegration();
        std::wcout << L"\n✅ DirectoryCache Integration Test: SUCCESS" << std::endl;
        return 0;
    }
    catch (const std::exception& e) {
        std::wcerr << L"❌ Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
    catch (...) {
        std::wcerr << L"❌ Test failed with unknown exception" << std::endl;
        return 1;
    }
}