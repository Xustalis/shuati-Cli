#include <filesystem>  // MSVC workaround: include before other headers

#include "shuati/adapters/leetcode_crawler.hpp"
#include "shuati/utils/string_utils.hpp"
#include <fmt/core.h>

namespace shuati {

bool LeetCodeCrawler::can_handle(const std::string& url) const {
    return utils::contains(url, "leetcode.com") ||
           utils::contains(url, "leetcode.cn");
}

Problem LeetCodeCrawler::fetch_problem(const std::string& url) {
    Problem p;
    p.source = "LeetCode";
    p.url = url;

    // Extract ID from URL
    std::string id = extract_regex(url, R"(/problems/([^/]+))");
    p.id = !id.empty() ? generate_id("lc", id) : generate_id("lc");

    try {
        // Simply fetch the HTML page
        std::string html = http_get(url);

        // Extract title from HTML <title> tag
        p.title = extract_title(html);
        p.title = clean_title(p.title, {" - LeetCode", " - 力扣"});

        // For LeetCode, we can also try to extract the main content
        // The problem statement is usually in <div class="question-content">
        auto content_start = html.find("<div class=\"question-content\"");
        if (content_start != std::string::npos) {
            auto content_end = html.find("</div>", content_start);
            if (content_end != std::string::npos) {
                std::string content_html = html.substr(content_start, content_end - content_start);
                p.description = utils::strip_html_tags(content_html);
                p.description = utils::trim(p.description);
            }
        }

        // If no description extracted, use a simple placeholder
        if (p.description.empty()) {
            p.description = "Problem statement extracted from HTML.";
        }

    } catch (const std::exception& e) {
        p.title = fmt::format("Error: {}", e.what());
    }

    return p;
}

std::vector<TestCase> LeetCodeCrawler::fetch_test_cases(const std::string& url) {
    // Skip test case extraction for simplicity - just get HTML
    return {};
}

} // namespace shuati
