///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DxilConvTestFramework.cpp                                                 //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Cross-platform test framework implementation for ARM64 macOS              //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "DxilConvTestFramework.h"
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <cstdlib>
#include <thread>
#include <future>

#ifdef __APPLE__
#include <mach-o/dyld.h>
#include <limits.h>
#include <spawn.h>
#include <sys/wait.h>
extern char **environ;
#endif

#ifdef __linux__
#include <limits.h>
#include <unistd.h>
#endif

namespace DxilConvTest {

//////////////////////////////////////////////////////////////////////////////
// TestSuite implementation
//////////////////////////////////////////////////////////////////////////////

std::vector<TestResult> TestSuite::Run() {
    std::vector<TestResult> results;
    
    for (auto& test : m_tests) {
        TestResult result;
        result.name = m_name + "::" + test->GetName();
        
        Timer timer;
        
        try {
            // Setup
            if (!test->Setup()) {
                throw std::runtime_error("Test setup failed");
            }
            
            // Run
            result.passed = test->Run();
            
            // Cleanup
            if (!test->Cleanup()) {
                std::cerr << "Warning: Test cleanup failed for " << result.name << std::endl;
            }
        } catch (const std::exception& e) {
            result.passed = false;
            result.errorMessage = e.what();
        } catch (...) {
            result.passed = false;
            result.errorMessage = "Unknown exception";
        }
        
        result.duration = timer.Elapsed();
        results.push_back(result);
    }
    
    return results;
}

//////////////////////////////////////////////////////////////////////////////
// FileSystem implementation
//////////////////////////////////////////////////////////////////////////////

bool FileSystem::FileExists(const std::string& path) {
#ifdef _WIN32
    DWORD attrib = GetFileAttributesA(path.c_str());
    return (attrib != INVALID_FILE_ATTRIBUTES && !(attrib & FILE_ATTRIBUTE_DIRECTORY));
#else
    struct stat st;
    return (stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode));
#endif
}

bool FileSystem::DirectoryExists(const std::string& path) {
#ifdef _WIN32
    DWORD attrib = GetFileAttributesA(path.c_str());
    return (attrib != INVALID_FILE_ATTRIBUTES && (attrib & FILE_ATTRIBUTE_DIRECTORY));
#else
    struct stat st;
    return (stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode));
#endif
}

std::string FileSystem::GetCurrentDirectory() {
    char buffer[MAX_PATH];
#ifdef _WIN32
    if (_getcwd(buffer, MAX_PATH)) {
        return std::string(buffer);
    }
#else
    if (getcwd(buffer, MAX_PATH)) {
        return std::string(buffer);
    }
#endif
    return "";
}

bool FileSystem::SetCurrentDirectory(const std::string& path) {
#ifdef _WIN32
    return _chdir(path.c_str()) == 0;
#else
    return chdir(path.c_str()) == 0;
#endif
}

std::string FileSystem::GetExecutablePath() {
#ifdef _WIN32
    char buffer[MAX_PATH];
    GetModuleFileNameA(NULL, buffer, MAX_PATH);
    return std::string(buffer);
#elif defined(__APPLE__)
    char buffer[PATH_MAX];
    uint32_t size = sizeof(buffer);
    if (_NSGetExecutablePath(buffer, &size) == 0) {
        char resolved[PATH_MAX];
        if (realpath(buffer, resolved)) {
            return std::string(resolved);
        }
    }
    return "";
#elif defined(__linux__)
    char buffer[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
    if (len != -1) {
        buffer[len] = '\0';
        return std::string(buffer);
    }
    return "";
#else
    return "";
#endif
}

std::string FileSystem::GetExecutableDirectory() {
    std::string path = GetExecutablePath();
    size_t pos = path.find_last_of("/\\");
    if (pos != std::string::npos) {
        return path.substr(0, pos);
    }
    return "";
}

std::vector<std::string> FileSystem::ListFiles(const std::string& directory, 
                                              const std::string& extension,
                                              bool recursive) {
    std::vector<std::string> files;
    
#ifdef _WIN32
    WIN32_FIND_DATAA findData;
    std::string pattern = JoinPath(directory, "*");
    HANDLE hFind = FindFirstFileA(pattern.c_str(), &findData);
    
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            std::string name(findData.cFileName);
            if (name != "." && name != "..") {
                std::string fullPath = JoinPath(directory, name);
                
                if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                    if (recursive) {
                        auto subFiles = ListFiles(fullPath, extension, recursive);
                        files.insert(files.end(), subFiles.begin(), subFiles.end());
                    }
                } else {
                    if (extension.empty() || GetFileExtension(name) == extension) {
                        files.push_back(fullPath);
                    }
                }
            }
        } while (FindNextFileA(hFind, &findData));
        FindClose(hFind);
    }
#else
    DIR* dir = opendir(directory.c_str());
    if (dir) {
        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr) {
            std::string name(entry->d_name);
            if (name != "." && name != "..") {
                std::string fullPath = JoinPath(directory, name);
                
                struct stat st;
                if (stat(fullPath.c_str(), &st) == 0) {
                    if (S_ISDIR(st.st_mode)) {
                        if (recursive) {
                            auto subFiles = ListFiles(fullPath, extension, recursive);
                            files.insert(files.end(), subFiles.begin(), subFiles.end());
                        }
                    } else if (S_ISREG(st.st_mode)) {
                        if (extension.empty() || GetFileExtension(name) == extension) {
                            files.push_back(fullPath);
                        }
                    }
                }
            }
        }
        closedir(dir);
    }
#endif
    
    return files;
}

std::string FileSystem::JoinPath(const std::string& path1, const std::string& path2) {
    if (path1.empty()) return path2;
    if (path2.empty()) return path1;
    
    char lastChar = path1[path1.length() - 1];
    if (lastChar == '/' || lastChar == '\\') {
        return path1 + path2;
    }
    return path1 + PATH_SEPARATOR + path2;
}

std::string FileSystem::GetFileExtension(const std::string& path) {
    size_t pos = path.find_last_of('.');
    if (pos != std::string::npos && pos < path.length() - 1) {
        return path.substr(pos);
    }
    return "";
}

std::string FileSystem::GetFileName(const std::string& path) {
    size_t pos = path.find_last_of("/\\");
    if (pos != std::string::npos) {
        return path.substr(pos + 1);
    }
    return path;
}

std::string FileSystem::GetDirectory(const std::string& path) {
    size_t pos = path.find_last_of("/\\");
    if (pos != std::string::npos) {
        return path.substr(0, pos);
    }
    return "";
}

bool FileSystem::CreateDirectory(const std::string& path) {
#ifdef _WIN32
    return CreateDirectoryA(path.c_str(), NULL) || GetLastError() == ERROR_ALREADY_EXISTS;
#else
    return mkdir(path.c_str(), 0755) == 0 || errno == EEXIST;
#endif
}

std::string FileSystem::ReadFile(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file: " + path);
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

bool FileSystem::WriteFile(const std::string& path, const std::string& content) {
    std::ofstream file(path, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }
    
    file << content;
    return file.good();
}

bool FileSystem::CopyFile(const std::string& source, const std::string& dest) {
    try {
        std::string content = ReadFile(source);
        return WriteFile(dest, content);
    } catch (...) {
        return false;
    }
}

bool FileSystem::DeleteFile(const std::string& path) {
    return std::remove(path.c_str()) == 0;
}

std::string FileSystem::GetTempDirectory() {
#ifdef _WIN32
    char buffer[MAX_PATH];
    GetTempPathA(MAX_PATH, buffer);
    return std::string(buffer);
#else
    const char* tmpdir = std::getenv("TMPDIR");
    if (tmpdir) return std::string(tmpdir);
    
    tmpdir = std::getenv("TMP");
    if (tmpdir) return std::string(tmpdir);
    
    tmpdir = std::getenv("TEMP");
    if (tmpdir) return std::string(tmpdir);
    
    return "/tmp";
#endif
}

std::string FileSystem::CreateTempFile(const std::string& prefix) {
    std::string tempDir = GetTempDirectory();
    
    // Generate unique filename
    static int counter = 0;
    std::stringstream ss;
    ss << tempDir << PATH_SEPARATOR << prefix << "_" 
       << std::this_thread::get_id() << "_"
       << std::chrono::system_clock::now().time_since_epoch().count() << "_"
       << (counter++) << ".tmp";
    
    return ss.str();
}

//////////////////////////////////////////////////////////////////////////////
// Process implementation
//////////////////////////////////////////////////////////////////////////////

Process::Result Process::Execute(const std::string& command, 
                                const std::vector<std::string>& args,
                                const std::string& workingDir,
                                int timeoutMs) {
    Result result;
    
#ifdef _WIN32
    // Windows implementation using CreateProcess
    std::string cmdLine = command;
    for (const auto& arg : args) {
        cmdLine += " " + arg;
    }
    
    STARTUPINFOA si = {0};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    
    HANDLE hStdOutRead, hStdOutWrite;
    HANDLE hStdErrRead, hStdErrWrite;
    
    SECURITY_ATTRIBUTES sa = {0};
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    
    CreatePipe(&hStdOutRead, &hStdOutWrite, &sa, 0);
    CreatePipe(&hStdErrRead, &hStdErrWrite, &sa, 0);
    
    SetHandleInformation(hStdOutRead, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(hStdErrRead, HANDLE_FLAG_INHERIT, 0);
    
    si.hStdOutput = hStdOutWrite;
    si.hStdError = hStdErrWrite;
    
    PROCESS_INFORMATION pi = {0};
    
    BOOL success = CreateProcessA(
        NULL,
        const_cast<char*>(cmdLine.c_str()),
        NULL,
        NULL,
        TRUE,
        0,
        NULL,
        workingDir.empty() ? NULL : workingDir.c_str(),
        &si,
        &pi
    );
    
    if (success) {
        CloseHandle(hStdOutWrite);
        CloseHandle(hStdErrWrite);
        
        // Read output
        char buffer[4096];
        DWORD bytesRead;
        
        while (ReadFile(hStdOutRead, buffer, sizeof(buffer) - 1, &bytesRead, NULL)) {
            buffer[bytesRead] = '\0';
            result.stdout += buffer;
        }
        
        while (ReadFile(hStdErrRead, buffer, sizeof(buffer) - 1, &bytesRead, NULL)) {
            buffer[bytesRead] = '\0';
            result.stderr += buffer;
        }
        
        // Wait for process
        WaitForSingleObject(pi.hProcess, timeoutMs);
        
        DWORD exitCode;
        GetExitCodeProcess(pi.hProcess, &exitCode);
        result.exitCode = exitCode;
        
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    } else {
        result.exitCode = -1;
        result.stderr = "Failed to create process";
    }
    
    CloseHandle(hStdOutRead);
    CloseHandle(hStdErrRead);
    
#else
    // Unix implementation using posix_spawn
    int pipeOut[2], pipeErr[2];
    if (pipe(pipeOut) != 0 || pipe(pipeErr) != 0) {
        result.exitCode = -1;
        result.stderr = "Failed to create pipes";
        return result;
    }
    
    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_adddup2(&actions, pipeOut[1], STDOUT_FILENO);
    posix_spawn_file_actions_adddup2(&actions, pipeErr[1], STDERR_FILENO);
    posix_spawn_file_actions_addclose(&actions, pipeOut[0]);
    posix_spawn_file_actions_addclose(&actions, pipeErr[0]);
    
    // Prepare arguments
    std::vector<char*> argv;
    argv.push_back(const_cast<char*>(command.c_str()));
    for (const auto& arg : args) {
        argv.push_back(const_cast<char*>(arg.c_str()));
    }
    argv.push_back(nullptr);
    
    pid_t pid;
    std::string savedCwd;
    if (!workingDir.empty()) {
        savedCwd = GetCurrentDirectory();
        chdir(workingDir.c_str());
    }
    
    int status = posix_spawn(&pid, command.c_str(), &actions, nullptr, 
                             argv.data(), environ);
    
    if (!workingDir.empty() && !savedCwd.empty()) {
        chdir(savedCwd.c_str());
    }
    
    posix_spawn_file_actions_destroy(&actions);
    
    close(pipeOut[1]);
    close(pipeErr[1]);
    
    if (status == 0) {
        // Read output
        char buffer[4096];
        ssize_t bytesRead;
        
        while ((bytesRead = read(pipeOut[0], buffer, sizeof(buffer) - 1)) > 0) {
            buffer[bytesRead] = '\0';
            result.stdout += buffer;
        }
        
        while ((bytesRead = read(pipeErr[0], buffer, sizeof(buffer) - 1)) > 0) {
            buffer[bytesRead] = '\0';
            result.stderr += buffer;
        }
        
        // Wait for process with timeout
        int waitStatus;
        auto start = std::chrono::steady_clock::now();
        
        while (true) {
            pid_t wpid = waitpid(pid, &waitStatus, WNOHANG);
            if (wpid == pid) {
                if (WIFEXITED(waitStatus)) {
                    result.exitCode = WEXITSTATUS(waitStatus);
                } else {
                    result.exitCode = -1;
                }
                break;
            }
            
            auto elapsed = std::chrono::steady_clock::now() - start;
            if (std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count() > timeoutMs) {
                kill(pid, SIGKILL);
                waitpid(pid, &waitStatus, 0);
                result.exitCode = -1;
                result.stderr += "\nProcess timed out";
                break;
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    } else {
        result.exitCode = -1;
        result.stderr = "Failed to spawn process";
    }
    
    close(pipeOut[0]);
    close(pipeErr[0]);
#endif
    
    return result;
}

Process::Result Process::ExecuteWithInput(const std::string& command,
                                         const std::vector<std::string>& args,
                                         const std::string& input,
                                         const std::string& workingDir,
                                         int timeoutMs) {
    // Create temp file with input
    std::string inputFile = FileSystem::CreateTempFile("input");
    FileSystem::WriteFile(inputFile, input);
    
    // Redirect stdin from file
    std::vector<std::string> modifiedArgs = args;
    modifiedArgs.push_back("<");
    modifiedArgs.push_back(inputFile);
    
    Result result = Execute(command, modifiedArgs, workingDir, timeoutMs);
    
    // Clean up
    FileSystem::DeleteFile(inputFile);
    
    return result;
}

//////////////////////////////////////////////////////////////////////////////
// DynamicLibrary implementation
//////////////////////////////////////////////////////////////////////////////

bool DynamicLibrary::Load(const std::string& path) {
    if (m_handle) {
        Unload();
    }
    
#ifdef _WIN32
    m_handle = LoadLibraryA(path.c_str());
#else
    m_handle = dlopen(path.c_str(), RTLD_LAZY);
#endif
    
    return m_handle != nullptr;
}

void DynamicLibrary::Unload() {
    if (m_handle) {
#ifdef _WIN32
        FreeLibrary((HMODULE)m_handle);
#else
        dlclose(m_handle);
#endif
        m_handle = nullptr;
    }
}

void* DynamicLibrary::GetFunction(const std::string& name) {
    if (!m_handle) {
        return nullptr;
    }
    
#ifdef _WIN32
    return GetProcAddress((HMODULE)m_handle, name.c_str());
#else
    return dlsym(m_handle, name.c_str());
#endif
}

//////////////////////////////////////////////////////////////////////////////
// TestRunner implementation
//////////////////////////////////////////////////////////////////////////////

TestRunner::TestRunner() : m_passedCount(0), m_failedCount(0) {
}

void TestRunner::RunAll() {
    m_results.clear();
    m_passedCount = 0;
    m_failedCount = 0;
    
    std::cout << "Running " << m_suites.size() << " test suites..." << std::endl;
    std::cout << std::string(60, '=') << std::endl;
    
    Timer totalTimer;
    
    for (auto& suite : m_suites) {
        std::cout << "\nTest Suite: " << suite->GetName() << std::endl;
        std::cout << std::string(40, '-') << std::endl;
        
        auto suiteResults = suite->Run();
        
        for (const auto& result : suiteResults) {
            PrintResult(result);
            m_results.push_back(result);
            
            if (result.passed) {
                m_passedCount++;
            } else {
                m_failedCount++;
            }
        }
    }
    
    auto totalTime = totalTimer.Elapsed();
    
    std::cout << std::endl;
    std::cout << std::string(60, '=') << std::endl;
    std::cout << "Test Summary:" << std::endl;
    std::cout << "  Total:  " << GetTotalCount() << " tests" << std::endl;
    std::cout << "  Passed: " << m_passedCount << " tests" << std::endl;
    std::cout << "  Failed: " << m_failedCount << " tests" << std::endl;
    std::cout << "  Time:   " << totalTime.count() << " ms" << std::endl;
    
    if (m_failedCount == 0) {
        std::cout << "\n✅ All tests passed!" << std::endl;
    } else {
        std::cout << "\n❌ Some tests failed!" << std::endl;
    }
}

void TestRunner::GenerateReport(const std::string& outputPath) {
    std::string reportPath = outputPath.empty() ? "test_report.txt" : outputPath;
    
    std::ofstream report(reportPath);
    if (!report.is_open()) {
        std::cerr << "Failed to create report file: " << reportPath << std::endl;
        return;
    }
    
    report << "Test Execution Report" << std::endl;
    report << "=====================" << std::endl;
    report << "Date: " << std::chrono::system_clock::now().time_since_epoch().count() << std::endl;
    report << "Platform: " << 
#ifdef _WIN32
        "Windows"
#elif defined(__APPLE__)
        "macOS ARM64"
#elif defined(__linux__)
        "Linux"
#else
        "Unknown"
#endif
        << std::endl;
    report << std::endl;
    
    report << "Summary:" << std::endl;
    report << "--------" << std::endl;
    report << "Total Tests: " << GetTotalCount() << std::endl;
    report << "Passed: " << m_passedCount << std::endl;
    report << "Failed: " << m_failedCount << std::endl;
    report << std::endl;
    
    report << "Test Results:" << std::endl;
    report << "-------------" << std::endl;
    
    for (const auto& result : m_results) {
        report << std::left << std::setw(50) << result.name;
        report << " [" << (result.passed ? "PASS" : "FAIL") << "]";
        report << " (" << result.duration.count() << " ms)";
        
        if (!result.passed && !result.errorMessage.empty()) {
            report << std::endl << "  Error: " << result.errorMessage;
        }
        report << std::endl;
    }
    
    report.close();
    std::cout << "\nReport generated: " << reportPath << std::endl;
}

void TestRunner::PrintProgress(const std::string& message) {
    if (TestConfig::Instance().IsVerbose()) {
        std::cout << message << std::endl;
    }
}

void TestRunner::PrintResult(const TestResult& result) {
    const int nameWidth = 50;
    std::cout << "  " << std::left << std::setw(nameWidth) << result.name;
    
    if (result.passed) {
        std::cout << " [PASS]";
    } else {
        std::cout << " [FAIL]";
    }
    
    std::cout << " (" << result.duration.count() << " ms)";
    
    if (!result.passed && !result.errorMessage.empty()) {
        std::cout << std::endl << "    Error: " << result.errorMessage;
    }
    
    std::cout << std::endl;
}

//////////////////////////////////////////////////////////////////////////////
// Assert implementation
//////////////////////////////////////////////////////////////////////////////

void Assert::IsTrue(bool condition, const std::string& message) {
    if (!condition) {
        std::string errorMsg = "Assertion failed: condition is false";
        if (!message.empty()) {
            errorMsg += " - " + message;
        }
        throw std::runtime_error(errorMsg);
    }
}

void Assert::IsFalse(bool condition, const std::string& message) {
    if (condition) {
        std::string errorMsg = "Assertion failed: condition is true";
        if (!message.empty()) {
            errorMsg += " - " + message;
        }
        throw std::runtime_error(errorMsg);
    }
}

void Assert::AreEqual(const std::string& expected, const std::string& actual, 
                     const std::string& message) {
    if (expected != actual) {
        std::stringstream ss;
        ss << "Assertion failed: strings are not equal" << std::endl;
        ss << "  Expected: \"" << expected << "\"" << std::endl;
        ss << "  Actual:   \"" << actual << "\"";
        if (!message.empty()) {
            ss << std::endl << "  Message:  " << message;
        }
        throw std::runtime_error(ss.str());
    }
}

void Assert::AreNotEqual(const std::string& expected, const std::string& actual,
                        const std::string& message) {
    if (expected == actual) {
        std::stringstream ss;
        ss << "Assertion failed: strings should not be equal" << std::endl;
        ss << "  Value: \"" << expected << "\"";
        if (!message.empty()) {
            ss << std::endl << "  Message: " << message;
        }
        throw std::runtime_error(ss.str());
    }
}

void Assert::IsNull(void* ptr, const std::string& message) {
    if (ptr != nullptr) {
        std::string errorMsg = "Assertion failed: pointer is not null";
        if (!message.empty()) {
            errorMsg += " - " + message;
        }
        throw std::runtime_error(errorMsg);
    }
}

void Assert::IsNotNull(void* ptr, const std::string& message) {
    if (ptr == nullptr) {
        std::string errorMsg = "Assertion failed: pointer is null";
        if (!message.empty()) {
            errorMsg += " - " + message;
        }
        throw std::runtime_error(errorMsg);
    }
}

void Assert::Fail(const std::string& message) {
    throw std::runtime_error(message.empty() ? "Test failed" : message);
}

} // namespace DxilConvTest