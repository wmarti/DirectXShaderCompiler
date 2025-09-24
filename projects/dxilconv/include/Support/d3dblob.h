///////////////////////////////////////////////////////////////////////////////
// d3dblob.h - Minimal D3D Blob interface for DxbcConverter
// Provides just the ID3DBlob interface needed for shader conversion
///////////////////////////////////////////////////////////////////////////////

#ifndef DXILCONV_D3DBLOB_H
#define DXILCONV_D3DBLOB_H

// Include basic types
#include <stddef.h>  // for size_t

// Define basic types if not already defined
#ifndef LPVOID
typedef void* LPVOID;
#endif

#ifndef SIZE_T
typedef size_t SIZE_T;
#endif

// Ensure STDMETHODCALLTYPE is defined
#ifndef STDMETHODCALLTYPE
#define STDMETHODCALLTYPE
#endif

// Forward declare ID3DBlob (also known as ID3D10Blob)
#ifdef __cplusplus

// IUnknown is provided by WinAdapter.h when building with DXC
// Only define for standalone builds
#if !defined(__IUnknown_INTERFACE_DEFINED__) && !defined(DXC_BUILD)
#define __IUnknown_INTERFACE_DEFINED__
struct IUnknown {
    virtual ~IUnknown() = default;
    virtual long QueryInterface(const void* riid, void** ppvObject) = 0;
    virtual unsigned long AddRef() = 0;
    virtual unsigned long Release() = 0;
};
#endif

// ID3DBlob is a simple interface for a data blob
struct ID3DBlob : public IUnknown {
    virtual LPVOID STDMETHODCALLTYPE GetBufferPointer() = 0;
    virtual SIZE_T STDMETHODCALLTYPE GetBufferSize() = 0;
};

// ID3D10Blob is the same as ID3DBlob
typedef ID3DBlob ID3D10Blob;

// The UUIDs will be defined elsewhere if needed

#endif // __cplusplus

#endif // DXILCONV_D3DBLOB_H