# Build DxbcConverter.lib.

# Platform-specific D3D12 package finding
if(APPLE)
  # On macOS, we use DXC's internal definitions instead of external D3D headers
  # No need to find D3D12 package
else()
  find_package(D3D12 REQUIRED)
endif()

add_dxilconv_project_library(DxbcConverter
  DxbcConverter.cpp
  DxbcUtil.cpp
)

# Platform-specific dependencies
if(APPLE)
  add_dependencies(DxbcConverter intrinsics_gen)
  
  # Link to platform abstraction layer on macOS
  target_link_libraries(DxbcConverter LLVMDxilConvSupport)
  
  # Add macOS-specific compile flags
  target_compile_options(DxbcConverter PRIVATE
    -Wno-deprecated-declarations
    -Wno-unknown-pragmas
    -Wno-microsoft-include
  )
  
  # Define macOS-specific macros
  target_compile_definitions(DxbcConverter PRIVATE
    LLVM_ON_UNIX=1
    HAVE_PTHREAD_H=1
    DXC_BUILD=1  # Signal that we're building with DXC
  )
else()
  add_dependencies(DxbcConverter intrinsics_gen DxcRuntimeEtw)
endif()

if(NOT APPLE)
  include_directories(
    ${D3D12_INCLUDE_DIRS}
  )
endif()

if(APPLE)
  # Include our custom adapter headers 
  include_directories(BEFORE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include/Support  # Our minimal D3D definitions
  )
endif()