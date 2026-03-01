#include <filesystem>  // MSVC workaround: include before other headers

#include "shuati/adapters/codeforces_crawler.hpp"
#include "shuati/adapters/base_crawler.hpp"
#include <ctime>
#include <nlohmann/json.hpp>

namespace shuati {

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
    std::string contest_id = extract_regex(url, R"((?:problemset|contest)/(\d+))");
    std::string problem_letter = extract_regex(url, R"(problem/(\w+))");

    if (!contest_id.empty() && !problem_letter.empty()) {
        p.id = generate_id("cf", contest_id + problem_letter);
    } else {
        p.id = generate_id("cf");
    }

    try {
        std::string html = http_get(url);
        p.title = extract_title(html);
        p.title = clean_title(p.title, {" - Codeforces", " - Problem"});

        // Try to extract description from the problem statement
        // Codeforces stores problem content in <div class="problem-statement">
        auto pos = html.find("<div class=\"problem-statement\">");
        if (pos != std::string::npos) {
            auto end_pos = html.find("</div>", pos + 30);
            if (end_pos != std::string::npos) {
                // Extract the main content section
                std::string statement = html.substr(pos, end_pos - pos + 6);

                // Try to find the actual problem description
                auto desc_start = statement.find("<div class=\"header\">");
                if (desc_start != std::string::npos) {
                    // Skip the header and get the first paragraph
                    desc_start = statement.find("<p>", desc_start);
                    if (desc_start != std::string::npos) {
                        auto desc_end = statement.find("</p>", desc_start);
                        if (desc_end != std::string::npos) {
                            p.description = statement.substr(desc_start + 3, desc_end - desc_start - 3);
                            // Simple HTML tag removal
                            p.description = utils::replace_all(p.description, "<br>", "\n");
                            p.description = utils::replace_all(p.description, "<br/>", "\n");
                            p.description = utils::strip_html_tags(p.description);
                        }
                    }
                }
            }
        }
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

        // Find all input/output pairs
        auto input_pos = sample_start;
        while (true) {
            input_pos = html.find("<div class=\"input\">", input_pos);
            if (input_pos == std::string::npos) break;

            auto input_content_start = html.find("<pre>", input_pos);
            if (input_content_start == std::string::npos) break;

            auto input_content_end = html.find("</pre>", input_content_start);
            if (input_content_end == std::string::npos) break;

            auto output_pos = html.find("<div class=\"output\">", input_pos);
            if (output_pos == std::string::npos) break;

            auto output_content_start = html.find("<pre>", output_pos);
            if (output_content_start == std::string::npos) break;

            auto output_content_end = html.find("</pre>", output_content_start);
            if (output_content_end == std::string::npos) break;

            // Extract input and output
            std::string input = html.substr(input_content_start + 5, input_content_end - input_content_start - 5);
            std::string output = html.substr(output_content_start + 5, output_content_end - output_content_start - 5);

            // Clean up
            input = utils::strip_html_tags(input);
            output = utils::strip_html_tags(output);
            input = utils::trim(input);
            output = utils::trim(output);

            TestCase tc;
            tc.input = input;
            tc.output = output;
            tc.is_sample = true;
            cases.push_back(tc);

            // Move forward
            input_pos = output_content_end;
        }
    } catch (...) {
        // Ignore errors in test case extraction
    }

    return cases;
}

} // namespace shuati
