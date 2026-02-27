#pragma once

#include "shuati/crawler.hpp"
#include <memory>

namespace shuati {

// Forward declaration
class LuoguCrawlerImpl;

class LuoguCrawler : public ICrawler {
public:
    LuoguCrawler();
    ~LuoguCrawler();

    bool can_handle(const std::string& url) const override;
    Problem fetch_problem(const std::string& url) override;
    std::vector<TestCase> fetch_test_cases(const std::string& url) override;

private:
    std::unique_ptr<LuoguCrawlerImpl> impl_;
};

} // namespace shuati
