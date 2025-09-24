#!/bin/bash

# Comprehensive comparison of Windows dxbc2dxil.exe with ARM64 native dxbc2dxil
# Tests all DXBC files and compares LLVM IR and Metal outputs

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
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
    echo -e "${YELLOW}ARM64 binary not found. Building it now...${NC}"
    ./build_dxbc2dxil_arm64.sh
    if [ ! -f "$ARM64_DXBC2DXIL" ]; then
        echo -e "${RED}Build failed!${NC}"
        exit 1
    fi
fi

# Check Wine is installed
if ! command -v wine &> /dev/null && ! command -v wine64 &> /dev/null; then
    echo -e "${RED}Error: wine is not installed. Please install Wine to run Windows binary.${NC}"
    echo "On macOS: brew install wine-stable"
    exit 1
fi

# Use wine64 if available, otherwise use wine
if command -v wine64 &> /dev/null; then
    WINE_CMD="wine64"
else
    WINE_CMD="wine"
fi

# Check metal-shaderconverter is installed
if ! command -v metal-shaderconverter &> /dev/null; then
    echo -e "${YELLOW}Warning: metal-shaderconverter not found. Metal comparison will be skipped.${NC}"
    SKIP_METAL=1
else
    SKIP_METAL=0
fi

# Create output directories
OUTPUT_DIR="$SCRIPT_DIR/comparison_results"
WINDOWS_DXIL="$OUTPUT_DIR/windows_dxil"
ARM64_DXIL="$OUTPUT_DIR/arm64_dxil"
WINDOWS_LLVM="$OUTPUT_DIR/windows_llvm"
ARM64_LLVM="$OUTPUT_DIR/arm64_llvm"
WINDOWS_METAL="$OUTPUT_DIR/windows_metal"
ARM64_METAL="$OUTPUT_DIR/arm64_metal"

rm -rf "$OUTPUT_DIR"
mkdir -p "$WINDOWS_DXIL" "$ARM64_DXIL" "$WINDOWS_LLVM" "$ARM64_LLVM" "$WINDOWS_METAL" "$ARM64_METAL"

# Test files location - find ALL DXBC files and Halo 3 shaders
echo "Finding all DXBC and Halo 3 shader files..." >&2
# Find .dxbc files and Halo 3 .frag/.vert files
DXBC_FILES=$(find "$SCRIPT_DIR" \( -name "*.dxbc" -o -name "*.frag" -o -name "*.vert" \) -type f | sort)
DXBC_COUNT=$(echo "$DXBC_FILES" | wc -l | tr -d ' ')
echo "Found $DXBC_COUNT shader files to test" >&2

# Log file
LOG_FILE="$OUTPUT_DIR/comparison.log"
DIFF_LOG="$OUTPUT_DIR/differences.log"

# Statistics
TOTAL=0
WINDOWS_SUCCESS=0
WINDOWS_FAILED=0
ARM64_SUCCESS=0
ARM64_FAILED=0
LLVM_IDENTICAL=0
LLVM_DIFFERENT=0
METAL_IDENTICAL=0
METAL_DIFFERENT=0
METAL_WINDOWS_ONLY=0
METAL_ARM64_ONLY=0
METAL_BOTH_FAILED=0
# File type counts
DXBC_COUNT_ONLY=0
FRAG_COUNT=0
VERT_COUNT=0

echo -e "${BLUE}=========================================${NC}" | tee "$LOG_FILE"
echo -e "${BLUE}DXBC2DXIL Windows vs ARM64 Comparison${NC}" | tee -a "$LOG_FILE"
echo -e "${BLUE}=========================================${NC}" | tee -a "$LOG_FILE"
echo "" | tee -a "$LOG_FILE"
echo "Windows binary: $WINDOWS_DXBC2DXIL" | tee -a "$LOG_FILE"
echo "ARM64 binary: $ARM64_DXBC2DXIL" | tee -a "$LOG_FILE"
echo "" | tee -a "$LOG_FILE"

# Function to run Windows dxbc2dxil
run_windows_dxbc2dxil() {
    local input="$1"
    local output="$2"
    local args="${3:-}"
    
    # Copy input file and DLL to temp Wine directory for better compatibility
    WINE_TEMP="$OUTPUT_DIR/wine_temp"
    mkdir -p "$WINE_TEMP"
    # Preserve original extension
    local input_ext="${input##*.}"
    cp "$input" "$WINE_TEMP/input.$input_ext"
    cp "$WINDOWS_DXILCONV" "$WINE_TEMP/" 2>/dev/null || true
    
    # Convert paths to Windows format for Wine
    # Use relative paths from the Windows binary location
    local wine_input="input.$input_ext"
    local wine_output="output.dxil"
    
    # Run with Wine from the directory containing the exe
    if [ -z "$args" ]; then
        (cd "$WINE_TEMP" && $WINE_CMD "$WINDOWS_DXBC2DXIL" "$wine_input" /o "$wine_output" 2>/dev/null)
        if [ -f "$WINE_TEMP/$wine_output" ]; then
            mv "$WINE_TEMP/$wine_output" "$output"
        fi
    else
        (cd "$WINE_TEMP" && $WINE_CMD "$WINDOWS_DXBC2DXIL" "$wine_input" $args 2>/dev/null > temp_output.txt)
        if [ -f "$WINE_TEMP/temp_output.txt" ]; then
            mv "$WINE_TEMP/temp_output.txt" "$output"
        fi
    fi
    
    rm -rf "$WINE_TEMP"
}

# Process each shader file
echo "Processing $DXBC_COUNT shader files..." | tee -a "$LOG_FILE"
echo "" | tee -a "$LOG_FILE"

for dxbc_file in $DXBC_FILES; do
    if [ ! -f "$dxbc_file" ]; then
        continue
    fi
    
    # Get relative path for better display
    relative_path=${dxbc_file#$SCRIPT_DIR/}
    # Get filename without extension
    filename=$(basename "$dxbc_file")
    extension="${filename##*.}"
    basename="${filename%.*}"
    # Create safe filename for outputs (replace / with _)
    safe_name=$(echo "$relative_path" | sed 's/\//_/g' | sed 's/\.[^.]*$//')
    TOTAL=$((TOTAL + 1))
    
    # Count file types
    case "$extension" in
        dxbc) DXBC_COUNT_ONLY=$((DXBC_COUNT_ONLY + 1)) ;;
        frag) FRAG_COUNT=$((FRAG_COUNT + 1)) ;;
        vert) VERT_COUNT=$((VERT_COUNT + 1)) ;;
    esac
    
    # Display shortened path for readability
    display_name=$(echo "$relative_path" | sed 's/^projects\/dxilconv\/test\///' | sed 's/\.[^.]*$//')
    printf "[%3d] %-50s " "$TOTAL" "$display_name:" | tee -a "$LOG_FILE"
    
    # Run Windows version
    WINDOWS_OK=0
    if run_windows_dxbc2dxil "$dxbc_file" "$WINDOWS_DXIL/$safe_name.dxil" 2>/dev/null; then
        if run_windows_dxbc2dxil "$dxbc_file" "$WINDOWS_LLVM/$safe_name.ll" "/emit-llvm" 2>/dev/null; then
            WINDOWS_OK=1
            WINDOWS_SUCCESS=$((WINDOWS_SUCCESS + 1))
            printf "${GREEN}[Win: ✓]${NC} " | tee -a "$LOG_FILE"
        fi
    fi
    
    if [ $WINDOWS_OK -eq 0 ]; then
        WINDOWS_FAILED=$((WINDOWS_FAILED + 1))
        printf "${RED}[Win: ✗]${NC} " | tee -a "$LOG_FILE"
    fi
    
    # Run ARM64 version
    ARM64_OK=0
    if "$ARM64_DXBC2DXIL" "$dxbc_file" -o "$ARM64_DXIL/$safe_name.dxil" 2>/dev/null; then
        if "$ARM64_DXBC2DXIL" "$dxbc_file" --emit-llvm > "$ARM64_LLVM/$safe_name.ll" 2>/dev/null; then
            ARM64_OK=1
            ARM64_SUCCESS=$((ARM64_SUCCESS + 1))
            printf "${GREEN}[ARM: ✓]${NC} " | tee -a "$LOG_FILE"
        fi
    fi
    
    if [ $ARM64_OK -eq 0 ]; then
        ARM64_FAILED=$((ARM64_FAILED + 1))
        printf "${RED}[ARM: ✗]${NC} " | tee -a "$LOG_FILE"
    fi
    
    # Compare LLVM IR if both succeeded
    if [ $WINDOWS_OK -eq 1 ] && [ $ARM64_OK -eq 1 ]; then
        if [ -f "$WINDOWS_LLVM/$safe_name.ll" ] && [ -f "$ARM64_LLVM/$safe_name.ll" ]; then
            # Normalize LLVM IR (remove comments and metadata that might differ)
            grep -v "^;" "$WINDOWS_LLVM/$safe_name.ll" | grep -v "^!" > "$WINDOWS_LLVM/$safe_name.ll.normalized" 2>/dev/null || true
            grep -v "^;" "$ARM64_LLVM/$safe_name.ll" | grep -v "^!" > "$ARM64_LLVM/$safe_name.ll.normalized" 2>/dev/null || true
            
            if diff -w -B -q "$WINDOWS_LLVM/$safe_name.ll.normalized" "$ARM64_LLVM/$safe_name.ll.normalized" > /dev/null 2>&1; then
                printf "${GREEN}[LLVM: ✓]${NC} " | tee -a "$LOG_FILE"
                LLVM_IDENTICAL=$((LLVM_IDENTICAL + 1))
            else
                printf "${YELLOW}[LLVM: ≠]${NC} " | tee -a "$LOG_FILE"
                LLVM_DIFFERENT=$((LLVM_DIFFERENT + 1))
                echo "" >> "$DIFF_LOG"
                echo "=== $display_name LLVM IR differences ===" >> "$DIFF_LOG"
                diff -u "$WINDOWS_LLVM/$safe_name.ll.normalized" "$ARM64_LLVM/$safe_name.ll.normalized" >> "$DIFF_LOG" 2>&1 || true
            fi
        fi
        
        # Metal conversion comparison (if not skipped)
        if [ $SKIP_METAL -eq 0 ]; then
            WINDOWS_METAL_OK=0
            ARM64_METAL_OK=0
            
            if metal-shaderconverter "$WINDOWS_DXIL/$safe_name.dxil" -o "$WINDOWS_METAL/$safe_name.metallib" 2>/dev/null; then
                WINDOWS_METAL_OK=1
            fi
            
            if metal-shaderconverter "$ARM64_DXIL/$safe_name.dxil" -o "$ARM64_METAL/$safe_name.metallib" 2>/dev/null; then
                ARM64_METAL_OK=1
            fi
            
            if [ $WINDOWS_METAL_OK -eq 1 ] && [ $ARM64_METAL_OK -eq 1 ]; then
                # Compare Metal binaries
                if cmp -s "$WINDOWS_METAL/$safe_name.metallib" "$ARM64_METAL/$safe_name.metallib"; then
                    printf "${GREEN}[Metal: ✓]${NC}" | tee -a "$LOG_FILE"
                    METAL_IDENTICAL=$((METAL_IDENTICAL + 1))
                else
                    printf "${YELLOW}[Metal: ≠]${NC}" | tee -a "$LOG_FILE"
                    METAL_DIFFERENT=$((METAL_DIFFERENT + 1))
                fi
            elif [ $WINDOWS_METAL_OK -eq 1 ] && [ $ARM64_METAL_OK -eq 0 ]; then
                printf "${RED}[Metal: Win only]${NC}" | tee -a "$LOG_FILE"
                METAL_WINDOWS_ONLY=$((METAL_WINDOWS_ONLY + 1))
            elif [ $WINDOWS_METAL_OK -eq 0 ] && [ $ARM64_METAL_OK -eq 1 ]; then
                printf "${RED}[Metal: ARM only]${NC}" | tee -a "$LOG_FILE"
                METAL_ARM64_ONLY=$((METAL_ARM64_ONLY + 1))
            else
                printf "${YELLOW}[Metal: both failed]${NC}" | tee -a "$LOG_FILE"
                METAL_BOTH_FAILED=$((METAL_BOTH_FAILED + 1))
            fi
        fi
    fi
    
    echo "" | tee -a "$LOG_FILE"
done

# Print summary
echo "" | tee -a "$LOG_FILE"
echo -e "${BLUE}=========================================${NC}" | tee -a "$LOG_FILE"
echo -e "${BLUE}Summary${NC}" | tee -a "$LOG_FILE"
echo -e "${BLUE}=========================================${NC}" | tee -a "$LOG_FILE"
echo "" | tee -a "$LOG_FILE"
echo "Total shader files tested: $TOTAL" | tee -a "$LOG_FILE"
if [ $DXBC_COUNT_ONLY -gt 0 ] || [ $FRAG_COUNT -gt 0 ] || [ $VERT_COUNT -gt 0 ]; then
    echo "  DXBC files: $DXBC_COUNT_ONLY" | tee -a "$LOG_FILE"
    if [ $FRAG_COUNT -gt 0 ] || [ $VERT_COUNT -gt 0 ]; then
        echo "  Halo 3 shaders:" | tee -a "$LOG_FILE"
        [ $FRAG_COUNT -gt 0 ] && echo "    Fragment shaders: $FRAG_COUNT" | tee -a "$LOG_FILE"
        [ $VERT_COUNT -gt 0 ] && echo "    Vertex shaders: $VERT_COUNT" | tee -a "$LOG_FILE"
    fi
fi
echo "" | tee -a "$LOG_FILE"

echo "Conversion Success:" | tee -a "$LOG_FILE"
echo "  Windows: $WINDOWS_SUCCESS/$TOTAL" | tee -a "$LOG_FILE"
echo "  ARM64:   $ARM64_SUCCESS/$TOTAL" | tee -a "$LOG_FILE"
echo "" | tee -a "$LOG_FILE"

if [ $((WINDOWS_SUCCESS)) -gt 0 ] && [ $((ARM64_SUCCESS)) -gt 0 ]; then
    echo "LLVM IR Comparison (for successful conversions):" | tee -a "$LOG_FILE"
    echo "  Identical: $LLVM_IDENTICAL" | tee -a "$LOG_FILE"
    echo "  Different: $LLVM_DIFFERENT" | tee -a "$LOG_FILE"
    
    if [ $LLVM_IDENTICAL -gt 0 ]; then
        PERCENT=$((LLVM_IDENTICAL * 100 / (LLVM_IDENTICAL + LLVM_DIFFERENT)))
        echo "  Match rate: ${PERCENT}%" | tee -a "$LOG_FILE"
    fi
    echo "" | tee -a "$LOG_FILE"
fi

if [ $SKIP_METAL -eq 0 ]; then
    echo "Metal Conversion Comparison:" | tee -a "$LOG_FILE"
    echo "  Identical outputs: $METAL_IDENTICAL" | tee -a "$LOG_FILE"
    echo "  Different outputs: $METAL_DIFFERENT" | tee -a "$LOG_FILE"
    echo "  Windows only: $METAL_WINDOWS_ONLY" | tee -a "$LOG_FILE"
    echo "  ARM64 only: $METAL_ARM64_ONLY" | tee -a "$LOG_FILE"
    echo "  Both failed: $METAL_BOTH_FAILED" | tee -a "$LOG_FILE"
    echo "" | tee -a "$LOG_FILE"
fi

# Final verdict
echo -e "${BLUE}=========================================${NC}" | tee -a "$LOG_FILE"
if [ $LLVM_IDENTICAL -eq $((WINDOWS_SUCCESS)) ] && [ $LLVM_IDENTICAL -eq $((ARM64_SUCCESS)) ] && [ $LLVM_DIFFERENT -eq 0 ]; then
    echo -e "${GREEN}✓ SUCCESS: All LLVM IR outputs are identical!${NC}" | tee -a "$LOG_FILE"
    echo -e "${GREEN}The ARM64 port is functionally equivalent to the Windows version!${NC}" | tee -a "$LOG_FILE"
else
    echo -e "${YELLOW}⚠ There are some differences between Windows and ARM64 outputs.${NC}" | tee -a "$LOG_FILE"
    if [ -f "$DIFF_LOG" ]; then
        echo "  See $DIFF_LOG for details." | tee -a "$LOG_FILE"
    fi
fi

echo "" | tee -a "$LOG_FILE"
echo "Full log saved to: $LOG_FILE" 
echo "Results saved to: $OUTPUT_DIR"

if [ $LLVM_DIFFERENT -gt 0 ]; then
    echo ""
    echo "Files with LLVM IR differences:"
    for f in "$ARM64_LLVM"/*.ll.normalized; do
        if [ -f "$f" ]; then
            safe_name=$(basename "$f" .ll.normalized)
            if [ -f "$WINDOWS_LLVM/$safe_name.ll.normalized" ]; then
                if ! diff -w -B -q "$WINDOWS_LLVM/$safe_name.ll.normalized" "$f" > /dev/null 2>&1; then
                    # Convert safe_name back to readable format
                    readable_name=$(echo "$safe_name" | sed 's/_/\//g')
                    echo "  - $readable_name"
                fi
            fi
        fi
    done
fi

if [ $METAL_DIFFERENT -gt 0 ] && [ $SKIP_METAL -eq 0 ]; then
    echo ""
    echo "Files with Metal library differences:"
    for f in "$ARM64_METAL"/*.metallib; do
        if [ -f "$f" ]; then
            safe_name=$(basename "$f" .metallib)
            if [ -f "$WINDOWS_METAL/$safe_name.metallib" ]; then
                if ! cmp -s "$WINDOWS_METAL/$safe_name.metallib" "$f"; then
                    # Convert safe_name back to readable format
                    readable_name=$(echo "$safe_name" | sed 's/_/\//g')
                    echo "  - $readable_name"
                fi
            fi
        fi
    done
fi