# Support library for cross-platform dxbc2dxil functionality

set(LLVM_LINK_COMPONENTS
  Core
  Support
)

if (APPLE)
  set(PLATFORM_SOURCES
    PlatformHelpers_macOS.cpp
  )
endif()

add_llvm_library(LLVMDxilConvSupport
  ${PLATFORM_SOURCES}
)

# Define DXC_BUILD to use DXC's WinAdapter.h instead of WSL headers
target_compile_definitions(LLVMDxilConvSupport PUBLIC DXC_BUILD=1)

target_include_directories(LLVMDxilConvSupport
  PUBLIC
    $<BUILD_INTERFACE:${LLVM_SOURCE_DIR}/projects/dxilconv/include>
    $<INSTALL_INTERFACE:include>
)

# WSL headers no longer needed - using DXC's WinAdapter.h