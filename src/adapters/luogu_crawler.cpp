#include "shuati/crawler.hpp"
#include <cpr/cpr.h>
#include <nlohmann/json.hpp>
#include <regex>
#include <fmt/core.h>

namespace shuati {

class LuoguCrawler : public ICrawler {
public:
    bool can_handle(const std::string& url) const override {
        return url.find("luogu.com.cn") != std::string::npos;
    }

    Problem fetch_problem(const std::string& url) override {
        Problem p;
        p.source = "web";
        p.url = url;
        
        // Extract problem ID from URL (e.g. https://www.luogu.com.cn/problem/P1001)
        std::regex id_re(R"(/problem/([A-Z0-9]+))");
        std::smatch m;
        std::string pid;
        if (std::regex_search(url, m, id_re)) {
            pid = m[1].str();
            p.id = "lg_" + pid;
        } else {
            p.id = "lg_unknown";
            p.title = "Unknown Problem";
            return p;
        }

        // Try Luogu API: https://www.luogu.com.cn/problem/<PID>?_contentOnly=1
        try {
            auto r = cpr::Get(
                cpr::Url{url},
                cpr::Parameters{{"_contentOnly", "1"}},
                cpr::Header{
                    {"User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/91.0.4472.124 Safari/537.36"}
                },
                cpr::Timeout{10000}
            );

            if (r.status_code == 200) {
                auto json = nlohmann::json::parse(r.text);
                if (json["code"] == 200) {
                    auto data = json["currentData"]["problem"];
                    p.title = data.value("title", "Untitled");
                    
                    int diff = data.value("difficulty", 0);
                    // Luogu difficulty: 0=入门, 1=普及-, 2=普及/提高-, 3=普及+/提高, 4=提高+/省选-, 5=省选/NOI-, 6=NOI/NOI+/CTSC, 7=暂无
                    if (diff <= 1) p.difficulty = "easy";
                    else if (diff <= 3) p.difficulty = "medium";
                    else p.difficulty = "hard";

                    if (data.contains("tags") && data["tags"].is_array()) {
                        std::vector<std::string> tags;
                        for (auto& tag : data["tags"]) {
                            // Tag can be object or ID? Luogu tags are objects usually
                            // Actually currentData.problem.tags might be IDs, 
                            // but let's check definition. 
                            // Assuming simple tag structure or skip for stability
                        }
                    }
                    return p;
                }
            }
        } catch (...) {
            // Fallback to HTML parsing if API fails
        }

        // HTML Fallback
        try {
           auto r = cpr::Get(cpr::Url{url}, cpr::Timeout{10000});
           if (r.status_code == 200) {
               std::regex title_re(R"(<title>(.*?)</title>)");
               if (std::regex_search(r.text, m, title_re)) {
                   p.title = m[1].str();
                   auto pos = p.title.find(" - ");
                   if (pos != std::string::npos) p.title = p.title.substr(0, pos);
               }
           }
        } catch (...) {}

        if (p.title.empty()) p.title = pid;
        
        return p;
    }

    std::vector<TestCase> fetch_test_cases(const std::string& url) override {
        // Luogu test cases are usually samples in description
        // API returns description in "content" or "description" field
        // Requires parsing markdown/HTML for "输入格式" "输出格式" "样例"
        // For MVP, we'll skip complex markdown parsing
        return {}; 
    }
};

} // namespace shuati
