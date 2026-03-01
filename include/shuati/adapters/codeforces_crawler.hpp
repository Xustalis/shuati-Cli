#pragma once

#include "shuati/adapters/base_crawler.hpp"
#include <vector>
#include <string>
#include <nlohmann/json.hpp>

namespace shuati {

class CodeforcesCrawler : public BaseCrawler {
public:
    using BaseCrawler::BaseCrawler; // Inherit constructor

    bool can_handle(const std::string& url) const override;
    Problem fetch_problem(const std::string& url) override;
    std::vector<TestCase> fetch_test_cases(const std::string& url) override;
};

} // namespace shuati
