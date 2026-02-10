#include "mistake_analyzer.hpp"
#include <fmt/core.h>

namespace shuati {

MistakeAnalyzer::MistakeAnalyzer(std::shared_ptr<Database> db) : db_(db) {}

const std::vector<std::string>& MistakeAnalyzer::mistake_types() {
    static const std::vector<std::string> types = {
        "Off-by-one",
        "Wrong state design",
        "Greedy misuse",
        "DP transition error",
        "TLE / complexity",
        "Edge case missed",
        "Wrong data structure",
        "Implementation bug",
        "Misread problem",
        "Other"
    };
    return types;
}

void MistakeAnalyzer::log_mistake(const std::string& pid, const std::string& type, const std::string& desc) {
    db_->log_mistake(pid, type, desc);
    fmt::print("[+] Mistake logged: [{}] {}\n", type, desc);
}

std::vector<Mistake> MistakeAnalyzer::get_mistakes(const std::string& pid) {
    return db_->get_mistakes_for(pid);
}

std::vector<std::pair<std::string, int>> MistakeAnalyzer::get_stats() {
    return db_->get_mistake_stats();
}

} // namespace shuati
