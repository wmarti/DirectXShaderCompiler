///////////////////////////////////////////////////////////////////////////////
// rpcndr.h - Minimal RPC NDR compatibility header for DirectX headers
// Bridges DirectX headers expectations with DXC's WinAdapter.h
///////////////////////////////////////////////////////////////////////////////

#ifndef DXILCONV_RPCNDR_H
#define DXILCONV_RPCNDR_H

// Include the minimal RPC definitions
#include "rpc.h"

// GUID should already be defined by WinAdapter.h which is included before this
// Just make sure the definition flag is set
#ifndef GUID_DEFINED
#define GUID_DEFINED
#endif

// Only define what's absolutely necessary and not already in WinAdapter.h

#ifndef __RPCNDR_H_VERSION__
#define __RPCNDR_H_VERSION__
#endif

// MIDL interface macro - WinAdapter.h may not have this
#ifndef MIDL_INTERFACE
#define MIDL_INTERFACE(x) struct
#endif

// These are used by DirectX headers but not defined in WinAdapter.h
#ifdef CONST_VTABLE
#define CONST_VTBL const
#else
#define CONST_VTBL
#endif

// Minimal RPC/NDR marshalling attributes (no-op on Unix)
#define __RPC__deref_out
#define __RPC__deref_out_opt
#define __RPC__deref_out_ecount_full_opt(size)
#define __RPC__deref_out_opt_string
#define __RPC__out
#define __RPC__out_ecount_full(size)
#define __RPC__in
#define __RPC__in_opt
#define __RPC__in_ecount_full(size)
#define __RPC__inout
#define __RPC__inout_opt

// Additional SAL annotations that DirectX headers might use
#ifndef _Outptr_result_bytebuffer_maybenull_
#define _Outptr_result_bytebuffer_maybenull_(x)
#endif
#ifndef _Outptr_result_maybenull_z_
#define _Outptr_result_maybenull_z_
#endif

// __uuidof emulation for DirectX headers when using DXC
// This adapts the WSL-style __uuidof to work with DXC's WinAdapter.h
#if defined(__cplusplus) && defined(__EMULATE_UUID)

// Import the __uuidof template from WSL headers style
#if __cpp_constexpr >= 200704l && __cpp_inline_variables >= 201606L
#define __wsl_stub_uuidof_use_constexpr 1
#else
#define __wsl_stub_uuidof_use_constexpr 0
#endif

// Forward declare the templates - GUID will be defined by the time they're instantiated
extern "C++"                                                         {
#if __wsl_stub_uuidof_use_constexpr
    template<typename T> struct __wsl_stub_uuidof_s;
    template<typename T> constexpr const GUID &__wsl_stub_uuidof();
#else
    template<typename T> const GUID &__wsl_stub_uuidof();
#endif
}

#if __wsl_stub_uuidof_use_constexpr
#define __CRT_UUID_DECL(type, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8) \
    extern "C++"                                                         \
    {                                                                    \
        template <>                                                      \
        struct __wsl_stub_uuidof_s<type>                                 \
        {                                                                \
            static constexpr GUID __uuid_inst = {                        \
                l, w1, w2, {b1, b2, b3, b4, b5, b6, b7, b8}};            \
        };                                                               \
        template <>                                                      \
        constexpr const GUID &__wsl_stub_uuidof<type>()           \
        {                                                                \
            return __wsl_stub_uuidof_s<type>::__uuid_inst;               \
        }                                                                \
        template <>                                                      \
        constexpr const GUID &__wsl_stub_uuidof<type *>()         \
        {                                                                \
            return __wsl_stub_uuidof_s<type>::__uuid_inst;               \
        }                                                                \
    }
#else
#define __CRT_UUID_DECL(type, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8) \
    extern "C++"                                                         \
    {                                                                    \
        template <>                                                      \
        inline const GUID &__wsl_stub_uuidof<type>()                     \
        {                                                                \
            static const GUID __uuid_inst = {                             \
                l, w1, w2, {b1, b2, b3, b4, b5, b6, b7, b8}};            \
            return __uuid_inst;                                          \
        }                                                                \
        template <>                                                      \
        inline const GUID &__wsl_stub_uuidof<type *>()            \
        {                                                                \
            return __wsl_stub_uuidof<type>();                            \
        }                                                                \
    }
#endif

#ifndef __uuidof
#define __uuidof(type) __wsl_stub_uuidof<__typeof(type)>()
#endif

#else // Not C++ or not emulating UUID
#define __CRT_UUID_DECL(type, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8)
#endif // __cplusplus && __EMULATE_UUID

#endif // DXILCONV_RPCNDR_H