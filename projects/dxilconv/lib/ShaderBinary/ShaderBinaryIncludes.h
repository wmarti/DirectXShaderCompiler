///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// ShaderBinaryIncludes.cpp                                                  //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#pragma once

// Cross-platform includes
#ifdef _WIN32
#include "windows.h"
#include <strsafe.h>
#include <intsafe.h>
#elif defined(DXC_BUILD)
// Use DXC's WinAdapter types when building with DXC
#include <stdint.h>
#include <string.h>
#include <cstdlib>
#include <cstring>

// String copy function stub for DXC builds
#ifndef StringCchCopyA
#define StringCchCopyA(dst, size, src) strncpy(dst, src, size)
#endif

// Min/max macros for DXC builds
#ifndef min
#define min(a,b) (((a) < (b)) ? (a) : (b))
#endif
#ifndef max
#define max(a,b) (((a) > (b)) ? (a) : (b))
#endif
#else
// macOS/Unix includes
#include <stdint.h>
#include <string.h>
#include <cstdlib>
#include <cstring>

// Windows type definitions for cross-platform compatibility
typedef uint32_t UINT;
typedef uint32_t DWORD;
typedef int32_t INT;
typedef uint8_t BYTE;
typedef int BOOL;
typedef void* LPVOID;
typedef const void* LPCVOID;
typedef wchar_t WCHAR;
typedef char* LPSTR;
typedef const char* LPCSTR;
typedef wchar_t* LPWSTR;
typedef const wchar_t* LPCWSTR;

#define TRUE 1
#define FALSE 0
#define CONST const
#define ZeroMemory(ptr, size) memset(ptr, 0, size)
#endif

#include <assert.h>
#include <float.h>

// Use our compatibility headers instead of system DirectX headers
#ifdef DXC_BUILD
#include "dxc/Support/WinAdapter.h"
#include "dxc/Support/WinIncludes.h"
#else
#include <dxgiformat.h>
#include <d3d12.h>
#define D3DX12_NO_STATE_OBJECT_HELPERS
#include "dxc/Support/d3dx12.h"
#endif

#include "D3D12TokenizedProgramFormat.hpp"
#include "ShaderBinary/ShaderBinary.h"

// Cross-platform ASSUME macro
#ifdef _WIN32
#define ASSUME( _exp ) { assert( _exp ); __analysis_assume( _exp ); __assume( _exp ); }
#else
#define ASSUME( _exp ) { assert( _exp ); }
#endif

// ARM64 alignment helpers
#ifdef __ARM_ARCH
#define ARM64_ALIGNMENT_REQUIRED 1
#else
#define ARM64_ALIGNMENT_REQUIRED 0
#endif

// Safe unaligned memory access functions
inline UINT ReadUnalignedUINT(const void* ptr) {
#if ARM64_ALIGNMENT_REQUIRED
    UINT value;
    memcpy(&value, ptr, sizeof(UINT));
    return value;
#else
    return *static_cast<const UINT*>(ptr);
#endif
}

inline void WriteUnalignedUINT(void* ptr, UINT value) {
#if ARM64_ALIGNMENT_REQUIRED
    memcpy(ptr, &value, sizeof(UINT));
#else
    *static_cast<UINT*>(ptr) = value;
#endif
}

inline float ReadUnalignedFloat(const void* ptr) {
#if ARM64_ALIGNMENT_REQUIRED
    float value;
    memcpy(&value, ptr, sizeof(float));
    return value;
#else
    return *static_cast<const float*>(ptr);
#endif
}

// Ensure 4-byte alignment for token pointers
inline const void* AlignTokenPointer(const void* ptr) {
    uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
    return reinterpret_cast<const void*>((addr + 3) & ~3);
}

inline void* AlignTokenPointer(void* ptr) {
    uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
    return reinterpret_cast<void*>((addr + 3) & ~3);
}
