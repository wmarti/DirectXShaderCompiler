set(DXILCONV_PROJECT_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(DXILCONV_PROJECT_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR})

include_directories(
  ${LLVM_MAIN_INCLUDE_DIR}
  ${DXC_PROJECTS_SOURCE_DIR}/include
  ${DXC_PROJECTS_BINARY_DIR}/include
  ${DXILCONV_PROJECT_SOURCE_DIR}/include
  ${DXILCONV_PROJECT_BINARY_DIR}/include
)

macro(add_dxilconv_project_library name)
  # Force enable exceptions for dxilconv libraries
  set(LLVM_REQUIRES_EH ON)
  set(LLVM_REQUIRES_RTTI ON)
  add_llvm_library(${name} ${ARGN})
#  add_definitions(/D_ITERATOR_DEBUG_LEVEL=0)
  set_output_directory(${name} ${LLVM_RUNTIME_OUTPUT_INTDIR} ${LLVM_LIBRARY_OUTPUT_INTDIR})  
  set_target_properties(${name} PROPERTIES FOLDER "Dxilconv libraries")
  # Enable exceptions for this target
  if(APPLE OR UNIX)
    target_compile_options(${name} PRIVATE -fexceptions -frtti)
  endif()
endmacro(add_dxilconv_project_library)

macro(add_dxilconv_project_executable name)
  # Force enable exceptions for dxilconv executables
  set(LLVM_REQUIRES_EH ON)
  set(LLVM_REQUIRES_RTTI ON)
  add_llvm_executable(${name} ${ARGN})
  set_target_properties(${name} PROPERTIES FOLDER "Dxilconv executables")
  # Enable exceptions for this target
  if(APPLE OR UNIX)
    target_compile_options(${name} PRIVATE -fexceptions -frtti)
  endif()
endmacro(add_dxilconv_project_executable)

macro(add_dxilconv_project_test_library name)
  add_dxilconv_project_library(${name} ${ARGN})
  set_target_properties(${name} PROPERTIES FOLDER "Dxilconv tests")
endmacro(add_dxilconv_project_test_library)


# Platform-specific build configurations
if(APPLE)
  # macOS build support
  message(STATUS "Building dxilconv for macOS (${CMAKE_SYSTEM_PROCESSOR})")
  
  # Detect architecture and set flags
  if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64")
    message(STATUS "Configuring for Apple Silicon (ARM64)")
    set(MACOS_ARCH "arm64")
  else()
    message(STATUS "Configuring for Intel macOS (x86_64)")
    set(MACOS_ARCH "x86_64")
  endif()
  
  # Set macOS-specific compiler flags
  add_compile_options(
    -arch ${MACOS_ARCH}
    -mmacosx-version-min=11.0
    -fPIC
    -fvisibility=hidden
    -fvisibility-inlines-hidden
  )
  
  # Add macOS framework search paths
  link_directories(
    /usr/local/lib
    /opt/homebrew/lib
  )
  
  # macOS-specific definitions
  add_definitions(
    -DDXILCONV_MACOS=1
    -D__APPLE__=1
    -DHAVE_PTHREAD_H=1
    -DHAVE_PTHREAD_MUTEX_LOCK=1
  )
  
  # Find DirectX headers for macOS
  find_package(D3D12_macOS REQUIRED)
  if(D3D12_FOUND)
    include_directories(${D3D12_INCLUDE_DIRS})
    message(STATUS "Found DirectX headers: ${D3D12_INCLUDE_DIRS}")
  endif()
  
  # Build libraries and tools for macOS
  add_subdirectory(lib)
  add_subdirectory(tools)
  
  # Build unittests on macOS (optional)
  if(BUILD_TESTING)
    add_subdirectory(unittests)
  endif()
  
elseif(WIN32)
  # Windows build support
  message(STATUS "Building dxilconv for Windows")
  add_subdirectory(lib)
  add_subdirectory(tools)
  add_subdirectory(unittests)
  
elseif(UNIX)
  # Generic Unix/Linux support
  message(STATUS "Building dxilconv for Unix/Linux")
  add_subdirectory(lib)
  add_subdirectory(tools)
  add_subdirectory(unittests)
  
endif()