#include <ce-win-file-cache/glob_matcher.hpp>
#include <iostream>
#include <string>
#include <vector>

using namespace CeWinFileCache;

struct GlobTestCase
{
    std::wstring path;
    std::wstring pattern;
    bool expected;
    std::string description;
};

void runGlobTests()
{
    std::vector<GlobTestCase> test_cases = {
        // Basic wildcard tests
        { L"test.exe", L"*.exe", true, "Basic * wildcard" },
        { L"test.dll", L"*.exe", false, "Basic * wildcard - no match" },
        { L"cl.exe", L"cl.*", true, "* at end" },
        { L"library.dll", L"lib*", true, "* at end with prefix" },
        { L"notlibrary.dll", L"lib*", false, "* at end with prefix - no match" },

        // Single character wildcard tests
        { L"test1.exe", L"test?.exe", true, "Single ? wildcard" },
        { L"test.exe", L"test?.exe", false, "Single ? wildcard - too short" },
        { L"test12.exe", L"test?.exe", false, "Single ? wildcard - too long" },
        { L"a.txt", L"?.txt", true, "Single ? wildcard only" },

        // Path separator handling
        { L"bin/cl.exe", L"bin/*.exe", true, "Path with separator" },
        { L"bin\\cl.exe", L"bin/*.exe", true, "Backslash normalized to forward slash" },
        { L"bin/sub/cl.exe", L"bin/*.exe", false, "* should not cross directories" },
        { L"bin/cl.exe", L"bin\\*.exe", true, "Pattern with backslash" },

        // Recursive wildcard tests
        { L"include/stdio.h", L"include/**/*.h", true, "** recursive wildcard" },
        { L"include/sys/types.h", L"include/**/*.h", true, "** multiple directories" },
        { L"include/nested/deep/header.h", L"include/**/*.h", true, "** deep nesting" },
        { L"stdio.h", L"include/**/*.h", false, "** no match without prefix" },
        { L"include/readme.txt", L"include/**/*.h", false, "** wrong extension" },

        // Complex patterns
        { L"bin/Hostx64/x64/cl.exe", L"bin/Hostx64/x64/*.exe", true, "Real compiler path" },
        { L"bin/Hostx64/x64/link.exe", L"bin/**/*.exe", true, "Recursive match compiler" },
        { L"include/ucrt/stdio.h", L"include/**/*.h", true, "Include header recursive" },
        { L"lib/x64/kernel32.lib", L"lib/**/*.lib", true, "Library recursive" },

        // Edge cases
        { L"", L"*", true, "Empty path matches *" },
        { L"test", L"", false, "Non-empty path doesn't match empty pattern" },
        { L"", L"", true, "Empty path matches empty pattern" },
        { L"a/b/c", L"**", true, "** matches everything" },
        { L"test.exe", L"**/*.exe", true, "** at start" },

    // Case sensitivity (should be case-insensitive on Windows, sensitive on Unix)
#ifdef _WIN32
        { L"Test.EXE", L"*.exe", true, "Case insensitive on Windows" },
        { L"BIN/CL.EXE", L"bin/*.exe", true, "Directory case insensitive on Windows" },
#else
        { L"Test.EXE", L"*.exe", false, "Case sensitive on Unix" },
        { L"BIN/CL.EXE", L"bin/*.exe", false, "Directory case sensitive on Unix" },
#endif

        // Real configuration patterns from compilers.json
        { L"bin/Hostx64/x64/cl.exe", L"bin/Hostx64/x64/*.exe", true, "MSVC compiler" },
        { L"bin/Hostx64/x64/mspdb140.dll", L"bin/Hostx64/x64/*.dll", true, "MSVC DLL" },
        { L"include/ucrt/stdio.h", L"include/**/*.h", true, "UCRT header" },
        { L"lib/x64/msvcrt.lib", L"lib/x64/*.lib", true, "MSVC library" },
        { L"include/sys/types.h", L"include/**/*.h", true, "System header" },
        { L"include/stdio.hpp", L"include/**/*.hpp", true, "C++ header" },
        { L"ninja.exe", L"*.exe", true, "Ninja executable" },
        { L"Include/windows.h", L"Include/**/*.h", true, "Windows SDK header" },
        { L"Lib/um/x64/kernel32.lib", L"Lib/**/*.lib", true, "Windows SDK library" },
        { L"bin/x64/rc.exe", L"bin/**/*.exe", true, "Resource compiler" },
    };

    std::wcout << L"Running glob matching tests...\n\n";

    int passed = 0;
    int total = test_cases.size();

    for (const auto &test : test_cases)
    {
        bool result = GlobMatcher::matches(test.path, test.pattern);
        bool success = (result == test.expected);

        if (success)
        {
            passed++;
            std::wcout << L"✓ ";
        }
        else
        {
            std::wcout << L"✗ ";
        }

        std::wcout << L"'" << test.path << L"' vs '" << test.pattern << L"' -> " << (result ? L"true" : L"false")
                   << L" (expected: " << (test.expected ? L"true" : L"false") << L") - "
                   << std::wstring(test.description.begin(), test.description.end()) << L"\n";
    }

    std::wcout << L"\nResults: " << passed << L"/" << total << L" tests passed\n";

    if (passed == total)
    {
        std::wcout << L"🎉 All glob matching tests passed!\n";
    }
    else
    {
        std::wcout << L"❌ Some tests failed. Please review the implementation.\n";
    }
}

void testMatchesAny()
{
    std::wcout << L"\nTesting matchesAny() function...\n";

    std::vector<std::wstring> patterns = { L"*.exe", L"*.dll", L"include/**/*.h" };

    struct TestCase
    {
        std::wstring path;
        bool expected;
        std::string description;
    };

    std::vector<TestCase> test_cases = {
        { L"cl.exe", true, "Matches *.exe" },
        { L"library.dll", true, "Matches *.dll" },
        { L"include/stdio.h", true, "Matches include/**/*.h" },
        { L"readme.txt", false, "No pattern matches" },
        { L"bin/tool.exe", false, "*.exe doesn't match paths with directories" },
    };

    int passed = 0;
    for (const auto &test : test_cases)
    {
        bool result = GlobMatcher::matchesAny(test.path, patterns);
        bool success = (result == test.expected);

        if (success)
        {
            passed++;
            std::wcout << L"✓ ";
        }
        else
        {
            std::wcout << L"✗ ";
        }

        std::wcout << L"'" << test.path << L"' -> " << (result ? L"true" : L"false") << L" (expected: "
                   << (test.expected ? L"true" : L"false") << L") - "
                   << std::wstring(test.description.begin(), test.description.end()) << L"\n";
    }

    std::wcout << L"matchesAny(): " << passed << L"/" << test_cases.size() << L" tests passed\n";
}

int main()
{
    std::wcout << L"=== Glob Matcher Test Suite ===\n";

    runGlobTests();
    testMatchesAny();

    std::wcout << L"\nTest completed.\n";
    return 0;
}