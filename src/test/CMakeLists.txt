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
    metrics_test
    json_config_test
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
set(metrics_test_SOURCES test_metrics.cpp ../config_parser.cpp)
set(json_config_test_SOURCES test_json_config.cpp ../config_parser.cpp)

# Create all test executables
foreach(TEST_PROGRAM ${TEST_PROGRAMS})
    # Add metrics source file conditionally for metrics_test
    set(PROGRAM_SOURCES ${${TEST_PROGRAM}_SOURCES})
    if(TEST_PROGRAM STREQUAL "metrics_test" AND HAVE_PROMETHEUS)
        list(APPEND PROGRAM_SOURCES ../metrics_collector.cpp ../prometheus_metrics_impl.cpp)
    endif()
    
    add_executable(${TEST_PROGRAM} ${PROGRAM_SOURCES})
    
    # Define NO_WINFSP for test programs
    target_compile_definitions(${TEST_PROGRAM} PRIVATE NO_WINFSP)
    
    # Link nlohmann_json for all test programs (config_parser.cpp dependency)
    target_link_libraries(${TEST_PROGRAM} nlohmann_json::nlohmann_json)
    
    # Add Prometheus definitions and libraries for metrics_test
    if(TEST_PROGRAM STREQUAL "metrics_test")
        if(HAVE_PROMETHEUS)
            target_compile_definitions(${TEST_PROGRAM} PRIVATE HAVE_PROMETHEUS)
            target_link_libraries(${TEST_PROGRAM} 
                prometheus-cpp::core 
                prometheus-cpp::pull
            )
        endif()
    endif()
    
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