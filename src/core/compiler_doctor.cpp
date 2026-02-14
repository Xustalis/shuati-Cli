#include "shuati/compiler_doctor.hpp"
#include <regex>
#include <fstream>
#include <fmt/core.h>
#include <nlohmann/json.hpp>

namespace shuati {

// Static members
std::vector<DiagnosticRule> CompilerDoctor::rules_;
bool CompilerDoctor::rules_loaded_ = false;

bool CompilerDoctor::load_rules(const std::string& json_path) {
    try {
        std::ifstream f(json_path);
        if (!f.is_open()) {
            return false;
        }
        
        nlohmann::json j = nlohmann::json::parse(f);
        rules_.clear();
        
        for (const auto& rule_json : j) {
            DiagnosticRule rule;
            rule.id = rule_json.value("id", "");
            rule.regex_pattern = rule_json.value("regex", "");
            rule.severity = rule_json.value("severity", "error");
            rule.title = rule_json.value("title", "未知错误");
            rule.description = rule_json.value("description", "");
            rule.suggestion = rule_json.value("suggestion", "");
            
            if (!rule.regex_pattern.empty()) {
                rules_.push_back(rule);
            }
        }
        
        rules_loaded_ = true;
        return true;
    } catch (const std::exception& e) {
        fmt::print("[!] 加载诊断规则失败: {}\n", e.what());
        return false;
    }
}

std::string CompilerDoctor::replace_placeholders(const std::string& template_str, const std::smatch& matches) {
    std::string result = template_str;
    
    // Replace {1}, {2}, etc. with captured groups
    for (size_t i = 1; i < matches.size(); i++) {
        std::string placeholder = fmt::format("{{{}}}", i);
        std::string replacement = matches[i].str();
        
        size_t pos = 0;
        while ((pos = result.find(placeholder, pos)) != std::string::npos) {
            result.replace(pos, placeholder.length(), replacement);
            pos += replacement.length();
        }
    }
    
    return result;
}

Diagnosis CompilerDoctor::diagnose(const std::string& error_output) {
    Diagnosis d;
    d.title = "未知错误";
    d.description = "无法识别的编译错误，请检查代码逻辑。";
    d.suggestion = "请尝试阅读上方的英文报错信息，或者使用 'hint' 命令询问 AI。";

    // Load rules on first use if not already loaded
    if (!rules_loaded_) {
        // Try to load from multiple possible locations
        std::vector<std::string> possible_paths = {
            "resources/rules/compiler_errors.json",
            "../resources/rules/compiler_errors.json",
            "../../resources/rules/compiler_errors.json"
        };
        
        for (const auto& path : possible_paths) {
            if (load_rules(path)) {
                break;
            }
        }
    }

    // Try to match against loaded rules
    for (const auto& rule : rules_) {
        try {
            std::regex pattern(rule.regex_pattern, std::regex::icase);
            std::smatch matches;
            
            if (std::regex_search(error_output, matches, pattern)) {
                d.title = replace_placeholders(rule.title, matches);
                d.description = replace_placeholders(rule.description, matches);
                d.suggestion = replace_placeholders(rule.suggestion, matches);
                return d;
            }
        } catch (const std::regex_error&) {
            // Skip invalid regex patterns
            continue;
        }
    }

    return d;
}


bool CompilerDoctor::check_environment(std::vector<std::string>& missing_tools) {
    missing_tools.clear();
    
#ifdef _WIN32
    const char* null_dev = "nul";
#else
    const char* null_dev = "/dev/null";
#endif

    // Check g++
    std::string cmd_cpp = fmt::format("g++ --version > {} 2>&1", null_dev);
    if (std::system(cmd_cpp.c_str()) != 0) {
        missing_tools.push_back("g++ (MinGW/GCC)");
    }
    
    // Check python
    std::string cmd_py = fmt::format("python --version > {} 2>&1", null_dev);
    if (std::system(cmd_py.c_str()) != 0) {
        // Try python3
        std::string cmd_py3 = fmt::format("python3 --version > {} 2>&1", null_dev);
        if (std::system(cmd_py3.c_str()) != 0) {
            missing_tools.push_back("python");
        }
    }
    
    return missing_tools.empty();
}

} // namespace shuati
