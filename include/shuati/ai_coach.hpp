#pragma once

#include <string>
#include <vector>
#include "shuati/config.hpp"

namespace shuati {

class MemoryManager;

class AICoach {
public:
    explicit AICoach(const Config& cfg, MemoryManager* mm = nullptr);

    // Check if AI is enabled (has key)
    bool enabled() const;

    // Send code + error context to AI, get coaching hint (not full solution)
    std::string get_hint(const std::string& problem_desc, const std::string& user_code, const std::string& mistake_type);

    // Send code + error context to AI, get coaching hint (not full solution)
    // Stream hint to stdout or callback
    std::string analyze(const std::string& problem_desc,
                 const std::string& user_code,
                 std::function<void(std::string)> callback);

    // Legacy non-streaming (deprecated but kept for compatibility if needed)
    std::string analyze_sync(const std::string& problem_desc, const std::string& user_code);

    // Diagnose failure with test case and history
    std::string diagnose(const std::string& problem_desc,
                         const std::string& user_code,
                         const std::string& failure_info,
                         const std::string& user_history,
                         std::function<void(std::string)> callback = nullptr);

    // Generate starter code with algorithm hints
    std::string generate_template(const std::string& title, const std::string& desc, const std::string& lang);

    std::string chat_sync(const std::string& system_prompt, const std::string& user_prompt);

    // Generate Python scripts for stress testing (gen.py and sol.py)
    // Returns a pair: {gen_code, sol_code}
    std::pair<std::string, std::string> generate_test_scripts(const std::string& problem_desc);

private:
    Config cfg_;
    MemoryManager* mm_ = nullptr;

    // Call DeepSeek API
    std::string call_api(const std::string& system_prompt, const std::string& user_prompt, bool stream = false, std::function<void(std::string)> callback = nullptr);
};

} // namespace shuati
