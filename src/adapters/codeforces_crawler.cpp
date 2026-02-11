#include "shuati/adapters/codeforces_crawler.hpp"
#include <fmt/core.h>

namespace shuati {

bool CodeforcesCrawler::can_handle(const std::string& url) const {
    return url.find("codeforces.com") != std::string::npos;
}

Problem CodeforcesCrawler::fetch_problem(const std::string& url) {
    Problem p;
    p.source = "Codeforces";
    p.url = url;
    p.title = "Codeforces Crawler Not Implemented";
    return p;
}

std::vector<TestCase> CodeforcesCrawler::fetch_test_cases(const std::string& url) {
    return {};
}

} // namespace shuati
