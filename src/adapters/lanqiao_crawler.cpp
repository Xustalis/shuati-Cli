#include <filesystem>  // MSVC workaround: include before other headers

#include "shuati/adapters/lanqiao_crawler.hpp"
#include "shuati/adapters/base_crawler.hpp"
#include <ctime>

namespace shuati {

class LanqiaoCrawlerImpl : public BaseCrawler {
public:
    using BaseCrawler::BaseCrawler;

    bool can_handle(const std::string& url) const override {
        return utils::contains(url, "lanqiao.cn");
    }

    // Implement random user agent rotation
    cpr::Header get_default_headers() const override {
        static const std::vector<std::string> user_agents = {
            "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/91.0.4472.124 Safari/537.36",
            "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/14.0.3 Safari/605.1.15",
            "Mozilla/5.0 (Windows NT 10.0; Win64; x64; rv:89.0) Gecko/20100101 Firefox/89.0",
            "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/92.0.4515.107 Safari/537.36"
        };
        // Simple random selection
        static int seed = 0;
        int idx = (seed++ + std::time(nullptr)) % user_agents.size();

        return cpr::Header{
            {"User-Agent", user_agents[idx]},
            {"Accept", "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8"},
            {"Referer", "https://www.lanqiao.cn/"}
        };
    }

    Problem fetch_problem(const std::string& url) override {
        Problem p;
        p.source = "Lanqiao";
        p.url = url;

        // Extract ID from URL
        std::string id_str = extract_regex(url, R"(problems/(\d+))");
        if (id_str.empty()) {
            p.id = generate_id("lq");
            p.title = "Lanqiao Problem (ID Unknown)";
            return p;
        }

        p.id = "lq_" + id_str;
        p.display_id = std::stoi(id_str);

        try {
            // Simply fetch the HTML page
            std::string html = http_get(url);

            // Extract title from HTML
            p.title = extract_title(html);
            p.title = clean_title(p.title, {" - 蓝桥云课"});

            // Just save that we got the HTML
            p.description = "Problem page fetched successfully from Lanqiao.";

        } catch (const std::exception& e) {
            p.title = fmt::format("Error: {}", e.what());
        }

        return p;
    }
};

// Public interface
LanqiaoCrawler::LanqiaoCrawler(std::shared_ptr<IHttpClient> client)
    : impl_(std::make_unique<LanqiaoCrawlerImpl>(client)) {}
LanqiaoCrawler::~LanqiaoCrawler() = default;

bool LanqiaoCrawler::can_handle(const std::string& url) const {
    return impl_->can_handle(url);
}

Problem LanqiaoCrawler::fetch_problem(const std::string& url) {
    return impl_->fetch_problem(url);
}

std::vector<TestCase> LanqiaoCrawler::fetch_test_cases(const std::string& url) {
    return impl_->fetch_test_cases(url);
}

} // namespace shuati
