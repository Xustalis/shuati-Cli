#include "shuati/adapters/lanqiao_crawler.hpp"
#include <cpr/cpr.h>
#include <fmt/core.h>
#include <regex>

namespace shuati {

bool LanqiaoCrawler::can_handle(const std::string& url) const {
    return url.find("lanqiao.cn") != std::string::npos;
}

Problem LanqiaoCrawler::fetch_problem(const std::string& url) {
    Problem p;
    p.source = "Lanqiao";
    p.url = url;
    
    try {
        auto r = cpr::Get(cpr::Url{url}, cpr::Timeout{10000});

        if (r.status_code != 200) {
            p.title = fmt::format("Fetch Failed (HTTP {})", r.status_code);
            return p;
        }

        std::regex re("<title>(.*?)</title>", std::regex::icase);
        std::smatch m;
        if (std::regex_search(r.text, m, re)) {
            p.title = m[1].str();
        } else {
            p.title = "Lanqiao Problem";
        }

        p.id = "lq_" + std::to_string(std::time(nullptr));

    } catch (const std::exception& e) {
        p.title = fmt::format("Error: {}", e.what());
    }

    return p;
}

std::vector<TestCase> LanqiaoCrawler::fetch_test_cases(const std::string& url) {
    return {};
}

} // namespace shuati
