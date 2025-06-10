#include <ce-win-file-cache/config_parser.hpp>
#include <ce-win-file-cache/hybrid_filesystem.hpp>
#include <iostream>
#include <shellapi.h>
#include <string>
#include <winfsp/winfsp.hpp>

using namespace CeWinFileCache;

class CompilerCacheService : public Fsp::Service
{
    public:
    CompilerCacheService(wchar_t *ServiceName) : Fsp::Service(ServiceName), filesystem_(), host_(filesystem_)
    {
    }

    protected:
    NTSTATUS OnStart(ULONG argc, PWSTR *argv) override
    {
        std::wstring config_file = L"compilers.yaml";
        std::wstring mount_point = L"M:";
        std::wstring volume_prefix;
        ULONG debug_flags = 0;

        // Parse command line arguments
        for (ULONG i = 1; i < argc; i++)
        {
            std::wstring arg(argv[i]);

            if (arg == L"-c" || arg == L"--config")
            {
                if (i + 1 < argc)
                {
                    config_file = argv[++i];
                }
                else
                {
                    std::wcerr << L"Error: --config requires a file path" << std::endl;
                    return STATUS_INVALID_PARAMETER;
                }
            }
            else if (arg == L"-m" || arg == L"--mount")
            {
                if (i + 1 < argc)
                {
                    mount_point = argv[++i];
                }
                else
                {
                    std::wcerr << L"Error: --mount requires a mount point" << std::endl;
                    return STATUS_INVALID_PARAMETER;
                }
            }
            else if (arg == L"-u" || arg == L"--volume-prefix")
            {
                if (i + 1 < argc)
                {
                    volume_prefix = argv[++i];
                }
                else
                {
                    std::wcerr << L"Error: --volume-prefix requires a prefix" << std::endl;
                    return STATUS_INVALID_PARAMETER;
                }
            }
            else if (arg == L"-d" || arg == L"--debug")
            {
                if (i + 1 < argc)
                {
                    debug_flags = wcstoul(argv[++i], nullptr, 0);
                }
                else
                {
                    debug_flags = static_cast<ULONG>(-1); // Enable all debug
                }
            }
            else if (arg == L"-h" || arg == L"--help")
            {
                printUsage();
                return STATUS_SUCCESS;
            }
            else
            {
                std::wcerr << L"Unknown argument: " << arg << std::endl;
                printUsage();
                return STATUS_INVALID_PARAMETER;
            }
        }

        // Load configuration
        auto config_opt = ConfigParser::parseYamlFile(config_file);
        if (!config_opt)
        {
            std::wcerr << L"Failed to load configuration from: " << config_file << std::endl;
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
        if (!volume_prefix.empty())
        {
            // todo: find a nice way to convert from const wchar_t* to wchar_t*
            host_.SetPrefix(volume_prefix.c_str());
        }

        // Mount filesystem
        result = host_.Mount(mount_point.c_str(), nullptr, FALSE, debug_flags);
        if (!NT_SUCCESS(result))
        {
            std::wcerr << L"Failed to mount filesystem at " << mount_point << L". Status: 0x" << std::hex << result << std::endl;
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
    void printUsage()
    {
        std::wcout << L"Usage: CeWinFileCacheFS [OPTIONS]\n"
                   << L"\n"
                   << L"Options:\n"
                   << L"  -c, --config FILE      Configuration file (default: compilers.yaml)\n"
                   << L"  -m, --mount POINT      Mount point (default: M:)\n"
                   << L"  -u, --volume-prefix    Volume prefix for UNC paths\n"
                   << L"  -d, --debug [LEVEL]    Enable debug logging\n"
                   << L"  -h, --help             Show this help message\n"
                   << L"\n"
                   << L"Examples:\n"
                   << L"  CeWinFileCacheFS --config compilers.yaml --mount M:\n"
                   << L"  CeWinFileCacheFS --mount C:\\compilers --debug\n"
                   << std::endl;
    }

    HybridFileSystem filesystem_;
    Fsp::FileSystemHost host_;
};

int wmain(int argc, wchar_t **argv)
{
    // Initialize WinFsp
    NTSTATUS result = Fsp::Initialize();
    if (!NT_SUCCESS(result))
    {
        std::wcerr << L"Failed to initialize WinFsp. Status: 0x" << std::hex << result << std::endl;
        return 1;
    }

    // Create and run service
    wchar_t service_name[] = L"CeWinFileCacheFS";
    CompilerCacheService service{ service_name };
    return service.Run();
};

int APIENTRY WinMain(HINSTANCE hInst, HINSTANCE hInstPrev, PSTR cmdline, int cmdshow)
{
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
