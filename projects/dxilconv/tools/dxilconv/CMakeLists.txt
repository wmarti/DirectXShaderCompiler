# CMakeLists.txt for dxilconv library (Unix/macOS port)

cmake_minimum_required(VERSION 3.10)

set(SOURCES
    dxilconv.cpp
)

# Add Unix-specific source file if not on Windows
if(NOT WIN32)
    set(SOURCES
        dxilconv_unix.cpp
        dxilconv_uuids.cpp
    )
endif()

# Create shared library
add_library(dxilconv SHARED ${SOURCES})

# Link dependencies
target_link_libraries(dxilconv
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
        LLVMSupport
        LLVMCore
        LLVMBitWriter
        LLVMAnalysis
        LLVMTransformUtils
        LLVMScalarOpts
)

# Include directories
target_include_directories(dxilconv
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/projects/dxilconv/include
        ${LLVM_INCLUDE_DIRS}
)

# Export symbols on Unix/macOS
if(NOT WIN32)
    set_target_properties(dxilconv PROPERTIES
        C_VISIBILITY_PRESET default
        CXX_VISIBILITY_PRESET default
        VISIBILITY_INLINES_HIDDEN OFF
    )
    
    # Create a version script for symbol export
    set(VERSION_SCRIPT "${CMAKE_CURRENT_BINARY_DIR}/dxilconv.version")
    file(WRITE ${VERSION_SCRIPT} 
"DXILCONV_1.0 {
    global:
        DxcCreateInstance;
        DxcCreateInstance2;
    local:
        *;
};
")
    
    if(APPLE)
        # macOS uses different linker flags
        set_target_properties(dxilconv PROPERTIES
            LINK_FLAGS "-Wl,-exported_symbols_list,${CMAKE_CURRENT_BINARY_DIR}/dxilconv.symbols"
        )
        # Create exported symbols list for macOS
        file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/dxilconv.symbols"
"_DxcCreateInstance
_DxcCreateInstance2
")
    else()
        # Linux uses version scripts
        set_target_properties(dxilconv PROPERTIES
            LINK_FLAGS "-Wl,--version-script=${VERSION_SCRIPT}"
        )
    endif()
endif()

# Set output name
set_target_properties(dxilconv PROPERTIES
    OUTPUT_NAME "dxilconv"
    PREFIX "lib"
)

# Installation
install(TARGETS dxilconv
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)