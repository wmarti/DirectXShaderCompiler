// dxilconv_uuids.cpp - UUID definitions for dxilconv interfaces
// These UUIDs are required when using __EMULATE_UUID on non-Windows platforms

#include "dxc/dxcapi.h"
#include "dxc/Support/FileIOHelper.h"
#include "dxc/Support/WinAdapter.h"

// Define the UUID symbols that are referenced but not defined elsewhere
// These use the pattern expected by DECLARE_CROSS_PLATFORM_UUIDOF

// IDxcBlob UUID
const size_t IDxcBlob::IDxcBlob_ID = 0x8ba5fb08;

// IDxcBlobEncoding UUID
const size_t IDxcBlobEncoding::IDxcBlobEncoding_ID = 0x7241d424;

// IDxcBlobUtf8 UUID  
const size_t IDxcBlobUtf8::IDxcBlobUtf8_ID = 0x3da636c9;

// IDxcBlobUtf16 UUID
const size_t IDxcBlobUtf16::IDxcBlobUtf16_ID = 0xa3f84eab;

// IDxcLibrary UUID
const size_t IDxcLibrary::IDxcLibrary_ID = 0xe5204dc7;

// IDxcCompiler UUID
const size_t IDxcCompiler::IDxcCompiler_ID = 0x8c210bf3;

// These are already defined in LLVMDxcSupport
// Commenting out to avoid duplicate symbols
// const size_t INoMarshal::INoMarshal_ID = 0xecc8691b;
// const size_t IStream::IStream_ID = 0x0000000c;
// const size_t ISequentialStream::ISequentialStream_ID = 0x0c733a30;