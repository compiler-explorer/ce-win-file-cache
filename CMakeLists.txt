cmake_minimum_required(VERSION 3.20)
project(CompilerCacheFS VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Use WinFsp submodule
if(WIN32)
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

if(WIN32)
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

set(HEADERS
    include/compiler_cache/hybrid_filesystem.hpp
    include/compiler_cache/cache_manager.hpp
    include/compiler_cache/network_client.hpp
    include/compiler_cache/config_parser.hpp
    types/file_state.hpp
    types/config.hpp
    types/cache_entry.hpp
)

# Create executable
add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

# Windows-specific settings
if(WIN32)
    target_link_libraries(${PROJECT_NAME} 
        ${WINFSP_LIBRARY}
        Userenv.lib
        netapi32.lib
        mpr.lib
    )
    
    # Set compiler flags
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        _CONSOLE
        UNICODE
        _UNICODE
        WIN32_LEAN_AND_MEAN
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

# Set output directory
set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Create build directories
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/cache)