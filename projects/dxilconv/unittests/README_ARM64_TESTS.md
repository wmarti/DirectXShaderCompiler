# ARM64 DxilConv Unit Tests

This directory contains comprehensive unit tests for the dxbc2dxil converter specifically designed for ARM64 macOS systems. These tests replace TAEF (Test Authoring and Execution Framework) with a custom cross-platform testing framework and add ARM64-specific validation.

## Overview

The ARM64 test suite validates that the dxbc2dxil converter works correctly on Apple Silicon Macs, addressing potential issues related to:

- Memory alignment requirements
- Endianness handling
- NEON vector instruction usage
- Cache efficiency
- Large buffer handling
- ARM64-specific optimizations

## Test Structure

### Core Files

#### `DxilConvTests_macOS.cpp`
- Main ARM64-specific test implementation
- Includes system information detection
- ARM64-specific test data generators
- Memory, alignment, performance, and validation tests

#### `DxilConvTestFramework.h/cpp`
- Cross-platform test framework
- Replaces Microsoft TAEF with GoogleTest-compatible assertions
- File system utilities
- Process execution helpers
- Dynamic library loading

#### `DxilConvTestsCrossPlatform.cpp`
- Original cross-platform test implementation
- FileCheck implementation for test validation
- Base test classes

### Test Data

#### `test_data/arm64/`
- `simple_vertex.hlsl` - Basic vertex shader test
- `alignment_test.hlsl` - ARM64 alignment validation
- `vector_ops.hlsl` - NEON vector operation tests
- `large_buffer.hlsl` - Large buffer access patterns
- `endian_test.hlsl` - Endianness validation

### Test Runner

#### `run_arm64_tests.sh`
- Comprehensive test execution script
- System feature detection
- Performance benchmarking
- Report generation
- Configurable test selection

## Test Categories

### 1. Alignment Tests (`ARM64AlignmentTest`)

Tests ARM64-specific alignment requirements:

- **Data Alignment**: Verifies optimal cache line alignment
- **Pointer Alignment**: Validates function and stack pointer alignment
- **Struct Alignment**: Checks structure padding and alignment
- **Vector Alignment**: NEON 128-bit vector alignment validation

**Key Requirements:**
- 16-byte alignment for NEON vectors
- 8-byte alignment for 64-bit values
- 4-byte alignment for function pointers
- Cache line alignment for optimal performance

### 2. Memory Tests (`ARM64MemoryTest`)

Validates memory handling on ARM64:

- **Large Buffers**: Tests allocation and access of buffers up to 1GB
- **Page Boundaries**: Cross-page access validation (4KB, 16KB, 64KB pages)
- **Memory Mapping**: mmap functionality validation

**Key Features:**
- Detects and handles different page sizes (4KB/16KB/64KB)
- Tests virtual memory management
- Validates memory access patterns

### 3. Performance Tests (`ARM64PerformanceTest`)

Measures ARM64-specific performance characteristics:

- **Conversion Speed**: dxbc2dxil conversion timing
- **Memory Usage**: Memory allocation patterns
- **Cache Efficiency**: L1/L2/L3 cache utilization

**Metrics:**
- Conversion time per shader
- Memory usage growth
- Cache miss rates
- Sequential vs. random access patterns

### 4. Endianness Tests (`ARM64EndianTest`)

Validates little-endian handling:

- **Byte Order**: Confirms little-endian byte ordering
- **Float Encoding**: IEEE 754 compliance validation
- **Integer Encoding**: Multi-byte integer handling

**Validation:**
- 32-bit value 0x12345678 stored as [0x78, 0x56, 0x34, 0x12]
- IEEE 754 float/double representation
- Consistent endianness across data types

### 5. DXIL Validation Tests (`ARM64DXILValidationTest`)

Ensures DXIL output correctness:

- **Bitcode Integrity**: LLVM bitcode format validation
- **Metadata Consistency**: Shader metadata preservation
- **Instruction Validation**: ARM64-compatible instruction generation
- **Resource Binding**: Correct resource binding translation

## System Information Detection

The test suite automatically detects ARM64 system capabilities:

### CPU Features
```cpp
ARM64SystemInfo::SupportsFeature("neon")    // NEON SIMD support
ARM64SystemInfo::SupportsFeature("crc32")   // CRC32 instructions
ARM64SystemInfo::SupportsFeature("crypto")  // Crypto extensions
```

### Cache Hierarchy
```cpp
auto cache = ARM64SystemInfo::GetCacheInfo();
// cache.l1_instruction_cache_size
// cache.l1_data_cache_size  
// cache.l2_cache_size
// cache.l3_cache_size
// cache.cache_line_size
```

### Memory Configuration
```cpp
auto memory = ARM64SystemInfo::GetMemoryInfo();
// memory.total_memory
// memory.page_size
// memory.supports_16k_pages
// memory.supports_64k_pages
```

## Building and Running

### Prerequisites

1. **ARM64 macOS system** (Apple Silicon Mac)
2. **DirectXShaderCompiler** built for ARM64
3. **CMake 3.14+**
4. **LLVM** (for bitcode validation)

### Build Commands

```bash
# Configure build
cmake -B build_Release -DCMAKE_BUILD_TYPE=Release
cmake --build build_Release --target dxilconv-tests-arm64

# Run all ARM64 tests
make run-arm64-tests

# Run specific test categories
make run-arm64-tests-alignment
make run-arm64-tests-performance

# Manual test execution
./run_arm64_tests.sh
./run_arm64_tests.sh --alignment-only
./run_arm64_tests.sh --verbose
```

### Test Runner Options

```bash
./run_arm64_tests.sh [options]

Options:
  --alignment-only     Run only alignment tests
  --memory-only        Run only memory tests  
  --performance-only   Run only performance tests
  --endian-only        Run only endianness tests
  --validation-only    Run only validation tests
  --verbose, -v        Enable verbose output
  --help, -h           Show help message
```

## Test Data Generation

The test suite includes sophisticated test data generators:

### `ARM64TestDataGenerator`

- **`GenerateVertexShader()`**: Creates basic vertex shader test data
- **`GeneratePixelShader()`**: Creates pixel shader with texture sampling
- **`GenerateComputeShader()`**: Creates compute shader test case
- **`GenerateComplexShader()`**: Creates complex multi-stage shader
- **`GenerateAlignmentTestData()`**: Creates alignment-sensitive data
- **`GenerateRandomDXBC()`**: Creates randomized DXBC for stress testing

### Example Usage

```cpp
auto test_data = ARM64TestDataGenerator::GeneratePixelShader("test_ps");
// test_data.dxbc_data - Raw DXBC bytecode
// test_data.hlsl_source - Original HLSL source
// test_data.metadata - Test metadata and expectations
```

## Expected Results

### Successful Test Run

```
ARM64 DxilConv Test Suite
=========================
Platform: macOS ARM64

System Information:
CPU: Apple M1 Pro
L1I Cache: 128 KB
L1D Cache: 64 KB  
L2 Cache: 4096 KB
Cache Line: 64 bytes

Running ARM64-specific alignment tests...
  Testing data alignment... PASSED
  Testing pointer alignment... PASSED
  Testing struct alignment... PASSED
  Testing NEON vector alignment... PASSED

Running ARM64-specific memory tests...
  Testing large buffer handling... PASSED
  Testing page boundary handling... PASSED
  Testing memory mapping... PASSED

Total Tests: 15
Passed: 15
Failed: 0
Success Rate: 100%
```

### Common Issues and Solutions

#### Alignment Failures
```
FAIL: Data not properly aligned for size 1024
```
**Solution:** Ensure compiler uses ARM64-optimized alignment settings

#### Memory Access Errors
```
FAIL: Memory mapping read/write failed  
```
**Solution:** Check virtual memory permissions and page size handling

#### Performance Warnings
```
WARNING: Conversion seems slow for ARM64
```
**Solution:** Verify native ARM64 compilation (not Rosetta 2)

## Integration with Xenia

These tests are designed specifically for the Xenia Xbox 360 emulator's Metal backend, ensuring that:

1. **Shader Translation**: DXBC shaders from Xbox 360 games translate correctly to DXIL
2. **Metal Compatibility**: Generated DXIL is compatible with Metal Shader Converter
3. **Performance**: ARM64 optimizations provide good performance for real-time emulation
4. **Correctness**: Translated shaders produce identical results to original Xbox 360 hardware

## Future Enhancements

### Planned Improvements

1. **GPU Validation**: Metal Shader Converter integration tests
2. **Real Game Data**: Tests using actual Xbox 360 game shaders
3. **Regression Testing**: Automated comparison with known-good outputs
4. **Performance Profiling**: Detailed ARM64 performance analysis
5. **Memory Optimization**: Tests for memory usage optimization

### Contributing

When adding new ARM64-specific tests:

1. Follow the existing test pattern in `DxilConvTests_macOS.cpp`
2. Add test data to `test_data/arm64/`
3. Update `run_arm64_tests.sh` if needed
4. Ensure tests work on both M1 and M2 systems
5. Add appropriate documentation

## References

- [ARM64 Architecture Reference Manual](https://developer.arm.com/documentation/ddi0487/latest)
- [Apple Silicon Developer Documentation](https://developer.apple.com/documentation/apple-silicon)
- [DirectX Shader Compiler](https://github.com/Microsoft/DirectXShaderCompiler)
- [LLVM ARM64 Backend](https://llvm.org/docs/CodeGenerator.html#arm64-target)
- [Metal Shader Converter](https://developer.apple.com/metal/shader-converter/)

---

*This test suite ensures robust ARM64 compatibility for the dxbc2dxil converter, enabling reliable Xbox 360 shader emulation on Apple Silicon Macs.*