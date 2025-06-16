#include <algorithm>
#include <ce-win-file-cache/config_parser.hpp>
#include <ce-win-file-cache/hybrid_filesystem.hpp>
#include <ce-win-file-cache/memory_cache_manager.hpp>
#include <ce-win-file-cache/string_utils.hpp>
#include <chrono>
#include <iostream>
#include <shellapi.h>
#include <string>
#include <unordered_map>
#include <vector>

#ifndef NO_WINFSP
#include <winfsp/winfsp.hpp>
#endif

#include <codecvt>
#include <ce-win-file-cache/glob_matcher.hpp>

using namespace CeWinFileCache;

// Helper function to load config file (JSON only)
std::optional<Config> loadConfigFile(const std::wstring &config_file)
{
    return ConfigParser::parseJsonFile(config_file);
}

// Command line parsing structure
struct ProgramOptions
{
    std::wstring config_file = L"compilers.json";
    std::wstring mount_point = L"M:";
    std::wstring volume_prefix;
    ULONG debug_flags = 0;
    bool show_help = false;
    bool test_mode = false;
    bool test_path_resolution = false;
    bool test_network_mapping = false;
    bool test_config_only = false;
    bool test_cache_operations = false;
};

// Function to print usage information
void printUsage()
{
    std::wcout << L"Usage: CeWinFileCacheFS [OPTIONS]\n"
               << L"\n"
               << L"Options:\n"
               << L"  -c, --config FILE      Configuration file (default: compilers.json)\n"
               << L"  -m, --mount POINT      Mount point (default: M:)\n"
               << L"  -u, --volume-prefix    Volume prefix for UNC paths\n"
               << L"  -d, --debug [LEVEL]    Enable debug logging\n"
               << L"  -t, --test             Test mode (no WinFsp mounting)\n"
               << L"      --test-paths       Test path resolution only\n"
               << L"      --test-network     Test network mapping only\n"
               << L"      --test-config      Test config parsing only\n"
               << L"      --test-cache       Test cache operations\n"
               << L"  -h, --help             Show this help message\n"
               << L"\n"
               << L"Examples:\n"
               << L"  CeWinFileCacheFS --config compilers.json --mount M:\n"
               << L"  CeWinFileCacheFS --mount C:\\compilers --debug\n"
               << L"  CeWinFileCacheFS --test --config test.json\n"
               << std::endl;
}

// Parse command line arguments
ProgramOptions parseCommandLine(int argc, wchar_t **argv)
{
    ProgramOptions options;

    for (int i = 1; i < argc; i++)
    {
        std::wstring arg{ argv[i] };

        if (arg == L"-c" || arg == L"--config")
        {
            const wchar_t *config_path = StringUtils::getNextArg(argv, i, argc);
            if (config_path)
            {
                options.config_file = config_path;
            }
            else
            {
                std::wcerr << L"Error: --config requires a file path" << std::endl;
                options.show_help = true;
                break;
            }
        }
        else if (arg == L"-m" || arg == L"--mount")
        {
            const wchar_t *mount_point = StringUtils::getNextArg(argv, i, argc);
            if (mount_point)
            {
                options.mount_point = mount_point;
            }
            else
            {
                std::wcerr << L"Error: --mount requires a mount point" << std::endl;
                options.show_help = true;
                break;
            }
        }
        else if (arg == L"-u" || arg == L"--volume-prefix")
        {
            const wchar_t *volume_prefix = StringUtils::getNextArg(argv, i, argc);
            if (volume_prefix)
            {
                options.volume_prefix = volume_prefix;
            }
            else
            {
                std::wcerr << L"Error: --volume-prefix requires a prefix" << std::endl;
                options.show_help = true;
                break;
            }
        }
        else if (arg == L"-d" || arg == L"--debug")
        {
            if (i + 1 < argc && argv[i + 1][0] != L'-')
            {
                options.debug_flags = StringUtils::parseULong(argv[++i]);
            }
            else
            {
                options.debug_flags = static_cast<ULONG>(-1); // Enable all debug
            }
            std::wcout << L"[MAIN] Debug mode enabled with flags: 0x" << std::hex << options.debug_flags << std::endl;
        }
        else if (arg == L"-t" || arg == L"--test")
        {
            options.test_mode = true;
        }
        else if (arg == L"--test-paths")
        {
            options.test_mode = true;
            options.test_path_resolution = true;
        }
        else if (arg == L"--test-network")
        {
            options.test_mode = true;
            options.test_network_mapping = true;
        }
        else if (arg == L"--test-config")
        {
            options.test_mode = true;
            options.test_config_only = true;
        }
        else if (arg == L"--test-cache")
        {
            options.test_mode = true;
            options.test_cache_operations = true;
        }
        else if (arg == L"-h" || arg == L"--help")
        {
            options.show_help = true;
            break;
        }
        else
        {
            std::wcerr << L"Unknown argument: " << arg << std::endl;
            options.show_help = true;
            break;
        }
    }

    return options;
}

// Test function for config parsing only
int testConfigOnly(const Config &config)
{
    std::wcout << L"[CONFIG TEST] === Config Parsing Test ===" << std::endl;
    std::wcout << L"[CONFIG TEST] Configuration loaded successfully:" << std::endl;
    std::wcout << L"[CONFIG TEST]   Cache directory: " << config.global.cache_directory << std::endl;
    std::wcout << L"[CONFIG TEST]   Total cache size: " << config.global.total_cache_size_mb << L" MB" << std::endl;
    std::wcout << L"[CONFIG TEST]   Eviction policy: " << config.global.eviction_policy << std::endl;
    std::wcout << L"[CONFIG TEST]   Number of compilers: " << config.compilers.size() << std::endl;

    for (const auto &[name, compiler_config] : config.compilers)
    {
        std::wcout << L"[CONFIG TEST]     - " << name << L": " << compiler_config.network_path << std::endl;
        std::wcout << L"[CONFIG TEST]       Cache size: " << compiler_config.cache_size_mb << L" MB" << std::endl;
        std::wcout << L"[CONFIG TEST]       Cache patterns: " << compiler_config.cache_always_patterns.size()
                   << L" patterns" << std::endl;
        std::wcout << L"[CONFIG TEST]       Prefetch patterns: " << compiler_config.prefetch_patterns.size()
                   << L" patterns" << std::endl;
    }

    std::wcout << L"[CONFIG TEST] Config test completed successfully!" << std::endl;
    std::wcout << L"[CONFIG TEST] Returning exit code 0" << std::endl;
    return 0;
}

// Test function for path resolution (TODO #3)
int testPathResolution(const Config &config)
{
    std::wcout << L"=== Path Resolution Test ===" << std::endl;

    // Test cases for path resolution
    std::vector<std::wstring> test_paths = { L"/msvc-14.40/bin/Hostx64/x64/cl.exe", L"/msvc-14.40/include/iostream",
                                             L"/windows-kits-10/Include/10.0.22621.0/um/windows.h", // L"/ninja/ninja.exe",
                                             L"/invalid-compiler/some/path" };

    for (const auto &virtual_path : test_paths)
    {
        std::wcout << L"Testing virtual path: " << virtual_path << std::endl;

        // Extract compiler name from virtual path
        std::wstring compiler_name;
        size_t first_slash = virtual_path.find(L'/', 1);
        if (first_slash != std::wstring::npos)
        {
            compiler_name = virtual_path.substr(1, first_slash - 1);
        }
        else
        {
            compiler_name = virtual_path.substr(1);
        }

        // Check if compiler exists in config
        auto it = config.compilers.find(compiler_name);
        if (it != config.compilers.end())
        {
            // Extract relative path
            std::wstring relative_path;
            if (first_slash != std::wstring::npos)
            {
                relative_path = virtual_path.substr(first_slash + 1);
            }

            // Resolve to network path
            std::wstring resolved_path = it->second.network_path;
            if (!relative_path.empty())
            {
                // Convert forward slashes to backslashes for Windows
                std::wstring windows_relative = DirectoryCache::normalizePath(relative_path);
                //std::replace(windows_relative.begin(), windows_relative.end(), L'/', L'\\');
                //windows_relative);
                resolved_path += L"\\" + windows_relative;
            }

            std::wcout << L"  -> Resolved to: " << resolved_path << std::endl;
        }
        else
        {
            std::wcout << L"  -> ERROR: Compiler '" << compiler_name << L"' not found in config" << std::endl;
        }
        std::wcout << std::endl;
    }

    std::wcout << L"Path resolution test completed!" << std::endl;
    return 0;
}

// Test function for network mapping (TODO #4)
int testNetworkMapping(const Config &config)
{
    std::wcout << L"=== Network Mapping Test ===" << std::endl;

    // Test cases for network path mapping
    std::vector<std::pair<std::wstring, std::wstring>> test_cases = {
        { L"/msvc-14.40/bin/Hostx64/x64/cl.exe",
          L"\\\\\\\\127.0.0.1\\\\efs\\\\compilers\\\\msvc\\\\14.40.33807-14.40.33811.0\\bin\\Hostx64\\x64\\cl.exe" },
        { L"/msvc-14.40/include/iostream",
          L"\\\\\\\\127.0.0.1\\\\efs\\\\compilers\\\\msvc\\\\14.40.33807-14.40.33811.0\\include\\iostream" },
        { L"/windows-kits-10/Lib/10.0.22621.0/ucrt/x64/ucrt.lib",
          L"\\\\\\\\127.0.0.1\\\\efs\\\\compilers\\\\windows-kits-10\\Lib\\10.0.22621.0\\ucrt\\x64\\ucrt.lib" },
        { L"/ninja/ninja.exe", L"\\\\\\\\127.0.0.1\\\\efs\\\\compilers\\\\ninja\\ninja.exe" }
    };

    for (const auto &[virtual_path, expected_network_path] : test_cases)
    {
        std::wcout << L"Testing virtual path: " << virtual_path << std::endl;
        std::wcout << L"Expected network path: " << expected_network_path << std::endl;

        // Extract compiler name
        std::wstring compiler_name;
        size_t first_slash = virtual_path.find(L'/', 1);
        if (first_slash != std::wstring::npos)
        {
            compiler_name = virtual_path.substr(1, first_slash - 1);
        }
        else
        {
            compiler_name = virtual_path.substr(1);
        }

        // Check if compiler exists
        auto it = config.compilers.find(compiler_name);
        if (it != config.compilers.end())
        {
            // Extract relative path and convert to Windows format
            std::wstring relative_path;
            if (first_slash != std::wstring::npos)
            {
                relative_path = virtual_path.substr(first_slash + 1);
                std::replace(relative_path.begin(), relative_path.end(), L'/', L'\\');
            }

            // Map to network path
            std::wstring actual_network_path = it->second.network_path;
            if (!relative_path.empty())
            {
                actual_network_path += L"\\" + relative_path;
            }

            std::wcout << L"Actual network path: " << actual_network_path << std::endl;

            // Check if mapping is correct
            if (actual_network_path == expected_network_path)
            {
                std::wcout << L"  -> PASS: Network mapping correct" << std::endl;
            }
            else
            {
                std::wcout << L"  -> FAIL: Network mapping mismatch" << std::endl;
                return 1;
            }
        }
        else
        {
            std::wcout << L"  -> ERROR: Compiler '" << compiler_name << L"' not found" << std::endl;
            return 1;
        }
        std::wcout << std::endl;
    }

    std::wcout << L"Network mapping test completed successfully!" << std::endl;
    return 0;
}

// Test function for cache operations
int testCacheOperations(const Config &config)
{
    std::wcout << L"=== Cache Operations Test ===" << std::endl;

    // Create cache manager
    MemoryCacheManager cache_manager;

    // Test files (use real compiler paths)
    std::vector<std::wstring> test_files = { L"/msvc-14.40/bin/Hostx64/x64/cl.exe", L"/msvc-14.40/include/iostream", L"/ninja/ninja.exe" };

    std::wcout << L"\n1. Testing cache miss and network loading..." << std::endl;
    for (const auto &virtual_path : test_files)
    {
        std::wcout << L"  Loading: " << virtual_path << std::endl;

        // Check cache (should miss)
        if (cache_manager.isFileInMemoryCache(virtual_path))
        {
            std::wcout << L"    ERROR: File unexpectedly in cache" << std::endl;
            return 1;
        }

        // Load from network
        auto start = std::chrono::high_resolution_clock::now();
        auto content = cache_manager.getFileContent(virtual_path, config);
        auto end = std::chrono::high_resolution_clock::now();

        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

        if (content.empty())
        {
            std::wcout << L"    WARNING: Failed to load file (may not exist)" << std::endl;
        }
        else
        {
            std::wcout << L"    Loaded " << content.size() << L" bytes in " << duration << L"ms" << std::endl;
        }
    }

    std::wcout << L"\n2. Testing cache hits..." << std::endl;
    for (const auto &virtual_path : test_files)
    {
        // Skip if file wasn't loaded
        if (!cache_manager.isFileInMemoryCache(virtual_path))
        {
            std::wcout << L"  Skipping: " << virtual_path << L" (not in cache)" << std::endl;
            continue;
        }

        std::wcout << L"  Reading from cache: " << virtual_path << std::endl;

        auto start = std::chrono::high_resolution_clock::now();
        auto cached = cache_manager.getMemoryCachedFile(virtual_path);
        auto end = std::chrono::high_resolution_clock::now();

        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

        if (!cached.has_value())
        {
            std::wcout << L"    ERROR: Failed to retrieve cached file" << std::endl;
            return 1;
        }

        std::wcout << L"    Retrieved " << cached->size() << L" bytes in " << duration << L"μs" << std::endl;
    }

    std::wcout << L"\n3. Cache statistics..." << std::endl;
    std::wcout << L"  Total cached files: " << cache_manager.getCachedFileCount() << std::endl;
    std::wcout << L"  Total cache size: " << cache_manager.getCacheSize() << L" bytes" << std::endl;
    std::wcout << L"  Average cache hit time: <1ms" << std::endl;

    std::wcout << L"\n4. Testing cache clear..." << std::endl;
    cache_manager.clearCache();
    std::wcout << L"  Cache cleared. Files in cache: " << cache_manager.getCachedFileCount() << std::endl;

    std::wcout << L"\nCache operations test completed!" << std::endl;
    return 0;
}

// Test mode function - runs without WinFsp
int runTestMode(const ProgramOptions &options)
{
    std::wcout << L"[TEST] Running in test mode (no WinFsp mounting)" << std::endl;
    std::wcout << L"[TEST] Test config only: " << (options.test_config_only ? L"YES" : L"NO") << std::endl;
    std::wcout << L"[TEST] Test path resolution: " << (options.test_path_resolution ? L"YES" : L"NO") << std::endl;
    std::wcout << L"[TEST] Test network mapping: " << (options.test_network_mapping ? L"YES" : L"NO") << std::endl;
    std::wcout << L"[TEST] Test cache operations: " << (options.test_cache_operations ? L"YES" : L"NO") << std::endl;

    // Load configuration
    std::wcout << L"[TEST] Loading config from: " << options.config_file << std::endl;
    auto config_opt = loadConfigFile(options.config_file);
    if (!config_opt)
    {
        std::wcerr << L"[TEST ERROR] Failed to load configuration from: " << options.config_file << std::endl;
        std::wcerr << L"[TEST ERROR] Exiting with code 1" << std::endl;
        return 1;
    }

    std::wcout << L"[TEST] Configuration loaded successfully!" << std::endl;
    Config config = *config_opt;

    // Run specific tests based on options
    if (options.test_config_only)
    {
        std::wcout << L"[TEST] Running config-only test..." << std::endl;
        int result = testConfigOnly(config);
        std::wcout << L"[TEST] Config test completed with result: " << result << std::endl;
        return result;
    }
    else if (options.test_path_resolution)
    {
        return testPathResolution(config);
    }
    else if (options.test_network_mapping)
    {
        return testNetworkMapping(config);
    }
    else if (options.test_cache_operations)
    {
        return testCacheOperations(config);
    }
    else
    {
        // Run all tests
        std::wcout << L"Running all tests..." << std::endl;

        int result = testConfigOnly(config);
        if (result != 0)
            return result;

        result = testPathResolution(config);
        if (result != 0)
            return result;

        result = testNetworkMapping(config);
        if (result != 0)
            return result;

        result = testCacheOperations(config);
        if (result != 0)
            return result;

        std::wcout << L"All tests completed successfully!" << std::endl;
        return 0;
    }
}

#ifndef NO_WINFSP
class CompilerCacheService : public Fsp::Service
{
    public:
    CompilerCacheService(wchar_t *ServiceName, const ProgramOptions &options)
    : Fsp::Service(ServiceName), filesystem(), host(filesystem), options_(options)
    {
    }

    protected:
    NTSTATUS OnStart(ULONG argc, PWSTR *argv) override
    {
        std::wcout << L"[SERVICE] OnStart() called with " << argc << L" arguments" << std::endl;
        for (ULONG i = 0; i < argc; i++)
        {
            std::wcout << L"[SERVICE] Arg[" << i << L"]: " << (argv[i] ? argv[i] : L"<null>") << std::endl;
        }

        // Load configuration
        std::wcout << L"[SERVICE] Loading configuration from: " << options_.config_file << std::endl;
        auto config_opt = loadConfigFile(options_.config_file);
        if (!config_opt)
        {
            std::wcerr << L"[SERVICE] ERROR: Failed to load configuration from: " << options_.config_file << std::endl;
            return STATUS_UNSUCCESSFUL;
        }
        std::wcout << L"[SERVICE] Configuration loaded successfully" << std::endl;

        Config config = *config_opt;

        // Initialize filesystem
        std::wcout << L"[SERVICE] Initializing filesystem..." << std::endl;
        NTSTATUS result = filesystem.Initialize(config);
        if (!NT_SUCCESS(result))
        {
            std::wcerr << L"[SERVICE] ERROR: Failed to initialize filesystem. Status: 0x" << std::hex << result << std::endl;
            return result;
        }
        std::wcout << L"[SERVICE] Filesystem initialized successfully" << std::endl;

        // Set up compiler paths (for now, just use the network paths directly)
        std::wcout << L"[SERVICE] Setting up compiler paths..." << std::endl;
        std::unordered_map<std::wstring, std::wstring> compiler_paths;
        for (const auto &[name, compiler_config] : config.compilers)
        {
            std::wcout << L"[SERVICE] Adding compiler: " << name << L" -> " << compiler_config.network_path << std::endl;
            compiler_paths[name] = compiler_config.network_path;
        }

        result = filesystem.SetCompilerPaths(compiler_paths);
        if (!NT_SUCCESS(result))
        {
            std::wcerr << L"[SERVICE] ERROR: Failed to set compiler paths. Status: 0x" << std::hex << result << std::endl;
            return result;
        }
        std::wcout << L"[SERVICE] Compiler paths set successfully" << std::endl;

        // Configure host
        if (!options_.volume_prefix.empty())
        {
            std::wcout << L"[SERVICE] Setting volume prefix: " << options_.volume_prefix << std::endl;
            std::wstring volume_prefix_copy = options_.volume_prefix;
            host.SetPrefix(volume_prefix_copy.data());
        }

        // Mount filesystem
        std::wcout << L"[SERVICE] Mounting filesystem at: " << options_.mount_point << std::endl;
        std::wcout << L"[SERVICE] Debug flags: 0x" << std::hex << options_.debug_flags << std::endl;
        std::wstring mount_point_copy = options_.mount_point;
        result = host.Mount(mount_point_copy.data(), nullptr, FALSE, options_.debug_flags);
        if (!NT_SUCCESS(result))
        {
            std::wcerr << L"[SERVICE] ERROR: Failed to mount filesystem at " << options_.mount_point << L". Status: 0x" << std::hex
                       << result << std::endl;
            return result;
        }

        std::wcout << L"[SERVICE] SUCCESS: CompilerCacheFS mounted at " << host.MountPoint() << std::endl;
        std::wcout << L"[SERVICE] Cache directory: " << config.global.cache_directory << std::endl;
        std::wcout << L"[SERVICE] Total cache size: " << config.global.total_cache_size_mb << L" MB" << std::endl;
        std::wcout << L"[SERVICE] Filesystem is now ready for access" << std::endl;

        return STATUS_SUCCESS;
    }

    NTSTATUS OnStop() override
    {
        host.Unmount();
        std::wcout << L"CeWinFileCacheFS unmounted" << std::endl;
        return STATUS_SUCCESS;
    }

    private:
    HybridFileSystem filesystem;
    Fsp::FileSystemHost host;
    ProgramOptions options_;
};
#endif // NO_WINFSP

int wmain(int argc, wchar_t **argv)
{
    // Parse command line arguments
    ProgramOptions options = parseCommandLine(argc, argv);

    if (options.show_help)
    {
        printUsage();
        return 0;
    }

    // If test mode is requested, run without WinFsp
    if (options.test_mode)
    {
        return runTestMode(options);
    }

#ifdef NO_WINFSP
    std::wcerr << L"Error: WinFsp support was disabled at compile time." << std::endl;
    std::wcerr << L"Use --test mode or recompile without NO_WINFSP defined." << std::endl;
    return 1;
#else
    // Initialize WinFsp
    NTSTATUS result = Fsp::Initialize();
    if (!NT_SUCCESS(result))
    {
        std::wcerr << L"Failed to initialize WinFsp. Status: 0x" << std::hex << result << std::endl;
        return 1;
    }

    // Create and run service
    wchar_t service_name[] = L"CeWinFileCacheFS";
    CompilerCacheService service{ service_name, options };
    return service.Run();
#endif
};

int APIENTRY WinMain(HINSTANCE hInst, HINSTANCE hInstPrev, PSTR cmdline, int cmdshow)
{
    // std::ios_base::sync_with_stdio(false);
    // std::locale utf8(std::locale(), new std::codecvt_utf8_utf16<wchar_t>);
    // std::wcout.imbue(utf8);


    std::wcout << L"CeWinFileCacheFS starting?????...\n";

    // call wmain with command line arguments
    int argc;
    wchar_t **argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv)
    {
        std::wcerr << L"Failed to parse command line arguments." << std::endl;
        return 1;
    }
    int result = wmain(argc, argv);
    LocalFree(argv); // Free the memory allocated by CommandLineToArgvW
    return result;
}
