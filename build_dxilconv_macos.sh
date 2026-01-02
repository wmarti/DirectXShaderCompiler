#!/bin/bash
# Build script for dxilconv on macOS ARM64

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}Building dxilconv for macOS ARM64${NC}"

# Check for DirectX headers
if ! brew list directx-headers &>/dev/null; then
    echo -e "${YELLOW}DirectX headers not found. Installing via Homebrew...${NC}"
    brew install directx-headers
fi

# Create build directory
BUILD_DIR="build_dxilconv_macos"
if [ ! -d "$BUILD_DIR" ]; then
    mkdir -p "$BUILD_DIR"
fi
cd "$BUILD_DIR"

# Configure with CMake
echo -e "${GREEN}Configuring CMake...${NC}"
# Use local headers if available
LOCAL_DX_HEADERS="../../../directx-headers/include"
DX_ARGS=""
if [ -d "$LOCAL_DX_HEADERS" ]; then
    DX_ARGS="-DD3D12_macOS_INCLUDE_DIR=$LOCAL_DX_HEADERS"
    echo -e "${GREEN}Using local DirectX headers from $LOCAL_DX_HEADERS${NC}"
fi

LTO_ARGS=""
if [ -n "$DXILCONV_LTO" ]; then
    LTO_ARGS="-DLLVM_ENABLE_LTO=$DXILCONV_LTO"
    echo -e "${GREEN}Using LTO mode: $DXILCONV_LTO${NC}"
fi

C_FLAGS=""
CXX_FLAGS="-stdlib=libc++ -Wno-deprecated-declarations -Wno-deprecated"
if [ -n "$DXILCONV_MCPU" ]; then
    C_FLAGS="$C_FLAGS -mcpu=$DXILCONV_MCPU"
    CXX_FLAGS="$CXX_FLAGS -mcpu=$DXILCONV_MCPU"
    echo -e "${GREEN}Using -mcpu=$DXILCONV_MCPU${NC}"
fi

if [ -n "$DXILCONV_LTO" ]; then
    if [ "$DXILCONV_LTO" = "Thin" ] || [ "$DXILCONV_LTO" = "thin" ]; then
        C_FLAGS="$C_FLAGS -flto=thin"
        CXX_FLAGS="$CXX_FLAGS -flto=thin"
    else
        C_FLAGS="$C_FLAGS -flto"
        CXX_FLAGS="$CXX_FLAGS -flto"
    fi
fi

if [ -n "$DXILCONV_PGO" ]; then
    if [ "$DXILCONV_PGO" = "gen" ]; then
        C_FLAGS="$C_FLAGS -fprofile-instr-generate"
        CXX_FLAGS="$CXX_FLAGS -fprofile-instr-generate"
        echo -e "${GREEN}Using PGO instrumentation (gen)${NC}"
    elif [ "$DXILCONV_PGO" = "use" ]; then
        if [ -z "$DXILCONV_PGO_PROFILE" ]; then
            echo -e "${RED}DXILCONV_PGO_PROFILE is required when DXILCONV_PGO=use${NC}"
            exit 1
        fi
        C_FLAGS="$C_FLAGS -fprofile-instr-use=$DXILCONV_PGO_PROFILE"
        CXX_FLAGS="$CXX_FLAGS -fprofile-instr-use=$DXILCONV_PGO_PROFILE"
        echo -e "${GREEN}Using PGO profile: $DXILCONV_PGO_PROFILE${NC}"
    else
        echo -e "${RED}Unknown DXILCONV_PGO value: $DXILCONV_PGO (use 'gen' or 'use')${NC}"
        exit 1
    fi
fi

cmake .. \
    $DX_ARGS \
    $LTO_ARGS \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_OSX_ARCHITECTURES=arm64 \
    -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 \
    -DLLVM_TARGETS_TO_BUILD="None" \
    -DLLVM_EXPERIMENTAL_TARGETS_TO_BUILD="" \
    -DLLVM_DEFAULT_TARGET_TRIPLE="arm64-apple-darwin" \
    -DLLVM_ENABLE_THREADS=ON \
    -DLLVM_ENABLE_PIC=ON \
    -DLLVM_BUILD_32_BITS=OFF \
    -DBUILD_SHARED_LIBS=OFF \
    -DLLVM_OPTIMIZED_TABLEGEN=ON \
    -DLLVM_USE_INTEL_JITEVENTS=OFF \
    -DLLVM_ENABLE_ZLIB=ON \
    -DLLVM_ENABLE_LIBXML2=OFF \
    -DCLANG_BUILD_EXAMPLES=OFF \
    -DLLVM_INCLUDE_TESTS=OFF \
    -DLLVM_INCLUDE_EXAMPLES=OFF \
    -DLLVM_INCLUDE_DOCS=OFF \
    -DCLANG_INCLUDE_TESTS=OFF \
    -DHLSL_INCLUDE_TESTS=OFF \
    -DENABLE_SPIRV_CODEGEN=OFF \
    -DSPIRV_BUILD_TESTS=OFF \
    -DCLANG_ENABLE_STATIC_ANALYZER=OFF \
    -DCLANG_ENABLE_ARCMT=OFF \
    -DLLVM_ENABLE_BINDINGS=OFF \
    -DLLVM_ENABLE_EH=ON \
    -DLLVM_ENABLE_RTTI=ON \
    -DLLVM_REQUIRES_EH=ON \
    -DLLVM_REQUIRES_RTTI=ON \
    -DCMAKE_C_COMPILER=clang \
    -DCMAKE_CXX_COMPILER=clang++ \
    -DCMAKE_CXX_STANDARD=17 \
    -DCMAKE_CXX_STANDARD_REQUIRED=ON \
    -DCMAKE_CXX_EXTENSIONS=OFF \
    -DCMAKE_C_FLAGS="$C_FLAGS" \
    -DCMAKE_CXX_FLAGS="$CXX_FLAGS" \
    -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

# Build
echo -e "${GREEN}Building dxilconv...${NC}"
cmake --build . --target dxilconv --config Release -j$(sysctl -n hw.ncpu)
cmake --build . --target dxbc2dxil --config Release -j$(sysctl -n hw.ncpu)

# Check if build succeeded
if [ -f "bin/dxbc2dxil" ]; then
    echo -e "${GREEN}Build successful!${NC}"
    echo "Binaries located at:"
    echo "  - $PWD/bin/dxbc2dxil"
    if [ -f "lib/libdxilconv.dylib" ]; then
        echo "  - $PWD/lib/libdxilconv.dylib"
    fi
else
    echo -e "${RED}Build failed!${NC}"
    exit 1
fi

echo -e "${GREEN}Done!${NC}"
