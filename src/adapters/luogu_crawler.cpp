#include "shuati/adapters/luogu_crawler.hpp"
#include <cpr/cpr.h>
#include <fmt/core.h>
#include <regex>

namespace shuati {

bool LuoguCrawler::can_handle(const std::string& url) const {
    return url.find("luogu.com.cn") != std::string::npos;
}

Problem LuoguCrawler::fetch_problem(const std::string& url) {
    Problem p;
    p.source = "Luogu";
    p.url = url;
    
    try {
        // Luogu might need User-Agent
        auto r = cpr::Get(cpr::Url{url}, 
                          cpr::Header{{"User-Agent", "Mozilla/5.0"}},
                          cpr::Timeout{10000});
        
        if (r.status_code != 200) {
            p.title = fmt::format("Fetch Failed (HTTP {})", r.status_code);
            return p;
        }

        std::regex re("<title>(.*?)</title>", std::regex::icase);
        std::smatch m;
        if (std::regex_search(r.text, m, re)) {
            p.title = m[1].str();
        } else {
            p.title = "Luogu Problem";
        }
        
        // Try to extract ID from URL: luogu.com.cn/problem/P1001
        std::regex id_re(R"(problem/(P\d+))");
        if (std::regex_search(url, m, id_re)) {
            p.id = "lg_" + m[1].str();
        } else {
            p.id = "lg_" + std::to_string(std::time(nullptr));
        }

    } catch (const std::exception& e) {
        p.title = fmt::format("Error: {}", e.what());
    }

    return p;
}

std::vector<TestCase> LuoguCrawler::fetch_test_cases(const std::string& url) {
    return {};
}

} // namespace shuati
