#pragma once

#include <string>
#include "shuati/config.hpp"

namespace shuati {

class AICoach {
public:
    explicit AICoach(const Config& cfg);

    // Check if AI is enabled (has key)
    bool enabled() const;

    // Send code + error context to AI, get coaching hint (not full solution)
    std::string get_hint(const std::string& problem_desc, const std::string& user_code, const std::string& mistake_type);

    // Analyze code for common mistakes
    std::string analyze(const std::string& problem_desc, const std::string& user_code);

    // Diagnose failure with test case and history
    std::string diagnose(const std::string& problem_desc, 
                         const std::string& user_code, 
                         const std::string& failure_info,
                         const std::string& user_history);

    // Generate starter code with algorithm hints
    std::string generate_template(const std::string& title, const std::string& desc, const std::string& lang);

private:
    Config cfg_;

    // Call DeepSeek API
    std::string call_api(const std::string& system_prompt, const std::string& user_prompt);
};

} // namespace shuati
