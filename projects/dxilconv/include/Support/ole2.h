///////////////////////////////////////////////////////////////////////////////
// ole2.h - Minimal OLE2 compatibility header for DirectX headers
// Provides minimal OLE2 definitions to satisfy DirectX headers
///////////////////////////////////////////////////////////////////////////////

#ifndef DXILCONV_OLE2_H
#define DXILCONV_OLE2_H

// OLE2 is mainly about COM, which we already have through WinAdapter.h
// This file exists to satisfy the #include "ole2.h" in DirectX headers

// Make sure basic COM types are available
#include "windows.h"

// OLE2 result codes (if not already defined)
#ifndef OLEOBJ_E_NOVERBS
#define OLEOBJ_E_NOVERBS 0x80040180L
#endif

#ifndef OLEOBJ_E_INVALIDVERB
#define OLEOBJ_E_INVALIDVERB 0x80040181L
#endif

// Basic OLE2 macros
#ifndef OLESTR
#define OLESTR(str) L##str
#endif

// OLE2 initialization functions (no-op on Unix)
#define OleInitialize(x) S_OK
#define OleUninitialize()
#define CoInitialize(x) S_OK
#define CoUninitialize()

#endif // DXILCONV_OLE2_H