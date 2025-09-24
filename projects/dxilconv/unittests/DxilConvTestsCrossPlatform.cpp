///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DxilConvTestsCrossPlatform.cpp                                            //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Cross-platform DxilConv tests for ARM64 macOS                            //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "DxilConvTestFramework.h"
#include "dxc/DxilContainer/DxilContainer.h"
#include "dxc/dxcapi.h"
#include "dxc/Support/Global.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Path.h"
#include "llvm/ADT/SmallString.h"
#include <regex>
#include <memory>

using namespace DxilConvTest;

// FileCheck implementation for test validation
class FileCheck {
public:
    struct CheckLine {
        enum Type { CHECK, CHECK_NOT, CHECK_DAG, CHECK_NEXT, CHECK_SAME };
        Type type;
        std::string pattern;
        int lineNum;
    };
    
    static bool RunCheck(const std::string& checkFile, 
                        const std::string& inputText,
                        std::string& errorMessage);
    
private:
    static std::vector<CheckLine> ParseCheckFile(const std::string& content);
    static bool MatchPattern(const std::string& pattern, const std::string& line);
    static std::string ExpandVariables(const std::string& pattern, 
                                      const std::map<std::string, std::string>& vars);
};

// Test configuration
class DxilConvTestConfig {
public:
    std::string dxilconvPath;
    std::string fxcPath;
    std::string dxbc2dxilPath;
    std::string optPath;
    std::string testDataDir;
    std::string outputDir;
    
    bool Initialize();
    std::string GetTestDataPath(const std::string& relativePath);
    bool FindTool(const std::string& toolName, std::string& toolPath);
};

// Base test class for DxilConv tests
class DxilConvTestBase : public TestCase {
public:
    DxilConvTestBase(const std::string& name) : TestCase(name) {}
    
protected:
    DxilConvTestConfig m_config;
    DynamicLibrary m_dxilconvLib;
    
    bool Setup() override;
    bool RunFileTest(const std::string& testFile);
    bool ProcessRunCommand(const std::string& command, 
                          const std::string& testDir,
                          std::map<std::string, std::string>& variables);
    std::string SubstituteVariables(const std::string& str,
                                   const std::map<std::string, std::string>& variables);
};

// Individual test implementations
class BatchDxbc2dxilTest : public DxilConvTestBase {
public:
    BatchDxbc2dxilTest() : DxilConvTestBase("BatchDxbc2dxil") {}
    bool Run() override;
};

class BatchDxbc2dxilAsmTest : public DxilConvTestBase {
public:
    BatchDxbc2dxilAsmTest() : DxilConvTestBase("BatchDxbc2dxilAsm") {}
    bool Run() override;
};

class BatchDxilCleanupTest : public DxilConvTestBase {
public:
    BatchDxilCleanupTest() : DxilConvTestBase("BatchDxilCleanup") {}
    bool Run() override;
};

class BatchNormalizeDxilTest : public DxilConvTestBase {
public:
    BatchNormalizeDxilTest() : DxilConvTestBase("BatchNormalizeDxil") {}
    bool Run() override;
};

class BatchScopeNestIteratorTest : public DxilConvTestBase {
public:
    BatchScopeNestIteratorTest() : DxilConvTestBase("BatchScopeNestIterator") {}
    bool Run() override;
};

class RegressionTestsTest : public DxilConvTestBase {
public:
    RegressionTestsTest() : DxilConvTestBase("RegressionTests") {}
    bool Run() override;
};

//////////////////////////////////////////////////////////////////////////////
// FileCheck Implementation
//////////////////////////////////////////////////////////////////////////////

bool FileCheck::RunCheck(const std::string& checkFile, 
                        const std::string& inputText,
                        std::string& errorMessage) {
    std::string checkContent = FileSystem::ReadFile(checkFile);
    auto checks = ParseCheckFile(checkContent);
    
    if (checks.empty()) {
        errorMessage = "No CHECK directives found in " + checkFile;
        return false;
    }
    
    std::istringstream input(inputText);
    std::string line;
    std::vector<std::string> lines;
    
    while (std::getline(input, line)) {
        lines.push_back(line);
    }
    
    size_t currentLine = 0;
    std::map<std::string, std::string> variables;
    
    for (const auto& check : checks) {
        bool found = false;
        std::string pattern = ExpandVariables(check.pattern, variables);
        
        switch (check.type) {
            case CheckLine::CHECK:
                // Find pattern starting from current position
                for (size_t i = currentLine; i < lines.size(); ++i) {
                    if (MatchPattern(pattern, lines[i])) {
                        found = true;
                        currentLine = i + 1;
                        break;
                    }
                }
                break;
                
            case CheckLine::CHECK_NEXT:
                // Pattern must match on the next line
                if (currentLine < lines.size() && 
                    MatchPattern(pattern, lines[currentLine])) {
                    found = true;
                    currentLine++;
                }
                break;
                
            case CheckLine::CHECK_NOT:
                // Pattern must not appear before next CHECK
                found = true; // Assume success
                for (size_t i = currentLine; i < lines.size(); ++i) {
                    if (MatchPattern(pattern, lines[i])) {
                        found = false;
                        errorMessage = "CHECK-NOT: found forbidden pattern: " + pattern;
                        break;
                    }
                }
                break;
                
            case CheckLine::CHECK_DAG:
                // Pattern can match in any order
                for (size_t i = 0; i < lines.size(); ++i) {
                    if (MatchPattern(pattern, lines[i])) {
                        found = true;
                        break;
                    }
                }
                break;
                
            case CheckLine::CHECK_SAME:
                // Pattern must match on the same line as previous match
                if (currentLine > 0 && currentLine <= lines.size() &&
                    MatchPattern(pattern, lines[currentLine - 1])) {
                    found = true;
                }
                break;
        }
        
        if (!found && check.type != CheckLine::CHECK_NOT) {
            std::stringstream ss;
            ss << checkFile << ":" << check.lineNum << ": error: ";
            
            switch (check.type) {
                case CheckLine::CHECK:
                    ss << "CHECK: ";
                    break;
                case CheckLine::CHECK_NEXT:
                    ss << "CHECK-NEXT: ";
                    break;
                case CheckLine::CHECK_DAG:
                    ss << "CHECK-DAG: ";
                    break;
                case CheckLine::CHECK_SAME:
                    ss << "CHECK-SAME: ";
                    break;
                default:
                    break;
            }
            
            ss << "expected string not found in input: " << pattern;
            errorMessage = ss.str();
            return false;
        }
    }
    
    return true;
}

std::vector<FileCheck::CheckLine> FileCheck::ParseCheckFile(const std::string& content) {
    std::vector<CheckLine> checks;
    std::istringstream stream(content);
    std::string line;
    int lineNum = 0;
    
    // Regex patterns for different check types
    std::regex checkRegex(R"(.*//\s*CHECK:\s*(.+))");
    std::regex checkNextRegex(R"(.*//\s*CHECK-NEXT:\s*(.+))");
    std::regex checkNotRegex(R"(.*//\s*CHECK-NOT:\s*(.+))");
    std::regex checkDagRegex(R"(.*//\s*CHECK-DAG:\s*(.+))");
    std::regex checkSameRegex(R"(.*//\s*CHECK-SAME:\s*(.+))");
    
    while (std::getline(stream, line)) {
        lineNum++;
        std::smatch match;
        
        if (std::regex_match(line, match, checkRegex)) {
            checks.push_back({CheckLine::CHECK, match[1].str(), lineNum});
        } else if (std::regex_match(line, match, checkNextRegex)) {
            checks.push_back({CheckLine::CHECK_NEXT, match[1].str(), lineNum});
        } else if (std::regex_match(line, match, checkNotRegex)) {
            checks.push_back({CheckLine::CHECK_NOT, match[1].str(), lineNum});
        } else if (std::regex_match(line, match, checkDagRegex)) {
            checks.push_back({CheckLine::CHECK_DAG, match[1].str(), lineNum});
        } else if (std::regex_match(line, match, checkSameRegex)) {
            checks.push_back({CheckLine::CHECK_SAME, match[1].str(), lineNum});
        }
    }
    
    return checks;
}

bool FileCheck::MatchPattern(const std::string& pattern, const std::string& line) {
    // Simple substring match for now
    // TODO: Implement regex patterns with {{.*}} syntax
    return line.find(pattern) != std::string::npos;
}

std::string FileCheck::ExpandVariables(const std::string& pattern,
                                      const std::map<std::string, std::string>& vars) {
    std::string result = pattern;
    
    for (const auto& var : vars) {
        std::string placeholder = "[[" + var.first + "]]";
        size_t pos = 0;
        while ((pos = result.find(placeholder, pos)) != std::string::npos) {
            result.replace(pos, placeholder.length(), var.second);
            pos += var.second.length();
        }
    }
    
    return result;
}

//////////////////////////////////////////////////////////////////////////////
// DxilConvTestConfig Implementation
//////////////////////////////////////////////////////////////////////////////

bool DxilConvTestConfig::Initialize() {
    // Get executable directory
    std::string exeDir = FileSystem::GetExecutableDirectory();
    
    // Look for dxilconv library
#ifdef _WIN32
    dxilconvPath = FileSystem::JoinPath(exeDir, "dxilconv.dll");
#elif defined(__APPLE__)
    dxilconvPath = FileSystem::JoinPath(exeDir, "libdxilconv.dylib");
#else
    dxilconvPath = FileSystem::JoinPath(exeDir, "libdxilconv.so");
#endif
    
    if (!FileSystem::FileExists(dxilconvPath)) {
        std::cerr << "DxilConv library not found at: " << dxilconvPath << std::endl;
        return false;
    }
    
    // Find test tools
    if (!FindTool("fxc", fxcPath)) {
        // Try to find fxc in SDK paths
        std::vector<std::string> searchPaths = {
            exeDir,
            "/usr/local/bin",
            "/opt/homebrew/bin"
        };
        
        // Check environment variable
        const char* sdkPath = std::getenv("WIN10_SDK_PATH");
        if (sdkPath) {
            searchPaths.push_back(std::string(sdkPath) + "/x64");
        }
        
        bool found = false;
        for (const auto& path : searchPaths) {
            std::string fxcExe = FileSystem::JoinPath(path, "fxc");
#ifdef _WIN32
            fxcExe += ".exe";
#endif
            if (FileSystem::FileExists(fxcExe)) {
                fxcPath = fxcExe;
                found = true;
                break;
            }
        }
        
        if (!found) {
            std::cerr << "Warning: fxc not found, some tests may fail" << std::endl;
        }
    }
    
    FindTool("dxbc2dxil", dxbc2dxilPath);
    FindTool("opt", optPath);
    
    // Find test data directory
    std::string currentDir = FileSystem::GetCurrentDirectory();
    std::vector<std::string> testDataSearchPaths = {
        FileSystem::JoinPath(currentDir, "test"),
        FileSystem::JoinPath(currentDir, "../test"),
        FileSystem::JoinPath(currentDir, "../../test"),
        FileSystem::JoinPath(exeDir, "../test"),
        FileSystem::JoinPath(exeDir, "../../test")
    };
    
    for (const auto& path : testDataSearchPaths) {
        if (FileSystem::DirectoryExists(path)) {
            testDataDir = path;
            break;
        }
    }
    
    if (testDataDir.empty()) {
        std::cerr << "Warning: Test data directory not found" << std::endl;
        // Use current directory as fallback
        testDataDir = currentDir;
    }
    
    // Create output directory
    outputDir = FileSystem::JoinPath(currentDir, "test_output");
    FileSystem::CreateDirectory(outputDir);
    
    return true;
}

std::string DxilConvTestConfig::GetTestDataPath(const std::string& relativePath) {
    return FileSystem::JoinPath(testDataDir, relativePath);
}

bool DxilConvTestConfig::FindTool(const std::string& toolName, std::string& toolPath) {
    std::string exeDir = FileSystem::GetExecutableDirectory();
    
    std::string toolExe = toolName;
#ifdef _WIN32
    toolExe += ".exe";
#endif
    
    toolPath = FileSystem::JoinPath(exeDir, toolExe);
    if (FileSystem::FileExists(toolPath)) {
        return true;
    }
    
    // Try PATH environment
    const char* pathEnv = std::getenv("PATH");
    if (pathEnv) {
        std::string pathStr(pathEnv);
        size_t start = 0;
        size_t end = pathStr.find(':');
        
        while (end != std::string::npos) {
            std::string dir = pathStr.substr(start, end - start);
            std::string fullPath = FileSystem::JoinPath(dir, toolExe);
            
            if (FileSystem::FileExists(fullPath)) {
                toolPath = fullPath;
                return true;
            }
            
            start = end + 1;
            end = pathStr.find(':', start);
        }
        
        // Check last segment
        std::string dir = pathStr.substr(start);
        std::string fullPath = FileSystem::JoinPath(dir, toolExe);
        if (FileSystem::FileExists(fullPath)) {
            toolPath = fullPath;
            return true;
        }
    }
    
    return false;
}

//////////////////////////////////////////////////////////////////////////////
// DxilConvTestBase Implementation
//////////////////////////////////////////////////////////////////////////////

bool DxilConvTestBase::Setup() {
    if (!m_config.Initialize()) {
        return false;
    }
    
    // Load dxilconv library
    if (!m_dxilconvLib.Load(m_config.dxilconvPath)) {
        std::cerr << "Failed to load dxilconv library" << std::endl;
        return false;
    }
    
    return true;
}

bool DxilConvTestBase::RunFileTest(const std::string& testFile) {
    std::string content = FileSystem::ReadFile(testFile);
    
    // Parse RUN commands
    std::istringstream stream(content);
    std::string line;
    std::vector<std::string> runCommands;
    
    while (std::getline(stream, line)) {
        if (line.find("// RUN:") == 0 || line.find("//RUN:") == 0) {
            size_t pos = line.find("RUN:");
            if (pos != std::string::npos) {
                runCommands.push_back(line.substr(pos + 4));
            }
        }
    }
    
    if (runCommands.empty()) {
        return true; // No RUN commands, test passes
    }
    
    // Set up variables
    std::map<std::string, std::string> variables;
    variables["%s"] = testFile;
    variables["%t"] = FileSystem::JoinPath(m_config.outputDir, 
                                          FileSystem::GetFileName(testFile) + ".tmp");
    variables["%b"] = FileSystem::JoinPath(FileSystem::GetDirectory(testFile),
                                          FileSystem::GetFileName(testFile));
    
    // Remove extension from %b
    size_t dotPos = variables["%b"].rfind('.');
    if (dotPos != std::string::npos) {
        variables["%b"] = variables["%b"].substr(0, dotPos);
    }
    
    variables["%fxc"] = m_config.fxcPath;
    variables["%dxbc2dxil"] = m_config.dxbc2dxilPath;
    variables["%opt-exe"] = m_config.optPath;
    
    // Process each RUN command
    for (const auto& cmd : runCommands) {
        if (!ProcessRunCommand(cmd, FileSystem::GetDirectory(testFile), variables)) {
            return false;
        }
    }
    
    return true;
}

bool DxilConvTestBase::ProcessRunCommand(const std::string& command,
                                        const std::string& testDir,
                                        std::map<std::string, std::string>& variables) {
    std::string cmd = SubstituteVariables(command, variables);
    
    // Trim whitespace
    size_t start = cmd.find_first_not_of(" \t");
    if (start != std::string::npos) {
        cmd = cmd.substr(start);
    }
    
    // Parse command and arguments
    std::vector<std::string> parts;
    std::stringstream ss(cmd);
    std::string part;
    
    while (ss >> part) {
        parts.push_back(part);
    }
    
    if (parts.empty()) {
        return true;
    }
    
    // Handle special commands
    if (parts[0] == "fc" || parts[0] == "diff") {
        // File comparison
        if (parts.size() < 3) {
            std::cerr << "Invalid file comparison command" << std::endl;
            return false;
        }
        
        std::string file1 = parts[1];
        std::string file2 = parts[2];
        
        std::string content1 = FileSystem::ReadFile(file1);
        std::string content2 = FileSystem::ReadFile(file2);
        
        if (content1 != content2) {
            std::cerr << "Files differ: " << file1 << " vs " << file2 << std::endl;
            return false;
        }
        
        return true;
    }
    
    // Execute external command
    std::string executable = parts[0];
    std::vector<std::string> args(parts.begin() + 1, parts.end());
    
    Process::Result result = Process::Execute(executable, args, testDir);
    
    if (result.exitCode != 0) {
        std::cerr << "Command failed: " << cmd << std::endl;
        std::cerr << "Exit code: " << result.exitCode << std::endl;
        if (!result.stderr.empty()) {
            std::cerr << "Error output: " << result.stderr << std::endl;
        }
        return false;
    }
    
    // Save output if redirected
    for (size_t i = 0; i < args.size(); ++i) {
        if (args[i] == "/o" || args[i] == "-o" || args[i] == ">") {
            if (i + 1 < args.size()) {
                FileSystem::WriteFile(args[i + 1], result.stdout);
            }
            break;
        }
    }
    
    return true;
}

std::string DxilConvTestBase::SubstituteVariables(const std::string& str,
                                                 const std::map<std::string, std::string>& variables) {
    std::string result = str;
    
    for (const auto& var : variables) {
        size_t pos = 0;
        while ((pos = result.find(var.first, pos)) != std::string::npos) {
            result.replace(pos, var.first.length(), var.second);
            pos += var.second.length();
        }
    }
    
    return result;
}

//////////////////////////////////////////////////////////////////////////////
// Test Implementations
//////////////////////////////////////////////////////////////////////////////

bool BatchDxbc2dxilTest::Run() {
    std::string testDir = m_config.GetTestDataPath("dxbc2dxil");
    auto files = FileSystem::ListFiles(testDir, ".hlsl", false);
    
    int passed = 0;
    int failed = 0;
    
    for (const auto& file : files) {
        std::cout << "    Testing: " << FileSystem::GetFileName(file) << " ... ";
        
        if (RunFileTest(file)) {
            std::cout << "PASS" << std::endl;
            passed++;
        } else {
            std::cout << "FAIL" << std::endl;
            failed++;
        }
    }
    
    std::cout << "    Results: " << passed << " passed, " << failed << " failed" << std::endl;
    return failed == 0;
}

bool BatchDxbc2dxilAsmTest::Run() {
    std::string testDir = m_config.GetTestDataPath("dxbc2dxil-asm");
    auto files = FileSystem::ListFiles(testDir, ".asm", false);
    
    int passed = 0;
    int failed = 0;
    
    for (const auto& file : files) {
        std::cout << "    Testing: " << FileSystem::GetFileName(file) << " ... ";
        
        if (RunFileTest(file)) {
            std::cout << "PASS" << std::endl;
            passed++;
        } else {
            std::cout << "FAIL" << std::endl;
            failed++;
        }
    }
    
    std::cout << "    Results: " << passed << " passed, " << failed << " failed" << std::endl;
    return failed == 0;
}

bool BatchDxilCleanupTest::Run() {
    std::string testDir = m_config.GetTestDataPath("dxil_cleanup");
    auto files = FileSystem::ListFiles(testDir, ".ll", false);
    
    // Save and change directory for this test
    std::string savedDir = FileSystem::GetCurrentDirectory();
    FileSystem::SetCurrentDirectory(testDir);
    
    int passed = 0;
    int failed = 0;
    
    for (const auto& file : files) {
        std::cout << "    Testing: " << FileSystem::GetFileName(file) << " ... ";
        
        if (RunFileTest(file)) {
            std::cout << "PASS" << std::endl;
            passed++;
        } else {
            std::cout << "FAIL" << std::endl;
            failed++;
        }
    }
    
    // Restore directory
    FileSystem::SetCurrentDirectory(savedDir);
    
    std::cout << "    Results: " << passed << " passed, " << failed << " failed" << std::endl;
    return failed == 0;
}

bool BatchNormalizeDxilTest::Run() {
    std::string testDir = m_config.GetTestDataPath("normalize_dxil");
    auto files = FileSystem::ListFiles(testDir, ".ll", false);
    
    int passed = 0;
    int failed = 0;
    
    for (const auto& file : files) {
        std::cout << "    Testing: " << FileSystem::GetFileName(file) << " ... ";
        
        if (RunFileTest(file)) {
            std::cout << "PASS" << std::endl;
            passed++;
        } else {
            std::cout << "FAIL" << std::endl;
            failed++;
        }
    }
    
    std::cout << "    Results: " << passed << " passed, " << failed << " failed" << std::endl;
    return failed == 0;
}

bool BatchScopeNestIteratorTest::Run() {
    std::string testDir = m_config.GetTestDataPath("scope_nest_iterator");
    auto files = FileSystem::ListFiles(testDir, ".ll", false);
    
    int passed = 0;
    int failed = 0;
    
    for (const auto& file : files) {
        std::cout << "    Testing: " << FileSystem::GetFileName(file) << " ... ";
        
        if (RunFileTest(file)) {
            std::cout << "PASS" << std::endl;
            passed++;
        } else {
            std::cout << "FAIL" << std::endl;
            failed++;
        }
    }
    
    std::cout << "    Results: " << passed << " passed, " << failed << " failed" << std::endl;
    return failed == 0;
}

bool RegressionTestsTest::Run() {
    std::string testDir = m_config.GetTestDataPath("regression_tests");
    auto files = FileSystem::ListFiles(testDir, ".hlsl", false);
    
    int passed = 0;
    int failed = 0;
    
    for (const auto& file : files) {
        std::cout << "    Testing: " << FileSystem::GetFileName(file) << " ... ";
        
        if (RunFileTest(file)) {
            std::cout << "PASS" << std::endl;
            passed++;
        } else {
            std::cout << "FAIL" << std::endl;
            failed++;
        }
    }
    
    std::cout << "    Results: " << passed << " passed, " << failed << " failed" << std::endl;
    return failed == 0;
}

//////////////////////////////////////////////////////////////////////////////
// Main test runner
//////////////////////////////////////////////////////////////////////////////

int main(int argc, char* argv[]) {
    std::cout << "DxilConv Cross-Platform Test Suite" << std::endl;
    std::cout << "===================================" << std::endl;
    std::cout << "Platform: " <<
#ifdef _WIN32
        "Windows"
#elif defined(__APPLE__)
        "macOS ARM64"
#elif defined(__linux__)
        "Linux"
#else
        "Unknown"
#endif
        << std::endl << std::endl;
    
    // Parse command line arguments
    bool verbose = false;
    bool parallel = false;
    std::string testFilter = "";
    std::string outputDir = "";
    
    for (int i = 1; i < argc; ++i) {
        std::string arg(argv[i]);
        
        if (arg == "-v" || arg == "--verbose") {
            verbose = true;
        } else if (arg == "-p" || arg == "--parallel") {
            parallel = true;
        } else if (arg == "-f" || arg == "--filter") {
            if (i + 1 < argc) {
                testFilter = argv[++i];
            }
        } else if (arg == "-o" || arg == "--output") {
            if (i + 1 < argc) {
                outputDir = argv[++i];
            }
        } else if (arg == "-h" || arg == "--help") {
            std::cout << "Usage: " << argv[0] << " [options]" << std::endl;
            std::cout << "Options:" << std::endl;
            std::cout << "  -v, --verbose    Enable verbose output" << std::endl;
            std::cout << "  -p, --parallel   Run tests in parallel" << std::endl;
            std::cout << "  -f, --filter     Filter tests by name" << std::endl;
            std::cout << "  -o, --output     Output directory for results" << std::endl;
            std::cout << "  -h, --help       Show this help message" << std::endl;
            return 0;
        }
    }
    
    // Configure test environment
    TestConfig::Instance().SetVerbose(verbose);
    TestConfig::Instance().SetParallel(parallel);
    
    if (!outputDir.empty()) {
        TestConfig::Instance().SetOutputDirectory(outputDir);
    }
    
    // Create test runner
    TestRunner runner;
    
    // Create test suite
    auto suite = std::make_unique<TestSuite>("DxilConv");
    
    // Add tests
    if (testFilter.empty() || testFilter == "BatchDxbc2dxil") {
        suite->AddTest(std::make_unique<BatchDxbc2dxilTest>());
    }
    if (testFilter.empty() || testFilter == "BatchDxbc2dxilAsm") {
        suite->AddTest(std::make_unique<BatchDxbc2dxilAsmTest>());
    }
    if (testFilter.empty() || testFilter == "BatchDxilCleanup") {
        suite->AddTest(std::make_unique<BatchDxilCleanupTest>());
    }
    if (testFilter.empty() || testFilter == "BatchNormalizeDxil") {
        suite->AddTest(std::make_unique<BatchNormalizeDxilTest>());
    }
    if (testFilter.empty() || testFilter == "BatchScopeNestIterator") {
        suite->AddTest(std::make_unique<BatchScopeNestIteratorTest>());
    }
    if (testFilter.empty() || testFilter == "RegressionTests") {
        suite->AddTest(std::make_unique<RegressionTestsTest>());
    }
    
    runner.AddSuite(std::move(suite));
    
    // Run tests
    runner.RunAll();
    
    // Generate report
    runner.GenerateReport();
    
    // Return exit code
    return runner.GetFailedCount() > 0 ? 1 : 0;
}