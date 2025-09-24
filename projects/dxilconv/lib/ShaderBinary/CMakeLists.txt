# Build ShaderBinary.lib.

# Platform-specific D3D12 package finding
if(APPLE)
  # Use macOS-specific D3D12 headers finder
  set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/../../../../cmake/modules")
  find_package(D3D12_macOS REQUIRED)
else()
  find_package(D3D12 REQUIRED)
endif()

add_dxilconv_project_library(ShaderBinary
  ShaderBinary.cpp
)

# macOS-specific configurations
if(APPLE)
  # Link to platform abstraction layer on macOS
  target_link_libraries(ShaderBinary LLVMDxilConvSupport)

  target_compile_options(ShaderBinary PRIVATE
    -Wno-deprecated-declarations
    -Wno-unknown-pragmas
    -Wno-microsoft-include
  )
  
  target_compile_definitions(ShaderBinary PRIVATE
    LLVM_ON_UNIX=1
    DXC_BUILD=1
  )
endif()

include_directories(
    ${D3D12_INCLUDE_DIRS}
)
