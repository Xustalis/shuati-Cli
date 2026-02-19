#pragma once

#include <string>
#include <vector>
#include <functional>
#include "shuati/config.hpp"

namespace shuati {

class MemoryManager;

class AICoach {
public:
    explicit AICoach(const Config& cfg, MemoryManager* mm = nullptr);

    // Check if cloud AI is enabled (has key)
    bool enabled() const;

    // Check if local model server is reachable
    bool local_enabled() const;

    // ─── Core AI Methods ──────────────────────────────────

    // Streaming analysis (hint command)
    std::string analyze(const std::string& problem_desc,
                 const std::string& user_code,
                 std::function<void(std::string)> callback);

    // Legacy non-streaming
    std::string analyze_sync(const std::string& problem_desc, const std::string& user_code);

    // Diagnose a WA / RE / TLE failure with test case context
    std::string diagnose(const std::string& problem_desc,
                         const std::string& user_code,
                         const std::string& failure_info,
                         const std::string& user_history,
                         std::function<void(std::string)> callback = nullptr);

    // ─── Compile Error Diagnosis (fallback chain) ─────────
    // Chain: compiler_errors.json rules → local model → cloud API
    // Returns the diagnosis text (or empty if all fail silently)
    std::string diagnose_compile_error(
        const std::string& error_output,
        const std::string& source_code,
        std::function<void(std::string)> stream_callback = nullptr);

    // ─── Utilities ────────────────────────────────────────
    std::string generate_template(const std::string& title, const std::string& desc, const std::string& lang);
    std::string chat_sync(const std::string& system_prompt, const std::string& user_prompt);
    std::pair<std::string, std::string> generate_test_scripts(const std::string& problem_desc);

    // ─── Model Management ─────────────────────────────────
    // Returns available models from active endpoint (local or cloud)
    std::vector<std::string> list_models();

    // ─── Local Server Management ──────────────────────────
    // Try to start local llama-server if configured and not running.
    // actual_url is set to the URL where the server was started (may differ in port).
    bool start_local_server(std::string& actual_url);

private:
    Config cfg_;
    MemoryManager* mm_ = nullptr;
    mutable std::string resolved_local_url_; // Port-resolved URL

    // Cloud API call (OpenAI-compatible, uses cfg_.api_key / api_base)
    std::string call_api(const std::string& system_prompt, const std::string& user_prompt,
                         bool stream = false, std::function<void(std::string)> callback = nullptr);

    // Local model call (llama.cpp server / OpenAI-compatible HTTP, no auth)
    std::string call_local(const std::string& system_prompt, const std::string& user_prompt,
                           std::function<void(std::string)> callback = nullptr);

    // Internal: resolve and cache the local server URL (handles port conflicts)
    std::string get_local_url() const;
};

} // namespace shuati
