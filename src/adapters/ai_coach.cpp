#include "shuati/ai_coach.hpp"
#include "shuati/memory_manager.hpp"
#include <cpr/cpr.h>
#include <nlohmann/json.hpp>
#include <fmt/core.h>
#include <string_view>
#include <regex>

namespace shuati {

AICoach::AICoach(const Config& cfg, MemoryManager* mm) : cfg_(cfg), mm_(mm) {}

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

                cpr::Timeout{120000}
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
    // For streaming, call_api returns error string if any, otherwise empty.
    // Increase timeout for long Chain of Thought
    std::string sys = 
        "你是一位严厉但负责的算法竞赛教练。你的目标是培养用户独立解决问题的能力。\n"
        "【重中之重】\n"
        "1. **绝不** 直接给出完整代码，只提供关键思路或局部代码片段（<10行）。\n"
        "2. **绝不** 复述用户已提供的正确代码，只指出问题。\n"
        "3. **必须** 检查用户代码的边界条件、数据范围匹配度和时间复杂度。\n"
        "4. **必须** 识别用户是否已掌握某个知识点，如果是，则跳过基础讲解。\n"
        "\n"
        "请按以下格式回复（支持 Markdown）：\n"
        "## 💡 核心思路\n"
        "(简要点拨，不超过3点)\n\n"
        "## 🔍 问题诊断\n"
        "(指出具体逻辑错误或潜在风险)\n\n"
        "## 🛠️ 修改建议\n"
        "(分步指导，包含代码片段)\n\n"
        "<!-- SYSTEM_OP: UPDATE_MEMORY\n"
        "{ \"new_mistake\": \"...\", \"mastery_reinforce\": \"...\" }\n"
        "-->";

    if (mm_) {
        // RAG-Lite: Inject memory context
        std::string memory_context = mm_->get_relevant_context("");
        if (!memory_context.empty()) {
            sys += "\n\n" + memory_context;
        }
    }

    std::string user = fmt::format(
        "题目信息：\n{}\n\n用户代码：\n```cpp\n{}\n```",
        problem_desc.substr(0, 8000),
        user_code.substr(0, 6000));

    std::string full_response;
    auto wrapped = [&](std::string chunk) {
        full_response += chunk;
        if (callback) callback(chunk);
    };
    
    std::string err = call_api(sys, user, true, wrapped);
    
    if (err.empty() && mm_) {
        mm_->update_memory_from_response(full_response);
    }
    
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

std::string AICoach::chat_sync(const std::string& system_prompt, const std::string& user_prompt) {
    return call_api(system_prompt, user_prompt, false, nullptr);
}

std::string AICoach::diagnose(const std::string& problem_desc, 
                              const std::string& user_code, 
                              const std::string& failure_info,
                              const std::string& user_history,
                              std::function<void(std::string)> callback) {
    std::string sys = "You are a competitive programming coach. "
                      "Diagnose the user's Wrong Answer or Runtime Error. "
                      "I will provide the Problem, Code, Failed Test Case, and User's stats. "
                      "Explain WHY the code failed on this specific case. "
                      "Reference their past weaknesses if relevant. "
                      "Keep it concise (< 200 words). Use Chinese.\n"
                      "Finally, if you identify a specific mistake pattern, output it in the system block:\n"
                      "<!-- SYSTEM_OP: UPDATE_MEMORY\n"
                      "{ \"new_mistake\": \"...\", \"mastery_reinforce\": \"...\" }\n"
                      "-->";

    std::string user = fmt::format(
        "Problem:\n{}\n\nCode:\n```\n{}\n```\n\nFailed Case:\n{}\n\nUser History:\n{}",
        problem_desc.substr(0, 1000), user_code.substr(0, 2000), failure_info, user_history);

    std::string full_response;
    auto wrapped = [&](std::string chunk) {
        full_response += chunk;
        if (callback) callback(chunk);
    };

    std::string err = call_api(sys, user, true, wrapped);
    
    if (err.empty() && mm_) {
        mm_->update_memory_from_response(full_response);
    }
    
    if (!err.empty() && callback) {
        callback(err);
    }

    return err;
}

std::pair<std::string, std::string> AICoach::generate_test_scripts(const std::string& problem_desc) {
    // Heuristic: Extract Input/Output/Constraints to reduce token usage
    // This is a simple substring extraction, can be improved with regex
    std::string limited_desc = problem_desc;
    if (problem_desc.length() > 2000) {
        // Try to find key sections
        std::vector<std::string> keys = {"Input", "Output", "Constraints", "输入", "输出", "数据范围"};
        std::string extracted;
        for (const auto& key : keys) {
            size_t pos = problem_desc.find(key);
            if (pos != std::string::npos) {
                // Take 500 chars after key
                extracted += problem_desc.substr(pos, 500) + "\n...\n";
            }
        }
        if (!extracted.empty()) limited_desc = extracted;
        else limited_desc = problem_desc.substr(0, 1500); // Fallback truncate
    }

    std::string sys = 
        "You are an algorithm testing expert. "
        "Generate two Python scripts based on the problem description.\n"
        "1. `gen.py`: Prints a random valid test case to STDOUT. MUST use `random`. NO INPUT reading. \n"
        "2. `sol.py`: Reads test case from STDIN, prints correct answer to STDOUT.\n"
        "Output ONLY the code blocks, wrapped in ```python ... ```. \n"
        "Mark them clearly as `gen.py` and `sol.py`.\n"
        "Constraint: Read the description CAREFULLY. If problem says 'two integers', generate exactly two.\n"
        "Do NOT assume the first line is N unless the problem explicitly says so.\n"
        "Robustness: `sol.py` must handle large input efficiently.";

    std::string user = "Problem:\n" + limited_desc;

    std::string raw = call_api(sys, user);
    
    // Parse response
    std::pair<std::string, std::string> result;
    
    auto extract_code = [&](const std::string& text, const std::string& marker) -> std::string {
        // Find marker (gen.py or sol.py), then find next ```python block
        size_t p_mark = text.find(marker);
        if (p_mark == std::string::npos) return "";
        size_t p_start = text.find("```", p_mark);
        if (p_start == std::string::npos) return "";
        size_t p_code_start = text.find('\n', p_start);
        if (p_code_start == std::string::npos) return "";
        size_t p_end = text.find("```", p_code_start);
        if (p_end == std::string::npos) return "";
        return text.substr(p_code_start + 1, p_end - p_code_start - 1);
    };

    // If models don't follow "gen.py" marker strictly, try just finding two blocks
    // But let's try strict first
    result.first = extract_code(raw, "gen.py");
    result.second = extract_code(raw, "sol.py");

    // Fallback: Just grab first two blocks if named extraction fails
    if (result.first.empty() || result.second.empty()) {
        // Regex to match ```python ... ``` or just ``` ... ```
        // Captures content inside the block
        std::regex re("```(?:python)?\\s*([\\s\\S]*?)```");
        std::sregex_iterator next(raw.begin(), raw.end(), re);
        std::sregex_iterator end;
        
        int count = 0;
        while (next != end && count < 2) {
            if (count == 0) result.first = next->str(1);
            else result.second = next->str(1);
            next++;
            count++;
        }
    }

    return result;

}

} // namespace shuati
