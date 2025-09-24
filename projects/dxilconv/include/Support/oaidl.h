///////////////////////////////////////////////////////////////////////////////
// oaidl.h - Minimal OLE Automation IDL compatibility header
// Provides minimal definitions to satisfy DirectX headers
///////////////////////////////////////////////////////////////////////////////

#ifndef DXILCONV_OAIDL_H
#define DXILCONV_OAIDL_H

// Forward declare WCHAR if not defined yet
#ifndef _WCHAR_T_DEFINED
typedef wchar_t WCHAR;
#define _WCHAR_T_DEFINED  
#endif

// Include base Windows types
#include "windows.h"

// OLE Automation types (minimal subset)
typedef struct tagVARIANT VARIANT;
typedef struct tagSAFEARRAY SAFEARRAY;

// IDispatch forward declaration (if needed)
#ifdef __cplusplus
interface IDispatch;
#endif

// VARIANT type constants
typedef enum VARENUM {
    VT_EMPTY = 0,
    VT_NULL = 1,
    VT_I2 = 2,
    VT_I4 = 3,
    VT_R4 = 4,
    VT_R8 = 5,
    VT_BSTR = 8,
    VT_BOOL = 11,
    VT_VARIANT = 12,
    VT_UNKNOWN = 13,
    VT_UI1 = 17,
    VT_UI2 = 18,
    VT_UI4 = 19,
    VT_I8 = 20,
    VT_UI8 = 21,
    VT_INT = 22,
    VT_UINT = 23,
    VT_LPSTR = 30,
    VT_LPWSTR = 31,
    VT_PTR = 26,
    VT_ARRAY = 0x2000,
    VT_BYREF = 0x4000
} VARENUM;

// BSTR type
typedef WCHAR* BSTR;

#endif // DXILCONV_OAIDL_H