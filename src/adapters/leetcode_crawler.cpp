#include <filesystem> // MSVC workaround: include before other headers

#include "shuati/adapters/leetcode_crawler.hpp"
#include "shuati/utils/string_utils.hpp"
#include <cpr/cpr.h>
#include <fmt/core.h>
#include <nlohmann/json.hpp>

namespace shuati {

namespace {
std::string extract_slug(const std::string &url) {
  return utils::extract_regex_group(url, R"(/problems/([^/]+))");
}

nlohmann::json query_graphql(const std::string &slug) {
  std::string query = R"({
        query questionData($titleSlug: String!) {
            question(titleSlug: $titleSlug) {
                questionFrontendId
                title
                difficulty
                topicTags { name }
                content
                exampleTestcases
            }
        }
        })";

  nlohmann::json body;
  body["query"] = query;
  body["variables"] = {{"titleSlug", slug}};

  auto r = cpr::Post(
      cpr::Url{"https://leetcode.com/graphql"},
      cpr::Header{
          {"Content-Type", "application/json"},
          {"User-Agent",
           "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36"}},
      cpr::Body{body.dump()}, cpr::Timeout{10000});

  if (r.status_code != 200) {
    throw std::runtime_error(fmt::format("HTTP {}", r.status_code));
  }

  auto resp = nlohmann::json::parse(r.text);
  if (resp.contains("errors")) {
    throw std::runtime_error("GraphQL Error: " +
                             resp["errors"][0]["message"].get<std::string>());
  }
  return resp["data"]["question"];
}
} // namespace

bool LeetCodeCrawler::can_handle(const std::string &url) const {
  return utils::contains(url, "leetcode.com") ||
         utils::contains(url, "leetcode.cn");
}

Problem LeetCodeCrawler::fetch_problem(const std::string &url) {
  Problem p;
  p.source = "LeetCode";
  p.url = url;

  auto slug = extract_slug(url);
  if (slug.empty()) {
    p.title = "Failed to extract problem slug";
    return p;
  }

  try {
    auto data = query_graphql(slug);

    if (data.is_null()) {
      throw std::runtime_error("Problem not found");
    }

    if (data.contains("questionFrontendId")) {
      auto id_str = data["questionFrontendId"].get<std::string>();
      p.id = "lc_" + id_str;
      p.display_id = utils::try_parse_number<int>(id_str).value_or(0);
    } else {
      p.id = "lc_" + slug;
    }

    p.title = data.value("title", "Untitled");
    p.description = utils::strip_html_tags(data.value("content", ""));
    p.difficulty = utils::to_lower(data.value("difficulty", "medium"));

    if (data.contains("topicTags") && data["topicTags"].is_array()) {
      std::vector<std::string> tags;
      for (auto &tag : data["topicTags"]) {
        tags.push_back(tag.value("name", ""));
      }
      p.tags = utils::join(tags, ",");
    }

  } catch (const std::exception &e) {
    p.title = fmt::format("Error: {}", e.what());
  }

  return p;
}

std::vector<TestCase>
LeetCodeCrawler::fetch_test_cases(const std::string &url) {
  std::vector<TestCase> cases;
  auto slug = extract_slug(url);
  if (slug.empty())
    return cases;

  try {
    auto data = query_graphql(slug);
    if (data.is_null())
      return cases;

    if (data.contains("exampleTestcases")) {
      std::string examples = data.value("exampleTestcases", "");
      auto lines = utils::split(examples, '\n');

      for (const auto &line : lines) {
        TestCase tc;
        tc.input = line;
        tc.output = "";
        tc.is_sample = true;
        cases.push_back(tc);
      }
    }
  } catch (...) {
  }

  return cases;
}

} // namespace shuati
