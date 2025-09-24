# FindD3D12_CrossPlatform.cmake
# Cross-platform D3D12 finder for dxilconv
# On Windows: Uses Windows SDK
# On other platforms: Uses cross-platform header stubs

# Platform detection
if(WIN32)
    # Use original Windows logic

    # Find the win10 SDK path.
    if ("$ENV{WIN10_SDK_PATH}$ENV{WIN10_SDK_VERSION}" STREQUAL "" )
      get_filename_component(WIN10_SDK_PATH "[HKEY_LOCAL_MACHINE\\SOFTWARE\\WOW6432Node\\Microsoft\\Microsoft SDKs\\Windows\\v10.0;InstallationFolder]" ABSOLUTE CACHE)
      get_filename_component(TEMP_WIN10_SDK_VERSION "[HKEY_LOCAL_MACHINE\\SOFTWARE\\WOW6432Node\\Microsoft\\Microsoft SDKs\\Windows\\v10.0;ProductVersion]" ABSOLUTE CACHE)
      get_filename_component(WIN10_SDK_VERSION ${TEMP_WIN10_SDK_VERSION} NAME)
    elseif(TRUE)
      set (WIN10_SDK_PATH $ENV{WIN10_SDK_PATH})
      set (WIN10_SDK_VERSION $ENV{WIN10_SDK_VERSION})
    endif ("$ENV{WIN10_SDK_PATH}$ENV{WIN10_SDK_VERSION}" STREQUAL "" )

    # WIN10_SDK_PATH will be something like C:\Program Files (x86)\Windows Kits\10
    # WIN10_SDK_VERSION will be something like 10.0.14393 or 10.0.14393.0; we need the
    # one that matches the directory name.

    if (IS_DIRECTORY "${WIN10_SDK_PATH}/Include/${WIN10_SDK_VERSION}.0")
      set(WIN10_SDK_VERSION "${WIN10_SDK_VERSION}.0")
    endif (IS_DIRECTORY "${WIN10_SDK_PATH}/Include/${WIN10_SDK_VERSION}.0")

    # Find the d3d12 and dxgi include path, it will typically look something like this.
    # C:\Program Files (x86)\Windows Kits\10\Include\10.0.10586.0\um\d3d12.h
    # C:\Program Files (x86)\Windows Kits\10\Include\10.0.10586.0\shared\dxgi1_4.h
    find_path(D3D12_INCLUDE_DIR    # Set variable D3D12_INCLUDE_DIR
              d3d12.h                # Find a path with d3d12.h
              HINTS "${WIN10_SDK_PATH}/Include/${WIN10_SDK_VERSION}/um"
              DOC "path to WIN10 SDK header files"
              HINTS
              )

    find_path(DXGI_INCLUDE_DIR    # Set variable DXGI_INCLUDE_DIR
              dxgi1_4.h           # Find a path with dxgi1_4.h
              HINTS "${WIN10_SDK_PATH}/Include/${WIN10_SDK_VERSION}/shared"
              DOC "path to WIN10 SDK header files"
              HINTS
              )
    set(D3D12_INCLUDE_DIRS ${D3D12_INCLUDE_DIR} ${DXGI_INCLUDE_DIR})

    # Find D3D libraries
    set(D3D12_LIB_NAMES d3d12.lib dxgi.lib d3dcompiler.lib)

    if ("${DXC_BUILD_ARCH}" STREQUAL "x64" )
        set(D3D12_HINTS_PATH ${WIN10_SDK_PATH}/Lib/${WIN10_SDK_VERSION}/um/x64)
    elseif (CMAKE_GENERATOR MATCHES "Visual Studio.*ARM" OR "${DXC_BUILD_ARCH}" STREQUAL "ARM")
        set(D3D12_HINTS_PATH ${WIN10_SDK_PATH}/Lib/${WIN10_SDK_VERSION}/um/arm)
    elseif (CMAKE_GENERATOR MATCHES "Visual Studio.*ARM64" OR "${DXC_BUILD_ARCH}" STREQUAL "ARM64")
        set(D3D12_HINTS_PATH ${WIN10_SDK_PATH}/Lib/${WIN10_SDK_VERSION}/um/arm64)
    elseif ("${DXC_BUILD_ARCH}" STREQUAL "Win32" )
        set(D3D12_HINTS_PATH ${WIN10_SDK_PATH}/Lib/${WIN10_SDK_VERSION}/um/x86)
    else ("${DXC_BUILD_ARCH}" STREQUAL "x64")
       message(FATAL_ERROR "Cannot match platform.")
    endif ("${DXC_BUILD_ARCH}" STREQUAL "x64")

    set(D3D12_LIBRARIES)
    foreach (D3D12_LIB_NAME ${D3D12_LIB_NAMES})
      find_library(${D3D12_LIB_NAME}_LOC NAMES ${D3D12_LIB_NAME} HINTS ${D3D12_HINTS_PATH})
      set(D3D12_LIBRARIES ${D3D12_LIBRARIES} ${${D3D12_LIB_NAME}_LOC})
    endforeach(D3D12_LIB_NAME)

else()
    # Cross-platform (macOS, Linux, etc.)
    message(STATUS "Using cross-platform D3D12 stubs for non-Windows platform")

    # Set include directories to our cross-platform headers
    set(DXILCONV_CROSS_PLATFORM_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/projects/dxilconv/include/Support")

    # Verify our cross-platform headers exist
    if(NOT EXISTS "${DXILCONV_CROSS_PLATFORM_INCLUDE_DIR}/DXIncludes_CrossPlatform.h")
        message(FATAL_ERROR "Cross-platform D3D12 headers not found at ${DXILCONV_CROSS_PLATFORM_INCLUDE_DIR}")
    endif()

    set(D3D12_INCLUDE_DIRS
        "${DXILCONV_CROSS_PLATFORM_INCLUDE_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}/projects/dxilconv/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
    )

    # No actual libraries needed for cross-platform stubs
    set(D3D12_LIBRARIES "")

    # Add preprocessor definitions for cross-platform build
    add_definitions(-DDXILCONV_CROSS_PLATFORM=1)

    # Platform-specific definitions
    if(APPLE)
        add_definitions(-DDXILCONV_MACOS=1)
        # Link against system frameworks that might be needed
        set(D3D12_LIBRARIES ${D3D12_LIBRARIES} "-framework Foundation")
    elseif(UNIX)
        add_definitions(-DDXILCONV_LINUX=1)
        # Link against required system libraries
        set(D3D12_LIBRARIES ${D3D12_LIBRARIES} "pthread" "dl")
    endif()

    message(STATUS "D3D12 cross-platform include dirs: ${D3D12_INCLUDE_DIRS}")
    message(STATUS "D3D12 cross-platform libraries: ${D3D12_LIBRARIES}")
endif()

include(FindPackageHandleStandardArgs)
# handle the QUIETLY and REQUIRED arguments and set D3D12_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args(D3D12  DEFAULT_MSG
                                  D3D12_INCLUDE_DIRS)

mark_as_advanced(D3D12_INCLUDE_DIRS D3D12_LIBRARIES)

# Set success flag
set(D3D12_FOUND TRUE)

# Debug output
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(STATUS "D3D12 Configuration:")
    message(STATUS "  Platform: ${CMAKE_SYSTEM_NAME}")
    message(STATUS "  Include dirs: ${D3D12_INCLUDE_DIRS}")
    message(STATUS "  Libraries: ${D3D12_LIBRARIES}")
    message(STATUS "  Found: ${D3D12_FOUND}")
endif()
