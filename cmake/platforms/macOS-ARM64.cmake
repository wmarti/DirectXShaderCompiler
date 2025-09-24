# CMake Toolchain file for macOS ARM64 (Apple Silicon)
# Usage: cmake -DCMAKE_TOOLCHAIN_FILE=cmake/platforms/macOS-ARM64.cmake ..

set(CMAKE_SYSTEM_NAME Darwin)
set(CMAKE_SYSTEM_PROCESSOR arm64)

# Specify the cross compiler
set(CMAKE_C_COMPILER clang)
set(CMAKE_CXX_COMPILER clang++)

# Target architecture
set(CMAKE_OSX_ARCHITECTURES arm64)
set(CMAKE_OSX_DEPLOYMENT_TARGET 11.0)

# Compiler flags for ARM64
set(CMAKE_C_FLAGS_INIT "-arch arm64 -mmacosx-version-min=11.0")
set(CMAKE_CXX_FLAGS_INIT "-arch arm64 -mmacosx-version-min=11.0 -stdlib=libc++")

# LLVM specific settings for ARM64
set(LLVM_DEFAULT_TARGET_TRIPLE "arm64-apple-darwin")
set(LLVM_HOST_TRIPLE "arm64-apple-darwin")
set(LLVM_TARGET_ARCH "AArch64")

# Enable position independent code
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# macOS specific paths
set(CMAKE_FIND_ROOT_PATH /opt/homebrew)
set(CMAKE_PREFIX_PATH /opt/homebrew)

# Search paths
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

# Framework paths
set(CMAKE_FRAMEWORK_PATH 
    /System/Library/Frameworks
    /Library/Frameworks
)

# Additional definitions
add_definitions(-DLLVM_ON_UNIX=1)
add_definitions(-DHAVE_PTHREAD_H=1)
add_definitions(-DHAVE_PTHREAD_MUTEX_LOCK=1)