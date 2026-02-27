#include "shuati/judge.hpp"
#include <iostream>
#include <vector>
#include <cassert>
#include <string>
#include <fmt/core.h>
#include <chrono>
#include <filesystem>
#include <fstream>

using namespace shuati;

void test_large_output() {
    std::cout << "[Test] Large Output Deadlock Check..." << std::endl;
    // Create a program that prints line by line until 1MB
    std::string code = R"(
#include <iostream>
#include <string>
int main() {
    std::string s(1024, 'a');
    // Print 1024 times -> 1MB
    for(int i=0; i<1024; ++i) {
        std::cout << s; // no newline to keep it dense
    }
    return 0;
}
    )";
    
    // Write source
    std::ofstream src("large_out.cpp");
    src << code;
    src.close();

    Judge judge;
    TestCase tc;
    tc.input = "";
    tc.output = ""; // We don't care about WA here, just SE/TLE/RE
    tc.is_sample = false;

    // Time limit 5s, should be enough for 1MB print
    auto results = judge.judge("large_out.cpp", "cpp", {tc}, 5000, 256*1024);

    if (results.empty()) {
        std::cerr << "FAIL: No results returned" << std::endl;
        std::filesystem::remove("large_out.cpp");
        exit(1);
    }

    auto res = results[0];
    if (res.verdict == Verdict::TLE) {
        std::cerr << "FAIL: TLE - Deadlock might still be present" << std::endl;
        std::filesystem::remove("large_out.cpp");
        #ifdef _WIN32
        std::filesystem::remove("large_out.exe");
        #else
        std::filesystem::remove("large_out");
        #endif
        exit(1);
    }
    
    if (res.output.size() < 1024 * 1024) {
        std::cerr << "FAIL: Output truncated. Got " << (unsigned long long)res.output.size() << " bytes." << std::endl;
        std::cerr << "Captured content (head): " << res.output.substr(0, 100) << std::endl;
        std::cerr << "Captured error output: " << res.error_output << std::endl;
        std::filesystem::remove("large_out.cpp");
        #ifdef _WIN32
        std::filesystem::remove("large_out.exe");
        #else
        std::filesystem::remove("large_out");
        #endif
        exit(1);
    }

    std::cout << "PASS: Captured " << (unsigned long long)res.output.size() << " bytes." << std::endl;
    
    // Cleanup
    std::filesystem::remove("large_out.cpp");
    #ifdef _WIN32
    std::filesystem::remove("large_out.exe");
    #else
    std::filesystem::remove("large_out");
    #endif
}

int main() {
    try {
        test_large_output();
        std::cout << "All Judge Complex Tests Passed!" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
