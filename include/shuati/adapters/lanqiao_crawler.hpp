#pragma once

#include "shuati/crawler.hpp"
#include <memory>
#include "shuati/http_client.hpp"

namespace shuati {

// Forward declaration
class LanqiaoCrawlerImpl;

class LanqiaoCrawler : public ICrawler {
public:
    explicit LanqiaoCrawler(std::shared_ptr<IHttpClient> client = nullptr);
    ~LanqiaoCrawler();

    bool can_handle(const std::string& url) const override;
    Problem fetch_problem(const std::string& url) override;
    std::vector<TestCase> fetch_test_cases(const std::string& url) override;

private:
    std::unique_ptr<LanqiaoCrawlerImpl> impl_;
};

} // namespace shuati
