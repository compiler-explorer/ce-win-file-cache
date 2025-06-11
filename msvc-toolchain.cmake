# Toolchain file for cross-compiling with MSVC from WSL
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_VERSION 10.0)
set(CMAKE_SYSTEM_PROCESSOR AMD64)

# Use wrapper scripts for path conversion
get_filename_component(TOOLCHAIN_DIR "${CMAKE_CURRENT_LIST_FILE}" PATH)
set(CMAKE_C_COMPILER "${TOOLCHAIN_DIR}/cl-wrapper.sh")
set(CMAKE_CXX_COMPILER "${TOOLCHAIN_DIR}/cl-wrapper.sh")
set(CMAKE_RC_COMPILER "/mnt/d/efs/compilers/msvc/14.40.33807-14.40.33811.0/bin/Hostx64/x64/rc.exe")
set(CMAKE_LINKER "${TOOLCHAIN_DIR}/link-wrapper.sh")
set(CMAKE_AR "/mnt/d/efs/compilers/msvc/14.40.33807-14.40.33811.0/bin/Hostx64/x64/lib.exe")

# Windows SDK paths
set(WINDOWS_SDK_PATH "/mnt/d/efs/compilers/windows-kits-10")
set(WINDOWS_SDK_VERSION "10.0.22621.0")  # Adjust if needed

# Include directories
set(MSVC_INCLUDE_PATH "/mnt/d/efs/compilers/msvc/14.40.33807-14.40.33811.0/include")
set(SDK_INCLUDE_PATH "${WINDOWS_SDK_PATH}/Include/${WINDOWS_SDK_VERSION}")

# Library directories
set(MSVC_LIB_PATH "/mnt/d/efs/compilers/msvc/14.40.33807-14.40.33811.0/lib/x64")
set(SDK_LIB_PATH "${WINDOWS_SDK_PATH}/Lib/${WINDOWS_SDK_VERSION}")

# Set compiler flags
set(CMAKE_CXX_FLAGS_INIT "/nologo /EHsc /MD /I\"${MSVC_INCLUDE_PATH}\" /I\"${SDK_INCLUDE_PATH}/ucrt\" /I\"${SDK_INCLUDE_PATH}/shared\" /I\"${SDK_INCLUDE_PATH}/um\" /I\"${SDK_INCLUDE_PATH}/winrt\" /I\"${SDK_INCLUDE_PATH}/cppwinrt\"")
set(CMAKE_C_FLAGS_INIT "/nologo /MD /I\"${MSVC_INCLUDE_PATH}\" /I\"${SDK_INCLUDE_PATH}/ucrt\" /I\"${SDK_INCLUDE_PATH}/shared\" /I\"${SDK_INCLUDE_PATH}/um\" /I\"${SDK_INCLUDE_PATH}/winrt\"")

# Set linker flags
set(CMAKE_EXE_LINKER_FLAGS_INIT "/LIBPATH:\"${MSVC_LIB_PATH}\" /LIBPATH:\"${SDK_LIB_PATH}/ucrt/x64\" /LIBPATH:\"${SDK_LIB_PATH}/um/x64\" /NOLOGO")
set(CMAKE_MODULE_LINKER_FLAGS_INIT "/LIBPATH:\"${MSVC_LIB_PATH}\" /LIBPATH:\"${SDK_LIB_PATH}/ucrt/x64\" /LIBPATH:\"${SDK_LIB_PATH}/um/x64\" /NOLOGO")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "/LIBPATH:\"${MSVC_LIB_PATH}\" /LIBPATH:\"${SDK_LIB_PATH}/ucrt/x64\" /LIBPATH:\"${SDK_LIB_PATH}/um/x64\" /NOLOGO")

# Tell CMake to use response files (important for long command lines in WSL)
set(CMAKE_C_USE_RESPONSE_FILE_FOR_OBJECTS 1)
set(CMAKE_CXX_USE_RESPONSE_FILE_FOR_OBJECTS 1)
set(CMAKE_C_USE_RESPONSE_FILE_FOR_INCLUDES 1)
set(CMAKE_CXX_USE_RESPONSE_FILE_FOR_INCLUDES 1)

# Force 64-bit architecture
set(CMAKE_SIZEOF_VOID_P 8)

# Disable CMake's compiler tests since they might fail in cross-compilation
set(CMAKE_C_COMPILER_WORKS 1)
set(CMAKE_CXX_COMPILER_WORKS 1)

# Enable Windows path style for MSVC
set(CMAKE_CL_SHOWINCLUDES_PREFIX "Note: including file: ")

# Set rules for converting paths
set(CMAKE_C_COMPILER_ARG1 "")
set(CMAKE_CXX_COMPILER_ARG1 "")

# Set the find root path mode to prevent CMake from searching in host directories
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)