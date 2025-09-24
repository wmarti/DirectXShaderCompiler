# DirectXShaderCompiler Command-Line Tools - Unix/macOS Porting Guide

## Overview
This guide documents the porting of DirectXShaderCompiler command-line tools (dxbc2dxil and dxilconv) from Windows to Unix/macOS platforms.

## Key Changes Made

### 1. Entry Point Conversion
- **Windows**: `wmain()` with wide character arguments
- **Unix/macOS**: `main()` with UTF-8 arguments converted to wide strings

```cpp
// Unix/macOS main function
int main(int argc, char **argv) {
    setlocale(LC_ALL, "");
    // Convert UTF-8 arguments to wide strings
    wchar_t** wargv = new wchar_t*[argc + 1];
    for (int i = 0; i < argc; i++) {
        std::wstring wstr = UTF8ToWString(argv[i]);
        wargv[i] = new wchar_t[wstr.length() + 1];
        wcscpy(wargv[i], wstr.c_str());
    }
    // ... use wargv
}
```

### 2. Dynamic Library Loading
- **Windows**: `LoadLibraryW()` / `GetProcAddress()`
- **Unix/macOS**: `dlopen()` / `dlsym()`

```cpp
// Unix/macOS library loading
void* hModule = dlopen(dllFileName, RTLD_LAZY | RTLD_LOCAL);
void* pFn = dlsym(hModule, "DxcCreateInstance");
```

### 3. Library Names
- **Windows**: `dxcompiler.dll`, `dxilconv.dll`
- **macOS**: `libdxcompiler.dylib`, `libdxilconv.dylib`
- **Linux**: `libdxcompiler.so`, `libdxilconv.so`

### 4. Wide Character String Handling
Helper functions for UTF-8 ↔ Wide String conversion:

```cpp
static std::string WStringToUTF8(const std::wstring& wstr) {
    std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
    return converter.to_bytes(wstr);
}

static std::wstring UTF8ToWString(const std::string& str) {
    std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
    return converter.from_bytes(str);
}
```

### 5. Library Initialization
- **Windows**: `DllMain()` with `DLL_PROCESS_ATTACH`/`DLL_PROCESS_DETACH`
- **Unix/macOS**: Constructor/destructor attributes

```cpp
// Unix/macOS library initialization
static void LibraryInit() __attribute__((constructor));
static void LibraryCleanup() __attribute__((destructor));
```

### 6. Command-Line Options
Updated to support Unix-style double-dash options:
- `/help` → `--help` (also supports `-h`)
- `/o` → `-o`
- `/disasm-dxbc` → `--disasm-dxbc`
- `/emit-llvm` → `--emit-llvm`
- `/emit-bc` → `--emit-bc`

### 7. Path Separators
- Automatically handled by LLVM's path utilities
- File operations use forward slashes on Unix/macOS

## Build Instructions

### Prerequisites
- CMake 3.10 or later
- C++14 compatible compiler
- LLVM libraries
- DirectXShaderCompiler source tree

### Building on macOS
```bash
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(sysctl -n hw.ncpu)
```

### Building on Linux
```bash
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

## Usage Examples

### Convert DXBC to DXIL
```bash
./dxbc2dxil input.dxbc -o output.dxil
```

### Disassemble DXBC
```bash
./dxbc2dxil input.dxbc --disasm-dxbc -o output.asm
```

### Emit LLVM IR
```bash
./dxbc2dxil input.dxbc --emit-llvm -o output.ll
```

## File Structure
```
projects/dxilconv/
├── tools/
│   ├── dxbc2dxil/
│   │   ├── dxbc2dxil.cpp          # Original Windows version
│   │   ├── dxbc2dxil_unix.cpp     # Unix/macOS port
│   │   └── CMakeLists.txt         # Build configuration
│   └── dxilconv/
│       ├── dxilconv.cpp           # Original Windows version
│       ├── dxilconv_unix.cpp      # Unix/macOS port
│       ├── dxilconv.def           # Windows exports
│       └── CMakeLists.txt         # Build configuration
└── lib/
    └── DxbcConverter/             # Core converter library
```

## Platform-Specific Considerations

### macOS
- Uses `.dylib` extension for shared libraries
- Requires `@loader_path` for RPATH to find libraries in same directory
- Exported symbols use underscore prefix (`_DxcCreateInstance`)

### Linux
- Uses `.so` extension for shared libraries
- Requires `$ORIGIN` for RPATH
- Uses version scripts for symbol export control

## Testing
Run the test suite to verify the port:
```bash
# Test basic conversion
./dxbc2dxil test.dxbc -o test.dxil

# Verify output
xxd test.dxil | head -20

# Test disassembly
./dxbc2dxil test.dxbc --disasm-dxbc
```

## Known Limitations
1. ETW tracing is stubbed out on Unix/macOS
2. Some Windows-specific error codes are mapped to generic Unix equivalents
3. Thread-local storage uses pthread instead of Windows TLS

## Troubleshooting

### Library Not Found
If you get "library not found" errors, ensure:
1. Libraries are in the same directory as the executable
2. RPATH is correctly set (check with `otool -l` on macOS or `readelf -d` on Linux)
3. Library names match the platform convention

### Symbol Resolution Issues
Check exported symbols:
- macOS: `nm -g libdxilconv.dylib | grep DxcCreateInstance`
- Linux: `nm -D libdxilconv.so | grep DxcCreateInstance`

### Wide Character Issues
Ensure locale is properly set:
```cpp
setlocale(LC_ALL, "");  // Use system locale
// or
setlocale(LC_ALL, "en_US.UTF-8");  // Force UTF-8
```