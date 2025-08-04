#pragma once
#include <stdint.h>
typedef uint8_t  BYTE;  typedef uint16_t WORD;  typedef uint32_t DWORD;
typedef int32_t  BOOL;  typedef int32_t HRESULT; typedef uint32_t UINT;
typedef unsigned long long UINT64;
typedef const char* LPCSTR;  typedef void* LPVOID; typedef uintptr_t SIZE_T;
struct _GUID { uint32_t D1; uint16_t D2,D3; BYTE D4[8]; };
typedef _GUID GUID, IID, CLSID;
#ifndef interface
#define interface struct
#endif
#define DEFINE_GUID(n,...) extern const GUID n
#define DECLSPEC_UUID(x)
#define DECLSPEC_NOVTABLE
#define __stdcall
#define STDMETHODCALLTYPE
#define MIDL_INTERFACE(x) struct __attribute__((uuid(x)))
#define EXTERN_C extern "C"
#define RPC_IF_HANDLE void*
#define UInt32Add(a,b,c) ((*(c)=uint32_t((a)+(b))),0)
#define DEFINE_ENUM_FLAG_OPERATORS(TYPE)
#ifndef __assume
#define __assume(x) ((void)0)
#endif
