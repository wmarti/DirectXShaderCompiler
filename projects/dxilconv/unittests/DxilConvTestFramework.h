///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DxilConvTestFramework.h                                                   //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Cross-platform test framework for DxilConv tests on ARM64 macOS          //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#ifndef DXILCONV_TEST_FRAMEWORK_H
#define DXILCONV_TEST_FRAMEWORK_H

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <chrono>
#include <functional>
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstring>

#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#define PATH_SEPARATOR "\\"
#else
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <dlfcn.h>
#define PATH_SEPARATOR "/"
#endif

// Cross-platform types
#ifndef _WIN32
typedef void* HMODULE;
typedef const char* LPCSTR;
typedef const wchar_t* LPCWSTR;
typedef wchar_t* LPWSTR;
typedef unsigned long DWORD;
typedef long HRESULT;
#define MAX_PATH 1024
#define S_OK ((HRESULT)0)
#define S_FALSE ((HRESULT)1)
#define E_FAIL ((HRESULT)0x80004005L)
#define SUCCEEDED(hr) (((HRESULT)(hr)) >= 0)
#define FAILED(hr) (((HRESULT)(hr)) < 0)
#endif

namespace DxilConvTest {

// Test result tracking
struct TestResult {
    std::string name;
    bool passed;
    std::string errorMessage;
    std::chrono::milliseconds duration;
};

// Test case base class
class TestCase {
public:
    TestCase(const std::string& name) : m_name(name) {}
    virtual ~TestCase() = default;
    
    virtual bool Setup() { return true; }
    virtual bool Run() = 0;
    virtual bool Cleanup() { return true; }
    
    const std::string& GetName() const { return m_name; }
    
protected:
    std::string m_name;
};

// Test suite for grouping related tests
class TestSuite {
public:
    TestSuite(const std::string& name) : m_name(name) {}
    
    void AddTest(std::unique_ptr<TestCase> test) {
        m_tests.push_back(std::move(test));
    }
    
    std::vector<TestResult> Run();
    
    const std::string& GetName() const { return m_name; }
    
private:
    std::string m_name;
    std::vector<std::unique_ptr<TestCase>> m_tests;
};

// File system utilities
class FileSystem {
public:
    static bool FileExists(const std::string& path);
    static bool DirectoryExists(const std::string& path);
    static std::string GetCurrentDirectory();
    static bool SetCurrentDirectory(const std::string& path);
    static std::string GetExecutablePath();
    static std::string GetExecutableDirectory();
    static std::vector<std::string> ListFiles(const std::string& directory, 
                                             const std::string& extension = "",
                                             bool recursive = false);
    static std::string JoinPath(const std::string& path1, const std::string& path2);
    static std::string GetFileExtension(const std::string& path);
    static std::string GetFileName(const std::string& path);
    static std::string GetDirectory(const std::string& path);
    static bool CreateDirectory(const std::string& path);
    static std::string ReadFile(const std::string& path);
    static bool WriteFile(const std::string& path, const std::string& content);
    static bool CopyFile(const std::string& source, const std::string& dest);
    static bool DeleteFile(const std::string& path);
    static std::string GetTempDirectory();
    static std::string CreateTempFile(const std::string& prefix = "test_");
};

// Process execution utilities
class Process {
public:
    struct Result {
        int exitCode;
        std::string stdout;
        std::string stderr;
    };
    
    static Result Execute(const std::string& command, 
                         const std::vector<std::string>& args = {},
                         const std::string& workingDir = "",
                         int timeoutMs = 30000);
    
    static Result ExecuteWithInput(const std::string& command,
                                  const std::vector<std::string>& args,
                                  const std::string& input,
                                  const std::string& workingDir = "",
                                  int timeoutMs = 30000);
};

// Dynamic library loading
class DynamicLibrary {
public:
    DynamicLibrary() : m_handle(nullptr) {}
    ~DynamicLibrary() { Unload(); }
    
    bool Load(const std::string& path);
    void Unload();
    void* GetFunction(const std::string& name);
    bool IsLoaded() const { return m_handle != nullptr; }
    
    template<typename FuncType>
    FuncType GetFunction(const std::string& name) {
        return reinterpret_cast<FuncType>(GetFunction(name));
    }
    
private:
    void* m_handle;
    DynamicLibrary(const DynamicLibrary&) = delete;
    DynamicLibrary& operator=(const DynamicLibrary&) = delete;
};

// Performance measurement
class Timer {
public:
    Timer() { Reset(); }
    
    void Reset() {
        m_start = std::chrono::high_resolution_clock::now();
    }
    
    std::chrono::milliseconds Elapsed() const {
        auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(end - m_start);
    }
    
private:
    std::chrono::high_resolution_clock::time_point m_start;
};

// Test runner with reporting
class TestRunner {
public:
    TestRunner();
    
    void AddSuite(std::unique_ptr<TestSuite> suite) {
        m_suites.push_back(std::move(suite));
    }
    
    void RunAll();
    void GenerateReport(const std::string& outputPath = "");
    
    int GetPassedCount() const { return m_passedCount; }
    int GetFailedCount() const { return m_failedCount; }
    int GetTotalCount() const { return m_passedCount + m_failedCount; }
    
private:
    std::vector<std::unique_ptr<TestSuite>> m_suites;
    std::vector<TestResult> m_results;
    int m_passedCount;
    int m_failedCount;
    
    void PrintProgress(const std::string& message);
    void PrintResult(const TestResult& result);
};

// Test assertions
class Assert {
public:
    static void IsTrue(bool condition, const std::string& message = "");
    static void IsFalse(bool condition, const std::string& message = "");
    static void AreEqual(const std::string& expected, const std::string& actual, 
                         const std::string& message = "");
    static void AreNotEqual(const std::string& expected, const std::string& actual,
                            const std::string& message = "");
    static void IsNull(void* ptr, const std::string& message = "");
    static void IsNotNull(void* ptr, const std::string& message = "");
    static void Fail(const std::string& message);
    
    template<typename T>
    static void AreEqual(T expected, T actual, const std::string& message = "") {
        if (expected != actual) {
            std::stringstream ss;
            ss << "Expected: " << expected << ", Actual: " << actual;
            if (!message.empty()) {
                ss << " - " << message;
            }
            throw std::runtime_error(ss.str());
        }
    }
    
    template<typename T>
    static void AreNotEqual(T expected, T actual, const std::string& message = "") {
        if (expected == actual) {
            std::stringstream ss;
            ss << "Values should not be equal: " << expected;
            if (!message.empty()) {
                ss << " - " << message;
            }
            throw std::runtime_error(ss.str());
        }
    }
};

// Configuration and environment
class TestConfig {
public:
    static TestConfig& Instance() {
        static TestConfig instance;
        return instance;
    }
    
    void SetDataDirectory(const std::string& dir) { m_dataDir = dir; }
    std::string GetDataDirectory() const { return m_dataDir; }
    
    void SetOutputDirectory(const std::string& dir) { m_outputDir = dir; }
    std::string GetOutputDirectory() const { return m_outputDir; }
    
    void SetToolPath(const std::string& name, const std::string& path) {
        m_toolPaths[name] = path;
    }
    std::string GetToolPath(const std::string& name) const {
        auto it = m_toolPaths.find(name);
        return (it != m_toolPaths.end()) ? it->second : "";
    }
    
    void SetVerbose(bool verbose) { m_verbose = verbose; }
    bool IsVerbose() const { return m_verbose; }
    
    void SetParallel(bool parallel) { m_parallel = parallel; }
    bool IsParallel() const { return m_parallel; }
    
private:
    TestConfig() : m_verbose(false), m_parallel(false) {}
    
    std::string m_dataDir;
    std::string m_outputDir;
    std::map<std::string, std::string> m_toolPaths;
    bool m_verbose;
    bool m_parallel;
};

// Helper macros for test definition
#define TEST_CLASS(className) \
    class className : public DxilConvTest::TestCase

#define TEST_METHOD(methodName) \
    class Test_##methodName : public DxilConvTest::TestCase { \
    public: \
        Test_##methodName() : TestCase(#methodName) {} \
        bool Run() override; \
    }; \
    bool Test_##methodName::Run()

#define REGISTER_TEST(suite, testClass) \
    suite.AddTest(std::make_unique<testClass>())

#define ASSERT_TRUE(condition) \
    DxilConvTest::Assert::IsTrue(condition, #condition)

#define ASSERT_FALSE(condition) \
    DxilConvTest::Assert::IsFalse(condition, #condition)

#define ASSERT_EQ(expected, actual) \
    DxilConvTest::Assert::AreEqual(expected, actual, #expected " == " #actual)

#define ASSERT_NE(expected, actual) \
    DxilConvTest::Assert::AreNotEqual(expected, actual, #expected " != " #actual)

#define ASSERT_NULL(ptr) \
    DxilConvTest::Assert::IsNull(ptr, #ptr " should be null")

#define ASSERT_NOT_NULL(ptr) \
    DxilConvTest::Assert::IsNotNull(ptr, #ptr " should not be null")

#define ASSERT_HRESULT_SUCCEEDED(hr) \
    ASSERT_TRUE(SUCCEEDED(hr))

#define ASSERT_HRESULT_FAILED(hr) \
    ASSERT_TRUE(FAILED(hr))

} // namespace DxilConvTest

#endif // DXILCONV_TEST_FRAMEWORK_H