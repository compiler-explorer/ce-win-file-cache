#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <ce-win-file-cache/glob_matcher.hpp>

using namespace CeWinFileCache;

TEST_CASE("GlobMatcher basic wildcard patterns", "[glob]")
{
    SECTION("Single asterisk matches within directory")
    {
        REQUIRE(GlobMatcher::matches(L"test.exe", L"*.exe"));
        REQUIRE(GlobMatcher::matches(L"program.dll", L"*.dll"));
        REQUIRE(GlobMatcher::matches(L"README.txt", L"*.txt"));

        REQUIRE_FALSE(GlobMatcher::matches(L"test.dll", L"*.exe"));
        REQUIRE_FALSE(GlobMatcher::matches(L"test", L"*.exe"));
    }

    SECTION("Asterisk at beginning")
    {
        REQUIRE(GlobMatcher::matches(L"library.dll", L"*rary.dll"));
        REQUIRE(GlobMatcher::matches(L"mylibrary.dll", L"*library.dll"));

        REQUIRE_FALSE(GlobMatcher::matches(L"library.exe", L"*library.dll"));
    }

    SECTION("Asterisk in middle")
    {
        REQUIRE(GlobMatcher::matches(L"test123file.txt", L"test*file.txt"));
        REQUIRE(GlobMatcher::matches(L"testfile.txt", L"test*file.txt"));

        REQUIRE_FALSE(GlobMatcher::matches(L"testfile.exe", L"test*file.txt"));
    }

    SECTION("Multiple asterisks")
    {
        REQUIRE(GlobMatcher::matches(L"test_v1_final.exe", L"test*v*final.exe"));
        REQUIRE(GlobMatcher::matches(L"lib_x64_release.dll", L"lib*x64*.dll"));
    }

    SECTION("Asterisk should not cross directory boundaries")
    {
        REQUIRE_FALSE(GlobMatcher::matches(L"bin/test.exe", L"*.exe"));
        REQUIRE_FALSE(GlobMatcher::matches(L"dir/subdir/file.txt", L"dir/*.txt"));

        REQUIRE(GlobMatcher::matches(L"bin/test.exe", L"bin/*.exe"));
    }
}

TEST_CASE("GlobMatcher question mark patterns", "[glob]")
{
    SECTION("Single question mark")
    {
        REQUIRE(GlobMatcher::matches(L"test1.exe", L"test?.exe"));
        REQUIRE(GlobMatcher::matches(L"testA.exe", L"test?.exe"));

        REQUIRE_FALSE(GlobMatcher::matches(L"test.exe", L"test?.exe"));
        REQUIRE_FALSE(GlobMatcher::matches(L"test12.exe", L"test?.exe"));
    }

    SECTION("Multiple question marks")
    {
        REQUIRE(GlobMatcher::matches(L"test12.exe", L"test??.exe"));
        REQUIRE(GlobMatcher::matches(L"testAB.exe", L"test??.exe"));

        REQUIRE_FALSE(GlobMatcher::matches(L"test1.exe", L"test??.exe"));
        REQUIRE_FALSE(GlobMatcher::matches(L"test123.exe", L"test??.exe"));
    }

    SECTION("Question mark should not match path separators")
    {
        REQUIRE_FALSE(GlobMatcher::matches(L"a/b", L"a?b"));
        REQUIRE_FALSE(GlobMatcher::matches(L"a\\b", L"a?b"));
    }
}

TEST_CASE("GlobMatcher recursive wildcard patterns", "[glob]")
{
    SECTION("Double asterisk matches zero directories")
    {
        REQUIRE(GlobMatcher::matches(L"file.txt", L"**/file.txt"));
        REQUIRE(GlobMatcher::matches(L"test.exe", L"**/*.exe"));
    }

    SECTION("Double asterisk matches one directory")
    {
        REQUIRE(GlobMatcher::matches(L"dir/file.txt", L"**/file.txt"));
        REQUIRE(GlobMatcher::matches(L"bin/test.exe", L"**/*.exe"));
    }

    SECTION("Double asterisk matches multiple directories")
    {
        REQUIRE(GlobMatcher::matches(L"a/b/c/file.txt", L"**/file.txt"));
        REQUIRE(GlobMatcher::matches(L"dir1/dir2/dir3/test.exe", L"**/*.exe"));
        REQUIRE(GlobMatcher::matches(L"very/deep/nested/path/file.h", L"**/*.h"));
    }

    SECTION("Double asterisk with prefix")
    {
        REQUIRE(GlobMatcher::matches(L"include/stdio.h", L"include/**/*.h"));
        REQUIRE(GlobMatcher::matches(L"include/sys/types.h", L"include/**/*.h"));
        REQUIRE(GlobMatcher::matches(L"include/a/b/c/d.h", L"include/**/*.h"));

        REQUIRE_FALSE(GlobMatcher::matches(L"src/file.h", L"include/**/*.h"));
        REQUIRE_FALSE(GlobMatcher::matches(L"stdio.h", L"include/**/*.h"));
    }

    SECTION("Double asterisk in middle")
    {
        REQUIRE(GlobMatcher::matches(L"src/main.cpp", L"src/**/main.cpp"));
        REQUIRE(GlobMatcher::matches(L"src/module/main.cpp", L"src/**/main.cpp"));
        REQUIRE(GlobMatcher::matches(L"src/a/b/c/main.cpp", L"src/**/main.cpp"));
    }

    SECTION("Double asterisk at end")
    {
        REQUIRE(GlobMatcher::matches(L"include/file.h", L"include/**"));
        REQUIRE(GlobMatcher::matches(L"include/dir/file.h", L"include/**"));
        REQUIRE(GlobMatcher::matches(L"include/", L"include/**"));
    }
}

TEST_CASE("GlobMatcher path normalization", "[glob]")
{
    SECTION("Backslashes normalized to forward slashes")
    {
        REQUIRE(GlobMatcher::matches(L"bin\\test.exe", L"bin/*.exe"));
        REQUIRE(GlobMatcher::matches(L"bin/test.exe", L"bin\\*.exe"));
        REQUIRE(GlobMatcher::matches(L"a\\b\\c.txt", L"a/b/*.txt"));
    }

    SECTION("Mixed path separators")
    {
        REQUIRE(GlobMatcher::matches(L"a/b\\c/d.txt", L"a/**/d.txt"));
        REQUIRE(GlobMatcher::matches(L"include\\sys/types.h", L"include/**/*.h"));
    }
}

TEST_CASE("GlobMatcher case sensitivity", "[glob]")
{
#ifdef _WIN32
    SECTION("Case insensitive on Windows")
    {
        REQUIRE(GlobMatcher::matches(L"Test.EXE", L"*.exe"));
        REQUIRE(GlobMatcher::matches(L"PROGRAM.DLL", L"*.dll"));
        REQUIRE(GlobMatcher::matches(L"BIN/CL.EXE", L"bin/*.exe"));
        REQUIRE(GlobMatcher::matches(L"Include/Windows.H", L"include/**/*.h"));
    }
#else
    SECTION("Case sensitive on Unix")
    {
        REQUIRE_FALSE(GlobMatcher::matches(L"Test.EXE", L"*.exe"));
        REQUIRE_FALSE(GlobMatcher::matches(L"PROGRAM.DLL", L"*.dll"));
        REQUIRE_FALSE(GlobMatcher::matches(L"BIN/CL.EXE", L"bin/*.exe"));
        REQUIRE_FALSE(GlobMatcher::matches(L"Include/Windows.H", L"include/**/*.h"));
    }
#endif
}

TEST_CASE("GlobMatcher edge cases", "[glob]")
{
    SECTION("Empty patterns and paths")
    {
        REQUIRE(GlobMatcher::matches(L"", L""));
        REQUIRE(GlobMatcher::matches(L"", L"*"));
        REQUIRE(GlobMatcher::matches(L"", L"**"));

        REQUIRE_FALSE(GlobMatcher::matches(L"test", L""));
        REQUIRE_FALSE(GlobMatcher::matches(L"test.exe", L""));
    }

    SECTION("Patterns with only wildcards")
    {
        REQUIRE(GlobMatcher::matches(L"anything", L"*"));
        REQUIRE(GlobMatcher::matches(L"path/to/file", L"**"));
        REQUIRE(GlobMatcher::matches(L"single", L"??????"));
    }

    SECTION("Literal matching without wildcards")
    {
        REQUIRE(GlobMatcher::matches(L"exact.txt", L"exact.txt"));
        REQUIRE(GlobMatcher::matches(L"path/to/file.exe", L"path/to/file.exe"));

        REQUIRE_FALSE(GlobMatcher::matches(L"exact.txt", L"different.txt"));
        REQUIRE_FALSE(GlobMatcher::matches(L"path/to/file", L"path/to/other"));
    }
}

TEST_CASE("GlobMatcher real-world patterns", "[glob]")
{
    SECTION("Compiler executable patterns")
    {
        REQUIRE(GlobMatcher::matches(L"bin/Hostx64/x64/cl.exe", L"bin/Hostx64/x64/*.exe"));
        REQUIRE(GlobMatcher::matches(L"bin/Hostx64/x64/link.exe", L"bin/Hostx64/x64/*.exe"));
        REQUIRE(GlobMatcher::matches(L"bin/Hostx64/x64/cl.exe", L"bin/**/*.exe"));
    }

    SECTION("DLL patterns")
    {
        REQUIRE(GlobMatcher::matches(L"bin/Hostx64/x64/mspdb140.dll", L"bin/Hostx64/x64/*.dll"));
        REQUIRE(GlobMatcher::matches(L"bin/Hostx64/x64/msvcr140.dll", L"bin/**/*.dll"));
    }

    SECTION("Header file patterns")
    {
        REQUIRE(GlobMatcher::matches(L"include/stdio.h", L"include/**/*.h"));
        REQUIRE(GlobMatcher::matches(L"include/sys/types.h", L"include/**/*.h"));
        REQUIRE(GlobMatcher::matches(L"include/ucrt/stdio.h", L"include/**/*.h"));
        REQUIRE(GlobMatcher::matches(L"include/memory.hpp", L"include/**/*.hpp"));
    }

    SECTION("Library patterns")
    {
        REQUIRE(GlobMatcher::matches(L"lib/x64/kernel32.lib", L"lib/x64/*.lib"));
        REQUIRE(GlobMatcher::matches(L"lib/x64/msvcrt.lib", L"lib/**/*.lib"));
        REQUIRE(GlobMatcher::matches(L"Lib/um/x64/kernel32.lib", L"Lib/**/*.lib"));
    }
}

TEST_CASE("GlobMatcher::matchesAny", "[glob]")
{
    std::vector<std::wstring> patterns = { L"*.exe", L"*.dll", L"include/**/*.h", L"lib/**/*.lib" };

    SECTION("Matches first pattern")
    {
        REQUIRE(GlobMatcher::matchesAny(L"test.exe", patterns));
        REQUIRE(GlobMatcher::matchesAny(L"program.exe", patterns));
    }

    SECTION("Matches second pattern")
    {
        REQUIRE(GlobMatcher::matchesAny(L"library.dll", patterns));
        REQUIRE(GlobMatcher::matchesAny(L"msvcrt.dll", patterns));
    }

    SECTION("Matches recursive pattern")
    {
        REQUIRE(GlobMatcher::matchesAny(L"include/stdio.h", patterns));
        REQUIRE(GlobMatcher::matchesAny(L"include/sys/types.h", patterns));
    }

    SECTION("No match")
    {
        REQUIRE_FALSE(GlobMatcher::matchesAny(L"readme.txt", patterns));
        REQUIRE_FALSE(GlobMatcher::matchesAny(L"src/main.cpp", patterns));
        REQUIRE_FALSE(GlobMatcher::matchesAny(L"bin/tool", patterns));
    }

    SECTION("Empty patterns list")
    {
        std::vector<std::wstring> empty;
        REQUIRE_FALSE(GlobMatcher::matchesAny(L"test.exe", empty));
    }
}

TEST_CASE("GlobMatcher absolute and non-relative paths", "[glob]")
{
    SECTION("Absolute Unix-style paths")
    {
        REQUIRE(GlobMatcher::matches(L"/usr/bin/gcc", L"/usr/bin/*"));
        REQUIRE(GlobMatcher::matches(L"/home/user/project/main.cpp", L"/home/user/**/*.cpp"));
        REQUIRE(GlobMatcher::matches(L"/opt/compiler/bin/cl.exe", L"/opt/**/bin/*.exe"));
        REQUIRE(GlobMatcher::matches(L"/var/cache/file.tmp", L"/var/cache/*.tmp"));

        REQUIRE_FALSE(GlobMatcher::matches(L"/usr/bin/gcc", L"/opt/bin/*"));
        REQUIRE_FALSE(GlobMatcher::matches(L"/home/user/file.txt", L"/tmp/**/*.txt"));
    }

    SECTION("Windows-style absolute paths")
    {
        REQUIRE(GlobMatcher::matches(L"C:/Program Files/MSVC/bin/cl.exe", L"C:/Program Files/**/*.exe"));
        REQUIRE(GlobMatcher::matches(L"D:\\tools\\ninja.exe", L"D:/tools/*.exe"));
        REQUIRE(GlobMatcher::matches(L"C:\\Windows\\System32\\kernel32.dll", L"C:/Windows/**/*.dll"));
        REQUIRE(GlobMatcher::matches(L"E:/cache/temp/file.obj", L"E:/cache/**/*.obj"));

        REQUIRE_FALSE(GlobMatcher::matches(L"C:/Program Files/file.exe", L"D:/**/*.exe"));
        REQUIRE_FALSE(GlobMatcher::matches(L"C:/temp/file.txt", L"C:/cache/*.txt"));
    }

    SECTION("UNC network paths")
    {
        REQUIRE(GlobMatcher::matches(L"//server/share/file.exe", L"//server/share/*.exe"));
        REQUIRE(
        GlobMatcher::matches(L"\\\\127.0.0.1\\efs\\compilers\\msvc\\bin\\cl.exe", L"\\\\127.0.0.1\\efs\\**\\*.exe"));
        REQUIRE(GlobMatcher::matches(L"//nas/backup/2024/file.zip", L"//nas/backup/**/*.zip"));
        REQUIRE(GlobMatcher::matches(L"\\\\fileserver\\projects\\src\\main.cpp", L"\\\\fileserver\\**\\*.cpp"));

        REQUIRE_FALSE(GlobMatcher::matches(L"//server1/share/file.exe", L"//server2/share/*.exe"));
        REQUIRE_FALSE(GlobMatcher::matches(L"\\\\host\\share\\file.dll", L"\\\\other\\share\\*.dll"));
    }

    SECTION("Mixed path separators in absolute paths")
    {
        REQUIRE(GlobMatcher::matches(L"C:\\Program Files/MSVC\\bin/cl.exe", L"C:/Program Files/**/*.exe"));
        REQUIRE(GlobMatcher::matches(L"/opt/tools\\bin/gcc", L"/opt/**/gcc"));
        REQUIRE(GlobMatcher::matches(L"\\\\server/share\\path/file.h", L"\\\\server\\**\\*.h"));
    }

    SECTION("Paths starting with drive letters")
    {
        REQUIRE(GlobMatcher::matches(L"C:file.txt", L"C:*.txt"));
        REQUIRE(GlobMatcher::matches(L"D:temp\\data.bin", L"D:temp/*.bin"));
        REQUIRE(GlobMatcher::matches(L"Z:project/build/output.exe", L"Z:**/*.exe"));

        REQUIRE_FALSE(GlobMatcher::matches(L"C:file.txt", L"D:*.txt"));
        REQUIRE_FALSE(GlobMatcher::matches(L"C:file.txt", L"file.txt")); // Drive letter should not be ignored
    }

    SECTION("Root directory patterns")
    {
        REQUIRE(GlobMatcher::matches(L"/bin/sh", L"/bin/*"));
        REQUIRE(GlobMatcher::matches(L"/etc/hosts", L"/etc/hosts"));
        REQUIRE(GlobMatcher::matches(L"C:/Windows/notepad.exe", L"C:/**/*.exe"));
        REQUIRE(GlobMatcher::matches(L"/usr/local/bin/tool", L"/**/*"));

        REQUIRE_FALSE(GlobMatcher::matches(L"/bin/sh", L"bin/*")); // Absolute vs relative
        REQUIRE_FALSE(GlobMatcher::matches(L"bin/sh", L"/bin/*")); // Relative vs absolute
    }
}

TEST_CASE("GlobMatcher complex patterns", "[glob]")
{
    SECTION("Combination of wildcards")
    {
        REQUIRE(GlobMatcher::matches(L"test_v1.exe", L"test?v*.exe"));
        REQUIRE(GlobMatcher::matches(L"lib/x64/debug/mylib.dll", L"lib/**/debug/*.dll"));
        REQUIRE(GlobMatcher::matches(L"a/b/test123.txt", L"**/test???.txt"));
    }

    SECTION("Patterns from configuration file")
    {
        // MSVC patterns
        REQUIRE(GlobMatcher::matches(L"bin/Hostx64/x64/cl.exe", L"bin/Hostx64/x64/*.exe"));
        REQUIRE(GlobMatcher::matches(L"bin/Hostx64/x64/vcruntime140.dll", L"bin/Hostx64/x64/*.dll"));

        // Windows SDK patterns
        REQUIRE(GlobMatcher::matches(L"Include/windows.h", L"Include/**/*.h"));
        REQUIRE(GlobMatcher::matches(L"Include/um/winnt.h", L"Include/**/*.h"));
        REQUIRE(GlobMatcher::matches(L"Lib/um/x64/kernel32.lib", L"Lib/**/*.lib"));

        // Ninja pattern
        REQUIRE(GlobMatcher::matches(L"ninja.exe", L"*.exe"));
    }

    SECTION("Real compiler paths from network shares")
    {
        // Actual UNC paths from compilers.json
        REQUIRE(GlobMatcher::matches(L"\\\\127.0.0.1\\efs\\compilers\\msvc\\14.40.33807-14.40.33811."
                                     L"0\\bin\\Hostx64\\x64\\cl.exe",
                                     L"\\\\127.0.0.1\\efs\\compilers\\msvc\\**\\bin\\**\\*.exe"));
        REQUIRE(
        GlobMatcher::matches(L"\\\\127.0.0.1\\efs\\compilers\\windows-kits-10\\Include\\10.0.22621.0\\ucrt\\stdio.h",
                             L"\\\\127.0.0.1\\efs\\compilers\\**\\Include\\**\\*.h"));
        REQUIRE(GlobMatcher::matches(L"\\\\127.0.0.1\\efs\\compilers\\ninja\\ninja.exe",
                                     L"\\\\127.0.0.1\\efs\\compilers\\ninja\\*.exe"));
    }
}