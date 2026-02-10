#pragma once

#include <string>
#include "config.hpp"

namespace shuati {

class AICoach {
public:
    explicit AICoach(const Config& cfg);

    // Send code + error context to AI, get coaching hint (not full solution)
    std::string get_hint(const std::string& problem_desc, const std::string& user_code, const std::string& mistake_type);

    // Analyze code for common mistakes
    std::string analyze(const std::string& problem_desc, const std::string& user_code);

private:
    Config cfg_;

    // Call DeepSeek API
    std::string call_api(const std::string& system_prompt, const std::string& user_prompt);
};

} // namespace shuati
