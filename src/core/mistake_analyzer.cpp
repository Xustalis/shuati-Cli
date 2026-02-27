#include "shuati/mistake_analyzer.hpp"
#include <fmt/core.h>

namespace shuati {

MistakeAnalyzer::MistakeAnalyzer(std::shared_ptr<Database> db) : db_(db) {}

const std::vector<std::string>& MistakeAnalyzer::mistake_types() {
    static const std::vector<std::string> types = {
        "Off-by-one (边界错误)",
        "状态定义错误",
        "贪心策略误用",
        "DP 转移方程错误",
        "时间/空间复杂度过高 (TLE/MLE)",
        "漏掉边缘情况 (Edge Case)",
        "数据结构选择不当",
        "代码实现 BUG (逻辑错误)",
        "读题/理解偏差",
        "其他"
    };
    return types;
}

void MistakeAnalyzer::log_mistake(const std::string& pid, const std::string& type, const std::string& desc) {
    db_->log_mistake(pid, type, desc);
    fmt::print("[+] 已记录错误类型: {}\n", type);
}

std::vector<Mistake> MistakeAnalyzer::get_mistakes(const std::string& pid) {
    return db_->get_mistakes_for(pid);
}

std::vector<std::pair<std::string, int>> MistakeAnalyzer::get_stats() {
    return db_->get_mistake_stats();
}

} // namespace shuati
