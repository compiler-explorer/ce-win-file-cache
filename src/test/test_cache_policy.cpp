#include <catch2/catch_test_macros.hpp>
#include <ce-win-file-cache/hybrid_filesystem.hpp>
#include <ce-win-file-cache/config_parser.hpp>

using namespace CeWinFileCache;

TEST_CASE("determineCachePolicy - simple compiler paths", "[cache][policy]")
{
    HybridFileSystem fs;
    Config test_config;

    // Setup simple config with single-level compiler names
    CompilerConfig msvc_config;
    msvc_config.network_path = L"\\\\server\\msvc-14.40";
    msvc_config.cache_always_patterns = {L"bin/Hostx64/x64/*.exe", L"bin/Hostx64/x64/*.dll"};

    test_config.compilers[L"msvc-14.40"] = msvc_config;
    fs.testSetConfig(test_config);

    SECTION("Files matching ALWAYS_CACHE patterns")
    {
        REQUIRE(fs.testDetermineCachePolicy(L"/msvc-14.40/bin/Hostx64/x64/cl.exe") == CachePolicy::ALWAYS_CACHE);
        REQUIRE(fs.testDetermineCachePolicy(L"/msvc-14.40/bin/Hostx64/x64/link.exe") == CachePolicy::ALWAYS_CACHE);
        REQUIRE(fs.testDetermineCachePolicy(L"/msvc-14.40/bin/Hostx64/x64/mspdbcore.dll") == CachePolicy::ALWAYS_CACHE);
    }

    SECTION("Files in compiler path but not matching ALWAYS_CACHE patterns")
    {
        REQUIRE(fs.testDetermineCachePolicy(L"/msvc-14.40/include/stdio.h") == CachePolicy::ON_DEMAND);
        REQUIRE(fs.testDetermineCachePolicy(L"/msvc-14.40/lib/msvcrt.lib") == CachePolicy::ON_DEMAND);
    }

    SECTION("Files not in any compiler path")
    {
        REQUIRE(fs.testDetermineCachePolicy(L"/unknown/file.txt") == CachePolicy::NEVER_CACHE);
        REQUIRE(fs.testDetermineCachePolicy(L"/other-compiler/bin/test.exe") == CachePolicy::NEVER_CACHE);
    }

    SECTION("Edge cases")
    {
        REQUIRE(fs.testDetermineCachePolicy(L"") == CachePolicy::NEVER_CACHE);
        REQUIRE(fs.testDetermineCachePolicy(L"no-leading-slash") == CachePolicy::NEVER_CACHE);
        REQUIRE(fs.testDetermineCachePolicy(L"/") == CachePolicy::NEVER_CACHE);
        REQUIRE(fs.testDetermineCachePolicy(L"/msvc-14.40") == CachePolicy::ON_DEMAND); // directory itself - ON_DEMAND is valid
    }
}

TEST_CASE("determineCachePolicy - multi-level compiler paths (real world)", "[cache][policy]")
{
    HybridFileSystem fs;
    Config test_config;

    // Setup config with real-world multi-level paths
    CompilerConfig msvc_config;
    msvc_config.network_path = L"\\\\127.0.0.1\\efs\\compilers\\msvc\\14.40.33807-14.40.33811.0";
    msvc_config.cache_always_patterns = {L"bin/**/*.exe", L"bin/**/*.dll", L"lib/x64/*.lib"};

    CompilerConfig mingw_config;
    mingw_config.network_path = L"\\\\127.0.0.1\\efs\\compilers\\mingw-w64-12.2.0-16.0.0-10.0.0-ucrt-r5";
    mingw_config.cache_always_patterns = {L"bin/*.exe", L"bin/*.dll"};

    CompilerConfig winkits_config;
    winkits_config.network_path = L"\\\\127.0.0.1\\efs\\compilers\\windows-kits-10";
    winkits_config.cache_always_patterns = {L"Lib/**/*.lib"};

    test_config.compilers[L"compilers/msvc/14.40.33807-14.40.33811.0"] = msvc_config;
    test_config.compilers[L"compilers/mingw-w64-12.2.0-16.0.0-10.0.0-ucrt-r5"] = mingw_config;
    test_config.compilers[L"compilers/windows-kits-10"] = winkits_config;

    fs.testSetConfig(test_config);

    SECTION("MSVC files with ALWAYS_CACHE patterns")
    {
        REQUIRE(fs.testDetermineCachePolicy(L"/compilers/msvc/14.40.33807-14.40.33811.0/bin/Hostx64/x64/cl.exe") == CachePolicy::ALWAYS_CACHE);
        REQUIRE(fs.testDetermineCachePolicy(L"/compilers/msvc/14.40.33807-14.40.33811.0/bin/Hostx64/x64/c1xx.dll") == CachePolicy::ALWAYS_CACHE);
        REQUIRE(fs.testDetermineCachePolicy(L"/compilers/msvc/14.40.33807-14.40.33811.0/lib/x64/msvcrt.lib") == CachePolicy::ALWAYS_CACHE);
    }

    SECTION("MSVC files with ON_DEMAND (not in ALWAYS_CACHE patterns)")
    {
        REQUIRE(fs.testDetermineCachePolicy(L"/compilers/msvc/14.40.33807-14.40.33811.0/include/stdio.h") == CachePolicy::ON_DEMAND);
        REQUIRE(fs.testDetermineCachePolicy(L"/compilers/msvc/14.40.33807-14.40.33811.0/data/something.txt") == CachePolicy::ON_DEMAND);
    }

    SECTION("MinGW files with ALWAYS_CACHE patterns")
    {
        REQUIRE(fs.testDetermineCachePolicy(L"/compilers/mingw-w64-12.2.0-16.0.0-10.0.0-ucrt-r5/bin/cmake.exe") == CachePolicy::ALWAYS_CACHE);
        REQUIRE(fs.testDetermineCachePolicy(L"/compilers/mingw-w64-12.2.0-16.0.0-10.0.0-ucrt-r5/bin/libstdc++-6.dll") == CachePolicy::ALWAYS_CACHE);
    }

    SECTION("MinGW files with ON_DEMAND")
    {
        REQUIRE(fs.testDetermineCachePolicy(L"/compilers/mingw-w64-12.2.0-16.0.0-10.0.0-ucrt-r5/share/cmake-3.26/Modules/Compiler/GNU-C-DetermineCompiler.cmake") == CachePolicy::ON_DEMAND);
    }

    SECTION("Windows Kits files")
    {
        REQUIRE(fs.testDetermineCachePolicy(L"/compilers/windows-kits-10/Lib/10.0.22621.0/um/x64/kernel32.Lib") == CachePolicy::ALWAYS_CACHE);
        REQUIRE(fs.testDetermineCachePolicy(L"/compilers/windows-kits-10/Include/10.0.22621.0/um/windows.h") == CachePolicy::ON_DEMAND);
    }

    SECTION("Files not in any configured compiler path")
    {
        REQUIRE(fs.testDetermineCachePolicy(L"/compilers/unknown-compiler/bin/test.exe") == CachePolicy::NEVER_CACHE);
        REQUIRE(fs.testDetermineCachePolicy(L"/compilers/msvc/other-version/bin/cl.exe") == CachePolicy::NEVER_CACHE);
        REQUIRE(fs.testDetermineCachePolicy(L"/other/path/file.txt") == CachePolicy::NEVER_CACHE);
    }

    SECTION("Path boundary checks - ensure exact matching")
    {
        // Should not match "/compilers/msvc/14.40.33807" when we have "/compilers/msvc/14.40.33807-14.40.33811.0"
        REQUIRE(fs.testDetermineCachePolicy(L"/compilers/msvc/14.40.33807/bin/cl.exe") == CachePolicy::NEVER_CACHE);

        // Should match exact prefix
        REQUIRE(fs.testDetermineCachePolicy(L"/compilers/msvc/14.40.33807-14.40.33811.0/bin/cl.exe") == CachePolicy::ALWAYS_CACHE);
    }
}

TEST_CASE("determineCachePolicy - overlapping compiler paths", "[cache][policy]")
{
    HybridFileSystem fs;
    Config test_config;

    // Test case where one compiler path is a prefix of another
    CompilerConfig short_path;
    short_path.network_path = L"\\\\server\\compilers";
    short_path.cache_always_patterns = {L"*.txt"};

    CompilerConfig long_path;
    long_path.network_path = L"\\\\server\\compilers\\msvc";
    long_path.cache_always_patterns = {L"bin/*.exe"};

    test_config.compilers[L"compilers"] = short_path;
    test_config.compilers[L"compilers/msvc"] = long_path;

    fs.testSetConfig(test_config);

    SECTION("Longest match should win")
    {
        // Should match the longer, more specific path
        REQUIRE(fs.testDetermineCachePolicy(L"/compilers/msvc/bin/cl.exe") == CachePolicy::ALWAYS_CACHE);

        // Should match the shorter path
        REQUIRE(fs.testDetermineCachePolicy(L"/compilers/test.txt") == CachePolicy::ALWAYS_CACHE);
        REQUIRE(fs.testDetermineCachePolicy(L"/compilers/other.txt") == CachePolicy::ALWAYS_CACHE);
    }

    SECTION("Ensure path boundary matching")
    {
        // "/compilers/msvc123" should NOT match "/compilers/msvc"
        REQUIRE(fs.testDetermineCachePolicy(L"/compilers/msvc123/bin/cl.exe") == CachePolicy::ON_DEMAND); // matches "compilers" only
    }
}

TEST_CASE("determineCachePolicy - glob pattern matching", "[cache][policy][glob]")
{
    HybridFileSystem fs;
    Config test_config;

    CompilerConfig compiler;
    compiler.network_path = L"\\\\server\\compiler";
    compiler.cache_always_patterns = {
        L"bin/**/*.exe",           // Recursive match
        L"lib/x64/*.lib",          // Single-level wildcard
        L"include/**/*.h",         // Recursive header files
        L"specific/file.txt"       // Exact file
    };

    test_config.compilers[L"compiler"] = compiler;
    fs.testSetConfig(test_config);

    SECTION("Recursive glob patterns")
    {
        REQUIRE(fs.testDetermineCachePolicy(L"/compiler/bin/cl.exe") == CachePolicy::ALWAYS_CACHE);
        REQUIRE(fs.testDetermineCachePolicy(L"/compiler/bin/subdir/tool.exe") == CachePolicy::ALWAYS_CACHE);
        REQUIRE(fs.testDetermineCachePolicy(L"/compiler/bin/deep/nested/path/prog.exe") == CachePolicy::ALWAYS_CACHE);
    }

    SECTION("Single-level wildcard patterns")
    {
        REQUIRE(fs.testDetermineCachePolicy(L"/compiler/lib/x64/msvcrt.lib") == CachePolicy::ALWAYS_CACHE);
        REQUIRE(fs.testDetermineCachePolicy(L"/compiler/lib/x64/ucrt.lib") == CachePolicy::ALWAYS_CACHE);

        // Should NOT match nested paths
        REQUIRE(fs.testDetermineCachePolicy(L"/compiler/lib/x64/subdir/nested.lib") == CachePolicy::ON_DEMAND);
    }

    SECTION("Exact file matching")
    {
        REQUIRE(fs.testDetermineCachePolicy(L"/compiler/specific/file.txt") == CachePolicy::ALWAYS_CACHE);
        REQUIRE(fs.testDetermineCachePolicy(L"/compiler/specific/other.txt") == CachePolicy::ON_DEMAND);
    }

    SECTION("Non-matching patterns default to ON_DEMAND")
    {
        REQUIRE(fs.testDetermineCachePolicy(L"/compiler/other/file.txt") == CachePolicy::ON_DEMAND);
        REQUIRE(fs.testDetermineCachePolicy(L"/compiler/bin/script.bat") == CachePolicy::ON_DEMAND);
    }
}
