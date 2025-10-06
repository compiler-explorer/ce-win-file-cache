# Test programs CMakeLists.txt

# Enable testing
enable_testing()
include(CTest)

# Test program sources and targets
# Note: directory_test is excluded on Linux as it depends on Windows-specific directory_tree code
set(TEST_PROGRAMS
    cache_test
    cache_demo
    async_test
    filesystem_async_test
    config_threads_test
    config_async_test
    single_thread_test
    edge_cases_test
    metrics_test
    json_config_test
    glob_test
    glob_matcher_unit_test
    file_access_tracker_test
    cache_policy_test
)

# Define test program configurations
set(cache_test_SOURCES test_cache_main.cpp ../memory_cache_manager.cpp ../config_parser.cpp ../string_utils.cpp ../logger.cpp)
set(cache_demo_SOURCES real_file_cache_demo.cpp ../memory_cache_manager.cpp ../config_parser.cpp ../string_utils.cpp ../logger.cpp)
set(directory_test_SOURCES test_directory_cache.cpp ../directory_cache.cpp ../directory_tree.cpp ../config_parser.cpp ../string_utils.cpp ../logger.cpp)
set(async_test_SOURCES test_async_download.cpp ../async_download_manager.cpp ../memory_cache_manager.cpp ../config_parser.cpp ../string_utils.cpp ../logger.cpp)
set(filesystem_async_test_SOURCES test_filesystem_async.cpp ../async_download_manager.cpp ../memory_cache_manager.cpp ../config_parser.cpp ../string_utils.cpp ../logger.cpp)
set(config_threads_test_SOURCES test_config_threads.cpp ../config_parser.cpp ../string_utils.cpp ../logger.cpp)
set(config_async_test_SOURCES test_config_async.cpp ../async_download_manager.cpp ../memory_cache_manager.cpp ../config_parser.cpp ../string_utils.cpp ../logger.cpp)
set(single_thread_test_SOURCES test_single_thread.cpp ../async_download_manager.cpp ../memory_cache_manager.cpp ../config_parser.cpp ../string_utils.cpp ../logger.cpp)
set(edge_cases_test_SOURCES test_edge_cases.cpp ../async_download_manager.cpp ../memory_cache_manager.cpp ../config_parser.cpp ../string_utils.cpp ../logger.cpp)
set(metrics_test_SOURCES test_metrics.cpp ../config_parser.cpp ../string_utils.cpp ../metrics_collector.cpp ../prometheus_metrics_impl.cpp ../logger.cpp)
set(json_config_test_SOURCES test_json_config.cpp ../config_parser.cpp ../string_utils.cpp ../logger.cpp)
set(glob_test_SOURCES glob_test.cpp ../glob_matcher.cpp)
set(glob_matcher_unit_test_SOURCES glob_matcher_test.cpp ../glob_matcher.cpp)
set(file_access_tracker_test_SOURCES test_file_access_tracker.cpp ../file_access_tracker.cpp ../string_utils.cpp ../logger.cpp ../time_utils.cpp)
set(cache_policy_test_SOURCES test_cache_policy.cpp ../hybrid_filesystem.cpp ../directory_cache.cpp ../directory_tree.cpp ../memory_cache_manager.cpp ../async_download_manager.cpp ../config_parser.cpp ../file_access_tracker.cpp ../glob_matcher.cpp ../string_utils.cpp ../metrics_collector.cpp ../prometheus_metrics_impl.cpp ../logger.cpp ../time_utils.cpp)
set(directory_integration_test_SOURCES ../../test_directory_integration.cpp ../hybrid_filesystem.cpp ../directory_cache.cpp ../directory_tree.cpp ../memory_cache_manager.cpp ../async_download_manager.cpp ../config_parser.cpp ../file_access_tracker.cpp ../glob_matcher.cpp ../string_utils.cpp ../metrics_collector.cpp ../prometheus_metrics_impl.cpp ../logger.cpp)
set(debug_directory_cache_test_SOURCES ../../debug_directory_cache.cpp ../directory_cache.cpp ../directory_tree.cpp ../config_parser.cpp ../string_utils.cpp ../logger.cpp)
set(logging_test_SOURCES ../../test_logging.cpp ../logger.cpp ../string_utils.cpp)

# Create all test executables
foreach(TEST_PROGRAM ${TEST_PROGRAMS})
    # Add metrics source file conditionally for metrics_test
    set(PROGRAM_SOURCES ${${TEST_PROGRAM}_SOURCES})
    add_executable(${TEST_PROGRAM} ${PROGRAM_SOURCES})

    # Define NO_WINFSP for test programs (except cache_policy_test which needs HybridFileSystem)
    if(NOT TEST_PROGRAM STREQUAL "cache_policy_test")
        target_compile_definitions(${TEST_PROGRAM} PRIVATE NO_WINFSP)
    endif()
    
    # Link nlohmann_json and fmt for all test programs
    target_link_libraries(${TEST_PROGRAM} nlohmann_json::nlohmann_json fmt::fmt)

    # Link Catch2 for unit test programs
    if(TEST_PROGRAM STREQUAL "glob_matcher_unit_test" OR TEST_PROGRAM STREQUAL "cache_policy_test")
        target_link_libraries(${TEST_PROGRAM} Catch2::Catch2WithMain)
    endif()

    # Link WinFsp for cache_policy_test (needs HybridFileSystem)
    if(TEST_PROGRAM STREQUAL "cache_policy_test")
        target_compile_definitions(${TEST_PROGRAM} PRIVATE ENABLE_TEST_ACCESS)
        target_link_libraries(${TEST_PROGRAM} ${WINFSP_LIBRARY})
        target_include_directories(${TEST_PROGRAM} PRIVATE ${WINFSP_INCLUDE_DIR})
        # Disable strict string conversion for WinFsp headers
        if(MSVC)
            target_compile_options(${TEST_PROGRAM} PRIVATE /Zc:strictStrings-)
        endif()
        add_test(NAME CachePolicyTests COMMAND cache_policy_test)
    endif()
    
    # Add Prometheus definitions and libraries for metrics_test, directory_integration_test, and cache_policy_test
    if(TEST_PROGRAM STREQUAL "metrics_test" OR TEST_PROGRAM STREQUAL "directory_integration_test" OR TEST_PROGRAM STREQUAL "cache_policy_test")
        if(HAVE_PROMETHEUS)
            target_compile_definitions(${TEST_PROGRAM} PRIVATE HAVE_PROMETHEUS)
            target_link_libraries(${TEST_PROGRAM}
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

# Enable test discovery for better integration with IDEs
if(TARGET Catch2)
    list(APPEND CMAKE_MODULE_PATH ${Catch2_SOURCE_DIR}/extras)
    include(Catch)
    catch_discover_tests(glob_matcher_unit_test)
    catch_discover_tests(cache_policy_test)
endif()

message(STATUS "Test programs will be built in: ${CMAKE_BINARY_DIR}/bin")