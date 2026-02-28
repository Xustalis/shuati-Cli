#pragma once

#include "shuati/crawler.hpp"
#include <memory>

namespace shuati {

// Forward declaration
class LanqiaoCrawlerImpl;

class LanqiaoCrawler : public ICrawler {
public:
  LanqiaoCrawler();
  ~LanqiaoCrawler();

  bool can_handle(const std::string &url) const override;
  Problem fetch_problem(const std::string &url) override;
  std::vector<TestCase> fetch_test_cases(const std::string &url) override;

  // Testability helpers
  static Problem parse_problem_from_html(const std::string &url,
                                         const std::string &html);

private:
  std::unique_ptr<LanqiaoCrawlerImpl> impl_;
};

} // namespace shuati
