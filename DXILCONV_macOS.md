# DXILConv macOS ARM64 Build Notes

This tree contains the upstream DXIL converter sources with additional shims to
build natively on Apple Silicon.  The goal is to supply `dxbc2dxil` and
`libdxilconv.dylib` for Xenia's Metal backend.

## Prerequisites

- Xcode command line tools (`xcode-select --install`)
- Homebrew
- `directx-headers` via Homebrew (`brew install directx-headers`)
- CMake ≥ 3.5

## Building

```bash
cd third_party/DirectXShaderCompiler
./build_dxilconv_macos.sh
```

Artifacts land in `build_dxilconv_macos/`:

- `bin/dxbc2dxil`
- `lib/libdxilconv.dylib`

The script enables EH/RTTI, targets arm64, and sets policy minimum 3.5 so the
embedded LLVM CMake files configure cleanly.

## Running Tests (WIP)

`projects/dxilconv/unittests/run_arm64_tests.sh` expects *real* DXBC blobs and a
visible LLVM config.  At the moment the script writes placeholder text files, so
conversion fails with `0x80004005`.  To enable the suite:

1. Compile the HLSL fixtures under `projects/dxilconv/unittests/test_data` to
   DXBC using `dxc`/`fxc`, or provide prebuilt DXBC payloads.
2. Point CMake at the generated LLVM package:
   `export LLVM_DIR=$PWD/build_dxilconv_macos/share/llvm/cmake` (or equivalent).

The large Halo 3 regression set lives in
`projects/dxilconv/test/halo3_shaders.tar.gz`.  Unpack it only when you need to
run the upstream `run_full_tests.sh` script.

## Integration

After building, copy the dylib/binary into your Xenia build or adjust search
paths accordingly.  No changes are required outside this submodule.

