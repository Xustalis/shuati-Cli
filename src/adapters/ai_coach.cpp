#include "shuati/ai_coach.hpp"
#include <cpr/cpr.h>
#include <nlohmann/json.hpp>
#include <fmt/core.h>

namespace shuati {

AICoach::AICoach(const Config& cfg) : cfg_(cfg) {}

std::string AICoach::call_api(const std::string& system_prompt, const std::string& user_prompt) {
    if (cfg_.api_key.empty()) {
        return "[Error] API key not set. Run: shuati config --api-key <YOUR_KEY>";
    }

    nlohmann::json body;
    body["model"] = cfg_.model;
    body["max_tokens"] = cfg_.max_tokens;
    body["temperature"] = 0.3;
    body["messages"] = nlohmann::json::array({
        {{"role", "system"}, {"content", system_prompt}},
        {{"role", "user"},   {"content", user_prompt}}
    });

    try {
        auto r = cpr::Post(
            cpr::Url{cfg_.api_base + "/chat/completions"},
            cpr::Header{
                {"Content-Type", "application/json"},
                {"Authorization", "Bearer " + cfg_.api_key}
            },
            cpr::Body{body.dump()},
            cpr::Timeout{30000}
        );

        if (r.status_code != 200) {
            return fmt::format("[API Error] HTTP {}: {}", r.status_code, r.text.substr(0, 200));
        }

        auto resp = nlohmann::json::parse(r.text);
        return resp["choices"][0]["message"]["content"].get<std::string>();
    } catch (const std::exception& e) {
        return fmt::format("[Error] {}", e.what());
    }
}

std::string AICoach::get_hint(const std::string& problem_desc, const std::string& user_code, const std::string& mistake_type) {
    std::string sys = "You are a strict algorithm coach. "
                      "NEVER give the full solution. "
                      "Only point out WHERE the mistake is and WHY it's wrong. "
                      "Keep your response under 150 words. Use Chinese.";

    std::string user = fmt::format(
        "Problem:\n{}\n\nMy Code:\n```\n{}\n```\n\nMistake Type: {}\n\nPlease give me a hint.",
        problem_desc.substr(0, 500), user_code.substr(0, 1000), mistake_type);

    return call_api(sys, user);
}

std::string AICoach::analyze(const std::string& problem_desc, const std::string& user_code) {
    std::string sys = "You are an algorithm coach. "
                      "Analyze the code for common mistakes (off-by-one, wrong state, greedy misuse, TLE). "
                      "Do NOT give the solution. Only identify the error category and give a brief hint. "
                      "Keep response under 100 words. Use Chinese.";

    std::string user = fmt::format(
        "Problem:\n{}\n\nCode:\n```\n{}\n```",
        problem_desc.substr(0, 500), user_code.substr(0, 1000));

    return call_api(sys, user);
}

} // namespace shuati
