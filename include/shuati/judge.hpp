#pragma once

#include <string>
#include <vector>
#include <optional>
#include "shuati/types.hpp"

namespace shuati {

class Judge {
public:
    Judge() = default;
    
    std::string prepare(const std::string& source_file, const std::string& language);
    JudgeResult run_prepared(const std::string& executable,
                             const TestCase& tc,
                             int time_limit_ms = 1000,
                             int memory_limit_kb = 256 * 1024);
    void cleanup_prepared(const std::string& executable, const std::string& language);

    // Compile and run solution against test cases
    // returns results for each test case
    std::vector<JudgeResult> judge(const std::string& source_file, 
                                   const std::string& language, 
                                   const std::vector<TestCase>& test_cases,
                                   int time_limit_ms = 1000,
                                   int memory_limit_kb = 256 * 1024);

    // Run a process and redirect I/O to files
    JudgeResult run_process_redirect(const std::string& cmd, 
                                     const std::string& input_file, 
                                     const std::string& output_file, 
                                     int time_limit_ms, 
                                     int memory_limit_kb);

    // Efficiently compare two files (return true if identical, ignoring trailing whitespace)
    bool stream_file_diff(const std::string& file1, const std::string& file2);



private:
    std::string compile(const std::string& source_file, const std::string& language);
    JudgeResult run_case(const std::string& executable, 
                         const TestCase& tc, 
                         int time_limit_ms, 
                         int memory_limit_kb);
    
    Verdict check_output(const std::string& user_out, const std::string& expected_out);
};

} // namespace shuati
