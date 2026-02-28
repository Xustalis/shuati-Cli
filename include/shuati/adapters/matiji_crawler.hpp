#pragma once

#include "shuati/crawler.hpp"
#include <memory>

namespace shuati {

class MatijiCrawlerImpl;

class MatijiCrawler : public ICrawler {
public:
    MatijiCrawler();
    ~MatijiCrawler();

    bool can_handle(const std::string& url) const override;
    Problem fetch_problem(const std::string& url) override;
    std::vector<TestCase> fetch_test_cases(const std::string& url) override;
    // Testability helpers
    static std::string extract_problem_id_from_url(const std::string& url);
    static std::vector<TestCase> parse_test_cases_from_html(const std::string& html);

private:
    std::unique_ptr<MatijiCrawlerImpl> impl_;
};

} // namespace shuati
