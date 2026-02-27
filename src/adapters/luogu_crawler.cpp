#include <filesystem>  // MSVC workaround: include before other headers

#include "shuati/adapters/luogu_crawler.hpp"
#include "shuati/adapters/base_crawler.hpp"
#include <ctime>

namespace shuati {

class LuoguCrawlerImpl : public BaseCrawler {
public:
    bool can_handle(const std::string& url) const override {
        return utils::contains(url, "luogu.com.cn");
    }

    cpr::Header get_default_headers() const override {
        return cpr::Header{
            {"User-Agent", "Mozilla/5.0"}
        };
    }

    Problem fetch_problem(const std::string& url) override {
        Problem p;
        p.source = "Luogu";
        p.url = url;

        try {
            std::string html = http_get(url);
            p.title = extract_title(html);
            if (p.title.empty()) {
                p.title = "Luogu Problem";
            }

            // Extract ID from URL: problem/P1001
            std::string id = extract_regex(url, R"(problem/(P\d+))");
            p.id = !id.empty() ? generate_id("lg", id) : generate_id("lg");
        } catch (const std::exception& e) {
            p.title = fmt::format("Error: {}", e.what());
        }

        return p;
    }
};

// Public interface
LuoguCrawler::LuoguCrawler() : impl_(std::make_unique<LuoguCrawlerImpl>()) {}
LuoguCrawler::~LuoguCrawler() = default;

bool LuoguCrawler::can_handle(const std::string& url) const {
    return impl_->can_handle(url);
}

Problem LuoguCrawler::fetch_problem(const std::string& url) {
    return impl_->fetch_problem(url);
}

std::vector<TestCase> LuoguCrawler::fetch_test_cases(const std::string& url) {
    return impl_->fetch_test_cases(url);
}

} // namespace shuati
