cmake_minimum_required(VERSION 3.20)
project(CompilerCacheFS VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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
        include_directories(${WINE_INCLUDE_DIR})
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

# Use WinFsp submodule
if(WIN32 OR USE_WINE_WINDOWS_API)
    set(WINFSP_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/external/winfsp")
    set(WINFSP_INCLUDE_DIR "${WINFSP_ROOT}/inc")
    
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(WINFSP_ARCH "x64")
    else()
        set(WINFSP_ARCH "x86") 
    endif()
    
    # Check if we have prebuilt libraries
    set(WINFSP_LIBRARY "${WINFSP_ROOT}/opt/fsext/lib/winfsp-${WINFSP_ARCH}.lib")
    
    if(NOT EXISTS ${WINFSP_INCLUDE_DIR}/winfsp/winfsp.h)
        message(FATAL_ERROR "WinFsp submodule not found. Run: git submodule update --init --recursive")
    endif()
    
    if(NOT EXISTS ${WINFSP_LIBRARY})
        message(WARNING "WinFsp prebuilt library not found at ${WINFSP_LIBRARY}")
        message(STATUS "You may need to build WinFsp or install the WinFsp development package")
        # Fall back to system installation
        find_library(WINFSP_LIBRARY
            NAMES winfsp-${WINFSP_ARCH}
            HINTS "C:/Program Files (x86)/WinFsp/lib"
            DOC "WinFsp library"
        )
        if(NOT WINFSP_LIBRARY)
            message(FATAL_ERROR "WinFsp library not found. Please build WinFsp or install WinFsp development package")
        endif()
    endif()
    
    message(STATUS "Using WinFsp from submodule: ${WINFSP_ROOT}")
    message(STATUS "WinFsp include dir: ${WINFSP_INCLUDE_DIR}")
    message(STATUS "WinFsp library: ${WINFSP_LIBRARY}")
endif()

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/types
    ${CMAKE_CURRENT_SOURCE_DIR}/utils
)

if(WIN32 OR USE_WINE_WINDOWS_API)
    include_directories(${WINFSP_INCLUDE_DIR})
endif()

# Source files
set(SOURCES
    src/hybrid_filesystem.cpp
    src/cache_manager.cpp
    src/network_client.cpp
    src/config_parser.cpp
    src/main.cpp
)

# Add Wine-specific stubs when using Wine
if(USE_WINE_WINDOWS_API)
    list(APPEND SOURCES src/wine_stubs.cpp)
endif()

set(HEADERS
    include/compiler_cache/hybrid_filesystem.hpp
    include/compiler_cache/cache_manager.hpp
    include/compiler_cache/network_client.hpp
    include/compiler_cache/config_parser.hpp
    include/compiler_cache/wine_compat.hpp
    types/file_state.hpp
    types/config.hpp
    types/cache_entry.hpp
)

# Create executable
add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

# Windows-specific settings (native Windows or Wine)
if(WIN32 OR USE_WINE_WINDOWS_API)
    # Set compiler definitions
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        _CONSOLE
        UNICODE
        _UNICODE
        WIN32_LEAN_AND_MEAN
    )
    
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
            Userenv.lib
            netapi32.lib
            mpr.lib
            shell32.lib
            shlwapi.lib
            ole32.lib
            oleaut32.lib
        )
        
        # Enable strict warnings as per CLAUDE.md
        target_compile_options(${PROJECT_NAME} PRIVATE
            /W4 /WX /Wall
            $<$<CONFIG:Release>:/O2 /GL>
        )
        
        # Enable LTO for release builds
        if(CMAKE_BUILD_TYPE STREQUAL "Release")
            set_property(TARGET ${PROJECT_NAME} PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
        endif()
    endif()
endif()

# Set output directory
set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Create build directories
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/cache)