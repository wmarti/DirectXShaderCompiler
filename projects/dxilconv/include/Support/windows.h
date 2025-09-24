///////////////////////////////////////////////////////////////////////////////
// windows.h - Additional Windows compatibility definitions for dxilconv
// When using DXC, most types come from WinAdapter.h
// This file only adds missing definitions
///////////////////////////////////////////////////////////////////////////////

#ifndef DXILCONV_WINDOWS_H
#define DXILCONV_WINDOWS_H

#include <stdint.h>  // For uint32_t, int64_t, etc.
#include <stddef.h>  // For size_t, offsetof
#include <string.h>  // For string functions
#include <stdbool.h> // For bool type
#include <stdlib.h>  // For malloc/free

// When building with DXC, WinAdapter.h provides most Windows types
// This file only adds what's missing

// Additional macros DirectX headers expect
#ifndef EXTERN_C
#ifdef __cplusplus
#define EXTERN_C extern "C"
#else
#define EXTERN_C extern
#endif
#endif

// Missing integer types
#ifndef INT64
typedef int64_t INT64;
#endif

// Missing limits
#ifndef _UI8_MAX
#define _UI8_MAX UINT8_MAX
#endif

// Field offset macro
#ifndef FIELD_OFFSET
#define FIELD_OFFSET(type, field) offsetof(type, field)
#endif

// DirectX-specific constants that may not be in WinAdapter.h
#ifndef MAKEFOURCC
#define MAKEFOURCC(ch0, ch1, ch2, ch3) \
  ((uint32_t)(uint8_t)(ch0) | ((uint32_t)(uint8_t)(ch1) << 8) | \
   ((uint32_t)(uint8_t)(ch2) << 16) | ((uint32_t)(uint8_t)(ch3) << 24))
#endif

// D3D constants
#ifndef D3D_FEATURE_LEVEL_9_1
#define D3D_FEATURE_LEVEL_9_1 0x9100
#define D3D_FEATURE_LEVEL_9_2 0x9200
#define D3D_FEATURE_LEVEL_9_3 0x9300
#define D3D_FEATURE_LEVEL_10_0 0xa000
#define D3D_FEATURE_LEVEL_10_1 0xa100
#define D3D_FEATURE_LEVEL_11_0 0xb000
#define D3D_FEATURE_LEVEL_11_1 0xb100
#define D3D_FEATURE_LEVEL_12_0 0xc000
#define D3D_FEATURE_LEVEL_12_1 0xc100
#endif

// SAL annotations not in WinAdapter.h
#ifndef _Out_writes_to_opt_
#define _Out_writes_to_opt_(x,y)
#endif
#ifndef __in_range
#define __in_range(x,y)
#endif
#ifndef __in_ecount
#define __in_ecount(x)
#endif
#ifndef __field_ecount_part
#define __field_ecount_part(x,y)
#endif
#ifndef __out_ecount
#define __out_ecount(x)
#endif
#ifndef CONST
#define CONST const
#endif

// GUID comparison helpers
// WinAdapter.h already provides these, so we don't redefine them

// Define macros for GUIDs that DirectX headers will use
// These will expand to extern declarations that will be resolved later
#ifndef DEFINE_GUID
#ifdef INITGUID
#define DEFINE_GUID(name, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8) \
    EXTERN_C const GUID name = {l, w1, w2, {b1, b2, b3, b4, b5, b6, b7, b8}}
#else
#define DEFINE_GUID(name, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8) \
    EXTERN_C const GUID name
#endif
#endif

// Minimal additional COM support
#ifndef STDMETHOD
#define STDMETHOD(method) virtual HRESULT STDMETHODCALLTYPE method
#endif

#ifndef STDMETHOD_
#define STDMETHOD_(type, method) virtual type STDMETHODCALLTYPE method
#endif

#ifndef THIS_
#define THIS_
#endif

#ifndef THIS
#define THIS
#endif

#ifndef PURE
#define PURE = 0
#endif

// COM memory allocation - WinAdapter.h defines these as macros
#ifndef CoTaskMemAlloc
#define CoTaskMemAlloc(cb) malloc(cb)
#endif

#ifndef CoTaskMemFree
#define CoTaskMemFree(pv) free(pv)
#endif

// Additional types for D3D compatibility
// LPCVOID is already defined in WinAdapter.h as const void*
#ifndef LPVOID
typedef void* LPVOID;
#endif

// LPOVERLAPPED definition for file I/O
struct _OVERLAPPED;
typedef struct _OVERLAPPED* LPOVERLAPPED;

// Additional constants
#ifndef INVALID_HANDLE_VALUE
#define INVALID_HANDLE_VALUE ((HANDLE)(LONG_PTR)-1)
#endif

#ifndef MAX_PATH
#define MAX_PATH 260
#endif

// File attributes
#ifndef FILE_ATTRIBUTE_NORMAL
#define FILE_ATTRIBUTE_NORMAL 0x00000080
#endif

#ifndef FILE_ATTRIBUTE_DIRECTORY
#define FILE_ATTRIBUTE_DIRECTORY 0x00000010
#endif

// File access modes
#ifndef GENERIC_READ
#define GENERIC_READ 0x80000000
#endif

#ifndef GENERIC_WRITE
#define GENERIC_WRITE 0x40000000
#endif

// File share modes
#ifndef FILE_SHARE_READ
#define FILE_SHARE_READ 0x00000001
#endif

#ifndef FILE_SHARE_WRITE
#define FILE_SHARE_WRITE 0x00000002
#endif

// File creation modes
#ifndef CREATE_ALWAYS
#define CREATE_ALWAYS 2
#endif

#ifndef OPEN_EXISTING
#define OPEN_EXISTING 3
#endif

// Memory protection flags
#ifndef PAGE_READONLY
#define PAGE_READONLY 0x02
#endif

#ifndef PAGE_READWRITE
#define PAGE_READWRITE 0x04
#endif

#ifndef PAGE_EXECUTE
#define PAGE_EXECUTE 0x10
#endif

#ifndef PAGE_EXECUTE_READ
#define PAGE_EXECUTE_READ 0x20
#endif

#ifndef PAGE_EXECUTE_READWRITE
#define PAGE_EXECUTE_READWRITE 0x40
#endif

// Wait constants
#ifndef INFINITE
#define INFINITE 0xFFFFFFFF
#endif

#ifndef WAIT_OBJECT_0
#define WAIT_OBJECT_0 0x00000000
#endif

#ifndef WAIT_TIMEOUT
#define WAIT_TIMEOUT 0x00000102
#endif

#ifndef WAIT_FAILED
#define WAIT_FAILED 0xFFFFFFFF
#endif

// Additional Windows error codes
#ifndef ERROR_INVALID_PARAMETER
#define ERROR_INVALID_PARAMETER 87
#endif

#ifndef ERROR_INVALID_HANDLE
#define ERROR_INVALID_HANDLE 6
#endif

#ifndef ERROR_NOT_ENOUGH_MEMORY
#define ERROR_NOT_ENOUGH_MEMORY 8
#endif

#ifndef ERROR_FILE_NOT_FOUND
#define ERROR_FILE_NOT_FOUND 2
#endif

#ifndef ERROR_ACCESS_DENIED
#define ERROR_ACCESS_DENIED 5
#endif

// Additional HRESULT codes
#ifndef E_POINTER
#define E_POINTER ((HRESULT)0x80004003L)
#endif

#ifndef E_ABORT
#define E_ABORT ((HRESULT)0x80004004L)
#endif

#ifndef E_ACCESSDENIED
#define E_ACCESSDENIED ((HRESULT)0x80070005L)
#endif

#ifndef E_HANDLE
#define E_HANDLE ((HRESULT)0x80070006L)
#endif

// HRESULT helper macros
#ifndef HRESULT_CODE
#define HRESULT_CODE(hr) ((hr) & 0xFFFF)
#endif

#ifndef HRESULT_FACILITY
#define HRESULT_FACILITY(hr) (((hr) >> 16) & 0x1FFF)
#endif

#ifndef HRESULT_SEVERITY
#define HRESULT_SEVERITY(hr) (((hr) >> 31) & 0x1)
#endif

#ifndef MAKE_HRESULT
#define MAKE_HRESULT(sev,fac,code) \
    ((HRESULT) (((unsigned long)(sev)<<31) | ((unsigned long)(fac)<<16) | ((unsigned long)(code))) )
#endif

// Critical section type (minimal definition for macOS)
#ifndef _CRITICAL_SECTION_DEFINED
#define _CRITICAL_SECTION_DEFINED
typedef struct _CRITICAL_SECTION {
    void* mutex_impl; // Will hold pthread_mutex_t*
} CRITICAL_SECTION, *LPCRITICAL_SECTION;
#endif

// UUID/IID helper macros
#ifndef __EMULATE_UUID
#define __EMULATE_UUID 1
#endif

// Make DirectX headers happy
#ifndef WINAPI_FAMILY
#define WINAPI_FAMILY 0
#endif

#ifndef WINAPI_PARTITION_DESKTOP
#define WINAPI_PARTITION_DESKTOP 1
#endif

#ifndef WINAPI_FAMILY_PARTITION
#define WINAPI_FAMILY_PARTITION(x) 1
#endif

// Unicode/ANSI helpers
#ifndef CP_UTF8
#define CP_UTF8 65001
#endif

#ifndef CP_ACP
#define CP_ACP 0
#endif

// Fake registry types (not used but may be referenced)
typedef unsigned long REGSAM;
typedef void* HKEY;

// Make sure we have basic Windows calling conventions
#ifndef STDMETHODCALLTYPE
#define STDMETHODCALLTYPE
#endif

#ifndef WINAPI
#define WINAPI
#endif

#ifndef CALLBACK
#define CALLBACK
#endif

// C++ keyword emulation
#ifndef __override
#define __override override
#endif

// Additional type definitions for compatibility
// HINSTANCE and HMODULE are already defined in WinAdapter.h
#ifndef HWND
typedef void* HWND;
#endif
#ifndef HDC
typedef void* HDC;
#endif
#ifndef HGLRC
typedef void* HGLRC;
#endif

// Function pointer types
typedef void (*PROC)(void);
typedef int (*FARPROC)(void);

// Thread local storage
#ifndef TLS_OUT_OF_INDEXES
#define TLS_OUT_OF_INDEXES ((DWORD)0xFFFFFFFF)
#endif

// GetProcAddress is defined in PlatformHelpers_macOS.cpp
// Forward declare it here for the cast helper
#ifdef __cplusplus
extern "C" LPVOID GetProcAddress(HMODULE hModule, LPCSTR lpProcName);

template<typename T>
inline T GetProcAddressCast(HMODULE module, const char* name) {
    return reinterpret_cast<T>(GetProcAddress(module, name));
}
#endif

#endif // DXILCONV_WINDOWS_H