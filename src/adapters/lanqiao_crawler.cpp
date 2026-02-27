#include <filesystem>  // MSVC workaround: include before other headers

#include "shuati/adapters/lanqiao_crawler.hpp"
#include "shuati/adapters/base_crawler.hpp"
#include <ctime>

namespace shuati {

class LanqiaoCrawlerImpl : public BaseCrawler {
public:
    bool can_handle(const std::string& url) const override {
        return utils::contains(url, "lanqiao.cn");
    }

    Problem fetch_problem(const std::string& url) override {
        Problem p;
        p.source = "Lanqiao";
        p.url = url;

        try {
            std::string html = http_get(url);
            p.title = extract_title(html);
            if (p.title.empty()) {
                p.title = "Lanqiao Problem";
            }
            p.id = generate_id("lq");
        } catch (const std::exception& e) {
            p.title = fmt::format("Error: {}", e.what());
        }

        return p;
    }
};

// Public interface
LanqiaoCrawler::LanqiaoCrawler() : impl_(std::make_unique<LanqiaoCrawlerImpl>()) {}
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
