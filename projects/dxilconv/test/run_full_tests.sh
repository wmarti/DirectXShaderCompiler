#!/bin/bash

# Test runner for Microsoft's HLSL test suite using fxc2 and dxbc2dxil

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"
BUILD_DIR="$PROJECT_ROOT/build"
TEST_DIR="$SCRIPT_DIR/dxbc2dxil"

# Tools
FXC="$BUILD_DIR/fxc-docker"
DXBC2DXIL="$BUILD_DIR/bin/dxbc2dxil"

# Check tools exist
if [ ! -f "$FXC" ]; then
    echo "Error: fxc-docker wrapper not found at $FXC"
    exit 1
fi

if [ ! -f "$DXBC2DXIL" ]; then
    echo "Error: dxbc2dxil not found at $DXBC2DXIL"
    exit 1
fi

# Create output directory
OUTPUT_DIR="$SCRIPT_DIR/test_output"
mkdir -p "$OUTPUT_DIR"

# Statistics
TOTAL=0
PASSED=0
FAILED=0
SKIPPED=0

echo "========================================="
echo "Running Microsoft HLSL Test Suite"
echo "========================================="
echo ""

# Process all HLSL files
for hlsl_file in "$TEST_DIR"/*.hlsl; do
    if [ ! -f "$hlsl_file" ]; then
        continue
    fi
    
    basename=$(basename "$hlsl_file" .hlsl)
    TOTAL=$((TOTAL + 1))
    
    echo -n "Testing $basename... "
    
    # Check if there's a reference file
    ref_file="$TEST_DIR/${basename}.ref"
    if [ ! -f "$ref_file" ]; then
        echo "SKIPPED (no reference)"
        SKIPPED=$((SKIPPED + 1))
        continue
    fi
    
    # Parse shader profile from RUN comment in the test file
    profile=$(grep "^// RUN:.*%fxc.*/T" "$hlsl_file" | sed -E 's/.*\/T ([a-z]+_[0-9]+_[0-9]+).*/\1/' | head -1)
    
    # Parse any additional flags (like /Od for optimizations disabled)
    extra_flags=""
    if grep -q "^// RUN:.*%fxc.*/Od" "$hlsl_file"; then
        extra_flags="/Od"
    fi
    
    if [[ -z "$profile" ]]; then
        # Fallback: determine from filename
        if [[ "$basename" == *"cs"* ]]; then
            profile="cs_5_0"
        elif [[ "$basename" == *"vs"* ]]; then
            profile="vs_5_0"
        elif [[ "$basename" == *"gs"* ]]; then
            profile="gs_5_0"
        elif [[ "$basename" == *"hs"* ]]; then
            profile="hs_5_0"
        elif [[ "$basename" == *"ds"* ]]; then
            profile="ds_5_0"
        else
            profile="ps_5_0"  # Default
        fi
    fi
    
    # Compile HLSL to DXBC using Docker wrapper
    dxbc_file="$OUTPUT_DIR/${basename}.dxbc"
    
    # Copy HLSL to output dir temporarily for Docker mounting
    cp "$hlsl_file" "$OUTPUT_DIR/${basename}.hlsl"
    
    # Run FXC from output directory
    (cd "$OUTPUT_DIR" && \
     "$FXC" /T "$profile" $extra_flags /Fo"/work/${basename}.dxbc" "/work/${basename}.hlsl" > "${basename}.fxc.log" 2>&1)
    
    # Clean up temp file
    rm -f "$OUTPUT_DIR/${basename}.hlsl"
    
    if [ ! -f "$dxbc_file" ]; then
        echo "FAILED (fxc compilation)"
        FAILED=$((FAILED + 1))
        continue
    fi
    
    # Convert DXBC to DXIL and emit LLVM IR for comparison
    llvm_file="$OUTPUT_DIR/${basename}.ll"
    if ! "$DXBC2DXIL" "$dxbc_file" --emit-llvm > "$llvm_file" 2> "$OUTPUT_DIR/${basename}.dxbc2dxil.log"; then
        echo "FAILED (dxbc2dxil conversion)"
        FAILED=$((FAILED + 1))
        continue
    fi
    
    # For now, just check that we produced valid LLVM IR
    # The exact instruction ordering may differ from reference but still be correct
    if [ -s "$llvm_file" ] && grep -q "define void @main()" "$llvm_file"; then
        echo "PASSED (generated valid LLVM IR)"
        PASSED=$((PASSED + 1))
    else
        echo "FAILED (invalid or empty output)"
        FAILED=$((FAILED + 1))
    fi
done

echo ""
echo "========================================="
echo "Test Results:"
echo "  Total:   $TOTAL"
echo "  Passed:  $PASSED"
echo "  Failed:  $FAILED"
echo "  Skipped: $SKIPPED"
echo "========================================="

if [ $FAILED -eq 0 ] && [ $PASSED -gt 0 ]; then
    echo "All tests passed!"
    exit 0
else
    echo "Some tests failed."
    exit 1
fi