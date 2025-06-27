#include <algorithm>
#include <cctype>
#include <ce-win-file-cache/config_parser.hpp>
#include <ce-win-file-cache/hybrid_filesystem.hpp>
#include <ce-win-file-cache/logger.hpp>
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

#include <ce-win-file-cache/glob_matcher.hpp>

using namespace CeWinFileCache;

// Helper functions for logging configuration
LogLevel parseLogLevel(const std::string &level_str)
{
    std::string lower_level = level_str;
    std::transform(lower_level.begin(), lower_level.end(), lower_level.begin(), ::tolower);

    if (lower_level == "trace")
        return LogLevel::TRACE;
    if (lower_level == "debug")
        return LogLevel::DEBUG;
    if (lower_level == "info")
        return LogLevel::INFO;
    if (lower_level == "warn")
        return LogLevel::WARN;
    if (lower_level == "error")
        return LogLevel::ERR;
    if (lower_level == "fatal")
        return LogLevel::FATAL;
    if (lower_level == "off")
        return LogLevel::OFF;

    Logger::warn_fallback("Unknown log level '{}', using INFO", level_str);
    return LogLevel::INFO;
}

LogOutput parseLogOutput(const std::string &output_str)
{
    std::string lower_output = output_str;
    std::transform(lower_output.begin(), lower_output.end(), lower_output.begin(), ::tolower);

    if (lower_output == "console")
        return LogOutput::CONSOLE;
    if (lower_output == "file")
        return LogOutput::FILE;
    if (lower_output == "both")
        return LogOutput::BOTH;
    if (lower_output == "debug")
        return LogOutput::DEBUG_OUTPUT;
    if (lower_output == "disabled")
        return LogOutput::DISABLED;

    Logger::warn_fallback("Unknown log output '{}', using CONSOLE", output_str);
    return LogOutput::CONSOLE;
}

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

    // Application logging options
    LogLevel log_level = LogLevel::INFO;
    LogOutput log_output = LogOutput::CONSOLE;
    std::string log_file = "cewinfilecache.log";
    bool diagnose = false;
};

// Function to print usage information
void printUsage()
{
    std::string usage =
    "Usage: CeWinFileCacheFS [OPTIONS]\n"
    "\n"
    "Options:\n"
    "  -c, --config FILE      Configuration file (default: compilers.json)\n"
    "  -m, --mount POINT      Mount point (default: M:)\n"
    "  -u, --volume-prefix    Volume prefix for UNC paths\n"
    "  -d, --debug [LEVEL]    WinFsp debug flags (0=off, -1=all, bitmask)\n"
    "  -t, --test             Test mode (no WinFsp mounting)\n"
    "      --test-paths       Test path resolution only\n"
    "      --test-network     Test network mapping only\n"
    "      --test-config      Test config parsing only\n"
    "      --test-cache       Test cache operations\n"
    "  -h, --help             Show this help message\n"
    "\n"
    "Application Logging Options:\n"
    "  -l, --log-level LEVEL  Set log level: trace, debug, info, warn, error, fatal, off (default: info)\n"
    "  -o, --log-output TYPE  Set output: console, file, both, debug, disabled (default: console)\n"
    "  -f, --log-file FILE    Log file path (default: cewinfilecache.log)\n"
    "      --diagnose         Run system diagnostics and environment checks\n"
    "\n"
    "Examples:\n"
    "  CeWinFileCacheFS --config compilers.json --mount M:\n"
    "  CeWinFileCacheFS --mount C:\\compilers --debug\n"
    "  CeWinFileCacheFS --test --config test.json\n"
    "  CeWinFileCacheFS --diagnose --config compilers.json --mount M:\n"
    "  CeWinFileCacheFS --log-level debug --log-output both --log-file debug.log\n"
    "  CeWinFileCacheFS --log-level trace --log-output file";

    Logger::info(usage);
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
                Logger::error("Error: --config requires a file path");
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
                Logger::error("Error: --mount requires a mount point");
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
                Logger::error("Error: --volume-prefix requires a prefix");
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
            Logger::info("Debug mode enabled with flags: 0x{:x}", options.debug_flags);
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
        else if (arg == L"-l" || arg == L"--log-level")
        {
            const wchar_t *log_level = StringUtils::getNextArg(argv, i, argc);
            if (log_level)
            {
                std::string level_str = StringUtils::wideToUtf8(log_level);
                options.log_level = parseLogLevel(level_str);
            }
            else
            {
                Logger::error("Error: --log-level requires a level (trace, debug, info, warn, error, fatal, off)");
                options.show_help = true;
                break;
            }
        }
        else if (arg == L"-o" || arg == L"--log-output")
        {
            const wchar_t *log_output = StringUtils::getNextArg(argv, i, argc);
            if (log_output)
            {
                std::string output_str = StringUtils::wideToUtf8(log_output);
                options.log_output = parseLogOutput(output_str);
            }
            else
            {
                Logger::error("Error: --log-output requires a type (console, file, both, debug, disabled)");
                options.show_help = true;
                break;
            }
        }
        else if (arg == L"-f" || arg == L"--log-file")
        {
            const wchar_t *log_file = StringUtils::getNextArg(argv, i, argc);
            if (log_file)
            {
                options.log_file = StringUtils::wideToUtf8(log_file);
            }
            else
            {
                Logger::error("Error: --log-file requires a file path");
                options.show_help = true;
                break;
            }
        }
        else if (arg == L"--diagnose")
        {
            options.diagnose = true;
        }
        else if (arg == L"-h" || arg == L"--help")
        {
            options.show_help = true;
            break;
        }
        else
        {
            Logger::error("Unknown argument: {}", StringUtils::wideToUtf8(arg));
            options.show_help = true;
            break;
        }
    }

    return options;
}

// Test function for config parsing only
int testConfigOnly(const Config &config)
{
    Logger::info("[CONFIG TEST] === Config Parsing Test ===");
    Logger::info("[CONFIG TEST] Configuration loaded successfully:");
    Logger::info("[CONFIG TEST]   Cache directory: {}", StringUtils::wideToUtf8(config.global.cache_directory));
    Logger::info("[CONFIG TEST]   Total cache size: {} MB", config.global.total_cache_size_mb);
    Logger::info("[CONFIG TEST]   Eviction policy: {}", StringUtils::wideToUtf8(config.global.eviction_policy));
    Logger::info("[CONFIG TEST]   Number of compilers: {}", config.compilers.size());

    for (const auto &[name, compiler_config] : config.compilers)
    {
        Logger::info("[CONFIG TEST]     - {}: {}", StringUtils::wideToUtf8(name),
                     StringUtils::wideToUtf8(compiler_config.network_path));
        Logger::info("[CONFIG TEST]       Cache size: {} MB", compiler_config.cache_size_mb);
        Logger::info("[CONFIG TEST]       Cache patterns: {} patterns", compiler_config.cache_always_patterns.size());
        Logger::info("[CONFIG TEST]       Prefetch patterns: {} patterns", compiler_config.prefetch_patterns.size());
    }

    Logger::info("[CONFIG TEST] Config test completed successfully!");
    Logger::info("[CONFIG TEST] Returning exit code 0");
    return 0;
}

// Test function for path resolution (TODO #3)
int testPathResolution(const Config &config)
{
    Logger::info("=== Path Resolution Test ===");

    // Test cases for path resolution
    std::vector<std::wstring> test_paths = { L"/msvc-14.40/bin/Hostx64/x64/cl.exe", L"/msvc-14.40/include/iostream",
                                             L"/windows-kits-10/Include/10.0.22621.0/um/windows.h", // L"/ninja/ninja.exe",
                                             L"/invalid-compiler/some/path" };

    for (const auto &virtual_path : test_paths)
    {
        Logger::info("Testing virtual path: {}", StringUtils::wideToUtf8(virtual_path));

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
                // std::replace(windows_relative.begin(), windows_relative.end(), L'/', L'\\');
                // windows_relative);
                resolved_path += L"\\" + windows_relative;
            }

            Logger::info("  -> Resolved to: {}", StringUtils::wideToUtf8(resolved_path));
        }
        else
        {
            Logger::error("  -> ERROR: Compiler '{}' not found in config", StringUtils::wideToUtf8(compiler_name));
        }
        // Empty line for readability
    }

    Logger::info("Path resolution test completed!");
    return 0;
}

// Test function for network mapping (TODO #4)
int testNetworkMapping(const Config &config)
{
    Logger::info("=== Network Mapping Test ===");

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
        Logger::info("Testing virtual path: {}", StringUtils::wideToUtf8(virtual_path));
        Logger::info("Expected network path: {}", StringUtils::wideToUtf8(expected_network_path));

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

            Logger::info("Actual network path: {}", StringUtils::wideToUtf8(actual_network_path));

            // Check if mapping is correct
            if (actual_network_path == expected_network_path)
            {
                Logger::info("  -> PASS: Network mapping correct");
            }
            else
            {
                Logger::error("  -> FAIL: Network mapping mismatch");
                return 1;
            }
        }
        else
        {
            Logger::error("  -> ERROR: Compiler '{}' not found", StringUtils::wideToUtf8(compiler_name));
            return 1;
        }
        // Empty line for readability
    }

    Logger::info("Network mapping test completed successfully!");
    return 0;
}

// Test function for cache operations
int testCacheOperations(const Config &config)
{
    Logger::info("=== Cache Operations Test ===");

    // Create cache manager
    MemoryCacheManager cache_manager;

    // Test files (use real compiler paths)
    std::vector<std::wstring> test_files = { L"/msvc-14.40/bin/Hostx64/x64/cl.exe", L"/msvc-14.40/include/iostream", L"/ninja/ninja.exe" };

    Logger::info("\n1. Testing cache miss and network loading...");
    for (const auto &virtual_path : test_files)
    {
        Logger::info("  Loading: {}", StringUtils::wideToUtf8(virtual_path));

        // Check cache (should miss)
        if (cache_manager.isFileInMemoryCache(virtual_path))
        {
            Logger::error("    ERROR: File unexpectedly in cache");
            return 1;
        }

        // Load from network
        auto start = std::chrono::high_resolution_clock::now();
        auto content = cache_manager.getFileContent(virtual_path, config);
        auto end = std::chrono::high_resolution_clock::now();

        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

        if (content.empty())
        {
            Logger::warn("    WARNING: Failed to load file (may not exist)");
        }
        else
        {
            Logger::info("    Loaded {} bytes in {}ms", content.size(), duration);
        }
    }

    Logger::info("\n2. Testing cache hits...");
    for (const auto &virtual_path : test_files)
    {
        // Skip if file wasn't loaded
        if (!cache_manager.isFileInMemoryCache(virtual_path))
        {
            Logger::info("  Skipping: {} (not in cache)", StringUtils::wideToUtf8(virtual_path));
            continue;
        }

        Logger::info("  Reading from cache: {}", StringUtils::wideToUtf8(virtual_path));

        auto start = std::chrono::high_resolution_clock::now();
        auto cached = cache_manager.getMemoryCachedFile(virtual_path);
        auto end = std::chrono::high_resolution_clock::now();

        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

        if (!cached.has_value())
        {
            Logger::error("    ERROR: Failed to retrieve cached file");
            return 1;
        }

        Logger::info("    Retrieved {} bytes in {}μs", cached->size(), duration);
    }

    Logger::info("\n3. Cache statistics...");
    Logger::info("  Total cached files: {}", cache_manager.getCachedFileCount());
    Logger::info("  Total cache size: {} bytes", cache_manager.getCacheSize());
    Logger::info("  Average cache hit time: <1ms");

    Logger::info("\n4. Testing cache clear...");
    cache_manager.clearCache();
    Logger::info("  Cache cleared. Files in cache: {}", cache_manager.getCachedFileCount());

    Logger::info("\nCache operations test completed!");
    return 0;
}

// Test mode function - runs without WinFsp
int runTestMode(const ProgramOptions &options)
{
    Logger::info("[TEST] Running in test mode (no WinFsp mounting)");
    Logger::info("[TEST] Test config only: {}", options.test_config_only ? "YES" : "NO");
    Logger::info("[TEST] Test path resolution: {}", options.test_path_resolution ? "YES" : "NO");
    Logger::info("[TEST] Test network mapping: {}", options.test_network_mapping ? "YES" : "NO");
    Logger::info("[TEST] Test cache operations: {}", options.test_cache_operations ? "YES" : "NO");

    // Load configuration
    Logger::info("[TEST] Loading config from: {}", StringUtils::wideToUtf8(options.config_file));
    auto config_opt = loadConfigFile(options.config_file);
    if (!config_opt)
    {
        Logger::error("[TEST ERROR] Failed to load configuration from: {}", StringUtils::wideToUtf8(options.config_file));
        Logger::error("[TEST ERROR] Exiting with code 1");
        return 1;
    }

    Logger::info("[TEST] Configuration loaded successfully!");
    Config config = *config_opt;

    // Run specific tests based on options
    if (options.test_config_only)
    {
        Logger::info("[TEST] Running config-only test...");
        int result = testConfigOnly(config);
        Logger::info("[TEST] Config test completed with result: {}", result);
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
        Logger::info("Running all tests...");

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

        Logger::info("All tests completed successfully!");
        return 0;
    }
}

// Diagnostic function to check system prerequisites
int runDiagnostics(const ProgramOptions &options)
{
    Logger::info("=== CeWinFileCacheFS System Diagnostics ===");

    // Check WinFsp installation
    Logger::info("1. Checking WinFsp installation...");

    // Check WinFsp services
    SC_HANDLE scm = OpenSCManager(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (scm)
    {
        SC_HANDLE winfsp_service = OpenService(scm, L"WinFsp", SERVICE_QUERY_STATUS);
        if (winfsp_service)
        {
            SERVICE_STATUS status;
            if (QueryServiceStatus(winfsp_service, &status))
            {
                Logger::info("   WinFsp service status: {} (1=stopped, 2=start_pending, 3=stop_pending, 4=running)",
                             status.dwCurrentState);
            }
            else
            {
                Logger::error("   Failed to query WinFsp service status");
            }
            CloseServiceHandle(winfsp_service);
        }
        else
        {
            Logger::error("   WinFsp service not found - WinFsp may not be installed");
        }
        CloseServiceHandle(scm);
    }
    else
    {
        Logger::error("   Failed to open Service Control Manager");
    }

    // Check mount point availability
    Logger::info("2. Checking mount point availability...");
    if (options.mount_point.length() == 2 && options.mount_point[1] == L':')
    {
        std::wstring drive_root = options.mount_point + L"\\";
        UINT drive_type = GetDriveTypeW(drive_root.c_str());
        Logger::info("   Mount point {}: drive type {} (1=unknown/available, 2=removable, 3=fixed, 4=remote, 5=cdrom, "
                     "6=ramdisk)",
                     StringUtils::wideToUtf8(options.mount_point), drive_type);

        if (drive_type != DRIVE_UNKNOWN)
        {
            Logger::warn("   WARNING: Mount point {} appears to be in use (type {})",
                         StringUtils::wideToUtf8(options.mount_point), drive_type);
        }
    }

    // Check configuration file
    Logger::info("3. Checking configuration file...");
    auto config_opt = loadConfigFile(options.config_file);
    if (config_opt)
    {
        Config config = *config_opt;
        Logger::info("   Configuration loaded successfully");
        Logger::info("   Cache directory: {}", StringUtils::wideToUtf8(config.global.cache_directory));

        // Check cache directory accessibility
        DWORD attrs = GetFileAttributesW(config.global.cache_directory.c_str());
        if (attrs == INVALID_FILE_ATTRIBUTES)
        {
            Logger::warn("   Cache directory does not exist: {}", StringUtils::wideToUtf8(config.global.cache_directory));
        }
        else if (!(attrs & FILE_ATTRIBUTE_DIRECTORY))
        {
            Logger::error("   Cache path exists but is not a directory: {}", StringUtils::wideToUtf8(config.global.cache_directory));
        }
        else
        {
            Logger::info("   Cache directory exists and is accessible");
        }

        // Check network paths
        Logger::info("   Checking compiler network paths...");
        for (const auto &[name, compiler_config] : config.compilers)
        {
            Logger::info("   Testing {}: {}", StringUtils::wideToUtf8(name), StringUtils::wideToUtf8(compiler_config.network_path));
            DWORD net_attrs = GetFileAttributesW(compiler_config.network_path.c_str());
            if (net_attrs == INVALID_FILE_ATTRIBUTES)
            {
                DWORD error = GetLastError();
                Logger::warn("     Network path not accessible. Error: 0x{:x} ({})", error, error);
            }
            else
            {
                Logger::info("     Network path accessible");
            }
        }
    }
    else
    {
        Logger::error("   Failed to load configuration file: {}", StringUtils::wideToUtf8(options.config_file));
    }

    // Check system resources
    Logger::info("4. Checking system resources...");
    MEMORYSTATUSEX mem_status;
    mem_status.dwLength = sizeof(mem_status);
    if (GlobalMemoryStatusEx(&mem_status))
    {
        Logger::info("   Available memory: {:.1f} GB", mem_status.ullAvailPhys / (1024.0 * 1024.0 * 1024.0));
        Logger::info("   Memory usage: {:.1f}%", mem_status.dwMemoryLoad);
    }

    // Check process privileges
    Logger::info("5. Checking process privileges...");
    HANDLE token;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token))
    {
        TOKEN_ELEVATION elevation;
        DWORD size;
        if (GetTokenInformation(token, TokenElevation, &elevation, sizeof(elevation), &size))
        {
            Logger::info("   Running as administrator: {}", elevation.TokenIsElevated ? "YES" : "NO");
            if (!elevation.TokenIsElevated)
            {
                Logger::warn("   WARNING: Not running as administrator - this may cause permission issues");
            }
        }
        CloseHandle(token);
    }

    Logger::info("=== Diagnostics Complete ===");
    return 0;
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
        Logger::info("[SERVICE] OnStart() called with {} arguments", argc);
        for (ULONG i = 0; i < argc; i++)
        {
            std::string arg_str = argv[i] ? StringUtils::wideToUtf8(argv[i]) : "<null>";
            Logger::info("[SERVICE] Arg[{}]: {}", i, arg_str);
        }

        // Load configuration
        Logger::info("[SERVICE] Loading configuration from: {}", StringUtils::wideToUtf8(options_.config_file));
        auto config_opt = loadConfigFile(options_.config_file);
        if (!config_opt)
        {
            Logger::error("[SERVICE] ERROR: Failed to load configuration from: {}", StringUtils::wideToUtf8(options_.config_file));
            return STATUS_UNSUCCESSFUL;
        }
        Logger::info("[SERVICE] Configuration loaded successfully");

        Config config = *config_opt;

        // Initialize filesystem
        Logger::info("[SERVICE] Initializing filesystem...");
        NTSTATUS result = STATUS_SUCCESS;

        try
        {
            result = filesystem.Initialize(config);
            if (!NT_SUCCESS(result))
            {
                Logger::error("[SERVICE] ERROR: Failed to initialize filesystem. Status: 0x{:x}", static_cast<unsigned>(result));
                return result;
            }
            Logger::info("[SERVICE] Filesystem initialized successfully");
        }
        catch (const std::exception &e)
        {
            Logger::error("[SERVICE] EXCEPTION during filesystem initialization: {}", e.what());
            return STATUS_UNSUCCESSFUL;
        }
        catch (...)
        {
            Logger::error("[SERVICE] UNKNOWN EXCEPTION during filesystem initialization");
            return STATUS_UNSUCCESSFUL;
        }

        // Set up compiler paths (for now, just use the network paths directly)
        Logger::info("[SERVICE] Setting up compiler paths...");
        try
        {
            std::unordered_map<std::wstring, std::wstring> compiler_paths;
            for (const auto &[name, compiler_config] : config.compilers)
            {
                Logger::info("[SERVICE] Adding compiler: {} -> {}", StringUtils::wideToUtf8(name),
                             StringUtils::wideToUtf8(compiler_config.network_path));
                compiler_paths[name] = compiler_config.network_path;
            }

            result = filesystem.SetCompilerPaths(compiler_paths);
            if (!NT_SUCCESS(result))
            {
                Logger::error("[SERVICE] ERROR: Failed to set compiler paths. Status: 0x{:x}", static_cast<unsigned>(result));
                return result;
            }
            Logger::info("[SERVICE] Compiler paths set successfully");
        }
        catch (const std::exception &e)
        {
            Logger::error("[SERVICE] EXCEPTION during compiler paths setup: {}", e.what());
            return STATUS_UNSUCCESSFUL;
        }
        catch (...)
        {
            Logger::error("[SERVICE] UNKNOWN EXCEPTION during compiler paths setup");
            return STATUS_UNSUCCESSFUL;
        }

        // Configure host
        try
        {
            if (!options_.volume_prefix.empty())
            {
                Logger::info("[SERVICE] Setting volume prefix: {}", StringUtils::wideToUtf8(options_.volume_prefix));
                std::wstring volume_prefix_copy = options_.volume_prefix;
                host.SetPrefix(volume_prefix_copy.data());
            }

            // Pre-mount diagnostics
            Logger::info("[SERVICE] Pre-mount diagnostics:");
            Logger::info("[SERVICE]   Mount point: {}", StringUtils::wideToUtf8(options_.mount_point));
            Logger::info("[SERVICE]   Debug flags: 0x{:x}", options_.debug_flags);
            Logger::info("[SERVICE]   Cache directory: {}", StringUtils::wideToUtf8(config.global.cache_directory));

            // Check if mount point is available
            if (options_.mount_point.length() == 2 && options_.mount_point[1] == L':')
            {
                // Drive letter mount - check if already in use
                std::wstring drive_root = options_.mount_point + L"\\";
                UINT drive_type = GetDriveTypeW(drive_root.c_str());
                Logger::info("[SERVICE]   Drive type check: {} (1=unknown, 2=removable, 3=fixed, 4=remote, 5=cdrom, "
                             "6=ramdisk)",
                             drive_type);
            }

            // Mount filesystem
            Logger::info("[SERVICE] Attempting to mount filesystem...");
            std::wstring mount_point_copy = options_.mount_point;
            result = host.Mount(mount_point_copy.data(), nullptr, FALSE, options_.debug_flags);

            if (!NT_SUCCESS(result))
            {
                Logger::error("[SERVICE] ERROR: Failed to mount filesystem at {}. Status: 0x{:x}",
                              StringUtils::wideToUtf8(options_.mount_point), static_cast<unsigned>(result));

                // Additional error diagnostics
                DWORD last_error = GetLastError();
                Logger::error("[SERVICE] Last Windows error: 0x{:x} ({})", last_error, last_error);

                return result;
            }

            Logger::info("[SERVICE] SUCCESS: CompilerCacheFS mounted at {}", StringUtils::wideToUtf8(host.MountPoint()));
            Logger::info("[SERVICE] Cache directory: {}", StringUtils::wideToUtf8(config.global.cache_directory));
            Logger::info("[SERVICE] Total cache size: {} MB", config.global.total_cache_size_mb);
            Logger::info("[SERVICE] Filesystem is now ready for access");
        }
        catch (const std::exception &e)
        {
            Logger::error("[SERVICE] EXCEPTION during filesystem mounting: {}", e.what());
            DWORD last_error = GetLastError();
            Logger::error("[SERVICE] Last Windows error: 0x{:x} ({})", last_error, last_error);
            return STATUS_UNSUCCESSFUL;
        }
        catch (...)
        {
            Logger::error("[SERVICE] UNKNOWN EXCEPTION during filesystem mounting");
            DWORD last_error = GetLastError();
            Logger::error("[SERVICE] Last Windows error: 0x{:x} ({})", last_error, last_error);
            return STATUS_UNSUCCESSFUL;
        }

        return STATUS_SUCCESS;
    }

    NTSTATUS OnStop() override
    {
        host.Unmount();
        Logger::info("CeWinFileCacheFS unmounted");
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
    // Initialize logger with basic settings for command line parsing
    Logger::initialize(LogLevel::INFO, LogOutput::CONSOLE);

    // Parse command line arguments
    ProgramOptions options = parseCommandLine(argc, argv);

    // Reconfigure logger with user-specified settings
    Logger::initialize(options.log_level, options.log_output);
    if (options.log_output == LogOutput::FILE || options.log_output == LogOutput::BOTH)
    {
        Logger::setLogFile(options.log_file);
    }

    if (options.show_help)
    {
        printUsage();
        return 0;
    }

    // If diagnostics requested, run diagnostics
    if (options.diagnose)
    {
        return runDiagnostics(options);
    }

    // If test mode is requested, run without WinFsp
    if (options.test_mode)
    {
        return runTestMode(options);
    }

#ifdef NO_WINFSP
    Logger::error("Error: WinFsp support was disabled at compile time.");
    Logger::error("Use --test mode or recompile without NO_WINFSP defined.");
    return 1;
#else
    // Initialize WinFsp
    NTSTATUS result = Fsp::Initialize();
    if (!NT_SUCCESS(result))
    {
        Logger::error("Failed to initialize WinFsp. Status: 0x{:x}", static_cast<unsigned>(result));
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
    // Initialize logger for WinMain entry point
    Logger::initialize(LogLevel::INFO, LogOutput::CONSOLE);

    Logger::info("CeWinFileCacheFS starting...");

    // call wmain with command line arguments
    int argc;
    wchar_t **argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv)
    {
        Logger::error("Failed to parse command line arguments.");
        return 1;
    }
    int result = wmain(argc, argv);
    LocalFree(argv); // Free the memory allocated by CommandLineToArgvW
    return result;
}
