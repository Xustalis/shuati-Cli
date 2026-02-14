#pragma once
#include <string>
#include <vector>
#include <regex>
#include <optional>

namespace shuati {

struct Diagnosis {
    std::string title;
    std::string description;
    std::string suggestion;
};

struct DiagnosticRule {
    std::string id;
    std::string regex_pattern;
    std::string severity;
    std::string title;
    std::string description;
    std::string suggestion;
};

class CompilerDoctor {
public:
    // Load rules from JSON file
    static bool load_rules(const std::string& json_path);
    
    // Analyze compiler output and return a friendly diagnosis
    static Diagnosis diagnose(const std::string& error_output);

    // Check availability of external tools (g++, python)
    static bool check_environment(std::vector<std::string>& missing_tools);

private:
    static std::vector<DiagnosticRule> rules_;
    static bool rules_loaded_;
    
    // Helper to replace {N} placeholders in strings with regex matches
    static std::string replace_placeholders(const std::string& template_str, const std::smatch& matches);
};

} // namespace shuati
