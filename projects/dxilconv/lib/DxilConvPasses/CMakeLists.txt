# Build DxilConvPasses.lib.

add_dxilconv_project_library(DxilConvPasses
  NormalizeDxil.cpp
  ScopeNestedCFG.cpp
  InitializePasses.cpp
  ScopeNestInfo.cpp
  DxilCleanup.cpp
)

# macOS-specific configurations
if(APPLE)
  target_compile_options(DxilConvPasses PRIVATE
    -Wno-deprecated-declarations
    -Wno-unknown-pragmas
  )
  
  target_compile_definitions(DxilConvPasses PRIVATE
    LLVM_ON_UNIX=1
  )
endif()
