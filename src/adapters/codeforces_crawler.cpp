#include "shuati/crawler.hpp"
#include <cpr/cpr.h>
#include <regex>
#include <fmt/core.h>

namespace shuati {

class CodeforcesCrawler : public ICrawler {
public:
    bool can_handle(const std::string& url) const override {
        return url.find("codeforces.com") != std::string::npos;
    }

    Problem fetch_problem(const std::string& url) override {
        Problem p;
        p.source = "web";
        p.url = url;
        
        try {
            auto r = cpr::Get(
                cpr::Url{url},
                cpr::Timeout{10000}
            );

            if (r.status_code != 200) {
                p.title = fmt::format("HTTP {} Error", r.status_code);
                return p;
            }

            std::string html = r.text;
            
            // Extract problem title from <div class="title">
            std::regex title_re(R"(<div[^>]*class="title"[^>]*>([^<]+)</div>)");
            std::smatch m;
            if (std::regex_search(html, m, title_re)) {
                p.title = trim(m[1].str());
                
                // Extract problem ID (e.g., "A. Two Sum")
                std::regex id_re(R"(^([A-Z][0-9]?)\.?\s+)");
                std::smatch id_m;
                if (std::regex_search(p.title, id_m, id_re)) {
                    p.id = "cf_" + id_m[1].str();
                } else {
                    // Fallback: extract from URL (e.g., /problemset/problem/1234/A)
                    std::regex url_re(R"(/problem/(\d+)/([A-Z][0-9]?))");
                    if (std::regex_search(url, id_m, url_re)) {
                        p.id = "cf_" + id_m[1].str() + id_m[2].str();
                    } else {
                        p.id = "cf_unknown";
                    }
                }
            } else {
                p.title = "Untitled Problem";
                p.id = "cf_unknown";
            }
            
            // Difficulty: Codeforces doesn't have explicit difficulty, default to medium
            p.difficulty = "medium";
            
            // Tags: extract from problem-statement (harder to parse reliably, skip for MVP)
            p.tags = "";
            
        } catch (const std::exception& e) {
            p.title = fmt::format("Error: {}", e.what());
            p.id = "cf_error";
        }
        
        return p;
    }

    std::vector<TestCase> fetch_test_cases(const std::string& url) override {
        std::vector<TestCase> cases;
        
        try {
            auto r = cpr::Get(cpr::Url{url}, cpr::Timeout{10000});
            if (r.status_code != 200) return cases;

            std::string html = r.text;
            
            // Codeforces sample test format:
            // <div class="input"><pre>...</pre></div>
            // <div class="output"><pre>...</pre></div>
            
            std::regex input_re(R"(<div[^>]*class="input"[^>]*>.*?<pre>([\s\S]*?)</pre>)", std::regex::icase);
            std::regex output_re(R"(<div[^>]*class="output"[^>]*>.*?<pre>([\s\S]*?)</pre>)", std::regex::icase);
            
            std::vector<std::string> inputs, outputs;
            
            auto inputs_begin = std::sregex_iterator(html.begin(), html.end(), input_re);
            auto inputs_end = std::sregex_iterator();
            for (std::sregex_iterator i = inputs_begin; i != inputs_end; ++i) {
                inputs.push_back(decode_html(trim((*i)[1].str())));
            }
            
            auto outputs_begin = std::sregex_iterator(html.begin(), html.end(), output_re);
            auto outputs_end = std::sregex_iterator();
            for (std::sregex_iterator i = outputs_begin; i != outputs_end; ++i) {
                outputs.push_back(decode_html(trim((*i)[1].str())));
            }
            
            // Pair inputs with outputs
            size_t count = (std::min)(inputs.size(), outputs.size());
            for (size_t i = 0; i < count; i++) {
                TestCase tc;
                tc.input = inputs[i];
                tc.output = outputs[i];
                tc.is_sample = true;
                cases.push_back(tc);
            }
            
        } catch (...) {
            // Return empty cases on error
        }
        
        return cases;
    }

private:
    std::string trim(const std::string& str) {
        size_t start = str.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) return "";
        size_t end = str.find_last_not_of(" \t\r\n");
        return str.substr(start, end - start + 1);
    }
    
    std::string decode_html(std::string s) {
        // Basic HTML entity decoding
        std::regex br_re("<br\\s*/?>", std::regex::icase);
        s = std::regex_replace(s, br_re, "\n");
        
        s = std::regex_replace(s, std::regex("&lt;"), "<");
        s = std::regex_replace(s, std::regex("&gt;"), ">");
        s = std::regex_replace(s, std::regex("&amp;"), "&");
        s = std::regex_replace(s, std::regex("&quot;"), "\"");
        
        return s;
    }
};

} // namespace shuati
