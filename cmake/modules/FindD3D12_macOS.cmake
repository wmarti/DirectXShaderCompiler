# FindD3D12_macOS.cmake - Find DirectX headers for macOS
# This module finds the DirectX headers that have been ported to macOS/Unix systems
# 
# It defines:
#  D3D12_FOUND - System has D3D12 headers
#  D3D12_INCLUDE_DIRS - The D3D12 include directories
#  D3D12_LIBRARIES - Empty on macOS (headers only)

# On macOS, we use the DirectX-Headers package (headers-only)
# Can be installed via: brew install directx-headers

# First try to find directx headers in common locations
find_path(D3D12_INCLUDE_DIR
    NAMES d3d12.h
    HINTS
        /opt/homebrew/include/directx
        /opt/homebrew/Cellar/directx-headers/*/include/directx
        /usr/local/include/directx
        /usr/local/Cellar/directx-headers/*/include/directx
        ${CMAKE_PREFIX_PATH}/include/directx
    PATH_SUFFIXES directx
    DOC "Path to DirectX 12 headers"
)

find_path(DXGI_INCLUDE_DIR
    NAMES dxgiformat.h dxgicommon.h
    HINTS
        /opt/homebrew/include/directx
        /opt/homebrew/Cellar/directx-headers/*/include/directx
        /usr/local/include/directx
        /usr/local/Cellar/directx-headers/*/include/directx
        ${CMAKE_PREFIX_PATH}/include/directx
        /opt/homebrew/include/wsl
        /opt/homebrew/Cellar/directx-headers/*/include/wsl
        /usr/local/include/wsl
        /usr/local/Cellar/directx-headers/*/include/wsl
    PATH_SUFFIXES directx wsl
    DOC "Path to DXGI headers"
)

# Also need the WSL stubs for some definitions
find_path(WSL_INCLUDE_DIR
    NAMES winadapter.h
    HINTS
        /opt/homebrew/include/wsl
        /opt/homebrew/Cellar/directx-headers/*/include/wsl
        /usr/local/include/wsl
        /usr/local/Cellar/directx-headers/*/include/wsl
        ${CMAKE_PREFIX_PATH}/include/wsl
    PATH_SUFFIXES wsl
    DOC "Path to WSL adapter headers"
)

# Set the include directories
if(D3D12_INCLUDE_DIR AND DXGI_INCLUDE_DIR)
    set(D3D12_INCLUDE_DIRS ${D3D12_INCLUDE_DIR} ${DXGI_INCLUDE_DIR})
    # Don't include WSL headers when building with DXC (which has WinAdapter.h)
    # if(WSL_INCLUDE_DIR)
    #     list(APPEND D3D12_INCLUDE_DIRS ${WSL_INCLUDE_DIR})
    # endif()
    
    # Remove duplicates
    list(REMOVE_DUPLICATES D3D12_INCLUDE_DIRS)
endif()

# On macOS, D3D12 is headers-only, no libraries needed
set(D3D12_LIBRARIES "")

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(D3D12
    DEFAULT_MSG
    D3D12_INCLUDE_DIR
    DXGI_INCLUDE_DIR
)

mark_as_advanced(D3D12_INCLUDE_DIR DXGI_INCLUDE_DIR WSL_INCLUDE_DIR D3D12_INCLUDE_DIRS D3D12_LIBRARIES)

# Provide compatibility macros for macOS
if(D3D12_FOUND)
    add_definitions(-DD3D12_HEADERS_ONLY)
    add_definitions(-D__EMULATE_UUID)
    
    # Add WSL compatibility definitions
    if(WSL_INCLUDE_DIR)
        add_definitions(-DHAVE_WINADAPTER_H)
    endif()
    
    # Create imported target for modern CMake usage
    if(NOT TARGET D3D12::D3D12)
        add_library(D3D12::D3D12 INTERFACE IMPORTED)
        set_target_properties(D3D12::D3D12 PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${D3D12_INCLUDE_DIRS}"
            INTERFACE_COMPILE_DEFINITIONS "D3D12_HEADERS_ONLY;__EMULATE_UUID"
        )
        
        if(WSL_INCLUDE_DIR)
            set_property(TARGET D3D12::D3D12 APPEND PROPERTY
                INTERFACE_COMPILE_DEFINITIONS "HAVE_WINADAPTER_H"
            )
        endif()
    endif()
endif()