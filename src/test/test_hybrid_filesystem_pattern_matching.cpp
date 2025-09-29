#include <catch2/catch_test_macros.hpp>
#include <ce-win-file-cache/hybrid_filesystem.hpp>
#include <ce-win-file-cache/glob_matcher.hpp>

using namespace CeWinFileCache;

// Test helper class to access private methods
class HybridFileSystemTestHelper : public HybridFileSystem
{
public:
    // Expose the private matchesPattern method for testing
    bool testMatchesPattern(const std::wstring &path, const std::wstring &pattern)
    {
        return matchesPattern(path, pattern);
    }
};

TEST_CASE("HybridFileSystem pattern matching for directory listings", "[filesystem][pattern]")
{
    HybridFileSystemTestHelper fs;

    SECTION("Exact filename matching (Windows Explorer pattern)")
    {
        // This is the actual case from the logs - Explorer searching for specific file
        REQUIRE(fs.testMatchesPattern(L"CASTGUARD.H", L"CASTGUARD.H"));
        REQUIRE(fs.testMatchesPattern(L"castguard.h", L"CASTGUARD.H"));
        REQUIRE(fs.testMatchesPattern(L"CastGuard.h", L"CASTGUARD.H"));

        // Should not match different files
        REQUIRE_FALSE(fs.testMatchesPattern(L"otherfile.h", L"CASTGUARD.H"));
        REQUIRE_FALSE(fs.testMatchesPattern(L"castguard.cpp", L"CASTGUARD.H"));
    }

    SECTION("Common Windows Explorer patterns")
    {
        // Explorer often searches for specific files by exact name
        REQUIRE(fs.testMatchesPattern(L"stdio.h", L"stdio.h"));
        REQUIRE(fs.testMatchesPattern(L"STDIO.H", L"stdio.h"));
        REQUIRE(fs.testMatchesPattern(L"StdIo.H", L"stdio.h"));

        // Different files should not match
        REQUIRE_FALSE(fs.testMatchesPattern(L"stdlib.h", L"stdio.h"));
        REQUIRE_FALSE(fs.testMatchesPattern(L"stdio.hpp", L"stdio.h"));
    }

    SECTION("Wildcard patterns from Windows Explorer")
    {
        // Sometimes Explorer uses wildcard patterns
        REQUIRE(fs.testMatchesPattern(L"test.exe", L"*.exe"));
        REQUIRE(fs.testMatchesPattern(L"TEST.EXE", L"*.exe"));
        REQUIRE(fs.testMatchesPattern(L"MyProgram.EXE", L"*.exe"));

        REQUIRE_FALSE(fs.testMatchesPattern(L"test.dll", L"*.exe"));
        REQUIRE_FALSE(fs.testMatchesPattern(L"test", L"*.exe"));
    }

    SECTION("Header file patterns")
    {
        // Common header file patterns
        REQUIRE(fs.testMatchesPattern(L"windows.h", L"*.h"));
        REQUIRE(fs.testMatchesPattern(L"WINDOWS.H", L"*.h"));
        REQUIRE(fs.testMatchesPattern(L"iostream", L"iostream"));
        REQUIRE(fs.testMatchesPattern(L"IOSTREAM", L"iostream"));
    }

    SECTION("Case insensitive exact matching")
    {
        // Test various case combinations for exact matches
        std::vector<std::pair<std::wstring, std::wstring>> test_cases = {
            {L"file.txt", L"FILE.TXT"},
            {L"FILE.TXT", L"file.txt"},
            {L"File.Txt", L"FILE.TXT"},
            {L"MyClass.hpp", L"MYCLASS.HPP"},
            {L"CamelCase.h", L"camelcase.h"},
            {L"snake_case.cpp", L"SNAKE_CASE.CPP"}
        };

        for (const auto& [filename, pattern] : test_cases)
        {
            INFO("Testing: " << StringUtils::wideToUtf8(filename) << " should match " << StringUtils::wideToUtf8(pattern));
            REQUIRE(fs.testMatchesPattern(filename, pattern));
        }
    }

    SECTION("Edge cases")
    {
        // Empty strings
        REQUIRE(fs.testMatchesPattern(L"", L""));

        // Single character files
        REQUIRE(fs.testMatchesPattern(L"a", L"A"));
        REQUIRE(fs.testMatchesPattern(L"A", L"a"));

        // Files with numbers and special characters
        REQUIRE(fs.testMatchesPattern(L"file_v1.2.h", L"FILE_V1.2.H"));
        REQUIRE(fs.testMatchesPattern(L"test-123.cpp", L"TEST-123.CPP"));
    }
}

TEST_CASE("Direct GlobMatcher testing for Windows patterns", "[glob][windows]")
{
    SECTION("Exact filename matching should work")
    {
        // This tests the underlying GlobMatcher that matchesPattern calls
        REQUIRE(GlobMatcher::matches(L"CASTGUARD.H", L"CASTGUARD.H"));
        REQUIRE(GlobMatcher::matches(L"castguard.h", L"CASTGUARD.H"));
        REQUIRE(GlobMatcher::matches(L"CastGuard.h", L"CASTGUARD.H"));
    }

    SECTION("Wildcard patterns")
    {
        REQUIRE(GlobMatcher::matches(L"castguard.h", L"*.h"));
        REQUIRE(GlobMatcher::matches(L"CASTGUARD.H", L"*.H"));
        REQUIRE(GlobMatcher::matches(L"castguard.h", L"*.H"));
    }

    SECTION("Question mark patterns")
    {
        REQUIRE(GlobMatcher::matches(L"a.h", L"?.h"));
        REQUIRE(GlobMatcher::matches(L"A.H", L"?.h"));
        REQUIRE(GlobMatcher::matches(L"x.cpp", L"?.CPP"));
    }
}