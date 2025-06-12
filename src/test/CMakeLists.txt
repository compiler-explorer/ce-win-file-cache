# Test programs CMakeLists.txt

# Test program sources and targets
set(TEST_PROGRAMS
    cache_test
    cache_demo
    directory_test
    async_test
    filesystem_async_test
    config_threads_test
    config_async_test
    single_thread_test
    edge_cases_test
)

# Define test program configurations
set(cache_test_SOURCES test_cache_main.cpp ../memory_cache_manager.cpp ../config_parser.cpp)
set(cache_demo_SOURCES real_file_cache_demo.cpp ../memory_cache_manager.cpp ../config_parser.cpp)
set(directory_test_SOURCES test_directory_cache.cpp ../directory_cache.cpp ../directory_tree.cpp ../config_parser.cpp)
set(async_test_SOURCES test_async_download.cpp ../async_download_manager.cpp ../memory_cache_manager.cpp ../config_parser.cpp)
set(filesystem_async_test_SOURCES test_filesystem_async.cpp ../async_download_manager.cpp ../memory_cache_manager.cpp ../config_parser.cpp)
set(config_threads_test_SOURCES test_config_threads.cpp ../config_parser.cpp)
set(config_async_test_SOURCES test_config_async.cpp ../async_download_manager.cpp ../memory_cache_manager.cpp ../config_parser.cpp)
set(single_thread_test_SOURCES test_single_thread.cpp ../async_download_manager.cpp ../memory_cache_manager.cpp ../config_parser.cpp)
set(edge_cases_test_SOURCES test_edge_cases.cpp ../async_download_manager.cpp ../memory_cache_manager.cpp ../config_parser.cpp)

# Create all test executables
foreach(TEST_PROGRAM ${TEST_PROGRAMS})
    add_executable(${TEST_PROGRAM} ${${TEST_PROGRAM}_SOURCES})
    
    # Define NO_WINFSP for test programs
    target_compile_definitions(${TEST_PROGRAM} PRIVATE NO_WINFSP)
    
    # Include directories for test programs
    target_include_directories(${TEST_PROGRAM} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../../include
    )
    
    # Set C++ standard and output directory
    set_target_properties(${TEST_PROGRAM} PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )
    
    # Enable warnings for test programs
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(${TEST_PROGRAM} PRIVATE -Wall -Wextra)
    endif()
endforeach()

message(STATUS "Test programs will be built in: ${CMAKE_BINARY_DIR}/bin")