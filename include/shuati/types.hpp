#pragma once

#include <string>
#include <vector>

namespace shuati {

struct Problem {
    int display_id = 0;   // 1-based numeric ID (TID)
    std::string id;       // UUID or original string ID
    std::string source;   // "web", "local"
    std::string title;
    std::string description; // Raw problem description (HTML or Markdown)
    std::string url;
    std::string content_path;
    std::string tags;      // comma-separated
    std::string difficulty; // "easy", "medium", "hard"
    long long created_at = 0;
    
    // Status
    std::string last_verdict;
    int pass_count = 0;
    int total_count = 0;
    long long last_checked_at = 0;
};

struct Mistake {
    int id = 0;
    std::string problem_id;
    std::string type;        // "Logic", "Boundary", "Greedy", "DP", "TLE"
    std::string description;
    long long timestamp = 0;
};

struct MemoryMistake {
    int id = 0;
    std::string tags;       // e.g. "dp,array"
    std::string pattern;    // e.g. "Forget to init DP array"
    int frequency = 0;
    long long last_seen = 0;
    std::string example_id; // Problem ID of a typical example
};

struct Mastery {
    int id = 0;
    std::string skill;          // e.g. "Binary Search"
    double confidence = 0.0;    // 0.0 - 100.0
    long long last_verified = 0;
};

struct UserProfile {
    int elo_rating = 1200;
    std::string preferences;    // JSON
};

struct ReviewItem {
    std::string problem_id;
    std::string title;
    long long next_review = 0;
    int interval = 1;       // days
    double ease_factor = 2.5;
    int repetitions = 0;
};

struct TestCase {
    std::string input;
    std::string output;
    bool is_sample = true;  // Sample cases vs. full test cases
};

enum class Verdict {
    AC,  // Accepted
    WA,  // Wrong Answer
    TLE, // Time Limit Exceeded
    MLE, // Memory Limit Exceeded
    RE,  // Runtime Error
    CE,  // Compilation Error
    SE   // System Error
};

struct JudgeResult {
    Verdict verdict;
    int time_ms;
    int memory_kb;
    std::string message;
    std::string error_output;
    std::string input;
    std::string output;
    std::string expected;
    
    std::string verdict_str() const;
};

struct TestReport {
    std::string problem_id;
    long long timestamp;
    std::string verdict;
    int pass_count;
    int total_count;
    std::vector<JudgeResult> cases;
};

inline std::string JudgeResult::verdict_str() const {
    switch (verdict) {
        case Verdict::AC: return "AC";
        case Verdict::WA: return "WA";
        case Verdict::TLE: return "TLE";
        case Verdict::MLE: return "MLE";
        case Verdict::RE: return "RE";
        case Verdict::CE: return "CE";
        case Verdict::SE: return "SE";
        default: return "UNKNOWN";
    }
}

} // namespace shuati
