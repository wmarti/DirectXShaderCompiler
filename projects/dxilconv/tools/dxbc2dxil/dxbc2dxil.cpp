///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// dxbc2dxil.cpp                                                             //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Provides the entry point for the dxbc2dxil console program.               //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "dxc/Support/Global.h"
#include "dxc/Support/Unicode.h"
#include "dxc/DxilContainer/DxilContainer.h"
#include "dxc/DxilContainer/DxilContainerReader.h"
#include "dxc/DXIL/DxilConstants.h"

#include "llvm/Support/Path.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/ManagedStatic.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/Bitcode/ReaderWriter.h"

// Platform-specific includes
#ifdef _WIN32
#include <atlbase.h>
#include "dxc/Support/microcom.h"
#include "Support/DXIncludes.h"
#else
// Unix/macOS includes
#include <dlfcn.h>
#include <unistd.h>
#include <locale.h>
#include <codecvt>
#include <cstring>
#include <cwchar>
// Minimal COM emulation for Unix
#include "dxc/Support/microcom.h"
#define __stdcall
#define _In_
#define _In_z_count_(x)
#define _Outptr_
#define LPCWSTR const wchar_t*
#define FARPROC void*
#endif

#include "dxc/Support/FileIOHelper.h"

#include "dxc/dxcapi.h"
#include "DxbcConverter.h"

#include <fstream>

using namespace llvm;
using std::string;
using std::wstring;
using std::unique_ptr;

// Unix/macOS helper functions
#ifndef _WIN32
static std::string WStringToUTF8(const std::wstring& wstr) {
    std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
    return converter.to_bytes(wstr);
}

static std::wstring UTF8ToWString(const std::string& str) {
    std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
    return converter.from_bytes(str);
}

// Case-insensitive wide string comparison for Unix
static int _wcsicmp(const wchar_t* s1, const wchar_t* s2) {
    while (*s1 && *s2) {
        wchar_t c1 = towlower(*s1);
        wchar_t c2 = towlower(*s2);
        if (c1 != c2) return c1 - c2;
        s1++;
        s2++;
    }
    return towlower(*s1) - towlower(*s2);
}

// Unix version of vswprintf_s
static int vswprintf_s(wchar_t* buffer, size_t sizeInWords, const wchar_t* format, va_list argptr) {
    return vswprintf(buffer, sizeInWords, format, argptr);
}

// Unix version of sprintf_s
template<size_t size>
static int sprintf_s(char (&buffer)[size], const char* format, ...) {
    va_list args;
    va_start(args, format);
    int result = vsnprintf(buffer, size, format, args);
    va_end(args);
    return result;
}

// Platform-specific library names
static const char* GetDxCompilerLibraryName() {
#ifdef __APPLE__
    return "./libdxcompiler.dylib";
#else
    return "./libdxcompiler.so";
#endif
}

static const char* GetDxilConvLibraryName() {
#ifdef __APPLE__
    return "./libdxilconv.dylib";
#else
    return "./libdxilconv.so";
#endif
}
#endif // !_WIN32


class Converter {
public:
    Converter();
    static void PrintUsage();
    void ParseCommandLine(int NumArgs, wchar_t **ppArgs);
    static void CmdLineError(const wchar_t *pFormat, ...);
    void Run();

protected:
    wstring m_InputFile;
    wstring m_OutputFile;
    bool m_bUsage;
    bool m_bDisasmDxbc;
    bool m_bEmitLLVM;
    bool m_bEmitBC;
    wstring m_ExtraOptions;

private:
    DxcCreateInstanceProc m_pfnDXCompiler_DxcCreateInstance;
    DxcCreateInstanceProc m_pfnDxilConv_DxcCreateInstance;
#ifndef _WIN32
    void* m_hDxCompilerModule;
    void* m_hDxilConvModule;
#endif

    HRESULT CreateDxcLibrary(_Outptr_ IDxcLibrary **ppLibrary);
    HRESULT CreateDxcCompiler(_Outptr_ IDxcCompiler **ppCompiler);
    HRESULT CreateDxbcConverter(_Outptr_ IDxbcConverter **ppConverter);
#ifdef _WIN32
    HRESULT GetDxcCreateInstance(LPCWSTR dllFileName, DxcCreateInstanceProc *ppFn);
#else
    HRESULT GetDxcCreateInstance(const char* dllFileName, DxcCreateInstanceProc *ppFn, void** phModule);
#endif

    static bool CheckOption(const wchar_t *pStr, const wchar_t *pOption);
};

Converter::Converter()
: m_bUsage(false)
, m_bDisasmDxbc(false)
, m_bEmitLLVM(false)
, m_bEmitBC(false)
, m_pfnDXCompiler_DxcCreateInstance(nullptr)
, m_pfnDxilConv_DxcCreateInstance(nullptr)
#ifndef _WIN32
, m_hDxCompilerModule(nullptr)
, m_hDxilConvModule(nullptr)
#endif
{
}

void Converter::PrintUsage() {
    wprintf(L"\n");
#ifdef _WIN32
    wprintf(L"Usage: dxbc2dxil.exe <input_file> <options>\n");
    wprintf(L"\n");
    wprintf(L"   /?, /h, /help                print this message\n");
    wprintf(L"\n");
    wprintf(L"   /o <file_name>               output file name\n");
    wprintf(L"   /disasm-dxbc                 print DXBC disassembly and exit\n");
    wprintf(L"   /emit-llvm                   print DXIL disassembly and exit\n");
    wprintf(L"   /emit-bc                     emit LLVM bitcode rather than DXIL container\n");
#else
    wprintf(L"Usage: dxbc2dxil <input_file> <options>\n");
    wprintf(L"\n");
    wprintf(L"   -h, --help                   print this message\n");
    wprintf(L"\n");
    wprintf(L"   -o <file_name>               output file name\n");
    wprintf(L"   --disasm-dxbc                print DXBC disassembly and exit\n");
    wprintf(L"   --emit-llvm                  print DXIL disassembly and exit\n");
    wprintf(L"   --emit-bc                    emit LLVM bitcode rather than DXIL container\n");
#endif
    wprintf(L"\n");
}

bool Converter::CheckOption(const wchar_t *pStr, const wchar_t *pOption) {
#ifdef _WIN32
    if (!pStr || (pStr[0] != L'-' && pStr[0] != L'/'))
        return false;
    return _wcsicmp(&pStr[1], pOption) == 0;
#else
    if (!pStr || (pStr[0] != L'-'))
        return false;

    // Handle single dash options
    if (pStr[1] != L'-') {
        return _wcsicmp(&pStr[1], pOption) == 0;
    }
    
    // Handle double dash options
    if (pStr[1] == L'-') {
        return _wcsicmp(&pStr[2], pOption) == 0;
    }
    
    return false;
#endif
}

void Converter::ParseCommandLine(int NumArgs, wchar_t **ppArgs) {
    try {
        bool bSeenHelp = false;
        bool bSeenInputFile = false;
        bool bSeenOutputFile = false;

        int iArg = 1;
        while(iArg < NumArgs)
        {
            if (bSeenHelp) CmdLineError(L"too many options");

            if (CheckOption(ppArgs[iArg], L"help") ||
               CheckOption(ppArgs[iArg], L"h") ||
               CheckOption(ppArgs[iArg], L"?")) {
                if (!bSeenInputFile && !bSeenOutputFile) {
                    m_bUsage = bSeenHelp = true;
                }
                else CmdLineError(L"too many options");
            }
            else if (CheckOption(ppArgs[iArg], L"o")) {
                iArg++;

                if (!bSeenOutputFile && iArg < NumArgs) {
                    m_OutputFile = wstring(ppArgs[iArg]);
                    bSeenOutputFile = true;
                }
#ifdef _WIN32
                else CmdLineError(L"/o output_filename can be specified only once");
#else
                else CmdLineError(L"-o output_filename can be specified only once");
#endif
            }
            else if (CheckOption(ppArgs[iArg], L"disasm-dxbc")) {
                m_bDisasmDxbc = true;
            }
            else if (CheckOption(ppArgs[iArg], L"emit-llvm")) {
                m_bEmitLLVM = true;
            }
            else if (CheckOption(ppArgs[iArg], L"emit-bc")) {
                m_bEmitBC = true;
            }
            else if (CheckOption(ppArgs[iArg], L"no-dxil-cleanup")) {
                m_ExtraOptions += L" -no-dxil-cleanup";
            }
#ifdef _WIN32
            else if (ppArgs[iArg] && (ppArgs[iArg][0] == L'-' || ppArgs[iArg][0] == L'/')) {
                CmdLineError(L"unrecognized option: %s", ppArgs[iArg]);
#else
            else if (ppArgs[iArg] && ppArgs[iArg][0] == L'-') {
                CmdLineError(L"unrecognized option: %ls", ppArgs[iArg]);
#endif
            }
            else {
                if (!bSeenInputFile) {
                    m_InputFile = wstring(ppArgs[iArg]);
                    bSeenInputFile = true;
                }
#ifdef _WIN32
                else CmdLineError(L"input file name can be specified only once (%s)", ppArgs[iArg]);
#else
                else CmdLineError(L"input file name can be specified only once (%ls)", ppArgs[iArg]);
#endif
            }

            iArg++;
        }

        if (!bSeenInputFile) CmdLineError(L"must specify input file name");
        if (!bSeenOutputFile && !(m_bDisasmDxbc || m_bEmitLLVM))
            CmdLineError(L"cannot output binary to the console; must specify output file name");
        if ((m_bDisasmDxbc?1:0) + (m_bEmitLLVM?1:0) + (m_bEmitBC?1:0) > 1)
#ifdef _WIN32
            CmdLineError(L"/disasm-dxbc, /emit-llvm and /emit-bc are mutually exclusive");
#else
            CmdLineError(L"--disasm-dxbc, --emit-llvm and --emit-bc are mutually exclusive");
#endif
    }
    catch(const wstring &Msg) {
#ifdef _WIN32
        wprintf(L"%s: %s\n", ppArgs[0], Msg.c_str());
#else
        wprintf(L"%ls: %ls\n", ppArgs[0], Msg.c_str());
#endif
        PrintUsage();
        exit(1);
    }
    catch(...) {
#ifdef _WIN32
        wprintf(L"%s: Failed to parse command line\n", ppArgs[0]);
#else
        wprintf(L"%ls: Failed to parse command line\n", ppArgs[0]);
#endif
        PrintUsage();
        exit(1);
    }
}

void Converter::CmdLineError(const wchar_t *pFormat, ...) {
  const int kBufSize = 4*1024;
  wchar_t buf[kBufSize + 1];
  int idx = 0;
  va_list args;
  va_start(args, pFormat);
  idx += vswprintf_s(&buf[idx], kBufSize, pFormat, args);
  va_end(args);

  // idx is the number of characters written, not including the terminating
  // null character, or a negative value if an output error occurs
  if (idx < 0) idx = 0;
  _Analysis_assume_(0 <= idx && idx <= kBufSize);
  buf[idx] = L'\0';

  throw wstring(buf);
}

void Converter::Run() {
  // Usage
  if (m_bUsage) {
    PrintUsage();
    return;
  }

  // Load DXBC blob.
  CComHeapPtr<void> pDxbcPtr;
  DWORD DxbcSize;
  hlsl::ReadBinaryFile(m_InputFile.c_str(), &pDxbcPtr, &DxbcSize);

  // Disassemble Dxbc blob and exit.
  if (m_bDisasmDxbc) {
    CComPtr<IDxcLibrary> library;
    IFT(CreateDxcLibrary(&library));

    CComPtr<IDxcBlobEncoding> source;
    IFT(library->CreateBlobWithEncodingFromPinned((LPBYTE)pDxbcPtr.m_pData, DxbcSize,
      CP_ACP, &source));

    CComPtr<IDxcCompiler> compiler;
    IFT(CreateDxcCompiler(&compiler));

    CComPtr<IDxcBlobEncoding> pDisasmBlob;
    IFT(compiler->Disassemble(source, &pDisasmBlob.p));

    const char *pText = (const char *)pDisasmBlob->GetBufferPointer();
    IFTPTR(pText);
    if (m_OutputFile.empty())
      printf("%s", pText);
    else
      hlsl::WriteBinaryFile(m_OutputFile.c_str(), pText, strlen(pText));

    return;
  }

  // Convert DXBC to DXIL.
  CComPtr<IDxbcConverter> converter;
  IFT(CreateDxbcConverter(&converter));

  void *pDxilPtr;
  UINT32 DxilSize;
  IFT(converter->Convert(pDxbcPtr, DxbcSize, m_ExtraOptions.empty() ? nullptr : m_ExtraOptions.c_str(),
                          &pDxilPtr, &DxilSize, nullptr));
  CComHeapPtr<void> pDxil(pDxilPtr);

  // Determine output.
  const void *pOutput = pDxil;  // DXIL blob (in DXBC container).
  UINT32 OutputSize = DxilSize;
  if (m_bEmitLLVM || m_bEmitBC) {
    // Retrieve DXIL.
    hlsl::DxilContainerReader dxilReader;
    IFT(dxilReader.Load(pDxil, DxilSize));

    UINT uDxilBlob;
    IFT(dxilReader.FindFirstPartKind(hlsl::DFCC_DXIL, &uDxilBlob));
    IFTBOOL(uDxilBlob != DXIL_CONTAINER_BLOB_NOT_FOUND, DXC_E_INCORRECT_DXBC);

    const char *pDxilBlob;
    UINT32 DxilBlobSize;
    IFTBOOL(dxilReader.GetPartContent(uDxilBlob, (const void **)&pDxilBlob, &DxilBlobSize) == S_OK, DXC_E_INCORRECT_DXBC);

    // Retrieve LLVM bitcode.
    const hlsl::DxilProgramHeader *pHeader = (const hlsl::DxilProgramHeader *)pDxilBlob;
    const char *pBitcode = hlsl::GetDxilBitcodeData(pHeader);
    UINT32 BitcodeSize = hlsl::GetDxilBitcodeSize(pHeader);
    IFTBOOL(BitcodeSize + sizeof(hlsl::DxilProgramHeader) <= DxilBlobSize, DXC_E_INCORRECT_DXBC);
    IFTBOOL(pHeader->BitcodeHeader.DxilMagic == *((const uint32_t *)"DXIL"), DXC_E_INCORRECT_DXBC);
    IFTBOOL(hlsl::DXIL::GetDxilVersionMajor(pHeader->BitcodeHeader.DxilVersion) == 1, DXC_E_INCORRECT_DXBC);
    IFTBOOL(hlsl::DXIL::GetDxilVersionMinor(pHeader->BitcodeHeader.DxilVersion) == 0, DXC_E_INCORRECT_DXBC);

    if (m_bEmitLLVM) {
      // Disassemble LLVM module and exit.
      unique_ptr<MemoryBuffer> pBitcodeBuf(MemoryBuffer::getMemBuffer(StringRef(pBitcode, BitcodeSize), "", false));
      ErrorOr<std::unique_ptr<Module>> pModule(parseBitcodeFile(pBitcodeBuf->getMemBufferRef(), getGlobalContext()));
      if (std::error_code ec = pModule.getError()) {
        throw hlsl::Exception(DXC_E_INCORRECT_DXBC);
      }
      string StreamStr;
      raw_string_ostream Stream(StreamStr);
      pModule.get()->print(Stream, nullptr);
      Stream.flush();
      if (m_OutputFile.empty())
        printf("%s", StreamStr.c_str());
      else {
#ifdef _WIN32
        std::ofstream ofs(m_OutputFile);
#else
        std::ofstream ofs(WStringToUTF8(m_OutputFile));
#endif
        if (!ofs)
            throw hlsl::Exception(E_ABORT, "unable to open output file");
        ofs << StreamStr;
      }

      return;
    }
    else if (m_bEmitBC) {
      // Emit only LLVM IR, e.g., to disassemble with llvm-dis.exe.
      pOutput = pBitcode;
      OutputSize = BitcodeSize;
    }
  }

  hlsl::WriteBinaryFile(m_OutputFile.c_str(), pOutput, OutputSize);
}

HRESULT Converter::CreateDxcLibrary(_Outptr_ IDxcLibrary **ppLibrary) {
    if (m_pfnDXCompiler_DxcCreateInstance == nullptr) {
#ifdef _WIN32
        IFR(GetDxcCreateInstance(L"dxcompiler.dll", &m_pfnDXCompiler_DxcCreateInstance));
#else
        IFR(GetDxcCreateInstance(GetDxCompilerLibraryName(), &m_pfnDXCompiler_DxcCreateInstance, &m_hDxCompilerModule));
#endif
    }
    IFR((*m_pfnDXCompiler_DxcCreateInstance)(CLSID_DxcLibrary, __uuidof(IDxcLibrary), reinterpret_cast<LPVOID*>(ppLibrary)));
    return S_OK;
}

HRESULT Converter::CreateDxcCompiler(_Outptr_ IDxcCompiler **ppCompiler) {
    if (m_pfnDXCompiler_DxcCreateInstance == nullptr) {
#ifdef _WIN32
        IFR(GetDxcCreateInstance(L"dxcompiler.dll", &m_pfnDXCompiler_DxcCreateInstance));
#else
        IFR(GetDxcCreateInstance(GetDxCompilerLibraryName(), &m_pfnDXCompiler_DxcCreateInstance, &m_hDxCompilerModule));
#endif
    }
    IFR((*m_pfnDXCompiler_DxcCreateInstance)(CLSID_DxcCompiler, __uuidof(IDxcCompiler), reinterpret_cast<LPVOID*>(ppCompiler)));
    return S_OK;
}

HRESULT Converter::CreateDxbcConverter(_Outptr_ IDxbcConverter **ppConverter) {
    if (m_pfnDxilConv_DxcCreateInstance == nullptr) {
#ifdef _WIN32
        IFR(GetDxcCreateInstance(L"dxilconv.dll", &m_pfnDxilConv_DxcCreateInstance));
#else
        IFR(GetDxcCreateInstance(GetDxilConvLibraryName(), &m_pfnDxilConv_DxcCreateInstance, &m_hDxilConvModule));
#endif
    }
    IFR((*m_pfnDxilConv_DxcCreateInstance)(CLSID_DxbcConverter, __uuidof(IDxbcConverter), reinterpret_cast<LPVOID*>(ppConverter)));
    return S_OK;
}

#ifdef _WIN32
HRESULT Converter::GetDxcCreateInstance(LPCWSTR dllFileName, DxcCreateInstanceProc *ppFn) {
    HMODULE hModule = LoadLibraryExW(dllFileName, NULL, LOAD_LIBRARY_SEARCH_APPLICATION_DIR);
    if (hModule == NULL) {
        return HRESULT_FROM_WIN32(GetLastError());
    }

    FARPROC pFn = GetProcAddress(hModule, "DxcCreateInstance");
    if (pFn == NULL) {
        return HRESULT_FROM_WIN32(GetLastError());
    }
    *ppFn = reinterpret_cast<DxcCreateInstanceProc>(pFn);

    return S_OK;
}
#else
HRESULT Converter::GetDxcCreateInstance(const char* dllFileName, DxcCreateInstanceProc *ppFn, void** phModule) {
    // Try to load the library
    void* hModule = dlopen(dllFileName, RTLD_LAZY | RTLD_LOCAL);
    if (hModule == nullptr) {
        fprintf(stderr, "Failed to load %s: %s\n", dllFileName, dlerror());
        return E_FAIL;
    }

    // Get the DxcCreateInstance function
    void* pFn = dlsym(hModule, "DxcCreateInstance");
    if (pFn == nullptr) {
        fprintf(stderr, "Failed to find DxcCreateInstance in %s: %s\n", dllFileName, dlerror());
        dlclose(hModule);
        return E_FAIL;
    }

    *ppFn = reinterpret_cast<DxcCreateInstanceProc>(pFn);
    *phModule = hModule;

    return S_OK;
}
#endif

#ifdef _WIN32
int __cdecl wmain(int argc, wchar_t **argv) {
#else
int main(int argc, char **argv) {
    // Set locale for proper wide character handling
    setlocale(LC_ALL, "");
    
    // Convert UTF-8 arguments to wide strings
    wchar_t** wargv = new wchar_t*[argc + 1];
    for (int i = 0; i < argc; i++) {
        std::wstring wstr = UTF8ToWString(argv[i]);
        wargv[i] = new wchar_t[wstr.length() + 1];
        wcscpy(wargv[i], wstr.c_str());
    }
    wargv[argc] = nullptr;
#endif

    llvm_shutdown_obj Y;

    try {
        Converter C;
#ifdef _WIN32
        C.ParseCommandLine(argc, argv);
#else
        C.ParseCommandLine(argc, wargv);
#endif
        C.Run();
    }
    catch (const std::bad_alloc) {
        printf("Conversion failed - out of memory.\n");
    }
    catch (const hlsl::Exception &E) {
        try {
            const char *pMsg = E.what();
            Unicode::acp_char printBuffer[128]; // printBuffer is safe to treat as UTF-8 because we use ASCII contents only
            if (pMsg == nullptr || *pMsg == '\0') {
                sprintf_s(printBuffer, "Conversion failed - error code 0x%08x.", E.hr);
                pMsg = printBuffer;
            }

            std::string textMessage;
            bool lossy;
            if (!Unicode::UTF8ToConsoleString(pMsg, &textMessage, &lossy) || lossy) {
                // Do a direct assignment as a last-ditch effort and print out as UTF-8.
                textMessage = pMsg;
            }

            printf("%s\n", textMessage.c_str());
        }
        catch (...) {
            printf("Conversion failed - unable to retrieve error message.\n");
        }

#ifndef _WIN32
        // Clean up wide character arguments
        for (int i = 0; i < argc; i++) {
            delete[] wargv[i];
        }
        delete[] wargv;
#endif
        return 1;
    }
    catch (...) {
        printf("Conversion failed - unable to retrieve error message.\n");
#ifndef _WIN32
        // Clean up wide character arguments
        for (int i = 0; i < argc; i++) {
            delete[] wargv[i];
        }
        delete[] wargv;
#endif
        return 1;
    }

#ifndef _WIN32
    // Clean up wide character arguments
    for (int i = 0; i < argc; i++) {
        delete[] wargv[i];
    }
    delete[] wargv;
#endif

    return 0;
}
