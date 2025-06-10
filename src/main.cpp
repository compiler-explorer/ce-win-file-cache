#include <ce-win-file-cache/config_parser.hpp>
#include <ce-win-file-cache/hybrid_filesystem.hpp>
#include <iostream>
#include <shellapi.h>
#include <string>
#include <unordered_map>

#ifndef NO_WINFSP
#include <winfsp/winfsp.hpp>
#endif

#include <codecvt>

using namespace CeWinFileCache;

// Command line parsing structure
struct ProgramOptions
{
    std::wstring config_file = L"compilers.yaml";
    std::wstring mount_point = L"M:";
    std::wstring volume_prefix;
    ULONG debug_flags = 0;
    bool show_help = false;
    bool test_mode = false;
};

// Function to print usage information
void printUsage()
{
    std::wcout << L"Usage: CeWinFileCacheFS [OPTIONS]\n"
               << L"\n"
               << L"Options:\n"
               << L"  -c, --config FILE      Configuration file (default: compilers.yaml)\n"
               << L"  -m, --mount POINT      Mount point (default: M:)\n"
               << L"  -u, --volume-prefix    Volume prefix for UNC paths\n"
               << L"  -d, --debug [LEVEL]    Enable debug logging\n"
               << L"  -t, --test             Test mode (no WinFsp mounting)\n"
               << L"  -h, --help             Show this help message\n"
               << L"\n"
               << L"Examples:\n"
               << L"  CeWinFileCacheFS --config compilers.yaml --mount M:\n"
               << L"  CeWinFileCacheFS --mount C:\\compilers --debug\n"
               << L"  CeWinFileCacheFS --test --config test.yaml\n"
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
            if (i + 1 < argc)
            {
                options.config_file = argv[++i];
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
            if (i + 1 < argc)
            {
                options.mount_point = argv[++i];
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
            if (i + 1 < argc)
            {
                options.volume_prefix = argv[++i];
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
                options.debug_flags = wcstoul((wchar_t *)argv[++i], nullptr, 0);
            }
            else
            {
                options.debug_flags = static_cast<ULONG>(-1); // Enable all debug
            }
        }
        else if (arg == L"-t" || arg == L"--test")
        {
            options.test_mode = true;
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

// Test mode function - runs without WinFsp
int runTestMode(const ProgramOptions &options)
{
    std::wcout << L"Running in test mode (no WinFsp mounting)" << std::endl;

    // Load configuration
    auto config_opt = ConfigParser::parseYamlFile(options.config_file);
    if (!config_opt)
    {
        std::wcerr << L"Failed to load configuration from: " << options.config_file << std::endl;
        return 1;
    }

    Config config = *config_opt;

    // Note: Initialize method requires WinFsp types, so we'll just test config parsing for now
    std::wcout << L"Configuration loaded successfully:" << std::endl;
    std::wcout << L"  Cache directory: " << config.global.cache_directory << std::endl;
    std::wcout << L"  Total cache size: " << config.global.total_cache_size_mb << L" MB" << std::endl;
    std::wcout << L"  Eviction policy: " << config.global.eviction_policy << std::endl;
    std::wcout << L"  Number of compilers: " << config.compilers.size() << std::endl;

    for (const auto &[name, compiler_config] : config.compilers)
    {
        std::wcout << L"    - " << name << L": " << compiler_config.network_path << std::endl;
    }

    std::wcout << L"Test completed successfully!" << std::endl;
    return 0;
}

#ifndef NO_WINFSP
class CompilerCacheService : public Fsp::Service
{
    public:
    CompilerCacheService(wchar_t *ServiceName, const ProgramOptions &options)
    : Fsp::Service(ServiceName), filesystem_(), host_(filesystem_), options_(options)
    {
    }

    protected:
    NTSTATUS OnStart(ULONG argc, PWSTR *argv) override
    {
        // Load configuration
        auto config_opt = ConfigParser::parseYamlFile(options_.config_file);
        if (!config_opt)
        {
            std::wcerr << L"Failed to load configuration from: " << options_.config_file << std::endl;
            return STATUS_UNSUCCESSFUL;
        }

        Config config = *config_opt;

        // Initialize filesystem
        NTSTATUS result = filesystem_.Initialize(config);
        if (!NT_SUCCESS(result))
        {
            std::wcerr << L"Failed to initialize filesystem. Status: 0x" << std::hex << result << std::endl;
            return result;
        }

        // Set up compiler paths (for now, just use the network paths directly)
        std::unordered_map<std::wstring, std::wstring> compiler_paths;
        for (const auto &[name, compiler_config] : config.compilers)
        {
            compiler_paths[name] = compiler_config.network_path;
        }

        result = filesystem_.SetCompilerPaths(compiler_paths);
        if (!NT_SUCCESS(result))
        {
            std::wcerr << L"Failed to set compiler paths. Status: 0x" << std::hex << result << std::endl;
            return result;
        }

        // Configure host
        if (!options_.volume_prefix.empty())
        {
            std::wstring volume_prefix_copy = options_.volume_prefix;
            host_.SetPrefix(volume_prefix_copy.data());
        }

        // Mount filesystem
        result = host_.Mount(options_.mount_point.c_str(), nullptr, FALSE, options_.debug_flags);
        if (!NT_SUCCESS(result))
        {
            std::wcerr << L"Failed to mount filesystem at " << options_.mount_point << L". Status: 0x" << std::hex
                       << result << std::endl;
            return result;
        }

        std::wcout << L"CompilerCacheFS mounted at " << host_.MountPoint() << std::endl;
        std::wcout << L"Cache directory: " << config.global.cache_directory << std::endl;
        std::wcout << L"Total cache size: " << config.global.total_cache_size_mb << L" MB" << std::endl;

        return STATUS_SUCCESS;
    }

    NTSTATUS OnStop() override
    {
        host_.Unmount();
        std::wcout << L"CeWinFileCacheFS unmounted" << std::endl;
        return STATUS_SUCCESS;
    }

    private:
    HybridFileSystem filesystem_;
    Fsp::FileSystemHost host_;
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
};
