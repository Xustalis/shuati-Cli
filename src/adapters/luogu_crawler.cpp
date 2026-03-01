#include <filesystem>  // MSVC workaround: include before other headers

#include "shuati/adapters/luogu_crawler.hpp"
#include "shuati/adapters/base_crawler.hpp"
#include <ctime>

namespace shuati {

class LuoguCrawlerImpl : public BaseCrawler {
public:
    using BaseCrawler::BaseCrawler;

    bool can_handle(const std::string& url) const override {
        return utils::contains(url, "luogu.com.cn");
    }

    cpr::Header get_default_headers() const override {
        return cpr::Header{
            {"User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/91.0.4472.124 Safari/537.36"}
        };
    }

    Problem fetch_problem(const std::string& url) override {
        Problem p;
        p.source = "Luogu";
        p.url = url;

        // Extract ID from URL: problem/P1001
        std::string id = extract_regex(url, R"(problem/(P\d+))");
        p.id = !id.empty() ? generate_id("lg", id) : generate_id("lg");
        if (!id.empty()) p.display_id = 0; // Luogu IDs are Pxxxx, not purely numeric

        try {
            // Simply fetch the HTML page
            std::string html = http_get(url);

            // Extract title from HTML
            p.title = extract_title(html);
            p.title = clean_title(p.title, {" - 洛谷"});

            // For now, just save that we got the HTML
            // We can improve text extraction later if needed
            p.description = "Problem page fetched successfully from Luogu.";

        } catch (const std::exception& e) {
            p.title = fmt::format("Error: {}", e.what());
        }

        return p;
    }
};

// Public interface
LuoguCrawler::LuoguCrawler(std::shared_ptr<IHttpClient> client)
    : impl_(std::make_unique<LuoguCrawlerImpl>(client)) {}
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
