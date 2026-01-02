///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// dxbc2dxil_unix.cpp                                                        //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Unix/macOS port of dxbc2dxil console program.                            //
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
#include "llvm/Support/FileSystem.h"
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
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif
// Include our Windows compatibility layer
#include "Support/windows.h"
// Minimal COM emulation for Unix
#include "dxc/Support/microcom.h"
// Additional definitions not in windows.h
#define _In_z_count_(x)
#define FARPROC void*
#endif

#include "dxc/Support/FileIOHelper.h"
#include "dxc/dxcapi.h"
#include "DxbcConverter.h"

#include <fstream>
#include <algorithm>
#include <atomic>
#include <mutex>
#include <thread>

using namespace llvm;
using std::string;
using std::vector;
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

// vswprintf_s and sprintf_s are already defined in WinAdapter.h

// Platform-specific library names
static const char* GetDxCompilerLibraryName() {
#ifdef __APPLE__
    // Try to find library relative to executable location (thread-safe init)
    static char libPath[1024] = {0};
    static std::once_flag initOnce;
    std::call_once(initOnce, []() {
        strncpy(libPath, "./libdxcompiler.dylib", sizeof(libPath) - 1);
        uint32_t size = sizeof(libPath);
        if (_NSGetExecutablePath(libPath, &size) == 0) {
            // Get the directory containing the executable
            char* lastSlash = strrchr(libPath, '/');
            if (lastSlash) {
                // Try ../lib/libdxcompiler.dylib first (for build directory structure)
                strcpy(lastSlash + 1, "../lib/libdxcompiler.dylib");
                if (access(libPath, F_OK) == 0) {
                    return;
                }
                // Try same directory as executable
                strcpy(lastSlash + 1, "libdxcompiler.dylib");
                if (access(libPath, F_OK) == 0) {
                    return;
                }
            }
        }
        strncpy(libPath, "./libdxcompiler.dylib", sizeof(libPath) - 1);
    });
    return libPath;
#else
    return "./libdxcompiler.so";
#endif
}

static const char* GetDxilConvLibraryName() {
#ifdef __APPLE__
    // Try to find library relative to executable location (thread-safe init)
    static char libPath[1024] = {0};
    static std::once_flag initOnce;
    std::call_once(initOnce, []() {
        strncpy(libPath, "./libdxilconv.dylib", sizeof(libPath) - 1);
        uint32_t size = sizeof(libPath);
        if (_NSGetExecutablePath(libPath, &size) == 0) {
            // Get the directory containing the executable
            char* lastSlash = strrchr(libPath, '/');
            if (lastSlash) {
                // Try ../lib/libdxilconv.dylib first (for build directory structure)
                strcpy(lastSlash + 1, "../lib/libdxilconv.dylib");
                if (access(libPath, F_OK) == 0) {
                    return;
                }
                // Try same directory as executable
                strcpy(lastSlash + 1, "libdxilconv.dylib");
                if (access(libPath, F_OK) == 0) {
                    return;
                }
            }
        }
        strncpy(libPath, "./libdxilconv.dylib", sizeof(libPath) - 1);
    });
    return libPath;
#else
    // Linux: try relative to executable, then current directory
    static char libPath[1024] = {0};
    if (libPath[0] == 0) {
        ssize_t len = readlink("/proc/self/exe", libPath, sizeof(libPath) - 1);
        if (len > 0) {
            libPath[len] = '\0';
            char* lastSlash = strrchr(libPath, '/');
            if (lastSlash) {
                // Try ../lib/libdxilconv.so first
                strcpy(lastSlash + 1, "../lib/libdxilconv.so");
                if (access(libPath, F_OK) == 0) {
                    return libPath;
                }
                // Try same directory as executable
                strcpy(lastSlash + 1, "libdxilconv.so");
                if (access(libPath, F_OK) == 0) {
                    return libPath;
                }
            }
        }
    }
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
    wstring m_BatchDir;
    wstring m_BatchList;
    wstring m_OutDir;
    unsigned m_ThreadCount;
    bool m_bUsage;
    bool m_bDisasmDxbc;
    bool m_bEmitLLVM;
    bool m_bEmitBC;
    bool m_bBatch;
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
    void RunBatch();
    void ConvertFileBatch(const wstring &InputFile, const wstring &OutputFile);
};

Converter::Converter()
: m_ThreadCount(0)
, m_bUsage(false)
, m_bDisasmDxbc(false)
, m_bEmitLLVM(false)
, m_bEmitBC(false)
, m_bBatch(false)
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
    wprintf(L"Usage: dxbc2dxil <input_file> <options>\n");
    wprintf(L"\n");

    wprintf(L"   -h, --help                   print this message\n");
    wprintf(L"\n");

    wprintf(L"   -o <file_name>               output file name\n");
    wprintf(L"   --disasm-dxbc                print DXBC disassembly and exit\n");
    wprintf(L"   --emit-llvm                  print DXIL disassembly and exit\n");
    wprintf(L"   --emit-bc                    emit LLVM bitcode rather than DXIL container\n");
    wprintf(L"   --no-dxil-cleanup            skip DXIL cleanup pass\n");
    wprintf(L"   --auto-skip-cleanup          skip cleanup when temp-reg ops are not used\n");
    wprintf(L"   --fast                       skip cleanup and optional container parts\n");
    wprintf(L"   --skip-container-parts       omit PSV, signature, root signature, feature info parts\n");
    wprintf(L"   --skip-psv                   omit pipeline state validation part\n");
    wprintf(L"   --skip-signatures            omit original input/output signature parts\n");
    wprintf(L"   --skip-root-signature        omit root signature part\n");
    wprintf(L"   --skip-feature-info          omit shader feature info part\n");
    wprintf(L"   --batch <dir>                convert all DXBC binaries in directory\n");
    wprintf(L"   --batch-list <file>          convert DXBC binaries listed in a file\n");
    wprintf(L"   --out-dir <dir>              output directory for batch conversion\n");
    wprintf(L"   --threads <n>                number of worker threads for batch conversion\n");
    wprintf(L"\n");
}

bool Converter::CheckOption(const wchar_t *pStr, const wchar_t *pOption) {
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
               CheckOption(ppArgs[iArg], L"h")) {
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
                else CmdLineError(L"-o output_filename can be specified only once");
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
            else if (CheckOption(ppArgs[iArg], L"auto-skip-cleanup")) {
                m_ExtraOptions += L" -auto-skip-cleanup";
            }
            else if (CheckOption(ppArgs[iArg], L"fast")) {
                m_ExtraOptions += L" -fast";
            }
            else if (CheckOption(ppArgs[iArg], L"skip-container-parts")) {
                m_ExtraOptions += L" -skip-container-parts";
            }
            else if (CheckOption(ppArgs[iArg], L"skip-psv")) {
                m_ExtraOptions += L" -skip-psv";
            }
            else if (CheckOption(ppArgs[iArg], L"skip-signatures")) {
                m_ExtraOptions += L" -skip-signatures";
            }
            else if (CheckOption(ppArgs[iArg], L"skip-root-signature")) {
                m_ExtraOptions += L" -skip-root-signature";
            }
            else if (CheckOption(ppArgs[iArg], L"skip-feature-info")) {
                m_ExtraOptions += L" -skip-feature-info";
            }
            else if (CheckOption(ppArgs[iArg], L"batch")) {
                iArg++;
                if (!m_BatchDir.empty() || !m_BatchList.empty())
                    CmdLineError(L"--batch or --batch-list can be specified only once");
                if (iArg < NumArgs) {
                    m_BatchDir = wstring(ppArgs[iArg]);
                    m_bBatch = true;
                } else {
                    CmdLineError(L"--batch requires a directory path");
                }
            }
            else if (CheckOption(ppArgs[iArg], L"batch-list")) {
                iArg++;
                if (!m_BatchDir.empty() || !m_BatchList.empty())
                    CmdLineError(L"--batch or --batch-list can be specified only once");
                if (iArg < NumArgs) {
                    m_BatchList = wstring(ppArgs[iArg]);
                    m_bBatch = true;
                } else {
                    CmdLineError(L"--batch-list requires a file path");
                }
            }
            else if (CheckOption(ppArgs[iArg], L"out-dir")) {
                iArg++;
                if (!m_OutDir.empty())
                    CmdLineError(L"--out-dir can be specified only once");
                if (iArg < NumArgs) {
                    m_OutDir = wstring(ppArgs[iArg]);
                } else {
                    CmdLineError(L"--out-dir requires a directory path");
                }
            }
            else if (CheckOption(ppArgs[iArg], L"threads")) {
                iArg++;
                if (iArg < NumArgs) {
                    wchar_t *endPtr = nullptr;
                    unsigned long value = wcstoul(ppArgs[iArg], &endPtr, 10);
                    if (endPtr == ppArgs[iArg] || value == 0)
                        CmdLineError(L"--threads requires a positive integer");
                    m_ThreadCount = static_cast<unsigned>(value);
                } else {
                    CmdLineError(L"--threads requires a value");
                }
            }
            else if (ppArgs[iArg] && ppArgs[iArg][0] == L'-') {
                CmdLineError(L"unrecognized option: %ls", ppArgs[iArg]);
            }
            else {
                if (!bSeenInputFile) {
                    m_InputFile = wstring(ppArgs[iArg]);
                    bSeenInputFile = true;
                }
                else CmdLineError(L"input file name can be specified only once (%ls)", ppArgs[iArg]);
            }

            iArg++;
        }

        if (m_bBatch) {
            if (bSeenInputFile)
                CmdLineError(L"input file name not allowed in batch mode");
            if (m_OutDir.empty())
                CmdLineError(L"--out-dir is required in batch mode");
            if ((m_bDisasmDxbc?1:0) + (m_bEmitLLVM?1:0) + (m_bEmitBC?1:0) > 0)
                CmdLineError(L"--disasm-dxbc/--emit-llvm/--emit-bc are not supported in batch mode");
        } else {
            if (!bSeenInputFile) CmdLineError(L"must specify input file name");
            if (!bSeenOutputFile && !(m_bDisasmDxbc || m_bEmitLLVM))
                CmdLineError(L"cannot output binary to the console; must specify output file name");
            if ((m_bDisasmDxbc?1:0) + (m_bEmitLLVM?1:0) + (m_bEmitBC?1:0) > 1)
                CmdLineError(L"--disasm-dxbc, --emit-llvm and --emit-bc are mutually exclusive");
        }
    }
    catch(const wstring &Msg) {
        wprintf(L"%ls: %ls\n", ppArgs[0], Msg.c_str());
        PrintUsage();
        exit(1);
    }
    catch(...) {
        wprintf(L"%ls: Failed to parse command line\n", ppArgs[0]);
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
    idx += vswprintf_s(buf, kBufSize, pFormat, args);
    va_end(args);

    // idx is the number of characters written, not including the terminating
    // null character, or a negative value if an output error occurs
    if (idx < 0) idx = 0;
    buf[idx] = L'\0';

    throw wstring(buf);
}

void Converter::Run() {
    // Usage
    if (m_bUsage) {
        PrintUsage();
        return;
    }

    if (m_bBatch) {
        RunBatch();
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
            // Emit only LLVM IR, e.g., to disassemble with llvm-dis.
            pOutput = pBitcode;
            OutputSize = BitcodeSize;
        }
    }

    hlsl::WriteBinaryFile(m_OutputFile.c_str(), pOutput, OutputSize);
}

struct BatchJob {
    wstring Input;
    wstring Output;
};

static bool IsDxbcBinaryPath(const std::string &Path) {
    StringRef P(Path);
    if (P.find(".d3d12") == StringRef::npos)
        return false;
    return P.endswith(".bin.frag") || P.endswith(".bin.vert");
}

void Converter::ConvertFileBatch(const wstring &InputFile, const wstring &OutputFile) {
    CComHeapPtr<void> pDxbcPtr;
    DWORD DxbcSize;
    hlsl::ReadBinaryFile(InputFile.c_str(), &pDxbcPtr, &DxbcSize);

    CComPtr<IDxbcConverter> converter;
    IFT(CreateDxbcConverter(&converter));

    void *pDxilPtr = nullptr;
    UINT32 DxilSize = 0;
    IFT(converter->Convert(pDxbcPtr, DxbcSize,
                           m_ExtraOptions.empty() ? nullptr : m_ExtraOptions.c_str(),
                           &pDxilPtr, &DxilSize, nullptr));
    CComHeapPtr<void> pDxil(pDxilPtr);
    hlsl::WriteBinaryFile(OutputFile.c_str(), pDxil, DxilSize);
}

void Converter::RunBatch() {
    vector<BatchJob> Jobs;
    std::string OutDirUtf8 = WStringToUTF8(m_OutDir);
    std::error_code ec = llvm::sys::fs::create_directories(OutDirUtf8);
    if (ec) {
        CmdLineError(L"failed to create output directory: %ls", m_OutDir.c_str());
    }

    if (!m_BatchDir.empty()) {
        std::string BatchDirUtf8 = WStringToUTF8(m_BatchDir);
        for (llvm::sys::fs::directory_iterator it(BatchDirUtf8, ec), end; it != end && !ec; it.increment(ec)) {
            std::string Path = it->path();
            if (!IsDxbcBinaryPath(Path))
                continue;
            BatchJob Job;
            Job.Input = UTF8ToWString(Path);
            llvm::SmallString<256> OutPath(OutDirUtf8);
            llvm::sys::path::append(OutPath, llvm::sys::path::filename(Path));
            OutPath += ".dxil";
            Job.Output = UTF8ToWString(OutPath.str().str());
            Jobs.emplace_back(std::move(Job));
        }
        if (ec) {
            CmdLineError(L"failed to enumerate batch directory: %ls", m_BatchDir.c_str());
        }
    } else if (!m_BatchList.empty()) {
        std::ifstream listFile(WStringToUTF8(m_BatchList));
        if (!listFile)
            CmdLineError(L"failed to open batch list: %ls", m_BatchList.c_str());
        std::string line;
        while (std::getline(listFile, line)) {
            StringRef ref(line);
            ref = ref.trim();
            if (ref.empty() || ref.startswith("#"))
                continue;
            std::string Path = ref.str();
            if (!IsDxbcBinaryPath(Path))
                continue;
            BatchJob Job;
            Job.Input = UTF8ToWString(Path);
            llvm::SmallString<256> OutPath(OutDirUtf8);
            llvm::sys::path::append(OutPath, llvm::sys::path::filename(Path));
            OutPath += ".dxil";
            Job.Output = UTF8ToWString(OutPath.str().str());
            Jobs.emplace_back(std::move(Job));
        }
    }

    if (Jobs.empty()) {
        CmdLineError(L"no DXBC binaries found for batch conversion");
    }

    std::sort(Jobs.begin(), Jobs.end(), [](const BatchJob &A, const BatchJob &B) {
        return A.Input < B.Input;
    });

    unsigned ThreadCount = m_ThreadCount;
    if (ThreadCount == 0) {
        ThreadCount = std::max(1u, static_cast<unsigned>(std::thread::hardware_concurrency()));
    }
    ThreadCount = std::min<unsigned>(ThreadCount, static_cast<unsigned>(Jobs.size()));

    std::atomic<size_t> NextIndex(0);
    std::atomic<size_t> Failures(0);
    std::mutex LogMutex;
    vector<std::thread> Workers;
    Workers.reserve(ThreadCount);

    for (unsigned t = 0; t < ThreadCount; ++t) {
        Workers.emplace_back([&]() {
            Converter Worker;
            Worker.m_ExtraOptions = m_ExtraOptions;
            for (;;) {
                size_t idx = NextIndex.fetch_add(1);
                if (idx >= Jobs.size())
                    break;
                try {
                    Worker.ConvertFileBatch(Jobs[idx].Input, Jobs[idx].Output);
                } catch (...) {
                    Failures.fetch_add(1);
                    std::lock_guard<std::mutex> lock(LogMutex);
                    fwprintf(stderr, L"batch convert failed: %ls\n", Jobs[idx].Input.c_str());
                }
            }
        });
    }

    for (auto &Thread : Workers)
        Thread.join();

    if (Failures.load() > 0) {
        fwprintf(stderr, L"batch conversion completed with %zu failures\n", Failures.load());
    }
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
int __cdecl wmain(int argc, _In_z_count_(argc + 1) wchar_t **argv) {
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
                snprintf(printBuffer, sizeof(printBuffer), "Conversion failed - error code 0x%08x.", E.hr);
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
