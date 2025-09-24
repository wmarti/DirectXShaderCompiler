///////////////////////////////////////////////////////////////////////////////
// ocidl.h - Minimal OLE Container IDL compatibility header
// Provides minimal definitions to satisfy DirectX headers
///////////////////////////////////////////////////////////////////////////////

#ifndef DXILCONV_OCIDL_H
#define DXILCONV_OCIDL_H

// Include base OLE automation definitions
#include "oaidl.h"

// OLE Container interfaces (forward declarations)
#ifdef __cplusplus
interface IOleContainer;
interface IOleClientSite;
interface IOleObject;
#endif

// Additional OLE types that might be needed
typedef struct tagMSG MSG;

#endif // DXILCONV_OCIDL_H