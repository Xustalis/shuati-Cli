#include "shuati/problem_manager.hpp"
#include "shuati/config.hpp"
#include "shuati/crawler.hpp"
#include "shuati/utils/string_utils.hpp"
#include <algorithm>
#include <cpr/cpr.h>
#include <ctime>
#include <fmt/color.h>
#include <fmt/core.h>
#include <fstream>
#include <regex>

namespace shuati {

ProblemManager::ProblemManager(std::shared_ptr<Database> db) : db_(db) {}

void ProblemManager::register_crawler(std::unique_ptr<ICrawler> crawler) {
  crawlers_.push_back(std::move(crawler));
}

void ProblemManager::pull_problem(const std::string &url) {
  if (db_->problem_exists(url)) {
    fmt::print(fg(fmt::color::yellow), "[!] 题目已存在。\n");
    return;
  }

  fmt::print("[*] 正在拉取 {}...\n", url);

  Problem p;
  std::vector<TestCase> cases;
  bool handled = false;

  // Try crawlers first
  for (auto &crawler : crawlers_) {
    if (crawler->can_handle(url)) {
      fmt::print(fg(fmt::color::cyan), "    使用适配器: {}\n",
                 typeid(*crawler).name());
      p = crawler->fetch_problem(url);
      if (p.id.empty() || utils::contains(p.id, "_error")) {
        fmt::print(fg(fmt::color::red), "[!] 爬取失败: {}\n", p.title);
        return;
      }
      cases = crawler->fetch_test_cases(url);
      handled = true;
      break;
    }
  }

  // Fallback to generic HTML parsing
  if (!handled) {
    std::string html = fetch_html(url);
    if (html.empty()) {
      fmt::print(fg(fmt::color::red), "[!] 拉取失败 (网络错误或无效 URL)。\n");
      return;
    }
    p = parse_html(html, url);
    if (p.title == "未命名题目" && html.empty()) {
      fmt::print(fg(fmt::color::red),
                 "[!] HTML 解析失败，无法提取题目信息。\n");
      return;
    }
  }

  // Sanitize ID to prevent path traversal
  p.id = utils::sanitize_filename(p.id);

  std::string problems_dir = Config::DIR_NAME + std::string("/problems/");
  if (!std::filesystem::exists(problems_dir))
    std::filesystem::create_directories(problems_dir);
  std::string filename = problems_dir + p.id + ".md";
  std::ofstream out(filename);
  std::string desc = utils::trim(p.description);
  if (desc.empty()) {
    desc = "(数据抓取自 " + p.source + ")";
  }

  out << "# " << p.title << "\n\n"
      << "来源: " << url << "\n"
      << "难度: " << p.difficulty << "\n"
      << "标签: " << p.tags << "\n\n"
      << "## 题目描述\n\n"
      << desc << "\n\n"
      << "## 笔记\n\n";
  out.close();

  p.content_path = std::filesystem::absolute(filename).string();
  db_->add_problem(p);

  // Save test cases (idempotent + incremental)
  if (!cases.empty()) {
    int inserted = 0;
    for (const auto &tc : cases) {
      if (db_->add_test_case(p.id, tc.input, tc.output, tc.is_sample))
        inserted++;
    }
    fmt::print(fg(fmt::color::cyan), "    测试用例: 新增 {} / 总抓取 {}。\n",
               inserted, cases.size());
  }

  ReviewItem r;
  r.problem_id = p.id;
  r.next_review = std::time(nullptr) + 86400; // 1 day
  r.interval = 1;
  r.ease_factor = 2.5;
  r.repetitions = 0;
  db_->upsert_review(r);

  fmt::print(fg(fmt::color::green), "[+] 题目 '{}' 已保存至 {}\n", p.title,
             filename);
  fmt::print("    使用 shuati solve {} 开始练习\n", p.id);
}

std::string ProblemManager::create_local(const std::string &title,
                                         const std::string &tags,
                                         const std::string &difficulty) {
  Problem p;
  p.id = "local_" + std::to_string(std::time(nullptr));
  p.source = "local";
  p.title = title;
  p.tags = tags;
  p.difficulty = difficulty;
  p.created_at = std::time(nullptr);

  std::string problems_dir = Config::DIR_NAME + std::string("/problems/");
  if (!std::filesystem::exists(problems_dir))
    std::filesystem::create_directories(problems_dir);
  std::string filename = problems_dir + p.id + ".md";
  std::ofstream out(filename);
  out << "# " << title << "\n\n"
      << "标签: " << tags << "\n"
      << "难度: " << difficulty << "\n\n"
      << "## 题目描述\n\n"
      << "(在此处输入题目描述)\n\n"
      << "## 输入/输出\n\n"
      << "## 示例\n\n";
  out.close();

  p.content_path = std::filesystem::absolute(filename).string();
  db_->add_problem(p);

  ReviewItem r;
  r.problem_id = p.id;
  r.next_review = std::time(nullptr) + 86400;
  db_->upsert_review(r);

  fmt::print(fg(fmt::color::green), "[+] 本地题目 '{}' 已创建: {}\n", title,
             filename);
  return p.id;
}

Problem ProblemManager::get_problem(const std::string &id) {
  // Try parsing as integer first
  if (auto tid = utils::try_parse_number<int>(id)) {
    auto p = db_->get_problem_by_display_id(*tid);
    if (!p.id.empty())
      return p;
  }

  // Fallback to UUID string
  return db_->get_problem(id);
}

void ProblemManager::delete_problem(int tid) {
  auto p = db_->get_problem_by_display_id(tid);
  if (!p.id.empty())
    delete_problem(p.id);
}

void ProblemManager::delete_problem(const std::string &id) {
  auto p = db_->get_problem(id);
  if (p.id.empty())
    return;

  // Delete problem file
  if (std::filesystem::exists(p.content_path)) {
    std::filesystem::remove(p.content_path);
  }

  // Delete solution file(s)
  for (const auto &ext : {".cpp", ".py"}) {
    std::string sol = "solution_" + p.id + ext;
    if (std::filesystem::exists(sol))
      std::filesystem::remove(sol);
  }

  // Delete from DB
  if (p.display_id > 0)
    db_->delete_problem(p.display_id);

  fmt::print(fg(fmt::color::green), "[+] 题目已删除: {} (TID: {})\n", p.title,
             p.display_id);
}

std::vector<Problem> ProblemManager::list_problems() {
  return db_->get_all_problems();
}

std::string ProblemManager::fetch_html(const std::string &url) {
  try {
    auto r = cpr::Get(cpr::Url{url}, cpr::Timeout{10000});
    if (r.status_code == 200)
      return r.text;
    fmt::print("[!] HTTP 状态码: {}\n", r.status_code);
  } catch (const std::exception &e) {
    fmt::print("[!] 网络异常: {}\n", e.what());
  }
  return "";
}

Problem ProblemManager::parse_html(const std::string &html,
                                   const std::string &url) {
  Problem p;
  p.id = "web_" + std::to_string(std::time(nullptr));
  p.source = "web";
  p.url = url;
  p.created_at = std::time(nullptr);

  // Extract <title>
  p.title = utils::extract_title_from_html(html);
  p.description = utils::extract_meta_content(html, "description");
  if (p.title.empty()) {
    p.title = "未命名题目";
  } else {
    // Remove common suffixes
    auto pos = p.title.find(" - ");
    if (pos != std::string::npos)
      p.title = p.title.substr(0, pos);
  }

  return p;
}

} // namespace shuati
