# Testing Guide

This project includes comprehensive testing capabilities with multiple test runners for different purposes.

## Quick Test Scripts

### Catch2 Unit Tests (Recommended)

```bash
# Run all Catch2 unit tests with detailed output
./run_unit_tests.sh

# Quick run with minimal output
./run_unit_tests.sh --quick

# List available tests
./run_unit_tests.sh --list

# Run via CTest only
./run_unit_tests.sh --ctest

# Show help
./run_unit_tests.sh --help
```

### All Tests (Integration + Unit + Static Analysis)

```bash
# Build with integrated static analysis and run all tests
./build-macos.sh

# Run comprehensive test suite (all tests)
./run_all_tests.sh

# Quick test run (faster)
./run_all_tests.sh --quick
```

## Available Test Types

### 1. Catch2 Unit Tests ✅ Fastest
- **Purpose**: Fast unit testing with modern C++ testing framework
- **Location**: `build-macos/bin/glob_matcher_unit_test`
- **What it tests**: GlobMatcher functionality with comprehensive pattern matching
- **Runtime**: ~1-2 seconds
- **Command**: `./run_unit_tests.sh --quick`

### 2. Integration Tests
- **Purpose**: Test complete functionality of cache, async downloads, config parsing
- **Location**: `build-macos/bin/*_test`
- **What it tests**: Real-world scenarios, file operations, network simulation
- **Runtime**: ~10-30 seconds
- **Command**: `./run_all_tests.sh`

### 3. Demo Programs
- **Purpose**: Interactive demonstration of features
- **Location**: `build-macos/bin/cache_demo`, `build-macos/bin/cache_test`
- **What it tests**: User-facing functionality, manual validation
- **Runtime**: Variable
- **Command**: Individual execution

## Test Runners Comparison

| Script | Purpose | Speed | Coverage | Best For |
|--------|---------|-------|----------|----------|
| `run_unit_tests.sh` | Catch2 only | ⚡ Fastest | Unit tests | Development |
| `run_all_tests.sh` | All tests | 🐌 Slowest | Complete | CI/CD |
| Individual executables | Specific areas | ⚡ Fast | Targeted | Debugging |

## Development Workflow

### During Development (Recommended)
```bash
# Fast feedback loop
./run_unit_tests.sh --quick
```

### Before Commit
```bash
# Comprehensive validation
./run_all_tests.sh
```

### Debugging Specific Issues
```bash
# List what's available
./run_unit_tests.sh --list

# Run specific test types
ctest -R "GlobMatcher" --verbose

# Manual testing
./build-macos/bin/cache_test --test-cache
```

## Available Test Executables

Built in `build-macos/bin/`:

### Unit Tests (Catch2)
- `glob_matcher_unit_test` - Pattern matching tests

### Integration Tests  
- `cache_test` - Memory cache functionality
- `async_test` - Asynchronous download system
- `config_*_test` - Configuration parsing
- `directory_test` - Directory tree management
- `metrics_test` - Prometheus metrics collection
- `edge_cases_test` - Error handling and edge cases

### Demo Programs
- `cache_demo` - Interactive cache demonstration
- `glob_test` - Pattern matching examples

## Test Options

### Catch2 Test Options
```bash
# Run specific test by name
./build-macos/bin/glob_matcher_unit_test "GlobMatcher basic wildcard patterns"

# Run tests with specific tags
./build-macos/bin/glob_matcher_unit_test [glob]

# Different output formats
./build-macos/bin/glob_matcher_unit_test --reporter=junit
./build-macos/bin/glob_matcher_unit_test --reporter=compact
```

### CTest Options
```bash
# Run all tests
ctest

# Run with parallel execution
ctest -j4

# Verbose output
ctest --verbose

# Run specific tests
ctest -R "GlobMatcher"

# Show test output on failure
ctest --output-on-failure
```

## Continuous Integration

The project includes GitHub Actions workflows:

- **`.github/workflows/test.yml`** - Runs all tests on Ubuntu with multiple compilers
- **`.github/workflows/code-quality.yml`** - Static analysis with cppcheck

Local equivalents:
```bash
# Simulate CI test run
./run_all_tests.sh

# Static analysis
cppcheck --enable=all --std=c++20 src/ include/
```

## Adding New Tests

### For Unit Tests (Recommended)
1. Add test cases to `src/test/glob_matcher_test.cpp` or create new Catch2 test file
2. Update `src/test/CMakeLists.txt` to include new test executable
3. Use Catch2 syntax:
   ```cpp
   TEST_CASE("Your test name", "[tag]") {
       REQUIRE(your_function() == expected_value);
   }
   ```

### For Integration Tests
1. Create new test file in `src/test/`
2. Add to `TEST_PROGRAMS` list in `src/test/CMakeLists.txt`
3. Define `<test_name>_SOURCES` variable

## Performance Testing

```bash
# Time the test runs
time ./run_unit_tests.sh --quick
time ./run_all_tests.sh --quick

# Profile memory usage (if needed)
valgrind ./build-macos/bin/glob_matcher_unit_test
```

## Troubleshooting

### Build Issues
```bash
# Clean build
rm -rf build-macos
./run_unit_tests.sh  # Will rebuild automatically
```

### Test Failures
```bash
# Verbose test output
./run_unit_tests.sh  # Default mode shows details
ctest --verbose

# Run individual test for debugging
./build-macos/bin/glob_matcher_unit_test --success
```

### Missing Dependencies
The scripts automatically handle:
- CMake configuration
- Dependency fetching (Catch2, nlohmann_json, prometheus-cpp)
- Parallel building

## Security and Code Quality Testing

### Static Analysis Integration

The build system includes integrated static analysis for code quality and security:

```bash
# Build with clang-tidy analysis (requires: brew install llvm)
./build-macos.sh
```

**Static analysis checks:**
- **readability-***: Code readability and maintainability
- **performance-***: Performance optimization opportunities  
- **modernize-***: Modern C++20 usage recommendations
- **bugprone-***: Potential bugs and error-prone patterns

### Memory Safety Analysis

The project includes comprehensive memory safety documentation:

- **[USE_AFTER_FREE_ANALYSIS.md](../USE_AFTER_FREE_ANALYSIS.md)**: Complete vulnerability analysis with fixes
- **[DYNAMIC_MEMORY_ANALYSIS.md](../DYNAMIC_MEMORY_ANALYSIS.md)**: Memory allocation patterns and safety
- **[CACHE_EVICTION_PROTECTION.md](../CACHE_EVICTION_PROTECTION.md)**: Thread-safe cache operations

**Recent security improvements:**
- ✅ Fixed critical use-after-free vulnerabilities in async callbacks
- ✅ Added atomic protection against cache eviction during downloads
- ✅ Eliminated use-after-move bugs with helper functions
- ✅ Added per-node mutex protection for concurrent directory operations

### Thread Safety Testing

**Concurrent operation tests:**
- Multi-threaded cache access patterns
- Async download manager stress testing
- Directory tree concurrent modification protection
- Memory cache thread safety validation

## Best Practices

1. **Use `run_unit_tests.sh --quick` during development** for fast feedback
2. **Run `./build-macos.sh` before commits** for build + static analysis
3. **Run `run_all_tests.sh` before commits** for full validation  
4. **Use `--list` to explore available tests** when debugging
5. **Individual executables for focused testing** of specific components
6. **CTest integration for IDE support** and automated discovery
7. **Review security analysis documents** for understanding memory safety improvements