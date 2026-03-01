#pragma once

#include "shuati/crawler.hpp"
#include <memory>
#include "shuati/http_client.hpp"

namespace shuati {

class LuoguCrawlerImpl;

class LuoguCrawler : public ICrawler {
public:
    explicit LuoguCrawler(std::shared_ptr<IHttpClient> client = nullptr);
    ~LuoguCrawler();

    bool can_handle(const std::string& url) const override;
    Problem fetch_problem(const std::string& url) override;
    std::vector<TestCase> fetch_test_cases(const std::string& url) override;

private:
    std::unique_ptr<LuoguCrawlerImpl> impl_;
};

} // namespace shuati
