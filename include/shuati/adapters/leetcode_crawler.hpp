#pragma once
#include "shuati/crawler.hpp"
#include <vector>
#include <string>

namespace shuati {

class LeetCodeCrawler : public ICrawler {
public:
    bool can_handle(const std::string& url) const override;
    Problem fetch_problem(const std::string& url) override;
    std::vector<TestCase> fetch_test_cases(const std::string& url) override;
};

} // namespace shuati
