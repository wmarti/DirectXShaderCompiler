###############################################################################
#                                                                             #
# CMakeLists.txt                                                             #
# Copyright (C) Microsoft Corporation. All rights reserved.                   #
# This file is distributed under the University of Illinois Open Source       #
# License. See LICENSE.TXT for details.                                       #
#                                                                             #
# Cross-platform build configuration for DxilConv tests                       #
#                                                                             #
###############################################################################

cmake_minimum_required(VERSION 3.14)
project(DxilConvTests)

# Set C++ standard
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Platform detection
if(APPLE)
    set(PLATFORM_MACOS TRUE)
    # Check for ARM64
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64")
        set(PLATFORM_ARM64 TRUE)
        message(STATUS "Building for macOS ARM64")
    endif()
elseif(WIN32)
    set(PLATFORM_WINDOWS TRUE)
    message(STATUS "Building for Windows")
elseif(UNIX)
    set(PLATFORM_LINUX TRUE)
    message(STATUS "Building for Linux")
endif()

# Find LLVM
find_package(LLVM REQUIRED CONFIG)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../include/dxc
    ${LLVM_INCLUDE_DIRS}
)

# Add LLVM definitions
add_definitions(${LLVM_DEFINITIONS})

# Platform-specific definitions
if(PLATFORM_MACOS)
    add_definitions(-D__APPLE__)
    add_definitions(-DUNIX)
    if(PLATFORM_ARM64)
        add_definitions(-DARM64)
        # Add ARM64-specific optimizations
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=armv8.5-a+crypto")
    endif()
elseif(PLATFORM_WINDOWS)
    add_definitions(-D_WIN32)
    add_definitions(-DUNICODE)
    add_definitions(-D_UNICODE)
elseif(PLATFORM_LINUX)
    add_definitions(-DLINUX)
    add_definitions(-DUNIX)
endif()

# Source files
set(TEST_FRAMEWORK_SOURCES
    DxilConvTestFramework.cpp
    DxilConvTestFramework.h
)

set(TEST_SOURCES
    DxilConvTestsCrossPlatform.cpp
)

# ARM64 macOS specific tests
if(PLATFORM_MACOS AND PLATFORM_ARM64)
    list(APPEND TEST_SOURCES DxilConvTests_macOS.cpp)
    message(STATUS "Including ARM64 macOS-specific tests")
endif()

# If original tests exist and we're on Windows, include them
if(PLATFORM_WINDOWS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/DxilConvTests.cpp")
    list(APPEND TEST_SOURCES DxilConvTests.cpp)
endif()

# Create test framework library
add_library(DxilConvTestFramework STATIC ${TEST_FRAMEWORK_SOURCES})

# Link LLVM libraries
llvm_map_components_to_libnames(llvm_libs
    Core
    Support
    BitReader
    BitWriter
)

target_link_libraries(DxilConvTestFramework ${llvm_libs})

# Platform-specific libraries
if(PLATFORM_MACOS)
    target_link_libraries(DxilConvTestFramework
        "-framework CoreFoundation"
        pthread
        dl
    )
elseif(PLATFORM_WINDOWS)
    target_link_libraries(DxilConvTestFramework
        kernel32
        user32
        ole32
        oleaut32
        advapi32
    )
elseif(PLATFORM_LINUX)
    target_link_libraries(DxilConvTestFramework
        pthread
        dl
        rt
    )
endif()

# Create test executable
add_executable(dxilconv-tests ${TEST_SOURCES})
target_link_libraries(dxilconv-tests DxilConvTestFramework)

# ARM64-specific test executable
if(PLATFORM_MACOS AND PLATFORM_ARM64)
    add_executable(dxilconv-tests-arm64 DxilConvTests_macOS.cpp)
    target_link_libraries(dxilconv-tests-arm64 DxilConvTestFramework)
endif()

# Add GoogleTest support if available
find_package(GTest QUIET)
if(GTest_FOUND)
    message(STATUS "GoogleTest found, enabling GTest integration")
    
    # Create GTest adapter
    add_library(DxilConvGTestAdapter STATIC
        DxilConvGTestAdapter.cpp
    )
    
    target_link_libraries(DxilConvGTestAdapter
        DxilConvTestFramework
        GTest::GTest
        GTest::Main
    )
    
    # Create GTest executable
    add_executable(dxilconv-gtest ${TEST_SOURCES})
    target_link_libraries(dxilconv-gtest
        DxilConvGTestAdapter
        DxilConvTestFramework
        GTest::GTest
        GTest::Main
    )
    
    # Enable testing
    enable_testing()
    add_test(NAME DxilConvTests COMMAND dxilconv-gtest)
endif()

# Copy test data to build directory
file(GLOB_RECURSE TEST_DATA_FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/../test/*.hlsl"
    "${CMAKE_CURRENT_SOURCE_DIR}/../test/*.ref"
    "${CMAKE_CURRENT_SOURCE_DIR}/../test/*.ll"
    "${CMAKE_CURRENT_SOURCE_DIR}/../test/*.asm"
    "${CMAKE_CURRENT_SOURCE_DIR}/../test/*.dxbc"
    "${CMAKE_CURRENT_SOURCE_DIR}/../test/*.fxc"
    "${CMAKE_CURRENT_SOURCE_DIR}/test_data/*.hlsl"
    "${CMAKE_CURRENT_SOURCE_DIR}/test_data/*.ref"
    "${CMAKE_CURRENT_SOURCE_DIR}/test_data/*.check"
)

foreach(TEST_FILE ${TEST_DATA_FILES})
    file(RELATIVE_PATH REL_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../" ${TEST_FILE})
    get_filename_component(REL_DIR ${REL_PATH} DIRECTORY)
    file(COPY ${TEST_FILE} DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/${REL_DIR}")
endforeach()

# Custom targets for running tests
add_custom_target(run-tests
    COMMAND dxilconv-tests
    DEPENDS dxilconv-tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Running DxilConv tests..."
)

add_custom_target(run-tests-verbose
    COMMAND dxilconv-tests --verbose
    DEPENDS dxilconv-tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Running DxilConv tests with verbose output..."
)

# ARM64-specific test targets
if(PLATFORM_MACOS AND PLATFORM_ARM64)
    add_custom_target(run-arm64-tests
        COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/run_arm64_tests.sh"
        DEPENDS dxilconv-tests-arm64
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMENT "Running ARM64-specific DxilConv tests..."
    )
    
    add_custom_target(run-arm64-tests-alignment
        COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/run_arm64_tests.sh" --alignment-only
        DEPENDS dxilconv-tests-arm64
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMENT "Running ARM64 alignment tests..."
    )
    
    add_custom_target(run-arm64-tests-performance
        COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/run_arm64_tests.sh" --performance-only
        DEPENDS dxilconv-tests-arm64
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMENT "Running ARM64 performance tests..."
    )
endif()

# Parallel test execution
if(NOT PLATFORM_WINDOWS)
    find_program(PARALLEL_EXEC parallel)
    if(PARALLEL_EXEC)
        add_custom_target(run-tests-parallel
            COMMAND dxilconv-tests --parallel
            DEPENDS dxilconv-tests
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
            COMMENT "Running DxilConv tests in parallel..."
        )
    endif()
endif()

# Installation
install(TARGETS dxilconv-tests
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../test
    DESTINATION share/dxilconv
    FILES_MATCHING
    PATTERN "*.hlsl"
    PATTERN "*.ref"
    PATTERN "*.ll"
    PATTERN "*.asm"
    PATTERN "*.dxbc"
    PATTERN "*.fxc"
)

# Print configuration summary
message(STATUS "")
message(STATUS "DxilConv Test Configuration Summary:")
message(STATUS "  Platform:        ${CMAKE_SYSTEM_NAME}")
message(STATUS "  Architecture:    ${CMAKE_SYSTEM_PROCESSOR}")
message(STATUS "  Compiler:        ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "  C++ Standard:    ${CMAKE_CXX_STANDARD}")
message(STATUS "  Build Type:      ${CMAKE_BUILD_TYPE}")
message(STATUS "  GoogleTest:      ${GTest_FOUND}")
message(STATUS "")

# Add benchmark target if Google Benchmark is available
find_package(benchmark QUIET)
if(benchmark_FOUND)
    message(STATUS "Google Benchmark found, enabling benchmarks")
    
    add_executable(dxilconv-bench
        DxilConvBenchmark.cpp
    )
    
    target_link_libraries(dxilconv-bench
        DxilConvTestFramework
        benchmark::benchmark
    )
    
    add_custom_target(run-bench
        COMMAND dxilconv-bench
        DEPENDS dxilconv-bench
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMENT "Running DxilConv benchmarks..."
    )
endif()