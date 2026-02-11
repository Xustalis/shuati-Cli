#include "shuati/crawler.hpp"
#include <cpr/cpr.h>
#include <regex>
#include <fmt/core.h>

namespace shuati {

class LanqiaoCrawler : public ICrawler {
public:
    bool can_handle(const std::string& url) const override {
        return url.find("lanqiao.cn") != std::string::npos;
    }

    Problem fetch_problem(const std::string& url) override {
        Problem p;
        p.source = "web";
        p.url = url;
        
        // ID: https://www.lanqiao.cn/problems/123/learning/ -> 123
        std::regex id_re(R"(/problems/(\d+))");
        std::smatch m;
        if (std::regex_search(url, m, id_re)) {
            p.id = "lq_" + m[1].str();
        } else {
            p.id = "lq_unknown";
        }

        try {
            auto r = cpr::Get(
                cpr::Url{url},
                cpr::Timeout{10000},
                cpr::Header{
                    {"User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/91.0.4472.124 Safari/537.36"}
                }
            );

            if (r.status_code == 200) {
                // Lanqiao is SPA (Single Page App), title might be in <title> or meta tags.
                // <title>题目详情 - 蓝桥云课</title> usually generic.
                // But let's try to find title in <title> first.
                // Or maybe in <meta property="og:title" content="...">
                
                std::regex title_re(R"(<title>(.*?)</title>)");
                if (std::regex_search(r.text, m, title_re)) {
                    p.title = m[1].str();
                    // Clean up suffix
                    auto pos = p.title.find(" - ");
                    if (pos != std::string::npos) p.title = p.title.substr(0, pos);
                }

                if (p.title.empty() || p.title == "蓝桥云课") {
                     // Try to find specific problem data if embedded in JS
                     // window.__NUXT__ or similar?
                     // For now, if we can't find it, just use generic ID
                     p.title = "Lanqiao Problem " + (p.id.size() > 3 ? p.id.substr(3) : "?");
                }
            }
        } catch (...) {
             p.title = "Network Error";
        }
        
        // Tags and difficulty are hard to parse from SPA HTML without executing JS
        p.difficulty = "medium";
        
        return p;
    }

    std::vector<TestCase> fetch_test_cases(const std::string& url) override {
        // Hard to get test cases from SPA without API
        return {}; 
    }
};

} // namespace shuati
