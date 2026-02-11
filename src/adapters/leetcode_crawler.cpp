#include "shuati/adapters/leetcode_crawler.hpp"
#include <cpr/cpr.h>
#include <nlohmann/json.hpp>
#include <fmt/core.h>
#include <regex>
#include <algorithm>
#include <sstream>

namespace shuati {

namespace {
    std::string extract_slug(const std::string& url) {
        std::regex re(R"(/problems/([^/]+))");
        std::smatch m;
        if (std::regex_search(url, m, re)) {
            return m[1].str();
        }
        return "";
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

    nlohmann::json query_graphql(const std::string& slug) {
        std::string query = R"(
        query questionData($titleSlug: String!) {
            question(titleSlug: $titleSlug) {
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
        body["variables"] = {{"titleSlug", slug}};

        auto r = cpr::Post(
            cpr::Url{"https://leetcode.com/graphql"},
            cpr::Header{
                {"Content-Type", "application/json"},
                {"User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/91.0.4472.124 Safari/537.36"}
            },
            cpr::Body{body.dump()},
            cpr::Timeout{10000}
        );

        if (r.status_code != 200) {
            throw std::runtime_error(fmt::format("HTTP {}", r.status_code));
        }

        auto resp = nlohmann::json::parse(r.text);
        if (resp.contains("errors")) {
             throw std::runtime_error("GraphQL Error: " + resp["errors"][0]["message"].get<std::string>());
        }
        return resp["data"]["question"];
    }
}

// LeetCodeCrawler Implementation

bool LeetCodeCrawler::can_handle(const std::string& url) const {
    return url.find("leetcode.com") != std::string::npos ||
           url.find("leetcode.cn") != std::string::npos;
}

Problem LeetCodeCrawler::fetch_problem(const std::string& url) {
    Problem p;
    p.source = "LeetCode";
    p.url = url;
    
    auto slug = extract_slug(url);
    if (slug.empty()) {
        p.title = "Failed to extract problem slug";
        return p;
    }

    try {
        auto data = query_graphql(slug);
        
        if (data.is_null()) {
             throw std::runtime_error("Problem not found");
        }

        if (data.contains("questionFrontendId")) {
            p.id = "lc_" + data["questionFrontendId"].get<std::string>();
            p.display_id = std::stoi(data["questionFrontendId"].get<std::string>());
        } else {
            p.id = "lc_" + slug;
        }
        
        p.title = data.value("title", "Untitled");
        p.difficulty = to_lower(data.value("difficulty", "medium"));
        
        if (data.contains("topicTags") && data["topicTags"].is_array()) {
            std::vector<std::string> tags;
            for (auto& tag : data["topicTags"]) {
                tags.push_back(tag.value("name", ""));
            }
            p.tags = join(tags, ",");
        }
        
    } catch (const std::exception& e) {
        p.title = fmt::format("Error: {}", e.what());
    }
    
    return p;
}

std::vector<TestCase> LeetCodeCrawler::fetch_test_cases(const std::string& url) {
    std::vector<TestCase> cases;
    auto slug = extract_slug(url);
    if (slug.empty()) return cases;

    try {
        auto data = query_graphql(slug);
        if (data.is_null()) return cases;
        
        if (data.contains("exampleTestcases")) {
            std::string examples = data.value("exampleTestcases", "");
            auto lines = split(examples, '\n');
            
            for (size_t i = 0; i < lines.size(); i++) {
                TestCase tc;
                tc.input = lines[i];
                tc.output = ""; 
                tc.is_sample = true;
                cases.push_back(tc);
            }
        }
    } catch (...) {}
    
    return cases;
}

} // namespace shuati
