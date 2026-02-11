#include "shuati/adapters/luogu_crawler.hpp"
#include <fmt/core.h>

namespace shuati {

bool LuoguCrawler::can_handle(const std::string& url) const {
    return url.find("luogu.com.cn") != std::string::npos;
}

Problem LuoguCrawler::fetch_problem(const std::string& url) {
    Problem p;
    p.source = "Luogu";
    p.url = url;
    p.title = "Luogu Crawler Not Implemented";
    return p;
}

std::vector<TestCase> LuoguCrawler::fetch_test_cases(const std::string& url) {
    return {};
}

} // namespace shuati
