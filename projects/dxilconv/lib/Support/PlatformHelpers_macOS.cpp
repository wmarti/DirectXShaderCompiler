/**
 ******************************************************************************
 * DirectXShaderCompiler macOS Platform Helpers                              *
 ******************************************************************************
 * Copyright 2024 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include "Support/DXIncludes_CrossPlatform.h"

#if XE_PLATFORM_MAC

#include <mach/mach_time.h>
#include <pthread.h>
#include <dlfcn.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <thread>

// Thread-local storage for last error
static thread_local DWORD g_lastError = 0;

// Module handle management
static std::mutex g_moduleMutex;
static std::map<std::string, void*> g_moduleHandles;

// Performance counter implementation using mach_absolute_time
static struct {
    mach_timebase_info_data_t timebase;
    bool initialized;
} g_perfCounter = {0};

static void InitializePerfCounter() {
    if (!g_perfCounter.initialized) {
        mach_timebase_info(&g_perfCounter.timebase);
        g_perfCounter.initialized = true;
    }
}

// Performance counter functions
// WinAdapter.h already provides QueryPerformanceCounter when building with DXC
// but with different signatures, so we skip these
#ifndef DXC_BUILD
extern "C" {

BOOL QueryPerformanceCounter(LARGE_INTEGER* lpPerformanceCount) {
    if (!lpPerformanceCount) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    
    InitializePerfCounter();
    uint64_t time = mach_absolute_time();
    
    // Convert to nanoseconds
    time = time * g_perfCounter.timebase.numer / g_perfCounter.timebase.denom;
    
    lpPerformanceCount->QuadPart = time;
    return TRUE;
}

BOOL QueryPerformanceFrequency(LARGE_INTEGER* lpFrequency) {
    if (!lpFrequency) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    
    // Return frequency in Hz (1 billion for nanosecond precision)
    lpFrequency->QuadPart = 1000000000LL;
    return TRUE;
}

} // extern "C"
#endif // !DXC_BUILD

// Module loading functions
HMODULE LoadLibraryA(LPCSTR lpLibFileName) {
    if (!lpLibFileName) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return nullptr;
    }
    
    std::lock_guard<std::mutex> lock(g_moduleMutex);
    
    // Check if already loaded
    auto it = g_moduleHandles.find(lpLibFileName);
    if (it != g_moduleHandles.end()) {
        return (HMODULE)it->second;
    }
    
    // Try to load the library
    void* handle = dlopen(lpLibFileName, RTLD_LAZY | RTLD_LOCAL);
    if (!handle) {
        SetLastError(ERROR_FILE_NOT_FOUND);
        return nullptr;
    }
    
    g_moduleHandles[lpLibFileName] = handle;
    return (HMODULE)handle;
}

HMODULE LoadLibraryW(LPCWSTR lpLibFileName) {
    if (!lpLibFileName) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return nullptr;
    }
    
    // Convert wide string to narrow
    size_t len = wcslen(lpLibFileName);
    char* narrowPath = (char*)alloca(len * 4 + 1);
    wcstombs(narrowPath, lpLibFileName, len * 4 + 1);
    
    return LoadLibraryA(narrowPath);
}

BOOL FreeLibrary(HMODULE hModule) {
    if (!hModule) {
        SetLastError(ERROR_INVALID_HANDLE);
        return FALSE;
    }
    
    std::lock_guard<std::mutex> lock(g_moduleMutex);
    
    // Find and remove from our tracking
    for (auto it = g_moduleHandles.begin(); it != g_moduleHandles.end(); ++it) {
        if (it->second == hModule) {
            g_moduleHandles.erase(it);
            break;
        }
    }
    
    return dlclose(hModule) == 0;
}

LPVOID GetProcAddress(HMODULE hModule, LPCSTR lpProcName) {
    if (!hModule || !lpProcName) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return nullptr;
    }
    
    void* symbol = dlsym(hModule, lpProcName);
    if (!symbol) {
        SetLastError(ERROR_NOT_FOUND);
    }
    return symbol;
}

HMODULE GetModuleHandleA(LPCSTR lpModuleName) {
    if (!lpModuleName) {
        // Return handle to main executable
        return (HMODULE)RTLD_DEFAULT;
    }
    
    std::lock_guard<std::mutex> lock(g_moduleMutex);
    auto it = g_moduleHandles.find(lpModuleName);
    if (it != g_moduleHandles.end()) {
        return (HMODULE)it->second;
    }
    
    SetLastError(ERROR_FILE_NOT_FOUND);
    return nullptr;
}

HMODULE GetModuleHandleW(LPCWSTR lpModuleName) {
    if (!lpModuleName) {
        return (HMODULE)RTLD_DEFAULT;
    }
    
    // Convert wide string to narrow
    size_t len = wcslen(lpModuleName);
    char* narrowPath = (char*)alloca(len * 4 + 1);
    wcstombs(narrowPath, lpModuleName, len * 4 + 1);
    
    return GetModuleHandleA(narrowPath);
}

DWORD GetModuleFileNameA(HMODULE hModule, LPSTR lpFilename, DWORD nSize) {
    if (!lpFilename || nSize == 0) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return 0;
    }
    
    Dl_info info;
    if (dladdr(hModule ? hModule : (void*)GetModuleFileNameA, &info) && info.dli_fname) {
        size_t len = strlen(info.dli_fname);
        if (len >= nSize) {
            SetLastError(ERROR_INSUFFICIENT_BUFFER);
            strncpy(lpFilename, info.dli_fname, nSize - 1);
            lpFilename[nSize - 1] = '\0';
            return nSize;
        }
        strcpy(lpFilename, info.dli_fname);
        return len;
    }
    
    SetLastError(ERROR_FILE_NOT_FOUND);
    return 0;
}

DWORD GetModuleFileNameW(HMODULE hModule, LPWSTR lpFilename, DWORD nSize) {
    if (!lpFilename || nSize == 0) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return 0;
    }
    
    char* narrowPath = (char*)alloca(nSize * 4);
    DWORD result = GetModuleFileNameA(hModule, narrowPath, nSize * 4);
    if (result > 0) {
        mbstowcs(lpFilename, narrowPath, nSize);
        return wcslen(lpFilename);
    }
    return 0;
}

// Thread functions
DWORD GetCurrentThreadId() {
    pthread_t tid = pthread_self();
    // Convert pthread_t to a DWORD (may truncate on some platforms)
    return (DWORD)(uintptr_t)tid;
}

DWORD GetCurrentProcessId() {
    return (DWORD)getpid();
}

// Error handling
// GetLastError is defined as a macro in WinAdapter.h when building with DXC
#ifndef GetLastError
DWORD GetLastError() {
    return g_lastError;
}
#endif

// SetLastError is defined as a macro in WinAdapter.h when building with DXC
#ifndef SetLastError
void SetLastError(DWORD dwErrCode) {
    g_lastError = dwErrCode;
}
#endif

// File I/O functions
HANDLE CreateFileA(LPCSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode,
                   LPVOID lpSecurityAttributes, DWORD dwCreationDisposition,
                   DWORD dwFlagsAndAttributes, HANDLE hTemplateFile) {
    if (!lpFileName) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return INVALID_HANDLE_VALUE;
    }
    
    int flags = 0;
    
    // Map access mode
    if ((dwDesiredAccess & GENERIC_READ) && (dwDesiredAccess & GENERIC_WRITE)) {
        flags |= O_RDWR;
    } else if (dwDesiredAccess & GENERIC_READ) {
        flags |= O_RDONLY;
    } else if (dwDesiredAccess & GENERIC_WRITE) {
        flags |= O_WRONLY;
    }
    
    // Map creation disposition
    switch (dwCreationDisposition) {
        case CREATE_NEW:
            flags |= O_CREAT | O_EXCL;
            break;
        case CREATE_ALWAYS:
            flags |= O_CREAT | O_TRUNC;
            break;
        case OPEN_EXISTING:
            // Default behavior
            break;
        case OPEN_ALWAYS:
            flags |= O_CREAT;
            break;
        case TRUNCATE_EXISTING:
            flags |= O_TRUNC;
            break;
        default:
            SetLastError(ERROR_INVALID_PARAMETER);
            return INVALID_HANDLE_VALUE;
    }
    
    int fd = open(lpFileName, flags, 0666);
    if (fd == -1) {
        SetLastError(errno == ENOENT ? ERROR_FILE_NOT_FOUND : ERROR_FUNCTION_NOT_CALLED);
        return INVALID_HANDLE_VALUE;
    }
    
    return (HANDLE)(intptr_t)fd;
}

HANDLE CreateFileW(LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode,
                   LPVOID lpSecurityAttributes, DWORD dwCreationDisposition,
                   DWORD dwFlagsAndAttributes, HANDLE hTemplateFile) {
    if (!lpFileName) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return INVALID_HANDLE_VALUE;
    }
    
    size_t len = wcslen(lpFileName);
    char* narrowPath = (char*)alloca(len * 4 + 1);
    wcstombs(narrowPath, lpFileName, len * 4 + 1);
    
    return CreateFileA(narrowPath, dwDesiredAccess, dwShareMode,
                      lpSecurityAttributes, dwCreationDisposition,
                      dwFlagsAndAttributes, hTemplateFile);
}

BOOL CloseHandle(HANDLE hObject) {
    if (hObject == INVALID_HANDLE_VALUE) {
        SetLastError(ERROR_INVALID_HANDLE);
        return FALSE;
    }
    
    int fd = (int)(intptr_t)hObject;
    return close(fd) == 0;
}

BOOL ReadFile(HANDLE hFile, LPVOID lpBuffer, DWORD nNumberOfBytesToRead,
              LPDWORD lpNumberOfBytesRead, LPVOID lpOverlapped) {
    if (hFile == INVALID_HANDLE_VALUE || !lpBuffer) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    
    int fd = (int)(intptr_t)hFile;
    ssize_t bytesRead = read(fd, lpBuffer, nNumberOfBytesToRead);
    
    if (bytesRead == -1) {
        SetLastError(ERROR_IO_DEVICE);
        return FALSE;
    }
    
    if (lpNumberOfBytesRead) {
        *lpNumberOfBytesRead = (DWORD)bytesRead;
    }
    
    return TRUE;
}

BOOL WriteFile(HANDLE hFile, LPCVOID lpBuffer, DWORD nNumberOfBytesToWrite,
               LPDWORD lpNumberOfBytesWritten, LPVOID lpOverlapped) {
    if (hFile == INVALID_HANDLE_VALUE || !lpBuffer) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    
    int fd = (int)(intptr_t)hFile;
    ssize_t bytesWritten = write(fd, lpBuffer, nNumberOfBytesToWrite);
    
    if (bytesWritten == -1) {
        SetLastError(ERROR_IO_DEVICE);
        return FALSE;
    }
    
    if (lpNumberOfBytesWritten) {
        *lpNumberOfBytesWritten = (DWORD)bytesWritten;
    }
    
    return TRUE;
}

DWORD GetFileSize(HANDLE hFile, LPDWORD lpFileSizeHigh) {
    if (hFile == INVALID_HANDLE_VALUE) {
        SetLastError(ERROR_INVALID_HANDLE);
        return INVALID_FILE_ATTRIBUTES;
    }
    
    int fd = (int)(intptr_t)hFile;
    struct stat st;
    if (fstat(fd, &st) == -1) {
        SetLastError(ERROR_FUNCTION_NOT_CALLED);
        return INVALID_FILE_ATTRIBUTES;
    }
    
    if (lpFileSizeHigh) {
        *lpFileSizeHigh = (DWORD)(st.st_size >> 32);
    }
    
    return (DWORD)(st.st_size & 0xFFFFFFFF);
}

BOOL SetFilePointer(HANDLE hFile, LONG lDistanceToMove, LONG* lpDistanceToMoveHigh, DWORD dwMoveMethod) {
    if (hFile == INVALID_HANDLE_VALUE) {
        SetLastError(ERROR_INVALID_HANDLE);
        return FALSE;
    }
    
    int fd = (int)(intptr_t)hFile;
    int whence;
    
    switch (dwMoveMethod) {
        case 0: // FILE_BEGIN
            whence = SEEK_SET;
            break;
        case 1: // FILE_CURRENT
            whence = SEEK_CUR;
            break;
        case 2: // FILE_END
            whence = SEEK_END;
            break;
        default:
            SetLastError(ERROR_INVALID_PARAMETER);
            return FALSE;
    }
    
    off_t offset = lDistanceToMove;
    if (lpDistanceToMoveHigh) {
        offset |= ((off_t)*lpDistanceToMoveHigh) << 32;
    }
    
    off_t result = lseek(fd, offset, whence);
    if (result == -1) {
        SetLastError(ERROR_FUNCTION_NOT_CALLED);
        return FALSE;
    }
    
    return TRUE;
}

// Critical section implementation using pthread_mutex
void InitializeCriticalSection(LPCRITICAL_SECTION lpCriticalSection) {
    if (!lpCriticalSection) return;
    
    pthread_mutex_t* mutex = new pthread_mutex_t;
    pthread_mutex_init(mutex, nullptr);
    lpCriticalSection->mutex_impl = mutex;
}

void EnterCriticalSection(LPCRITICAL_SECTION lpCriticalSection) {
    if (!lpCriticalSection || !lpCriticalSection->mutex_impl) return;
    pthread_mutex_lock((pthread_mutex_t*)lpCriticalSection->mutex_impl);
}

void LeaveCriticalSection(LPCRITICAL_SECTION lpCriticalSection) {
    if (!lpCriticalSection || !lpCriticalSection->mutex_impl) return;
    pthread_mutex_unlock((pthread_mutex_t*)lpCriticalSection->mutex_impl);
}

void DeleteCriticalSection(LPCRITICAL_SECTION lpCriticalSection) {
    if (!lpCriticalSection || !lpCriticalSection->mutex_impl) return;
    
    pthread_mutex_t* mutex = (pthread_mutex_t*)lpCriticalSection->mutex_impl;
    pthread_mutex_destroy(mutex);
    delete mutex;
    lpCriticalSection->mutex_impl = nullptr;
}

// Event implementation using condition variables
struct Event {
    std::mutex mutex;
    std::condition_variable cv;
    bool is_set;
    bool manual_reset;
};

HANDLE CreateEventA(LPVOID lpEventAttributes, BOOL bManualReset,
                    BOOL bInitialState, LPCSTR lpName) {
    Event* event = new Event;
    event->is_set = bInitialState;
    event->manual_reset = bManualReset;
    return (HANDLE)event;
}

HANDLE CreateEventW(LPVOID lpEventAttributes, BOOL bManualReset,
                    BOOL bInitialState, LPCWSTR lpName) {
    return CreateEventA(lpEventAttributes, bManualReset, bInitialState, nullptr);
}

BOOL SetEvent(HANDLE hEvent) {
    if (!hEvent) {
        SetLastError(ERROR_INVALID_HANDLE);
        return FALSE;
    }
    
    Event* event = (Event*)hEvent;
    std::lock_guard<std::mutex> lock(event->mutex);
    event->is_set = true;
    if (event->manual_reset) {
        event->cv.notify_all();
    } else {
        event->cv.notify_one();
    }
    return TRUE;
}

BOOL ResetEvent(HANDLE hEvent) {
    if (!hEvent) {
        SetLastError(ERROR_INVALID_HANDLE);
        return FALSE;
    }
    
    Event* event = (Event*)hEvent;
    std::lock_guard<std::mutex> lock(event->mutex);
    event->is_set = false;
    return TRUE;
}

DWORD WaitForSingleObject(HANDLE hHandle, DWORD dwMilliseconds) {
    if (!hHandle) {
        SetLastError(ERROR_INVALID_HANDLE);
        return WAIT_FAILED;
    }
    
    Event* event = (Event*)hHandle;
    std::unique_lock<std::mutex> lock(event->mutex);
    
    if (dwMilliseconds == INFINITE) {
        event->cv.wait(lock, [event] { return event->is_set; });
    } else {
        auto result = event->cv.wait_for(lock,
            std::chrono::milliseconds(dwMilliseconds),
            [event] { return event->is_set; });
        if (!result) {
            return WAIT_TIMEOUT;
        }
    }
    
    if (!event->manual_reset) {
        event->is_set = false;
    }
    
    return WAIT_OBJECT_0;
}

DWORD WaitForMultipleObjects(DWORD nCount, const HANDLE* lpHandles,
                             BOOL bWaitAll, DWORD dwMilliseconds) {
    // Simplified implementation - only supports waiting for any
    if (bWaitAll || nCount == 0 || !lpHandles) {
        SetLastError(ERROR_NOT_CAPABLE);
        return WAIT_FAILED;
    }
    
    // Poll events in a loop
    auto start = std::chrono::steady_clock::now();
    auto timeout = dwMilliseconds == INFINITE ? 
        std::chrono::steady_clock::time_point::max() :
        start + std::chrono::milliseconds(dwMilliseconds);
    
    while (std::chrono::steady_clock::now() < timeout) {
        for (DWORD i = 0; i < nCount; i++) {
            Event* event = (Event*)lpHandles[i];
            std::unique_lock<std::mutex> lock(event->mutex);
            if (event->is_set) {
                if (!event->manual_reset) {
                    event->is_set = false;
                }
                return WAIT_OBJECT_0 + i;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    
    return WAIT_TIMEOUT;
}

// Debug output - already defined as macros in WinAdapter.h when building with DXC
#ifndef OutputDebugStringA
void OutputDebugStringA(LPCSTR lpOutputString) {
    if (lpOutputString) {
        fprintf(stderr, "%s", lpOutputString);
    }
}
#endif

#ifndef OutputDebugStringW
void OutputDebugStringW(LPCWSTR lpOutputString) {
    if (lpOutputString) {
        fwprintf(stderr, L"%ls", lpOutputString);
    }
}
#endif

// Unicode/ANSI conversion
int MultiByteToWideChar(UINT CodePage, DWORD dwFlags, LPCSTR lpMultiByteStr,
                        int cbMultiByte, LPWSTR lpWideCharStr, int cchWideChar) {
    if (!lpMultiByteStr) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return 0;
    }
    
    size_t len = (cbMultiByte == -1) ? strlen(lpMultiByteStr) + 1 : cbMultiByte;
    
    if (cchWideChar == 0) {
        // Return required buffer size
        return len;
    }
    
    if (!lpWideCharStr) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return 0;
    }
    
    size_t converted = mbstowcs(lpWideCharStr, lpMultiByteStr, cchWideChar);
    if (converted == (size_t)-1) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return 0;
    }
    
    return converted;
}

int WideCharToMultiByte(UINT CodePage, DWORD dwFlags, LPCWSTR lpWideCharStr,
                        int cchWideChar, LPSTR lpMultiByteStr, int cbMultiByte,
                        LPCSTR lpDefaultChar, BOOL* lpUsedDefaultChar) {
    if (!lpWideCharStr) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return 0;
    }
    
    size_t len = (cchWideChar == -1) ? wcslen(lpWideCharStr) + 1 : cchWideChar;
    
    if (cbMultiByte == 0) {
        // Return required buffer size (worst case)
        return len * 4;
    }
    
    if (!lpMultiByteStr) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return 0;
    }
    
    size_t converted = wcstombs(lpMultiByteStr, lpWideCharStr, cbMultiByte);
    if (converted == (size_t)-1) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return 0;
    }
    
    if (lpUsedDefaultChar) {
        *lpUsedDefaultChar = FALSE;
    }
    
    return converted;
}

#endif // XE_PLATFORM_MAC