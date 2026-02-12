#include "shuati/ai_coach.hpp"
#include <cpr/cpr.h>
#include <nlohmann/json.hpp>
#include <fmt/core.h>
#include <string_view>

namespace shuati {

AICoach::AICoach(const Config& cfg) : cfg_(cfg) {}

std::string AICoach::call_api(const std::string& system_prompt, const std::string& user_prompt, bool stream, std::function<void(std::string)> callback) {
    if (cfg_.api_key.empty()) {
        return "[Error] API key not set. Run: shuati config --api-key <YOUR_KEY>";
    }

    nlohmann::json body;
    body["model"] = cfg_.model;
    body["max_tokens"] = cfg_.max_tokens;
    body["temperature"] = 0.3;
    body["stream"] = stream;
    body["messages"] = nlohmann::json::array({
        {{"role", "system"}, {"content", system_prompt}},
        {{"role", "user"},   {"content", user_prompt}}
    });

    try {
        if (stream && callback) {
            auto url = cpr::Url{cfg_.api_base + "/chat/completions"};
            auto header = cpr::Header{
                {"Content-Type", "application/json"},
                {"Authorization", "Bearer " + cfg_.api_key}
            };
            auto body_str = cpr::Body{body.dump()};
            
            std::string buffer;
            
            auto r = cpr::Post(
                url,
                header,
                body_str,
                cpr::WriteCallback{
                    [&](std::string_view data, intptr_t) -> bool {
                        buffer.append(data.data(), data.size());
                        size_t pos = 0;
                        while ((pos = buffer.find('\n')) != std::string::npos) {
                            std::string line = buffer.substr(0, pos);
                            buffer.erase(0, pos + 1);

                            if (!line.empty() && line.back() == '\r') line.pop_back();
                            if (line.empty()) continue;

                            if (line.rfind("data: ", 0) == 0) {
                                std::string json_str = line.substr(6);
                                if (json_str == "[DONE]") return true;

                                try {
                                    auto j = nlohmann::json::parse(json_str);
                                    if (j.contains("choices") && !j["choices"].empty()) {
                                        auto& delta = j["choices"][0]["delta"];
                                        if (delta.contains("content")) {
                                            callback(delta["content"].get<std::string>());
                                        }
                                    }
                                } catch (...) {
                                }
                            }
                        }
                        return true;
                    }
                },
                cpr::Timeout{60000}
            );
            
            if (r.status_code != 200) {
                return fmt::format(fmt::runtime("[API Error] HTTP {}: {}"), r.status_code, r.text.substr(0, 200));
            }
            return "";
        } else {
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
                return fmt::format(fmt::runtime("[API Error] HTTP {}: {}"), r.status_code, r.text.substr(0, 200));
            }

            auto resp = nlohmann::json::parse(r.text);
            return resp["choices"][0]["message"]["content"].get<std::string>();
        }
    } catch (const std::exception& e) {
        return fmt::format(fmt::runtime("[Error] {}"), e.what());
    }
}

std::string AICoach::analyze(const std::string& problem_desc, const std::string& user_code, std::function<void(std::string)> callback) {
    std::string sys = "You are an algorithm coach. "
                      "Analyze the user's code for logical errors or provide a hint if they are stuck. "
                      "Do NOT give the full solution code. "
                      "Guide them with hints. Use Markdown. Use Chinese.";

    std::string user = fmt::format(
        "Problem Description:\n{}\n\nUser's Current Code:\n```\n{}\n```\n\nPlease provide a hint.",
        problem_desc.substr(0, 1000), user_code.substr(0, 2000));

    // For streaming, call_api returns error string if any, otherwise empty.
    std::string err = call_api(sys, user, true, callback);
    if (!err.empty() && callback) {
        callback(err);
    }
    return err;
}

std::string AICoach::analyze_sync(const std::string& problem_desc, const std::string& user_code) {
    // Fallback for non-streaming usage if any
     std::string sys = "You are an algorithm coach. Use Chinese.";
     std::string user = fmt::format("Problem:\n{}\n\nCode:\n```\n{}\n```", problem_desc, user_code);
     return call_api(sys, user, false, nullptr);
}

bool AICoach::enabled() const {
    return !cfg_.api_key.empty();
}

std::string AICoach::generate_template(const std::string& title, const std::string& desc, const std::string& lang) {
    std::string sys = "You are an algorithm contest expert. "
                      "Generate a starter code template for the given problem. "
                      "Include comments about potential algorithms (Time Complexity constraints). "
                      "Do NOT solve the problem, just set up IO and structure. "
                      "Use " + lang + ".";
    
    std::string user = fmt::format("Title: {}\nDescription:\n{}", title, desc.substr(0, 1000));
    return call_api(sys, user);
}

std::string AICoach::diagnose(const std::string& problem_desc, 
                              const std::string& user_code, 
                              const std::string& failure_info,
                              const std::string& user_history) {
    std::string sys = "You are a competitive programming coach. "
                      "Diagnose the user's Wrong Answer or Runtime Error. "
                      "I will provide the Problem, Code, Failed Test Case, and User's stats. "
                      "Explain WHY the code failed on this specific case. "
                      "Reference their past weaknesses if relevant. "
                      "Keep it concise (< 200 words). Use Chinese.";

    std::string user = fmt::format(
        "Problem:\n{}\n\nCode:\n```\n{}\n```\n\nFailed Case:\n{}\n\nUser History:\n{}",
        problem_desc.substr(0, 500), user_code.substr(0, 1000), failure_info, user_history);

    return call_api(sys, user);
}

} // namespace shuati
