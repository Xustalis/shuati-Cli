#pragma once
#include <string>
#include <vector>

namespace shuati {

struct Diagnosis {
    std::string title;
    std::string description;
    std::string suggestion;
};

class CompilerDoctor {
public:
    // Analyze compiler output and return a friendly diagnosis
    static Diagnosis diagnose(const std::string& error_output);
};

} // namespace shuati
