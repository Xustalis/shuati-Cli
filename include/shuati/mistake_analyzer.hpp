#pragma once

#include <string>
#include <vector>
#include <memory>
#include <utility>
#include "shuati/database.hpp"

namespace shuati {

class MistakeAnalyzer {
public:
    explicit MistakeAnalyzer(std::shared_ptr<Database> db);

    void log_mistake(const std::string& problem_id, const std::string& type, const std::string& desc);
    std::vector<Mistake> get_mistakes(const std::string& problem_id);
    std::vector<std::pair<std::string, int>> get_stats();

    // Predefined mistake types
    static const std::vector<std::string>& mistake_types();

private:
    std::shared_ptr<Database> db_;
};

} // namespace shuati
