#include <filesystem>  // MSVC workaround: include before other headers

#include "shuati/adapters/codeforces_crawler.hpp"
#include "shuati/adapters/base_crawler.hpp"
#include <ctime>

namespace shuati {

class CodeforcesCrawlerImpl : public BaseCrawler {
public:
    bool can_handle(const std::string& url) const override {
        return utils::contains(url, "codeforces.com");
    }

    Problem fetch_problem(const std::string& url) override {
        Problem p;
        p.source = "Codeforces";
        p.url = url;

        try {
            std::string html = http_get(url);
            p.title = extract_title(html);
            p.title = clean_title(p.title, {" - Codeforces"});

            // Extract ID from URL: problem/1500/A
            std::string id = extract_regex(url, R"(problem/(\d+)/(\w+))");
            if (!id.empty()) {
                std::string part = extract_regex(url, R"(problem/(\d+)/(\w+))", 2);
                p.id = generate_id("cf", id + part);
            } else {
                p.id = generate_id("cf");
            }
        } catch (const std::exception& e) {
            p.title = fmt::format("Error: {}", e.what());
        }

        return p;
    }
};

// Public interface
CodeforcesCrawler::CodeforcesCrawler() : impl_(std::make_unique<CodeforcesCrawlerImpl>()) {}
CodeforcesCrawler::~CodeforcesCrawler() = default;

bool CodeforcesCrawler::can_handle(const std::string& url) const {
    return impl_->can_handle(url);
}

Problem CodeforcesCrawler::fetch_problem(const std::string& url) {
    return impl_->fetch_problem(url);
}

std::vector<TestCase> CodeforcesCrawler::fetch_test_cases(const std::string& url) {
    return impl_->fetch_test_cases(url);
}

} // namespace shuati
