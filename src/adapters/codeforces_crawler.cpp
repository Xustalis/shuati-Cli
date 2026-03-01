#include <filesystem>  // MSVC workaround: include before other headers

#include "shuati/adapters/codeforces_crawler.hpp"
#include "shuati/adapters/base_crawler.hpp"
#include <ctime>
#include <nlohmann/json.hpp>

namespace shuati {

// Map CF rating to difficulty string
static std::string cf_rating_to_difficulty(int rating) {
    if (rating <= 0) return "";
    if (rating <= 800) return "入门 (800)";
    if (rating <= 1200) return "普及- (≤1200)";
    if (rating <= 1600) return "普及/提高 (≤1600)";
    if (rating <= 2000) return "提高+ (≤2000)";
    if (rating <= 2400) return "省选 (≤2400)";
    return fmt::format("NOI+ ({})", rating);
}

bool CodeforcesCrawler::can_handle(const std::string& url) const {
    return utils::contains(url, "codeforces.com");
}

Problem CodeforcesCrawler::fetch_problem(const std::string& url) {
    Problem p;
    p.source = "Codeforces";
    p.url = url;

    // Extract contest ID and problem letter from URL
    // Format: https://codeforces.com/problemset/problem/1500/A
    // or: https://codeforces.com/contest/1500/problem/A
    std::string contest_id = extract_regex(url, R"((?:problemset/problem|contest)/(\d+))");
    // The problem letter is the last path segment (after contest_id/)
    std::string problem_letter = extract_regex(url, R"(/(\w+)\s*$)");
    // Validate: problem_letter should be something like A, B, C1, etc. (not a number)
    if (!problem_letter.empty() && std::isdigit(problem_letter[0])) {
        problem_letter = ""; // This matched the contest ID, not the letter
    }

    if (!contest_id.empty() && !problem_letter.empty()) {
        p.id = generate_id("cf", contest_id + problem_letter);
    } else {
        p.id = generate_id("cf");
    }

    try {
        // 1. Fetch problem metadata from CF API
        if (!contest_id.empty() && !problem_letter.empty()) {
            try {
                std::string api_url = fmt::format(
                    "https://codeforces.com/api/contest.standings?contestId={}&from=1&count=1",
                    contest_id);
                std::string api_response = http_get(api_url);
                auto api_json = nlohmann::json::parse(api_response);

                if (api_json.value("status", "") == "OK" &&
                    api_json.contains("result") &&
                    api_json["result"].contains("problems")) {
                    for (const auto& prob : api_json["result"]["problems"]) {
                        if (prob.value("index", "") == problem_letter) {
                            // Title from API
                            p.title = prob.value("name", "");

                            // Rating -> difficulty
                            if (prob.contains("rating")) {
                                int rating = prob["rating"].get<int>();
                                p.difficulty = cf_rating_to_difficulty(rating);
                            }

                            // Tags
                            if (prob.contains("tags") && prob["tags"].is_array()) {
                                std::vector<std::string> tag_list;
                                for (const auto& t : prob["tags"]) {
                                    tag_list.push_back(t.get<std::string>());
                                }
                                p.tags = utils::join(tag_list, ",");
                            }
                            break;
                        }
                    }
                }
            } catch (...) {
                // API failed, will fallback to HTML
            }
        }

        // 2. Fetch HTML page for description
        std::string html = http_get(url);

        // If title not set from API, extract from HTML
        if (p.title.empty()) {
            p.title = extract_title(html);
            p.title = clean_title(p.title, {" - Codeforces", " - Problem"});
        } else {
            // Format title nicely with contest/problem ID
            p.title = fmt::format("{}{}. {}", contest_id, problem_letter, p.title);
        }

        // Extract full problem statement from HTML
        p.description = extract_cf_description(html);

    } catch (const std::exception& e) {
        p.title = fmt::format("Error: {}", e.what());
    }

    return p;
}

std::vector<TestCase> CodeforcesCrawler::fetch_test_cases(const std::string& url) {
    std::vector<TestCase> cases;

    try {
        std::string html = http_get(url);

        // Codeforces test cases are in <div class="sample-test">
        auto sample_start = html.find("<div class=\"sample-test\">");
        if (sample_start == std::string::npos) {
            return cases;
        }

        // Find the end of sample-test section
        auto sample_end = html.find("</div>", sample_start);
        // We need to find a reasonable end boundary
        auto section_end = html.find("<div class=\"note\">", sample_start);
        if (section_end == std::string::npos) {
            section_end = html.size();
        }

        // Find all input/output pairs
        auto input_pos = sample_start;
        while (input_pos < section_end) {
            input_pos = html.find("<div class=\"input\">", input_pos);
            if (input_pos == std::string::npos || input_pos >= section_end) break;

            auto input_content_start = html.find("<pre", input_pos);
            if (input_content_start == std::string::npos) break;
            // Skip to content after the <pre> or <pre ...> tag
            input_content_start = html.find(">", input_content_start);
            if (input_content_start == std::string::npos) break;
            input_content_start++;

            auto input_content_end = html.find("</pre>", input_content_start);
            if (input_content_end == std::string::npos) break;

            auto output_pos = html.find("<div class=\"output\">", input_pos);
            if (output_pos == std::string::npos) break;

            auto output_content_start = html.find("<pre", output_pos);
            if (output_content_start == std::string::npos) break;
            output_content_start = html.find(">", output_content_start);
            if (output_content_start == std::string::npos) break;
            output_content_start++;

            auto output_content_end = html.find("</pre>", output_content_start);
            if (output_content_end == std::string::npos) break;

            // Extract input and output
            std::string input = html.substr(input_content_start, input_content_end - input_content_start);
            std::string output = html.substr(output_content_start, output_content_end - output_content_start);

            // Clean up: replace <br>, <br/>, <div> with newlines
            input = utils::replace_all(input, "<br>", "\n");
            input = utils::replace_all(input, "<br/>", "\n");
            input = utils::replace_all(input, "<br />", "\n");
            output = utils::replace_all(output, "<br>", "\n");
            output = utils::replace_all(output, "<br/>", "\n");
            output = utils::replace_all(output, "<br />", "\n");

            input = utils::strip_html_tags(input);
            output = utils::strip_html_tags(output);
            input = utils::trim(input);
            output = utils::trim(output);

            if (!input.empty() || !output.empty()) {
                TestCase tc;
                tc.input = input;
                tc.output = output;
                tc.is_sample = true;
                cases.push_back(tc);
            }

            // Move forward
            input_pos = output_content_end;
        }
    } catch (...) {
        // Ignore errors in test case extraction
    }

    return cases;
}

// Private helper: extract full problem description from CF HTML
std::string CodeforcesCrawler::extract_cf_description(const std::string& html) {
    std::string desc;

    // Find problem-statement div
    auto stmt_start = html.find("<div class=\"problem-statement\">");
    if (stmt_start == std::string::npos) return "";

    // Helper to extract a section
    auto extract_section = [&](const std::string& section_class, const std::string& title) -> std::string {
        std::string marker = "<div class=\"" + section_class + "\">";
        auto pos = html.find(marker, stmt_start);
        if (pos == std::string::npos) return "";

        // Find the content after the section header div
        auto content_start = html.find(">", pos + marker.length());
        if (content_start == std::string::npos) return "";
        
        // For sections with nested structure, look for the actual text content
        std::string section_text;
        auto next_div = pos;
        
        // Find end of this section (next sibling div at the same level)
        int depth = 0;
        auto scan = pos;
        bool started = false;
        while (scan < html.size()) {
            auto open = html.find("<div", scan);
            auto close = html.find("</div>", scan);
            
            if (!started) {
                started = true;
                depth = 1;
                scan = pos + marker.length();
                continue;
            }
            
            if (open < close && open != std::string::npos) {
                depth++;
                scan = open + 4;
            } else if (close != std::string::npos) {
                depth--;
                if (depth <= 0) {
                    section_text = html.substr(pos + marker.length(), close - pos - marker.length());
                    break;
                }
                scan = close + 6;
            } else {
                break;
            }
        }

        if (section_text.empty()) return "";

        // Clean HTML
        section_text = utils::replace_all(section_text, "<br>", "\n");
        section_text = utils::replace_all(section_text, "<br/>", "\n");
        section_text = utils::replace_all(section_text, "<br />", "\n");
        section_text = utils::replace_all(section_text, "</p>", "\n");
        section_text = utils::replace_all(section_text, "</div>", "\n");
        section_text = utils::strip_html_tags(section_text);
        section_text = utils::trim(section_text);

        if (!section_text.empty()) {
            return "## " + title + "\n\n" + section_text;
        }
        return "";
    };

    // Try extracting full statement between problem-statement and sample-tests
    auto sample_pos = html.find("<div class=\"sample-test\">", stmt_start);
    if (sample_pos == std::string::npos) {
        sample_pos = html.size();
    }

    // Extract the raw statement area
    std::string raw_stmt = html.substr(stmt_start, sample_pos - stmt_start);

    // Clean up and format
    raw_stmt = utils::replace_all(raw_stmt, "<br>", "\n");
    raw_stmt = utils::replace_all(raw_stmt, "<br/>", "\n");
    raw_stmt = utils::replace_all(raw_stmt, "<br />", "\n");
    raw_stmt = utils::replace_all(raw_stmt, "</p>", "\n\n");
    raw_stmt = utils::replace_all(raw_stmt, "</div>", "\n");

    // Replace section headers
    raw_stmt = utils::replace_all(raw_stmt, "<div class=\"section-title\">", "\n## ");

    raw_stmt = utils::strip_html_tags(raw_stmt);
    raw_stmt = utils::trim(raw_stmt);

    return raw_stmt;
}

} // namespace shuati
