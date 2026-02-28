#include <filesystem> // MSVC workaround: include before other headers

#include "shuati/adapters/base_crawler.hpp"
#include "shuati/adapters/lanqiao_crawler.hpp"
#include <ctime>

namespace shuati {

class LanqiaoCrawlerImpl : public BaseCrawler {
public:
  bool can_handle(const std::string &url) const override {
    return utils::contains(url, "lanqiao.cn");
  }

  Problem fetch_problem(const std::string &url) override {
    Problem p;
    p.source = "Lanqiao";
    p.url = url;

    try {
      std::string html = http_get(url);
      p.title = extract_title(html);
      p.title =
          clean_title(p.title, {" - 蓝桥云课", "- 蓝桥云课", " | 蓝桥云课"});
      if (p.title.empty()) {
        p.title = "Lanqiao Problem";
      }

      p.id = generate_id("lq",
                         utils::extract_regex_group(url, R"(problems/(\d+))"));

      std::string keywords = utils::extract_meta_content(html, "keywords");
      if (!keywords.empty()) {
        p.tags = utils::replace_all(keywords, "，", ",");
      }

      std::string diff = extract_regex(
          html,
          R"((?:难度|difficulty)[\s\S]{0,20}?([A-Za-z]+|简单|中等|困难))");
      if (diff.empty())
        diff = utils::extract_meta_content(html, "difficulty");
      p.difficulty = utils::to_lower(utils::trim(diff));

      p.description = utils::extract_meta_content(html, "description");
      if (p.description.empty()) {
        std::string body = extract_regex(
            html,
            R"((?:题目描述|Description)[\s\S]{0,40}?(<div[^>]*>[\s\S]{20,2000}</div>))");
        p.description = utils::strip_html_tags(body);
      }
    } catch (const std::exception &e) {
      p.title = fmt::format("Error: {}", e.what());
    }

    return p;
  }
};

LanqiaoCrawler::LanqiaoCrawler()
    : impl_(std::make_unique<LanqiaoCrawlerImpl>()) {}
LanqiaoCrawler::~LanqiaoCrawler() = default;

bool LanqiaoCrawler::can_handle(const std::string &url) const {
  return impl_->can_handle(url);
}

Problem LanqiaoCrawler::fetch_problem(const std::string &url) {
  return impl_->fetch_problem(url);
}

std::vector<TestCase> LanqiaoCrawler::fetch_test_cases(const std::string &url) {
  return impl_->fetch_test_cases(url);
}

} // namespace shuati
