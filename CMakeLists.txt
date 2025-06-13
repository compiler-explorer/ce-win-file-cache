cmake_minimum_required(VERSION 3.20)
project(CeWinFileCacheFS VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Option to enable Prometheus metrics
option(ENABLE_METRICS "Enable Prometheus metrics collection" ON)

# Option to use Wine's Windows API implementation for cross-compilation
option(USE_WINE_WINDOWS_API "Use Wine's Windows API implementation for cross-compilation" OFF)

if(USE_WINE_WINDOWS_API)
    message(STATUS "Using Wine Windows API implementation")
    set(CMAKE_SYSTEM_NAME Windows)
    set(CMAKE_C_COMPILER winegcc)
    set(CMAKE_CXX_COMPILER wineg++)
    
    # Wine-specific settings
    add_definitions(-DWINE_CROSS_COMPILE)
    add_definitions(-D_WIN32_WINNT=0x0A00)  # Windows 10
    add_definitions(-DNTDDI_VERSION=0x0A000000)
    
    # Wine headers path (common locations)
    find_path(WINE_INCLUDE_DIR 
        NAMES windows.h
        PATHS
            /usr/include/wine/wine/windows
            /usr/include/wine/windows
            /usr/local/include/wine/windows
            /opt/wine-stable/include/wine/windows
            /usr/include/wine-development/windows
        DOC "Wine Windows headers directory"
    )
    
    if(WINE_INCLUDE_DIR)
        include_directories(SYSTEM ${WINE_INCLUDE_DIR})
        # Add our Wine stub headers for missing Windows headers
        include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include/wine_stubs)
        message(STATUS "Found Wine headers: ${WINE_INCLUDE_DIR}")
        message(STATUS "Using Wine stub headers: ${CMAKE_CURRENT_SOURCE_DIR}/include/wine_stubs")
    else()
        message(FATAL_ERROR "Wine Windows headers not found. Please install wine-dev or wine-devel package")
    endif()
    
    # Wine libraries (winegcc handles linking automatically for most libraries)
    set(WINE_LIBRARIES "")
    
    # Common Wine library paths
    set(WINE_LIB_PATHS 
        /usr/lib/wine 
        /usr/local/lib/wine 
        /usr/lib/x86_64-linux-gnu/wine
        /usr/lib64/wine
    )
    
    # Try to find Wine libraries, but they're optional as winegcc often handles them
    find_library(WINE_KERNEL32 NAMES kernel32 PATHS ${WINE_LIB_PATHS})
    find_library(WINE_USER32 NAMES user32 PATHS ${WINE_LIB_PATHS})
    find_library(WINE_ADVAPI32 NAMES advapi32 PATHS ${WINE_LIB_PATHS})
    find_library(WINE_NETAPI32 NAMES netapi32 PATHS ${WINE_LIB_PATHS})
    find_library(WINE_MPR NAMES mpr PATHS ${WINE_LIB_PATHS})
    
    # Add found libraries to the list
    foreach(lib WINE_KERNEL32 WINE_USER32 WINE_ADVAPI32 WINE_NETAPI32 WINE_MPR)
        if(${lib})
            list(APPEND WINE_LIBRARIES ${${lib}})
            message(STATUS "Found ${lib}: ${${lib}}")
        else()
            message(STATUS "${lib} not found - will rely on winegcc automatic linking")
        endif()
    endforeach()
    
    # Wine typically provides these through winegcc, so we can add them as link flags
    if(NOT WINE_LIBRARIES)
        set(WINE_LINK_FLAGS "-lkernel32 -luser32 -ladvapi32 -lnetapi32 -lmpr -lshell32 -lshlwapi -lole32 -loleaut32")
        message(STATUS "Using Wine link flags: ${WINE_LINK_FLAGS}")
    endif()
endif()

# Use WinFsp - prefer system installation over submodule
if(WIN32 OR USE_WINE_WINDOWS_API)
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(WINFSP_ARCH "x64")
    else()
        set(WINFSP_ARCH "x86") 
    endif()
    
    # First try to find system WinFsp installation using Windows paths without spaces
    if(EXISTS "C:/PROGRA~2/WinFsp/lib/winfsp-${WINFSP_ARCH}.lib")
        # Set Windows paths directly for MSVC compiler (using short paths to avoid parentheses)
        set(WINFSP_LIBRARY "C:/PROGRA~2/WinFsp/lib/winfsp-${WINFSP_ARCH}.lib")
        set(WINFSP_INCLUDE_DIR "C:/PROGRA~2/WinFsp/inc")
        # Use submodule for C++ headers since system installation only has C headers
        set(WINFSP_CPP_INCLUDE_DIR "D:/opt/ce-win-file-cache/external/winfsp/inc")
        set(WINFSP_FOUND TRUE)
        message(STATUS "Found system WinFsp at C:/PROGRA~2/WinFsp")
        message(STATUS "Using C++ headers from submodule: ${WINFSP_CPP_INCLUDE_DIR}")
    elseif(EXISTS "C:/PROGRA~1/WinFsp/lib/winfsp-${WINFSP_ARCH}.lib")
        # Set Windows paths directly for MSVC compiler (using short paths to avoid parentheses)
        set(WINFSP_LIBRARY "C:/PROGRA~1/WinFsp/lib/winfsp-${WINFSP_ARCH}.lib")
        set(WINFSP_INCLUDE_DIR "C:/PROGRA~1/WinFsp/inc")
        # Use submodule for C++ headers since system installation only has C headers
        set(WINFSP_CPP_INCLUDE_DIR "D:/opt/ce-win-file-cache/external/winfsp/inc")
        set(WINFSP_FOUND TRUE)
        message(STATUS "Found system WinFsp at C:/PROGRA~1/WinFsp")
        message(STATUS "Using C++ headers from submodule: ${WINFSP_CPP_INCLUDE_DIR}")
    else()
        set(WINFSP_FOUND FALSE)
    endif()
    
    # If system installation not found, fall back to submodule
    if(NOT WINFSP_FOUND)
        message(STATUS "System WinFsp not found, trying submodule...")
        set(WINFSP_ROOT "D:/opt/ce-win-file-cache/external/winfsp")
        set(WINFSP_INCLUDE_DIR "${WINFSP_ROOT}/inc")
        set(WINFSP_CPP_INCLUDE_DIR "${WINFSP_ROOT}/inc")
        set(WINFSP_LIBRARY "${WINFSP_ROOT}/opt/fsext/lib/winfsp-${WINFSP_ARCH}.lib")
        
        if(NOT EXISTS "D:/opt/ce-win-file-cache/external/winfsp/inc/winfsp/winfsp.h")
            message(FATAL_ERROR "WinFsp not found. Please install WinFsp or run: git submodule update --init --recursive")
        endif()
        
        if(NOT EXISTS "D:/opt/ce-win-file-cache/external/winfsp/opt/fsext/lib/winfsp-${WINFSP_ARCH}.lib")
            message(FATAL_ERROR "WinFsp library not found")
        endif()
        
        message(STATUS "Using WinFsp from submodule: ${WINFSP_ROOT}")
    else()
        message(STATUS "Using system WinFsp installation")
    endif()
    
    message(STATUS "WinFsp include dir: ${WINFSP_INCLUDE_DIR}")
    message(STATUS "WinFsp library: ${WINFSP_LIBRARY}")
endif()

# Prometheus metrics support
if(ENABLE_METRICS)
    # First try to find system-installed prometheus-cpp
    find_package(prometheus-cpp CONFIG QUIET)
    if(prometheus-cpp_FOUND)
        message(STATUS "Found system prometheus-cpp, enabling metrics support")
        set(HAVE_PROMETHEUS TRUE)
    else()
        message(STATUS "System prometheus-cpp not found, downloading via FetchContent...")
        
        include(FetchContent)
        
        # Download and build prometheus-cpp
        FetchContent_Declare(
            prometheus-cpp
            GIT_REPOSITORY https://github.com/jupp0r/prometheus-cpp.git
            GIT_TAG        v1.3.0  # Use a stable release
            GIT_SHALLOW    TRUE
        )
        
        # Configure prometheus-cpp options
        set(ENABLE_PUSH OFF CACHE BOOL "" FORCE)  # We only need pull metrics
        set(ENABLE_COMPRESSION OFF CACHE BOOL "" FORCE)  # Disable compression for simplicity
        set(ENABLE_TESTING OFF CACHE BOOL "" FORCE)  # Don't build tests
        set(USE_THIRDPARTY_LIBRARIES ON CACHE BOOL "" FORCE)  # Use bundled dependencies
        set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)  # Build static libraries
        
        FetchContent_MakeAvailable(prometheus-cpp)
        
        message(STATUS "prometheus-cpp downloaded and configured successfully")
        set(HAVE_PROMETHEUS TRUE)
    endif()
else()
    message(STATUS "Metrics support disabled")
    set(HAVE_PROMETHEUS FALSE)
endif()

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if(WIN32 OR USE_WINE_WINDOWS_API)
    include_directories(SYSTEM ${WINFSP_INCLUDE_DIR})
    # Add C++ headers if available (for system installation + submodule headers)
    if(WINFSP_CPP_INCLUDE_DIR)
        include_directories(SYSTEM ${WINFSP_CPP_INCLUDE_DIR})
    endif()
endif()

# Source files
set(SOURCES
    src/hybrid_filesystem.cpp
    src/cache_manager.cpp
    src/memory_cache_manager.cpp
    src/directory_tree.cpp
    src/directory_cache.cpp
    src/network_client.cpp
    src/async_download_manager.cpp
    src/config_parser.cpp
    src/main.cpp
)

# Add metrics source if prometheus is available
if(HAVE_PROMETHEUS)
    list(APPEND SOURCES src/metrics_collector.cpp)
endif()

# Add Wine-specific stubs when using Wine
if(USE_WINE_WINDOWS_API)
    list(APPEND SOURCES src/wine_stubs.cpp)
endif()

# Only build main executable on Windows or with Wine
if(NOT APPLE OR USE_WINE_WINDOWS_API)
    # Create executable
    add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})
else()
    message(STATUS "Skipping main CeWinFileCacheFS target on macOS (use cache_test instead)")
endif()

# Windows-specific settings (native Windows or Wine)
if((WIN32 OR USE_WINE_WINDOWS_API) AND TARGET ${PROJECT_NAME})
    # Set compiler definitions
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        _CONSOLE
        UNICODE
        _UNICODE
        WIN32_LEAN_AND_MEAN
    )
    
    # Add Prometheus definitions if available
    if(HAVE_PROMETHEUS)
        target_compile_definitions(${PROJECT_NAME} PRIVATE HAVE_PROMETHEUS)
    endif()
    
    if(USE_WINE_WINDOWS_API)
        # Wine-specific settings
        if(WINE_LIBRARIES)
            target_link_libraries(${PROJECT_NAME} 
                ${WINFSP_LIBRARY}
                ${WINE_LIBRARIES}
            )
        else()
            # Use target_link_libraries for proper ordering instead of LINK_FLAGS
            target_link_libraries(${PROJECT_NAME} 
                ${WINFSP_LIBRARY}
                kernel32 user32 advapi32 netapi32 mpr shell32 shlwapi ole32 oleaut32
            )
        endif()
        
        # Add Prometheus libraries for Wine builds
        if(HAVE_PROMETHEUS)
            target_link_libraries(${PROJECT_NAME} 
                prometheus-cpp::core 
                prometheus-cpp::pull
            )
        endif()
        
        # Wine-compatible compiler flags
        target_compile_options(${PROJECT_NAME} PRIVATE
            -Wall -Wextra -Wno-unused-parameter -Wno-unknown-pragmas -Wno-attributes -fpermissive -mconsole
            $<$<CONFIG:Release>:-O3>
        )
        
        # Additional Wine-specific link flags (only export-dynamic now)
        set_target_properties(${PROJECT_NAME} PROPERTIES
            LINK_FLAGS "-Wl,--export-dynamic"
        )
        
        # Define architecture macros for WinFsp compatibility
        if(CMAKE_SIZEOF_VOID_P EQUAL 8)
            target_compile_definitions(${PROJECT_NAME} PRIVATE _AMD64_)
        else()
            target_compile_definitions(${PROJECT_NAME} PRIVATE _X86_)
        endif()
        
        message(STATUS "Configured for Wine compilation")
    else()
        # Native Windows settings
        target_link_libraries(${PROJECT_NAME} 
            ${WINFSP_LIBRARY}
            UserEnv.lib
            netapi32.lib
            mpr.lib
            shell32.lib
            shlwapi.lib
            ole32.lib
            oleaut32.lib
        )
        
        # Add Prometheus libraries for native Windows builds
        if(HAVE_PROMETHEUS)
            target_link_libraries(${PROJECT_NAME} 
                prometheus-cpp::core 
                prometheus-cpp::pull
            )
        endif()
        
        # Enable strict warnings as per CLAUDE.md
        target_compile_options(${PROJECT_NAME} PRIVATE
            /W4 /wd4464 /wd5039 /wd4005 /wd4820 /wd4625 /wd4626 /wd5219 /wd4244 /wd4100 /wd4815
            /Zc:strictStrings-
            $<$<CONFIG:Release>:/O2 /GL>
        )
        
        # Enable LTO for release builds (disabled for cross-compilation compatibility)
        # if(CMAKE_BUILD_TYPE STREQUAL "Release")
        #     set_property(TARGET ${PROJECT_NAME} PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
        # endif()
        
        # Disable embedded manifest to avoid RC issues for now
        set_target_properties(${PROJECT_NAME} PROPERTIES
            LINK_FLAGS "/MANIFEST:NO"
        )
    endif()
endif()

# Set output directory for main target if it exists
if(TARGET ${PROJECT_NAME})
    set_target_properties(${PROJECT_NAME} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )
endif()

# Create build directories
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/cache)

# ========================================
# Test programs
# ========================================
option(BUILD_CACHE_TEST "Build the cache test program" OFF)

if(BUILD_CACHE_TEST OR APPLE)
    message(STATUS "Building test programs")
    add_subdirectory(src/test)
endif()