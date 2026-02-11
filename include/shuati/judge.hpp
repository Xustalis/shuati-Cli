#pragma once

#include <string>
#include <vector>
#include <optional>
#include "shuati/database.hpp"
#include "shuati/crawler.hpp"

namespace shuati {

enum class Verdict {
    AC,  // Accepted
    WA,  // Wrong Answer
    TLE, // Time Limit Exceeded
    MLE, // Memory Limit Exceeded
    RE,  // Runtime Error
    CE,  // Compilation Error
    SE   // System Error
};

struct JudgeResult {
    Verdict verdict;
    int time_ms;
    int memory_kb;
    std::string message;
    std::string error_output;
    std::string input;
    std::string output;
    std::string expected;
    
    std::string verdict_str() const;
};

class Judge {
public:
    Judge() = default;
    
    // Compile and run solution against test cases
    // returns results for each test case
    std::vector<JudgeResult> judge(const std::string& source_file, 
                                   const std::string& language, 
                                   const std::vector<TestCase>& test_cases,
                                   int time_limit_ms = 1000,
                                   int memory_limit_kb = 256 * 1024);

private:
    std::string compile(const std::string& source_file, const std::string& language);
    JudgeResult run_case(const std::string& executable, 
                         const TestCase& tc, 
                         int time_limit_ms, 
                         int memory_limit_kb);
    
    Verdict check_output(const std::string& user_out, const std::string& expected_out);
};

} // namespace shuati
