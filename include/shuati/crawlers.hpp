#pragma once

#include "shuati/crawler.hpp"
#include <memory>

namespace shuati {

class LeetCodeCrawler : public ICrawler {
public:
    bool can_handle(const std::string& url) const override;
    Problem fetch_problem(const std::string& url) override;
    std::vector<TestCase> fetch_test_cases(const std::string& url) override;
};

class CodeforcesCrawler : public ICrawler {
public:
    bool can_handle(const std::string& url) const override;
    Problem fetch_problem(const std::string& url) override;
    std::vector<TestCase> fetch_test_cases(const std::string& url) override;
};

class LuoguCrawler : public ICrawler {
public:
    bool can_handle(const std::string& url) const override;
    Problem fetch_problem(const std::string& url) override;
    std::vector<TestCase> fetch_test_cases(const std::string& url) override;
};

class LanqiaoCrawler : public ICrawler {
public:
    bool can_handle(const std::string& url) const override;
    Problem fetch_problem(const std::string& url) override;
    std::vector<TestCase> fetch_test_cases(const std::string& url) override;
};

} // namespace shuati
