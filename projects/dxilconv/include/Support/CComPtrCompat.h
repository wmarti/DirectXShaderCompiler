///////////////////////////////////////////////////////////////////////////////
// CComPtrCompat.h - ATL CComPtr compatibility wrapper for WRL ComPtr
// Provides ATL-like behavior using Microsoft::WRL::ComPtr as the backend
///////////////////////////////////////////////////////////////////////////////

#ifndef CCOMPTR_COMPAT_H
#define CCOMPTR_COMPAT_H

// Microsoft WRL for ComPtr (must include this first to get base functionality)
#include <wrl/client.h>

// Don't redefine if already included
#ifndef CCOMPTR_DEFINED_COMPAT
#define CCOMPTR_DEFINED_COMPAT

// Define necessary constants if not already defined
#ifndef CLSCTX_ALL
#define CLSCTX_ALL 0x17
#endif

#ifndef E_NOTIMPL
#define E_NOTIMPL ((HRESULT)0x80004001L)
#endif

#ifndef E_POINTER  
#define E_POINTER ((HRESULT)0x80004003L)
#endif

// Create an ATL-compatible CComPtr wrapper around WRL's ComPtr
template<typename T>
class CComPtr : public Microsoft::WRL::ComPtr<T> {
public:
    using Base = Microsoft::WRL::ComPtr<T>;
    
    // Constructors
    CComPtr() = default;
    CComPtr(T* p) : Base(p) {}
    CComPtr(const CComPtr& other) : Base(other) {}
    CComPtr(CComPtr&& other) : Base(std::move(other)) {}
    
    // ATL compatibility: implicit conversion to T*
    operator T*() const { return this->Get(); }
    
    // ATL compatibility: address-of operator for out parameters
    T** operator&() { 
        this->ReleaseAndGetAddressOf();
        return this->GetAddressOf(); 
    }
    
    // Assignment operators
    CComPtr& operator=(T* p) {
        Base::operator=(p);
        return *this;
    }
    
    CComPtr& operator=(const CComPtr& other) {
        Base::operator=(other);
        return *this;
    }
    
    CComPtr& operator=(CComPtr&& other) {
        Base::operator=(std::move(other));
        return *this;
    }
    
    // ATL compatibility: member access
    T* p() const { return this->Get(); }
    
    // ATL compatibility methods
    HRESULT CoCreateInstance(REFCLSID rclsid, LPUNKNOWN pUnkOuter = NULL, DWORD dwClsContext = CLSCTX_ALL) {
        // CoCreateInstance is not available on macOS, return failure
        return E_NOTIMPL;
    }
    
    HRESULT CopyTo(T** ppT) {
        if (ppT == NULL) return E_POINTER;
        *ppT = this->Get();
        if (*ppT) (*ppT)->AddRef();
        return S_OK;
    }
    
    bool IsEqualObject(IUnknown* pOther) {
        if (this->Get() == NULL && pOther == NULL) return true;
        if (this->Get() == NULL || pOther == NULL) return false;
        CComPtr<IUnknown> punk1;
        CComPtr<IUnknown> punk2;
        this->Get()->QueryInterface(__uuidof(IUnknown), (void**)&punk1);
        pOther->QueryInterface(__uuidof(IUnknown), (void**)&punk2);
        return punk1.Get() == punk2.Get();
    }
};

// Note: CHeapPtr should be included from WinAdapter.h when needed

#endif // CCOMPTR_DEFINED_COMPAT

#endif // CCOMPTR_COMPAT_H