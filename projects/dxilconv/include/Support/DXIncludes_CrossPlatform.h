/**
 ******************************************************************************
 * DirectXShaderCompiler Cross-Platform Includes                             *
 ******************************************************************************
 * Copyright 2024 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#ifndef DXILCONV_DXINCLUDES_CROSSPLATFORM_H_
#define DXILCONV_DXINCLUDES_CROSSPLATFORM_H_

// Cross-platform DirectX includes wrapper for macOS ARM64
// This file provides platform-agnostic includes for DirectX types and APIs

// Define platform macros for DirectXShaderCompiler context
#if defined(__APPLE__) && defined(__MACH__)
#define XE_PLATFORM_MAC 1
#else
#define XE_PLATFORM_MAC 0
#endif

// Define missing Windows types for macOS
#if XE_PLATFORM_MAC

// Allow WinAdapter.h to be included when building with DXC

#include <cstdint>
#include <cstring>
#include <cassert>
#include <atomic>
#include <string>
#include <vector>
#include <alloca.h>
#include <pthread.h>

// When building with DXC, just use WinAdapter.h
// Otherwise use WSL headers for standalone builds
#ifdef DXC_BUILD
  // DXC build - use WinAdapter.h for Windows types
  #include "dxc/Support/WinAdapter.h"
  #include "dxc/Support/WinIncludes.h"
#else
  // Standalone build - use WSL headers
  #define USING_WSL_HEADERS
  #include <wsl/stubs/basetsd.h>
  #include <wsl/stubs/rpcndr.h>
  #include <wsl/stubs/rpc.h>
  #include <wsl/stubs/unknwn.h>
  // Include WRL adapter for COM support (provides ComPtr)
  #include <wsl/wrladapter.h>
  // Include our ATL-compatible CComPtr wrapper
  #include "Support/CComPtrCompat.h"
#endif

// When not building with DXC, define missing interfaces and types
#ifndef DXC_BUILD

// Define ISequentialStream interface if not provided
#ifndef __ISequentialStream_INTERFACE_DEFINED__
#define __ISequentialStream_INTERFACE_DEFINED__
interface ISequentialStream : public IUnknown {
    virtual HRESULT Read(void *pv, ULONG cb, ULONG *pcbRead) = 0;
    virtual HRESULT Write(const void *pv, ULONG cb, ULONG *pcbWritten) = 0;
};
#endif

// ULARGE_INTEGER is already defined in basetsd.h

// CLSID is defined in basetsd.h
// FILETIME needs to be defined for macOS
typedef struct _FILETIME {
    DWORD dwLowDateTime;
    DWORD dwHighDateTime;
} FILETIME, *PFILETIME, *LPFILETIME;

// Define IStream interface if not already defined
#ifndef __IStream_INTERFACE_DEFINED__
#define __IStream_INTERFACE_DEFINED__

// STATSTG is forward declared in basetsd.h, define the full struct here if needed
#ifndef STATSTG_DEFINED
#define STATSTG_DEFINED
struct STATSTG {
    LPWSTR pwcsName;
    DWORD type;
    ULARGE_INTEGER cbSize;
    FILETIME mtime;
    FILETIME ctime;
    FILETIME atime;
    DWORD grfMode;
    DWORD grfLocksSupported;
    CLSID clsid;
    DWORD grfStateBits;
    DWORD reserved;
};
#endif

typedef enum tagSTGTY {
    STGTY_STORAGE = 1,
    STGTY_STREAM = 2,
    STGTY_LOCKBYTES = 3,
    STGTY_PROPERTY = 4
} STGTY;

typedef enum tagSTREAM_SEEK {
    STREAM_SEEK_SET = 0,
    STREAM_SEEK_CUR = 1,
    STREAM_SEEK_END = 2
} STREAM_SEEK;

interface IStream : public ISequentialStream {
    virtual HRESULT Seek(LARGE_INTEGER dlibMove, DWORD dwOrigin, ULARGE_INTEGER *plibNewPosition) = 0;
    virtual HRESULT SetSize(ULARGE_INTEGER libNewSize) = 0;
    virtual HRESULT CopyTo(IStream *pstm, ULARGE_INTEGER cb, ULARGE_INTEGER *pcbRead, ULARGE_INTEGER *pcbWritten) = 0;
    virtual HRESULT Commit(DWORD grfCommitFlags) = 0;
    virtual HRESULT Revert(void) = 0;
    virtual HRESULT LockRegion(ULARGE_INTEGER libOffset, ULARGE_INTEGER cb, DWORD dwLockType) = 0;
    virtual HRESULT UnlockRegion(ULARGE_INTEGER libOffset, ULARGE_INTEGER cb, DWORD dwLockType) = 0;
    virtual HRESULT Stat(STATSTG *pstatstg, DWORD grfStatFlag) = 0;
    virtual HRESULT Clone(IStream **ppstm) = 0;
};
#endif

// Define IMalloc interface
#ifndef __IMalloc_INTERFACE_DEFINED__
#define __IMalloc_INTERFACE_DEFINED__
interface IMalloc : public IUnknown {
    virtual void* Alloc(SIZE_T cb) = 0;
    virtual void* Realloc(void *pv, SIZE_T cb) = 0;
    virtual void Free(void *pv) = 0;
    virtual SIZE_T GetSize(void *pv) = 0;
    virtual int DidAlloc(void *pv) = 0;
    virtual void HeapMinimize(void) = 0;
};
#endif

// Additional types not in WSL stubs
typedef wchar_t WCHAR;
typedef char* LPSTR;
typedef const char* LPCSTR;
typedef wchar_t* LPWSTR;
typedef const wchar_t* LPCWSTR;
typedef DWORD* LPDWORD;
typedef BYTE* LPBYTE;
typedef void* HANDLE;
typedef HANDLE HMODULE;
typedef HANDLE HINSTANCE;
// These types are already defined in basetsd.h
// Only define what's missing
typedef size_t SIZE_T;

// Error codes are already in basetsd.h
// Only define what's missing
#ifndef E_POINTER
#define E_POINTER               ((HRESULT)0x80004003L)
#endif
#ifndef E_ABORT
#define E_ABORT                 ((HRESULT)0x80004004L)
#endif
#ifndef E_ACCESSDENIED
#define E_ACCESSDENIED          ((HRESULT)0x80070005L)
#endif
#define E_HANDLE                ((HRESULT)0x80070006L)

// Windows system error codes
#define ERROR_SUCCESS            0
#define ERROR_FILE_NOT_FOUND     2
#define ERROR_ACCESS_DENIED      5
#define ERROR_INVALID_HANDLE     6
#define ERROR_NOT_ENOUGH_MEMORY  8
#define ERROR_INVALID_PARAMETER  87
#define ERROR_INSUFFICIENT_BUFFER 122
#define ERROR_NOT_FOUND          1168
#define ERROR_NOT_CAPABLE        775
#define ERROR_IO_DEVICE          1117
#define ERROR_FUNCTION_NOT_CALLED 1626

// HRESULT helper macros
// SUCCEEDED and FAILED are already defined in basetsd.h
#define HRESULT_CODE(hr)        ((hr) & 0xFFFF)
#define HRESULT_FACILITY(hr)    (((hr) >> 16) & 0x1FFF)
#define HRESULT_SEVERITY(hr)    (((hr) >> 31) & 0x1)
#define MAKE_HRESULT(sev,fac,code) \
    ((HRESULT) (((unsigned long)(sev)<<31) | ((unsigned long)(fac)<<16) | ((unsigned long)(code))) )

// Windows constants
#define FALSE 0
#define TRUE 1
#define MAX_PATH 260
#define INVALID_HANDLE_VALUE ((HANDLE)(LONG_PTR)-1)

// COM constants
#define CLSCTX_ALL 0x17
#define CLSCTX_INPROC_SERVER 0x1

// Memory management
#define CoTaskMemAlloc(size) malloc(size)
#define CoTaskMemFree(ptr) free(ptr)
#define CoTaskMemRealloc(ptr, size) realloc(ptr, size)

// String functions
#define StringCchCopyA(dst, size, src) strncpy(dst, src, size)
#define StringCchCopyW(dst, size, src) wcsncpy(dst, src, size)
#define StringCchPrintfA snprintf
#define StringCchPrintfW swprintf

// Safe string functions
#define strcpy_s(dst, size, src) strncpy(dst, src, size)
#define strcat_s(dst, size, src) strncat(dst, src, size)
#define sprintf_s snprintf
#define vsprintf_s vsnprintf
#define swprintf_s swprintf
#define vswprintf_s vswprintf

// COM calling conventions
#define STDMETHODCALLTYPE
#define STDAPICALLTYPE
#define WINAPI
#define __stdcall
#define __cdecl
#define __override override

// __declspec attributes (no-op on macOS)
#define __declspec(x)
#define uuid(x)

// UUID helper macros for cross-platform support
#define DECLARE_CROSS_PLATFORM_UUIDOF(T)
#define CROSS_PLATFORM_UUIDOF(T) __uuidof(T)
#define __uuidof(T) IID_##T

// Define common IIDs that are used in the code
#define IID_IUnknown GUID{0x00000000,0x0000,0x0000,{0xC0,0x00,0x00,0x00,0x00,0x00,0x00,0x46}}
#define IID_IMalloc GUID{0x00000002,0x0000,0x0000,{0xC0,0x00,0x00,0x00,0x00,0x00,0x00,0x46}}
#define IID_IStream GUID{0x0000000C,0x0000,0x0000,{0xC0,0x00,0x00,0x00,0x00,0x00,0x00,0x46}}
#define IID_ISequentialStream GUID{0x0c733a30,0x2a1c,0x11ce,{0xad,0xe5,0x00,0xaa,0x00,0x44,0x77,0x3d}}
#define IID_INoMarshal GUID{0xecc8691b,0xc1db,0x4dc0,{0x85,0x5e,0x65,0xf6,0xc5,0x51,0xaf,0x49}}

// Template magic for extracting interface IID
template<typename T> struct __uuidof_helper { };
#define IID_TInterface __uuidof(TInterface)

// SAL annotations (no-op on macOS)
#define _In_
#define _In_opt_
#define _In_z_
#define _In_reads_(size)
#define _In_reads_bytes_(size)
#define _In_reads_opt_(size)
#define _In_reads_bytes_opt_(size)
#define _Out_
#define _Out_opt_
#define _Out_writes_(size)
#define _Out_writes_bytes_(size)
#define _Out_writes_opt_(size)
#define _Outptr_
#define _Outptr_opt_
#define _Outptr_result_maybenull_
#define _Outptr_result_maybenull_z_
#define _Outptr_result_bytebuffer_(x)
#define _Outptr_result_bytebuffer_maybenull_(x)
#define _COM_Outptr_
#define _COM_Outptr_opt_
#define _Inout_
#define _Inout_opt_
#define _Ret_maybenull_
#define _Ret_notnull_
#define _Use_decl_annotations_
#define _Success_(expr)
#define _Analysis_assume_(expr)
#define __field_ecount_part(x,y)
#define __out_ecount(x)
#define __out_ecount_part(x,y)
#define __in_range(x,y)
#define __in_ecount(x)
#define _Out_bytecap_(x)

// GUID references and comparison functions are already defined in basetsd.h
// Use InlineIsEqualGUID from basetsd.h
#define IsEqualGUID InlineIsEqualGUID
#define IsEqualIID InlineIsEqualGUID

// LARGE_INTEGER is already defined in basetsd.h

// File information structures
typedef struct _BY_HANDLE_FILE_INFORMATION {
    DWORD dwFileAttributes;
    FILETIME ftCreationTime;
    FILETIME ftLastAccessTime;
    FILETIME ftLastWriteTime;
    DWORD dwVolumeSerialNumber;
    DWORD nFileSizeHigh;
    DWORD nFileSizeLow;
    DWORD nNumberOfLinks;
    DWORD nFileIndexHigh;
    DWORD nFileIndexLow;
} BY_HANDLE_FILE_INFORMATION, *PBY_HANDLE_FILE_INFORMATION, *LPBY_HANDLE_FILE_INFORMATION;

// Find file data structure
typedef struct _WIN32_FIND_DATAW {
    DWORD dwFileAttributes;
    FILETIME ftCreationTime;
    FILETIME ftLastAccessTime;
    FILETIME ftLastWriteTime;
    DWORD nFileSizeHigh;
    DWORD nFileSizeLow;
    DWORD dwReserved0;
    DWORD dwReserved1;
    WCHAR cFileName[MAX_PATH];
    WCHAR cAlternateFileName[14];
} WIN32_FIND_DATAW, *PWIN32_FIND_DATAW, *LPWIN32_FIND_DATAW;

typedef WIN32_FIND_DATAW WIN32_FIND_DATA;
typedef LPWIN32_FIND_DATAW LPWIN32_FIND_DATA;

// Performance counter functions (implemented in PlatformHelpers_macOS.cpp)
extern "C" {
    BOOL QueryPerformanceCounter(LARGE_INTEGER* lpPerformanceCount);
    BOOL QueryPerformanceFrequency(LARGE_INTEGER* lpFrequency);
}

// Module loading functions (implemented in PlatformHelpers_macOS.cpp)
HMODULE LoadLibraryA(LPCSTR lpLibFileName);
HMODULE LoadLibraryW(LPCWSTR lpLibFileName);
BOOL FreeLibrary(HMODULE hModule);
LPVOID GetProcAddress(HMODULE hModule, LPCSTR lpProcName);
HMODULE GetModuleHandleA(LPCSTR lpModuleName);
HMODULE GetModuleHandleW(LPCWSTR lpModuleName);
DWORD GetModuleFileNameA(HMODULE hModule, LPSTR lpFilename, DWORD nSize);
DWORD GetModuleFileNameW(HMODULE hModule, LPWSTR lpFilename, DWORD nSize);

// Thread functions
DWORD GetCurrentThreadId();
DWORD GetCurrentProcessId();

// Error handling
DWORD GetLastError();
void SetLastError(DWORD dwErrCode);

// File I/O types and functions
#define GENERIC_READ    0x80000000
#define GENERIC_WRITE   0x40000000
#define CREATE_ALWAYS   2
#define CREATE_NEW      1
#define OPEN_ALWAYS     4
#define OPEN_EXISTING   3
#define TRUNCATE_EXISTING 5

#define FILE_SHARE_READ   0x00000001
#define FILE_SHARE_WRITE  0x00000002
#define FILE_SHARE_DELETE 0x00000004

#define FILE_ATTRIBUTE_NORMAL    0x00000080
#define FILE_ATTRIBUTE_DIRECTORY 0x00000010
#define INVALID_FILE_ATTRIBUTES  ((DWORD)-1)

HANDLE CreateFileA(LPCSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode,
                   LPVOID lpSecurityAttributes, DWORD dwCreationDisposition,
                   DWORD dwFlagsAndAttributes, HANDLE hTemplateFile);
HANDLE CreateFileW(LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode,
                   LPVOID lpSecurityAttributes, DWORD dwCreationDisposition,
                   DWORD dwFlagsAndAttributes, HANDLE hTemplateFile);
BOOL CloseHandle(HANDLE hObject);
BOOL ReadFile(HANDLE hFile, LPVOID lpBuffer, DWORD nNumberOfBytesToRead,
              LPDWORD lpNumberOfBytesRead, LPVOID lpOverlapped);
BOOL WriteFile(HANDLE hFile, LPCVOID lpBuffer, DWORD nNumberOfBytesToWrite,
               LPDWORD lpNumberOfBytesWritten, LPVOID lpOverlapped);
DWORD GetFileSize(HANDLE hFile, LPDWORD lpFileSizeHigh);
BOOL SetFilePointer(HANDLE hFile, LONG lDistanceToMove, LONG* lpDistanceToMoveHigh, DWORD dwMoveMethod);

// Critical section (mutex wrapper)
typedef struct _CRITICAL_SECTION {
    void* mutex_impl; // Will hold pthread_mutex_t*
} CRITICAL_SECTION, *LPCRITICAL_SECTION;

void InitializeCriticalSection(LPCRITICAL_SECTION lpCriticalSection);
void EnterCriticalSection(LPCRITICAL_SECTION lpCriticalSection);
void LeaveCriticalSection(LPCRITICAL_SECTION lpCriticalSection);
void DeleteCriticalSection(LPCRITICAL_SECTION lpCriticalSection);

// Event handling
HANDLE CreateEventA(LPVOID lpEventAttributes, BOOL bManualReset,
                    BOOL bInitialState, LPCSTR lpName);
HANDLE CreateEventW(LPVOID lpEventAttributes, BOOL bManualReset,
                    BOOL bInitialState, LPCWSTR lpName);
BOOL SetEvent(HANDLE hEvent);
BOOL ResetEvent(HANDLE hEvent);
DWORD WaitForSingleObject(HANDLE hHandle, DWORD dwMilliseconds);
DWORD WaitForMultipleObjects(DWORD nCount, const HANDLE* lpHandles,
                             BOOL bWaitAll, DWORD dwMilliseconds);

// Wait constants
#define INFINITE 0xFFFFFFFF
#define WAIT_OBJECT_0 0x00000000
#define WAIT_TIMEOUT  0x00000102
#define WAIT_FAILED   0xFFFFFFFF

// Output debug string
void OutputDebugStringA(LPCSTR lpOutputString);
void OutputDebugStringW(LPCWSTR lpOutputString);

// Unicode/ANSI conversion helpers
int MultiByteToWideChar(UINT CodePage, DWORD dwFlags, LPCSTR lpMultiByteStr,
                        int cbMultiByte, LPWSTR lpWideCharStr, int cchWideChar);
int WideCharToMultiByte(UINT CodePage, DWORD dwFlags, LPCWSTR lpWideCharStr,
                        int cchWideChar, LPSTR lpMultiByteStr, int cbMultiByte,
                        LPCSTR lpDefaultChar, BOOL* lpUsedDefaultChar);

#define CP_UTF8 65001
#define CP_ACP  0

#endif // !DXC_BUILD

#else // XE_PLATFORM_MAC

// On Windows, use the actual DirectX includes
#define NOMINMAX 1
#define WIN32_LEAN_AND_MEAN 1
#define VC_EXTRALEAN 1
#include <windows.h>
#include <strsafe.h>
#include <dxgitype.h>
#include <d3dcommon.h>
#include <d3d11.h>
#include <d3d12.h>
#include <d3dcompiler.h>
#include <wincrypt.h>

#endif // XE_PLATFORM_MAC

// Common DirectX includes for all platforms
#ifdef XE_PLATFORM_MAC
// Skip problematic Homebrew DirectX headers and use only what we need
// Instead, use our minimal definitions
// Make sure Windows compatibility types are available first
#include "Support/windows.h"
#include "Support/d3dblob.h"  // Minimal ID3DBlob interface
#include "Support/d3dcommon_minimal.h"  // Minimal D3D enums
// Include only the essential headers that work with DXC
#include "DxbcSignatures.h"
#include "D3D12TokenizedProgramFormat.hpp"
#include <ShaderBinary/ShaderBinary.h>
#endif

#endif // DXILCONV_DXINCLUDES_CROSSPLATFORM_H_