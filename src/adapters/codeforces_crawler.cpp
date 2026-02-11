#include "shuati/adapters/codeforces_crawler.hpp"
#include <cpr/cpr.h>
#include <fmt/core.h>
#include <regex>

namespace shuati {

bool CodeforcesCrawler::can_handle(const std::string& url) const {
    return url.find("codeforces.com") != std::string::npos;
}

Problem CodeforcesCrawler::fetch_problem(const std::string& url) {
    Problem p;
    p.source = "Codeforces";
    p.url = url;
    
    // Generic Fetch & Parse
    try {
        auto r = cpr::Get(cpr::Url{url}, cpr::Timeout{10000});
        if (r.status_code != 200) {
            p.title = fmt::format("Fetch Failed (HTTP {})", r.status_code);
            return p;
        }

        // Extract title
        std::regex re("<title>(.*?)</title>", std::regex::icase);
        std::smatch m;
        if (std::regex_search(r.text, m, re)) {
            p.title = m[1].str();
            // Cleanup title (e.g. "Problem - A - Codeforces" -> "Problem - A")
            auto pos = p.title.find(" - Codeforces");
            if (pos != std::string::npos) p.title = p.title.substr(0, pos);
        } else {
            p.title = "Unknown Problem";
        }
        
        // Extract ID from URL if possible
        // https://codeforces.com/problemset/problem/1500/A
        std::regex id_re(R"(problem/(\d+)/(\w+))");
        if (std::regex_search(url, m, id_re)) {
            p.id = "cf_" + m[1].str() + m[2].str();
            p.display_id = 0; 
        } else {
            p.id = "cf_" + std::to_string(std::time(nullptr));
        }

    } catch (const std::exception& e) {
        p.title = fmt::format("Error: {}", e.what());
    }

    return p;
}

std::vector<TestCase> CodeforcesCrawler::fetch_test_cases(const std::string& url) {
    // Advanced parsing for CF inputs/outputs can be added later
    // For now, return empty to not break flow
    return {};
}

} // namespace shuati
