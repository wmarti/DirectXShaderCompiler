///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// dxilconv.cpp                                                              //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Implements the DLL entry point and DxcCreateInstance function.            //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "llvm/Support/ManagedStatic.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/MD5.h"

#ifdef _WIN32
#include "dxc/Support/WinIncludes.h"
#else
// Unix/macOS platform includes
#include <pthread.h>
#include <dlfcn.h>
#include <cstring>
// Minimal Windows compatibility definitions
#define __stdcall
#define DLL_PROCESS_ATTACH 1
#define DLL_PROCESS_DETACH 0
#define WINAPI
#define BOOL int
#define TRUE 1
#define FALSE 0
#define HINSTANCE void*
#define DWORD unsigned long
#define LPVOID void*
#define _In_ 
#define _Out_
#define REFCLSID const IID&
#define REFIID const IID&
#define IMalloc void
typedef long HRESULT;
#define S_OK ((HRESULT)0)
#define E_FAIL ((HRESULT)0x80004005L)
#define E_POINTER ((HRESULT)0x80004003L)
#define REGDB_E_CLASSNOTREG ((HRESULT)0x80040154L)
#define IFC(x) do { hr = (x); if (FAILED(hr)) goto Cleanup; } while(0)
#define IFR(x) do { HRESULT __hr = (x); if (FAILED(__hr)) return __hr; } while(0)
#define FAILED(hr) ((HRESULT)(hr) < 0)

// COM interface definitions for Unix
struct IID {
    unsigned long x;
    unsigned short s1;
    unsigned short s2;
    unsigned char c[8];
};

inline bool IsEqualCLSID(const IID& a, const IID& b) {
    return memcmp(&a, &b, sizeof(IID)) == 0;
}

// Forward declare CLSID_DxbcConverter
extern const IID CLSID_DxbcConverter;
#endif

#include "dxc/Support/Global.h"
#include "Tracing/DxcRuntimeEtw.h"

#define DXC_API_IMPORT

#include "dxc/dxcisense.h"
#include "dxc/dxctools.h"

#ifndef _WIN32
// Stub implementations for Unix
#define EventRegisterMicrosoft_Windows_DxcRuntime_API()
#define EventUnregisterMicrosoft_Windows_DxcRuntime_API()
#define DxcRuntimeEtw_DxcRuntimeInitialization_Start()
#define DxcRuntimeEtw_DxcRuntimeInitialization_Stop(x)
#define DxcRuntimeEtw_DxcRuntimeShutdown_Start()
#define DxcRuntimeEtw_DxcRuntimeShutdown_Stop(x)
#define DxcEtw_DXCompilerCreateInstance_Start()
#define DxcEtw_DXCompilerCreateInstance_Stop(x)
#else
#include "dxcetw.h"
#include "Tracing/DxcRuntimeEtw.h"
#endif

#include "DxbcConverter.h"

// Thread-local storage for memory allocation
#ifndef _WIN32
static pthread_key_t g_ThreadMallocKey;
static pthread_once_t g_ThreadMallocKeyOnce = PTHREAD_ONCE_INIT;

static void CreateThreadMallocKey() {
    pthread_key_create(&g_ThreadMallocKey, nullptr);
}

class DxcThreadMalloc {
public:
    DxcThreadMalloc(IMalloc* pMalloc) {
        pthread_once(&g_ThreadMallocKeyOnce, CreateThreadMallocKey);
        m_pPreviousMalloc = pthread_getspecific(g_ThreadMallocKey);
        pthread_setspecific(g_ThreadMallocKey, pMalloc);
    }
    
    ~DxcThreadMalloc() {
        pthread_setspecific(g_ThreadMallocKey, m_pPreviousMalloc);
    }
    
private:
    void* m_pPreviousMalloc;
};

// Stub implementations for thread malloc functions
static HRESULT DxcInitThreadMalloc() { return S_OK; }
static void DxcSetThreadMallocToDefault() {}
static void DxcClearThreadMalloc() {}
static void DxcCleanupThreadMalloc() {}
#endif

// Defined in DxbcConverter.lib (projects/dxilconv/lib/DxbcConverter/DxbcConverter.cpp)
HRESULT CreateDxbcConverter(_In_ REFIID riid, _Out_ LPVOID *ppv);

/// <summary>
/// Creates a single uninitialized object of the class associated with a specified CLSID.
/// </summary>
/// <param name="rclsid">The CLSID associated with the data and code that will be used to create the object.</param>
/// <param name="riid">A reference to the identifier of the interface to be used to communicate with the object.</param>
/// <param name="ppv">Address of pointer variable that receives the interface pointer requested in riid. Upon successful return, *ppv contains the requested interface pointer. Upon failure, *ppv contains NULL.</param>
/// <remarks>
/// While this function is similar to CoCreateInstance, there is no COM involvement.
/// </remarks>
static HRESULT ThreadMallocDxcCreateInstance(
    _In_ REFCLSID rclsid,
    _In_ REFIID riid,
    _Out_ LPVOID *ppv) {
    *ppv = nullptr;
    if (IsEqualCLSID(rclsid, CLSID_DxbcConverter)) {
        return CreateDxbcConverter(riid, ppv);
    }
    return REGDB_E_CLASSNOTREG;
}

#ifdef _WIN32
DXC_API_IMPORT HRESULT __stdcall
#else
extern "C" HRESULT
#endif
DxcCreateInstance(_In_ REFCLSID   rclsid,
    _In_ REFIID     riid,
    _Out_ LPVOID   *ppv) {
    HRESULT hr = S_OK;
    DxcEtw_DXCompilerCreateInstance_Start();
    DxcThreadMalloc TM(nullptr);
    hr = ThreadMallocDxcCreateInstance(rclsid, riid, ppv);
    DxcEtw_DXCompilerCreateInstance_Stop(hr);
    return hr;
}

#ifdef _WIN32
DXC_API_IMPORT HRESULT __stdcall
#else
extern "C" HRESULT
#endif
DxcCreateInstance2(_In_ IMalloc *pMalloc,
    _In_ REFCLSID   rclsid,
    _In_ REFIID     riid,
    _Out_ LPVOID   *ppv) {
    if (ppv == nullptr) {
        return E_POINTER;
    }
    HRESULT hr = S_OK;
    DxcEtw_DXCompilerCreateInstance_Start();
    DxcThreadMalloc TM(pMalloc);
    hr = ThreadMallocDxcCreateInstance(rclsid, riid, ppv);
    DxcEtw_DXCompilerCreateInstance_Stop(hr);
    return hr;
}


// C++ exception specification ignored except to indicate a function is not __declspec(nothrow)
static HRESULT InitMaybeFail() throw() {
    HRESULT hr = S_OK;
    bool memSetup = false;
    IFC(DxcInitThreadMalloc());
    DxcSetThreadMallocToDefault();
    memSetup = true;
    if (::llvm::sys::fs::SetupPerThreadFileSystem()) {
        hr = E_FAIL;
        goto Cleanup;
    }
Cleanup:
    if (FAILED(hr)) {
        if (memSetup) {
            DxcClearThreadMalloc();
            DxcCleanupThreadMalloc();
        }
    }
    else {
        DxcClearThreadMalloc();
    }
    return hr;
}

#ifdef _WIN32
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD Reason, LPVOID )
{
    if (Reason == DLL_PROCESS_ATTACH)
    {
        EventRegisterMicrosoft_Windows_DxcRuntime_API();

        DxcRuntimeEtw_DxcRuntimeInitialization_Start();
        HRESULT hr = InitMaybeFail();
        if (FAILED(hr)) {
            DxcRuntimeEtw_DxcRuntimeInitialization_Stop(hr);
            return FALSE;
        }

        DxcRuntimeEtw_DxcRuntimeInitialization_Stop(S_OK);
    }
    else if (Reason == DLL_PROCESS_DETACH)
    {
        DxcRuntimeEtw_DxcRuntimeShutdown_Start();

        DxcSetThreadMallocToDefault();
        ::llvm::sys::fs::CleanupPerThreadFileSystem();
        ::llvm::llvm_shutdown();
        DxcClearThreadMalloc();
        DxcCleanupThreadMalloc();

        DxcRuntimeEtw_DxcRuntimeShutdown_Stop(S_OK);

        EventUnregisterMicrosoft_Windows_DxcRuntime_API();
    }
    
    return TRUE;
}
#else
// Unix/macOS library initialization and cleanup
static void LibraryInit() __attribute__((constructor));
static void LibraryCleanup() __attribute__((destructor));

static void LibraryInit() {
    EventRegisterMicrosoft_Windows_DxcRuntime_API();
    
    DxcRuntimeEtw_DxcRuntimeInitialization_Start();
    HRESULT hr = InitMaybeFail();
    if (FAILED(hr)) {
        DxcRuntimeEtw_DxcRuntimeInitialization_Stop(hr);
        // Note: Can't really fail constructor, so just log
        fprintf(stderr, "dxilconv initialization failed: 0x%08lx\n", hr);
    }
    
    DxcRuntimeEtw_DxcRuntimeInitialization_Stop(S_OK);
}

static void LibraryCleanup() {
    DxcRuntimeEtw_DxcRuntimeShutdown_Start();
    
    DxcSetThreadMallocToDefault();
    ::llvm::sys::fs::CleanupPerThreadFileSystem();
    ::llvm::llvm_shutdown();
    DxcClearThreadMalloc();
    DxcCleanupThreadMalloc();
    
    DxcRuntimeEtw_DxcRuntimeShutdown_Stop(S_OK);
    
    EventUnregisterMicrosoft_Windows_DxcRuntime_API();
}
#endif

