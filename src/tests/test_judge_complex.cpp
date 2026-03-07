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

void test_mle() {
    std::cout << "[Test] Memory Limit Exceeded (MLE) Check..." << std::endl;
    std::string code = R"(
#include <iostream>
#include <vector>
int main() {
    std::vector<char*> ptrs;
    while(true) {
        // Allocate smaller chunks (1MB) to ensure we hit the 90% threshold
        // before std::bad_alloc is thrown prematurely
        char* p = new char[1 * 1024 * 1024];
        // Touch memory to force page allocation (increasing RSS on Linux)
        for (int i = 0; i < 1 * 1024 * 1024; i += 4096) {
            p[i] = 1;
        }
        ptrs.push_back(p);
    }
    return 0;
}
    )";
    
    std::ofstream src("mle_test.cpp");
    src << code;
    src.close();

    Judge judge;
    TestCase tc;
    tc.input = ""; tc.output = ""; tc.is_sample = false;

    // 64MB limit
    auto results = judge.judge("mle_test.cpp", "cpp", {tc}, 5000, 64 * 1024);

    if (results.empty()) {
        std::cerr << "FAIL: MLE Test - No results returned" << std::endl;
        exit(1);
    }
    
    auto res = results[0];
    if (res.verdict != Verdict::MLE) {
        std::cerr << "FAIL: MLE Test - Expected MLE, but got " << (int)res.verdict << std::endl;
        std::cerr << "Message: " << res.message << std::endl;
        std::cerr << "Memory: " << res.memory_kb << " KB" << std::endl;
        exit(1);
    }
    
    std::cout << "PASS: MLE caught successfully. Peak Memory: " << res.memory_kb << " KB" << std::endl;
    std::filesystem::remove("mle_test.cpp");
    #ifdef _WIN32
    std::filesystem::remove("mle_test.exe");
    #else
    std::filesystem::remove("mle_test");
    #endif
}


int main() {
    try {
        test_large_output();
        test_mle();
        std::cout << "All Judge Complex Tests Passed!" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
