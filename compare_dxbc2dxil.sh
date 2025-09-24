#!/bin/bash

# Compare Windows dxbc2dxil.exe with ARM64 native dxbc2dxil

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Binaries
WINDOWS_DXBC2DXIL="$SCRIPT_DIR/dxbc2dxil/bin/dxbc2dxil.exe"
WINDOWS_DXILCONV="$SCRIPT_DIR/dxbc2dxil/bin/dxilconv.dll"
ARM64_DXBC2DXIL="$SCRIPT_DIR/build_arm64/bin/dxbc2dxil"

# Check binaries exist
if [ ! -f "$WINDOWS_DXBC2DXIL" ]; then
    echo -e "${RED}Error: Windows dxbc2dxil.exe not found at $WINDOWS_DXBC2DXIL${NC}"
    exit 1
fi

if [ ! -f "$ARM64_DXBC2DXIL" ]; then
    echo -e "${RED}Error: ARM64 dxbc2dxil not found at $ARM64_DXBC2DXIL${NC}"
    echo "Please run ./build_dxbc2dxil_arm64.sh first"
    exit 1
fi

# Create output directories
OUTPUT_DIR="$SCRIPT_DIR/comparison_output"
WINDOWS_OUTPUT="$OUTPUT_DIR/windows"
ARM64_OUTPUT="$OUTPUT_DIR/arm64"
WINDOWS_METAL="$OUTPUT_DIR/windows_metal"
ARM64_METAL="$OUTPUT_DIR/arm64_metal"

rm -rf "$OUTPUT_DIR"
mkdir -p "$WINDOWS_OUTPUT" "$ARM64_OUTPUT" "$WINDOWS_METAL" "$ARM64_METAL"

# Test files
TEST_DIR="$SCRIPT_DIR/projects/dxilconv/test"

# Statistics
TOTAL=0
IDENTICAL_DXIL=0
DIFFERENT_DXIL=0
IDENTICAL_METAL=0
DIFFERENT_METAL=0
METAL_FAILED=0

echo -e "${GREEN}=========================================${NC}"
echo -e "${GREEN}Comparing Windows vs ARM64 dxbc2dxil${NC}"
echo -e "${GREEN}=========================================${NC}"
echo ""

# Process each DXBC file
for dxbc_file in "$TEST_DIR"/*.dxbc; do
    if [ ! -f "$dxbc_file" ]; then
        continue
    fi
    
    basename=$(basename "$dxbc_file" .dxbc)
    TOTAL=$((TOTAL + 1))
    
    echo -n "Testing $basename... "
    
    # Run Windows version (using Wine)
    WINE_OUTPUT=$(wine64 "$WINDOWS_DXBC2DXIL" "$dxbc_file" -o "$WINDOWS_OUTPUT/$basename.dxil" 2>&1 || true)
    
    # Run ARM64 version
    "$ARM64_DXBC2DXIL" "$dxbc_file" -o "$ARM64_OUTPUT/$basename.dxil" 2>/dev/null || {
        echo -e "${RED}ARM64 conversion failed${NC}"
        continue
    }
    
    # Compare DXIL outputs
    if [ -f "$WINDOWS_OUTPUT/$basename.dxil" ] && [ -f "$ARM64_OUTPUT/$basename.dxil" ]; then
        # Extract LLVM IR for comparison (containers might differ in metadata)
        wine64 "$WINDOWS_DXBC2DXIL" "$dxbc_file" --emit-llvm > "$WINDOWS_OUTPUT/$basename.ll" 2>/dev/null || true
        "$ARM64_DXBC2DXIL" "$dxbc_file" --emit-llvm > "$ARM64_OUTPUT/$basename.ll" 2>/dev/null || true
        
        if diff -q "$WINDOWS_OUTPUT/$basename.ll" "$ARM64_OUTPUT/$basename.ll" > /dev/null 2>&1; then
            echo -n -e "${GREEN}[DXIL: ✓]${NC} "
            IDENTICAL_DXIL=$((IDENTICAL_DXIL + 1))
        else
            echo -n -e "${YELLOW}[DXIL: ≠]${NC} "
            DIFFERENT_DXIL=$((DIFFERENT_DXIL + 1))
        fi
        
        # Try Metal conversion for both
        metal-shaderconverter "$WINDOWS_OUTPUT/$basename.dxil" -o "$WINDOWS_METAL/$basename.metallib" 2>/dev/null && WINDOWS_METAL_OK=1 || WINDOWS_METAL_OK=0
        metal-shaderconverter "$ARM64_OUTPUT/$basename.dxil" -o "$ARM64_METAL/$basename.metallib" 2>/dev/null && ARM64_METAL_OK=1 || ARM64_METAL_OK=0
        
        if [ $WINDOWS_METAL_OK -eq 1 ] && [ $ARM64_METAL_OK -eq 1 ]; then
            # Both converted to Metal successfully
            if diff -q "$WINDOWS_METAL/$basename.metallib" "$ARM64_METAL/$basename.metallib" > /dev/null 2>&1; then
                echo -e "${GREEN}[Metal: ✓]${NC}"
                IDENTICAL_METAL=$((IDENTICAL_METAL + 1))
            else
                echo -e "${YELLOW}[Metal: ≠]${NC}"
                DIFFERENT_METAL=$((DIFFERENT_METAL + 1))
            fi
        elif [ $WINDOWS_METAL_OK -eq 0 ] && [ $ARM64_METAL_OK -eq 0 ]; then
            echo -e "${YELLOW}[Metal: both failed]${NC}"
            METAL_FAILED=$((METAL_FAILED + 1))
        else
            echo -e "${RED}[Metal: inconsistent]${NC}"
            METAL_FAILED=$((METAL_FAILED + 1))
        fi
    else
        echo -e "${RED}Windows conversion failed${NC}"
    fi
done

echo ""
echo -e "${GREEN}=========================================${NC}"
echo -e "${GREEN}Results Summary${NC}"
echo -e "${GREEN}=========================================${NC}"
echo "Total tests: $TOTAL"
echo ""
echo "DXIL Comparison:"
echo "  Identical: $IDENTICAL_DXIL"
echo "  Different: $DIFFERENT_DXIL"
echo ""
echo "Metal Comparison:"
echo "  Identical: $IDENTICAL_METAL"
echo "  Different: $DIFFERENT_METAL"
echo "  Failed: $METAL_FAILED"

if [ $IDENTICAL_DXIL -eq $TOTAL ]; then
    echo ""
    echo -e "${GREEN}✓ All DXIL outputs are identical!${NC}"
fi

if [ $IDENTICAL_METAL -gt 0 ] && [ $DIFFERENT_METAL -eq 0 ]; then
    echo -e "${GREEN}✓ All successful Metal outputs are identical!${NC}"
fi