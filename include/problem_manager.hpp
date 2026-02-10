#pragma once

#include <string>
#include <vector>
#include <memory>
#include "database.hpp"

namespace shuati {

class ProblemManager {
public:
    explicit ProblemManager(std::shared_ptr<Database> db);

    void pull_problem(const std::string& url);
    void create_local(const std::string& title, const std::string& tags, const std::string& difficulty);
    Problem get_problem(const std::string& id);
    std::vector<Problem> list_problems();

private:
    std::shared_ptr<Database> db_;
    std::string fetch_html(const std::string& url);
    Problem parse_html(const std::string& html, const std::string& url);
};

} // namespace shuati
