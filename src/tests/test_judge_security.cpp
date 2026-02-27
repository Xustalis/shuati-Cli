#include <iostream>
#include <fstream>
#include <filesystem>
#include <cassert>
#include "shuati/judge.hpp"

using namespace shuati;

void test_security() {
    Judge judge;
    
    // 1. Test invalid characters in filename
    std::string malicious_path = "test\"; echo pwned; \".cpp";
    try {
        judge.prepare(malicious_path, "cpp");
        std::cerr << "Failed: Should have thrown exception for malicious path\n";
        exit(1);
    } catch (const std::exception& e) {
        std::string msg = e.what();
        if (msg.find("Invalid source file path") == std::string::npos) {
            std::cerr << "Failed: Unexpected exception message: " << msg << "\n";
            exit(1);
        }
        std::cout << "Caught expected security exception: " << msg << "\n";
    }

    // 2. Test valid path
    std::string safe_path = "safe_test.cpp";
    // Create a dummy file
    {
        std::ofstream f(safe_path);
        f << "int main() { return 0; }";
    }
    
    try {
        // This might fail if no compiler, but shouldn't throw security error
        // We just want to ensure it passes the security check
        // The compile function might throw "Compilation failed" or similar, 
        // but checking the *path* happens first.
        judge.prepare(safe_path, "cpp");
    } catch (const std::exception& e) {
        std::string msg = e.what();
        if (msg.find("Invalid source file path") != std::string::npos) {
             std::cerr << "Failed: Safe path rejected: " << msg << "\n";
             exit(1);
        }
        // Other errors (compilation failed) are fine for this test
    }
    
    try {
        std::filesystem::remove(safe_path);
    } catch(...) {}

    std::cout << "test_judge_security passed!\n";
}

int main() {
    test_security();
    return 0;
}
