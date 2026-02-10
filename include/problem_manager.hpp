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
    void delete_problem(int tid);
    void delete_problem(const std::string& id);

private:
    std::shared_ptr<Database> db_;
    std::string fetch_html(const std::string& url);
    Problem parse_html(const std::string& html, const std::string& url);
};

} // namespace shuati
