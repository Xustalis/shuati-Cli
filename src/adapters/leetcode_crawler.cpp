#include "shuati/crawler.hpp"
#include <cpr/cpr.h>
#include <nlohmann/json.hpp>
#include <fmt/core.h>
#include <regex>

namespace shuati {

class LeetCodeCrawler : public ICrawler {
public:
    bool can_handle(const std::string& url) const override {
        return url.find("leetcode.com") != std::string::npos ||
               url.find("leetcode.cn") != std::string::npos;
    }

    Problem fetch_problem(const std::string& url) override {
        Problem p;
        p.source = "web";
        p.url = url;
        
        auto slug = extract_slug(url);
        if (slug.empty()) {
            p.title = "Failed to extract problem slug";
            return p;
        }

        try {
            auto data = query_graphql(slug);
            if (data.contains("questionFrontendId")) {
                p.id = "lc_" + data["questionFrontendId"].get<std::string>();
            } else {
                p.id = "lc_" + slug;
            }
            
            p.title = data.value("title", "Untitled");
            p.difficulty = to_lower(data.value("difficulty", "medium"));
            
            // Extract tags
            if (data.contains("topicTags") && data["topicTags"].is_array()) {
                std::vector<std::string> tags;
                for (auto& tag : data["topicTags"]) {
                    tags.push_back(tag.value("name", ""));
                }
                p.tags = join(tags, ",");
            }
            
        } catch (const std::exception& e) {
            p.title = fmt::format("Error fetching from LeetCode: {}", e.what());
        }
        
        return p;
    }

    std::vector<TestCase> fetch_test_cases(const std::string& url) override {
        std::vector<TestCase> cases;
        auto slug = extract_slug(url);
        if (slug.empty()) return cases;

        try {
            auto data = query_graphql(slug);
            
            // Parse exampleTestcases (LeetCode格式: "input1\ninput2\n...")
            if (data.contains("exampleTestcases")) {
                std::string examples = data["exampleTestcases"];
                auto lines = split(examples, '\n');
                
                // Typically pairs of input/output, but LeetCode doesn't always provide expected output
                // We'll just store inputs as test cases
                for (size_t i = 0; i < lines.size(); i++) {
                    TestCase tc;
                    tc.input = lines[i];
                    tc.output = "";  // LeetCode doesn't provide outputs in GraphQL
                    tc.is_sample = true;
                    cases.push_back(tc);
                }
            }
        } catch (...) {
            // If fetching test cases fails, return empty
        }
        
        return cases;
    }

private:
    std::string extract_slug(const std::string& url) {
        // Extract problem slug from URL like: https://leetcode.com/problems/two-sum/
        std::regex re(R"(/problems/([^/]+))");
        std::smatch m;
        if (std::regex_search(url, m, re)) {
            return m[1].str();
        }
        return "";
    }

    nlohmann::json query_graphql(const std::string& slug) {
        std::string query = R"(
        {
            question(titleSlug: ")" + slug + R"(") {
                questionFrontendId
                title
                difficulty
                topicTags { name }
                content
                exampleTestcases
            }
        }
        )";

        nlohmann::json body;
        body["query"] = query;

        auto r = cpr::Post(
            cpr::Url{"https://leetcode.com/graphql/"},
            cpr::Header{{"Content-Type", "application/json"}},
            cpr::Body{body.dump()},
            cpr::Timeout{10000}
        );

        if (r.status_code != 200) {
            throw std::runtime_error(fmt::format("HTTP {}", r.status_code));
        }

        auto resp = nlohmann::json::parse(r.text);
        return resp["data"]["question"];
    }

    std::string to_lower(std::string s) {
        std::transform(s.begin(), s.end(), s.begin(), ::tolower);
        return s;
    }

    std::string join(const std::vector<std::string>& vec, const std::string& delim) {
        if (vec.empty()) return "";
        std::string result = vec[0];
        for (size_t i = 1; i < vec.size(); i++) {
            result += delim + vec[i];
        }
        return result;
    }

    std::vector<std::string> split(const std::string& str, char delim) {
        std::vector<std::string> result;
        std::stringstream ss(str);
        std::string item;
        while (std::getline(ss, item, delim)) {
            if (!item.empty()) result.push_back(item);
        }
        return result;
    }
};

} // namespace shuati
