#include "shuati/adapters/lanqiao_crawler.hpp"
#include <fmt/core.h>

namespace shuati {

bool LanqiaoCrawler::can_handle(const std::string& url) const {
    return url.find("lanqiao.cn") != std::string::npos;
}

Problem LanqiaoCrawler::fetch_problem(const std::string& url) {
    Problem p;
    p.source = "Lanqiao";
    p.url = url;
    p.title = "Lanqiao Crawler Not Implemented";
    return p;
}

std::vector<TestCase> LanqiaoCrawler::fetch_test_cases(const std::string& url) {
    return {};
}

} // namespace shuati
