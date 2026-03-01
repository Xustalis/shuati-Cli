#include <filesystem>  // MSVC workaround: include before other headers

#include "shuati/adapters/leetcode_crawler.hpp"
#include "shuati/utils/string_utils.hpp"
#include <fmt/core.h>
#include <nlohmann/json.hpp>

namespace shuati {

bool LeetCodeCrawler::can_handle(const std::string& url) const {
    return utils::contains(url, "leetcode.com") ||
           utils::contains(url, "leetcode.cn");
}

Problem LeetCodeCrawler::fetch_problem(const std::string& url) {
    Problem p;
    p.source = "LeetCode";
    p.url = url;

    // Extract slug from URL: /problems/two-sum/
    std::string slug = extract_regex(url, R"(/problems/([^/]+))");
    p.id = !slug.empty() ? generate_id("lc", slug) : generate_id("lc");

    try {
        // Determine if it's leetcode.cn or leetcode.com
        bool is_cn = utils::contains(url, "leetcode.cn");

        // Use GraphQL API to fetch problem details
        auto problem_data = query_graphql(slug, url);

        if (!problem_data.is_null()) {
            // Title
            std::string title = problem_data.value("title", "");
            std::string translated_title = problem_data.value("translatedTitle", "");
            std::string frontend_id = problem_data.value("questionFrontendId", "");

            if (!frontend_id.empty()) {
                if (is_cn && !translated_title.empty()) {
                    p.title = fmt::format("{}. {}", frontend_id, translated_title);
                } else if (!title.empty()) {
                    p.title = fmt::format("{}. {}", frontend_id, title);
                }
            } else {
                p.title = is_cn && !translated_title.empty() ? translated_title : title;
            }

            // Difficulty
            p.difficulty = problem_data.value("difficulty", "");

            // Tags
            if (problem_data.contains("topicTags") && problem_data["topicTags"].is_array()) {
                std::vector<std::string> tag_names;
                for (const auto& tag : problem_data["topicTags"]) {
                    // Prefer Chinese name if available
                    std::string tag_name = tag.value("translatedName", "");
                    if (tag_name.empty()) {
                        tag_name = tag.value("name", "");
                    }
                    if (!tag_name.empty()) {
                        tag_names.push_back(tag_name);
                    }
                }
                p.tags = utils::join(tag_names, ",");
            }

            // Description (HTML content)
            std::string content;
            if (is_cn) {
                content = problem_data.value("translatedContent", "");
            }
            if (content.empty()) {
                content = problem_data.value("content", "");
            }

            if (!content.empty()) {
                // Convert HTML to plain text
                content = utils::replace_all(content, "<br>", "\n");
                content = utils::replace_all(content, "<br/>", "\n");
                content = utils::replace_all(content, "<br />", "\n");
                content = utils::replace_all(content, "</p>", "\n\n");
                content = utils::replace_all(content, "</li>", "\n");
                content = utils::replace_all(content, "<li>", "- ");
                content = utils::replace_all(content, "&nbsp;", " ");
                content = utils::replace_all(content, "&lt;", "<");
                content = utils::replace_all(content, "&gt;", ">");
                content = utils::replace_all(content, "&amp;", "&");
                content = utils::replace_all(content, "&quot;", "\"");
                content = utils::strip_html_tags(content);
                content = utils::trim(content);
                p.description = content;
            } else {
                p.description = "无法获取题目内容";
            }
        } else {
            // Fallback: simple HTML fetch
            std::string html = http_get(url);
            p.title = extract_title(html);
            p.title = clean_title(p.title, {" - LeetCode", " - 力扣"});
            p.description = "无法通过 GraphQL 获取题目内容";
        }

    } catch (const std::exception& e) {
        p.title = fmt::format("Error: {}", e.what());
    }

    return p;
}

std::vector<TestCase> LeetCodeCrawler::fetch_test_cases(const std::string& url) {
    // LeetCode doesn't expose test cases easily through the API
    // The sample inputs/outputs are embedded in the problem description HTML
    // For now, we extract what we can from the GraphQL response
    std::vector<TestCase> cases;

    try {
        std::string slug = extract_regex(url, R"(/problems/([^/]+))");
        auto problem_data = query_graphql(slug, url);

        if (!problem_data.is_null() && problem_data.contains("exampleTestcaseList") &&
            problem_data["exampleTestcaseList"].is_array()) {
            for (const auto& tc_input : problem_data["exampleTestcaseList"]) {
                TestCase tc;
                tc.input = utils::trim(tc_input.get<std::string>());
                tc.output = ""; // LeetCode doesn't provide expected output in API
                tc.is_sample = true;
                cases.push_back(tc);
            }
        }
    } catch (...) {
        // Ignore errors
    }

    return cases;
}

nlohmann::json LeetCodeCrawler::query_graphql(const std::string& slug, const std::string& url) {
    bool is_cn = utils::contains(url, "leetcode.cn");
    std::string api_url = is_cn
        ? "https://leetcode.cn/graphql/"
        : "https://leetcode.com/graphql/";

    // Build GraphQL query
    nlohmann::json query_body = {
        {"query", R"(
            query questionData($titleSlug: String!) {
                question(titleSlug: $titleSlug) {
                    questionFrontendId
                    title
                    translatedTitle
                    difficulty
                    content
                    translatedContent
                    topicTags {
                        name
                        translatedName
                    }
                    exampleTestcaseList
                }
            }
        )"},
        {"variables", {{"titleSlug", slug}}}
    };

    std::string json_body = query_body.dump();

    // POST to GraphQL endpoint
    std::map<std::string, std::string> headers;
    headers["Content-Type"] = "application/json";
    headers["User-Agent"] = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36";
    headers["Referer"] = url;

    auto response = http_client_->post(api_url, json_body, headers, get_default_timeout_ms());

    if (response.status_code == 0) {
        throw std::runtime_error(fmt::format("Network Error: Could not reach {} ({})", api_url, response.error_message));
    }
    if (response.status_code != 200) {
        throw std::runtime_error(fmt::format("HTTP {} from LeetCode GraphQL", response.status_code));
    }

    auto resp_json = nlohmann::json::parse(response.text);
    if (resp_json.contains("data") && resp_json["data"].contains("question") &&
        !resp_json["data"]["question"].is_null()) {
        return resp_json["data"]["question"];
    }

    return nlohmann::json();
}

} // namespace shuati
