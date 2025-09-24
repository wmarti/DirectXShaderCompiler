///////////////////////////////////////////////////////////////////////////////
// rpc.h - Minimal RPC compatibility header for DirectX headers
// Bridges DirectX headers expectations with DXC's WinAdapter.h
///////////////////////////////////////////////////////////////////////////////

#ifndef DXILCONV_RPC_H
#define DXILCONV_RPC_H

// This header is only needed when using DirectX headers with DXC
// It provides minimal RPC definitions that DirectX headers expect

#ifndef __RPC_H__
#define __RPC_H__

// RPC calling conventions - these are no-ops on Unix
#ifndef __RPC_USER
#define __RPC_USER
#endif

#ifndef __RPC_STUB
#define __RPC_STUB
#endif

#ifndef __RPC_FAR
#define __RPC_FAR
#endif

#ifndef RPC_ENTRY
#define RPC_ENTRY
#endif

// Minimal RPC types needed by DirectX headers
typedef void* RPC_IF_HANDLE;

#endif // __RPC_H__

#endif // DXILCONV_RPC_H