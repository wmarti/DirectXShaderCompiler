# CMakeLists.txt for dxbc2dxil tool (Unix/macOS port)

cmake_minimum_required(VERSION 3.10)

# Source files
if(WIN32)
    set(SOURCES dxbc2dxil.cpp)
else()
    set(SOURCES 
        dxbc2dxil_unix.cpp
        ../dxilconv/dxilconv_uuids.cpp  # Include UUID definitions
    )
endif()

# Create executable
add_executable(dxbc2dxil ${SOURCES})

# Link dependencies
target_link_libraries(dxbc2dxil
    PRIVATE
        DxbcConverter
        DxilConvPasses
        ShaderBinary
        LLVMDxilContainer
        LLVMDxilRootSignature
        LLVMDXIL
        LLVMHLSL
        LLVMDxcSupport
        LLVMMSSupport
        LLVMipo
        LLVMCore
        LLVMSupport
        LLVMBitReader
        LLVMBitWriter
        LLVMAnalysis
        LLVMTransformUtils
        LLVMScalarOpts
)

# Include directories
target_include_directories(dxbc2dxil
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/projects/dxilconv/include
        ${LLVM_INCLUDE_DIRS}
)

# Platform-specific include directories
if(APPLE)
    # Include our custom RPC adapter headers first
    target_include_directories(dxbc2dxil BEFORE PRIVATE
        ${CMAKE_SOURCE_DIR}/projects/dxilconv/include/Support  # Our rpc.h and rpcndr.h adapters
    )
    # DirectX headers no longer needed - using local copies
endif()

# Platform-specific settings
if(NOT WIN32)
    # Add dl library for dlopen/dlsym
    target_link_libraries(dxbc2dxil PRIVATE dl)
    
    # Add pthread for thread support
    target_link_libraries(dxbc2dxil PRIVATE pthread)
    
    # Set RPATH to find libraries in the same directory
    if(APPLE)
        set_target_properties(dxbc2dxil PROPERTIES
            BUILD_RPATH "@loader_path"
            INSTALL_RPATH "@loader_path"
        )
    else()
        set_target_properties(dxbc2dxil PROPERTIES
            BUILD_RPATH "$ORIGIN"
            INSTALL_RPATH "$ORIGIN"
        )
    endif()
endif()

# Installation
install(TARGETS dxbc2dxil
    RUNTIME DESTINATION bin
)