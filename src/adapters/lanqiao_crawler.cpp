#include <filesystem> // MSVC workaround: include before other headers

#include "shuati/adapters/base_crawler.hpp"
#include "shuati/adapters/lanqiao_crawler.hpp"
#include <ctime>

namespace shuati {

namespace {

Problem build_lanqiao_problem(const std::string &url, const std::string &html) {
  Problem p;
  p.source = "Lanqiao";
  p.url = url;

  p.title = utils::extract_title_from_html(html);
  for (const auto &suf : {std::string(" - 蓝桥云课"), std::string("- 蓝桥云课"),
                          std::string(" | 蓝桥云课")}) {
    auto pos = p.title.find(suf);
    if (pos != std::string::npos)
      p.title = p.title.substr(0, pos);
  }
  p.title = utils::trim(p.title);
  if (p.title.empty())
    p.title = "Lanqiao Problem";

  std::string pid = utils::extract_regex_group(url, R"(problems/(\d+))");
  p.id =
      !pid.empty() ? "lq_" + pid : "lq_" + std::to_string(std::time(nullptr));

  std::string keywords = utils::extract_meta_content(html, "keywords");
  if (!keywords.empty())
    p.tags = utils::replace_all(keywords, "，", ",");

  std::string diff = utils::extract_regex_group(
      html, R"((?:难度|difficulty)[\s\S]{0,20}?([A-Za-z]+|简单|中等|困难))");
  if (diff.empty())
    diff = utils::extract_meta_content(html, "difficulty");
  p.difficulty = utils::to_lower(utils::trim(diff));

  p.description = utils::extract_meta_content(html, "description");
  if (p.description.empty()) {
    std::string body = utils::extract_regex_group(
        html,
        R"((?:题目描述|Description)[\s\S]{0,40}?(<div[^>]*>[\s\S]{20,2000}</div>))");
    p.description = utils::strip_html_tags(body);
  }
  return p;
}

} // namespace

class LanqiaoCrawlerImpl : public BaseCrawler {
public:
  bool can_handle(const std::string &url) const override {
    return utils::contains(url, "lanqiao.cn");
  }

  Problem fetch_problem(const std::string &url) override {
    try {
      return build_lanqiao_problem(url, http_get(url));
    } catch (const std::exception &e) {
      Problem p;
      p.source = "Lanqiao";
      p.url = url;
      p.title = fmt::format("Error: {}", e.what());
      return p;
    }
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

Problem LanqiaoCrawler::parse_problem_from_html(const std::string &url,
                                                const std::string &html) {
  return build_lanqiao_problem(url, html);
}

} // namespace shuati
