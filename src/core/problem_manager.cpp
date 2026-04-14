#include "shuati/problem_manager.hpp"
#include "shuati/utils/string_utils.hpp"
#include <cpr/cpr.h>
#include <fmt/core.h>
#include <fmt/color.h>
#include <regex>
#include <fstream>
#include <ctime>
#include "shuati/config.hpp"
#include <algorithm>
#include "shuati/crawler.hpp"

namespace shuati {

ProblemManager::ProblemManager(std::shared_ptr<Database> db) : db_(db) {}

void ProblemManager::register_crawler(std::unique_ptr<ICrawler> crawler) {
    crawlers_.push_back(std::move(crawler));
}

void ProblemManager::pull_problem(const std::string& url) {
    if (db_->problem_exists(url)) {
        fmt::print(fg(fmt::color::yellow), "[!] 题目已存在。\n");
        return;
    }

    fmt::print("[*] 正在拉取 {}...\n", url);

    Problem p;
    std::vector<TestCase> cases;
    bool handled = false;

    // Try crawlers first
    for (auto& crawler : crawlers_) {
        if (crawler->can_handle(url)) {
            fmt::print(fg(fmt::color::cyan), "    使用适配器: {}\n", typeid(*crawler.get()).name());
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
            fmt::print(fg(fmt::color::red), "[!] HTML 解析失败，无法提取题目信息。\n");
            return;
        }
    }

    // Sanitize ID and source to prevent path traversal
    p.id = utils::sanitize_filename(p.id);
    p.source = utils::sanitize_filename(p.source);

    auto root = Config::find_root();
    if (root.empty()) throw std::runtime_error("Root not found");
    
    std::filesystem::path prob_dir = root / Config::DIR_NAME / "problems" / p.source / p.id;
    if (!std::filesystem::exists(prob_dir)) std::filesystem::create_directories(prob_dir);
    std::string filename = (prob_dir / "problem.md").string();
    
    // DB is the authoritative store. Write all DB records under a transaction;
    // the file is written only after the transaction commits successfully.
    {
        SQLite::Transaction tx(*db_);
        p.content_path = std::filesystem::absolute(filename).string();
        db_->add_problem(p);

        if (!cases.empty()) {
            for (const auto& tc : cases) {
                db_->add_test_case(p.id, tc.input, tc.output, tc.is_sample);
            }
            fmt::print(fg(fmt::color::cyan), "    获取到 {} 个测试用例。\n", cases.size());
        }

        ReviewItem r;
        r.problem_id = p.id;
        r.next_review = std::time(nullptr) + 86400; // 1 day
        r.interval = 1;
        r.ease_factor = 2.5;
        r.repetitions = 0;
        db_->upsert_review(r);

        tx.commit();  // DB now consistent; safe to write derived file
    }

    // Write file only after DB transaction commits
    std::ofstream out(filename);
    out << "# " << p.title << "\n\n"
        << "来源: " << url << "\n"
        << "难度: " << p.difficulty << "\n"
        << "标签: " << p.tags << "\n\n"
        << "## 题目描述\n\n"
        << p.description << "\n\n"
        << "(数据抓取自 " << p.source << ")\n\n"
        << "## 笔记\n\n";
    out.close();

    fmt::print(fg(fmt::color::green), "[+] 题目 '{}' 已保存至 {}\n", p.title, filename);
    fmt::print("    使用 shuati solve {} 开始练习\n", p.id);
}

std::string ProblemManager::create_local(const std::string& title, const std::string& tags, const std::string& difficulty) {
    Problem p;
    p.id = "local_" + std::to_string(std::time(nullptr));
    p.source = "local";
    p.title = title;
    p.tags = tags;
    p.difficulty = difficulty.empty() ? "medium" : difficulty;
    p.created_at = std::time(nullptr);

    auto root = Config::find_root();
    std::filesystem::path prob_dir;
    if (!root.empty()) {
        prob_dir = root / Config::DIR_NAME / "problems" / utils::canonical_source(p.source) / p.id;
    } else {
        prob_dir = std::filesystem::path(Config::DIR_NAME) / "problems" / utils::canonical_source(p.source) / p.id;
    }
    if (!std::filesystem::exists(prob_dir)) std::filesystem::create_directories(prob_dir);
    auto file_path = prob_dir / "problem.md";
    std::ofstream out(file_path);
    out << "# " << title << "\n\n"
        << "标签: " << tags << "\n"
        << "难度: " << difficulty << "\n\n"
        << "## 题目描述\n\n"
        << "(在此处输入题目描述)\n\n"
        << "## 输入/输出\n\n"
        << "## 示例\n\n";
    out.close();

    p.content_path = std::filesystem::absolute(file_path).string();
    db_->add_problem(p);

    ReviewItem r;
    r.problem_id = p.id;
    r.next_review = std::time(nullptr) + 86400;
    db_->upsert_review(r);

    fmt::print(fg(fmt::color::green), "[+] 本地题目 '{}' 已创建: {}\n", title, p.content_path);
    return p.id;
}


Problem ProblemManager::get_problem(const std::string& id) {
    // Try parsing as integer first
    if (auto tid = utils::try_parse_number<int>(id)) {
        auto p = db_->get_problem_by_display_id(*tid);
        if (!p.id.empty()) return p;
    }

    // Fallback to UUID string
    return db_->get_problem(id);
}

void ProblemManager::delete_problem(int tid) {
    auto p = db_->get_problem_by_display_id(tid);
    if (!p.id.empty()) delete_problem(p.id);
}

void ProblemManager::delete_problem(const std::string& id) {
    auto p = db_->get_problem(id);
    if (p.id.empty()) return;

    // Delete from DB first — if this fails, files are untouched
    if (p.display_id > 0) db_->delete_problem(p.display_id);

    // Only delete files after DB deletion succeeds
    if (std::filesystem::exists(p.content_path)) {
        std::filesystem::remove(p.content_path);
    }

    // Delete solution file(s) — both old and new naming formats
    for (const auto& ext : {".cpp", ".py"}) {
        // Old format
        std::string sol_old = "solution_" + p.id + ext;
        if (std::filesystem::exists(sol_old)) std::filesystem::remove(sol_old);
        // New format (TID_title.ext) — scan for files starting with "TID_"
        std::string tid_prefix = std::to_string(p.display_id) + "_";
        try {
            for (const auto& entry : std::filesystem::directory_iterator(".")) {
                std::string name = entry.path().filename().string();
                if (name.rfind(tid_prefix, 0) == 0 && name.size() > 4 &&
                    name.substr(name.size() - std::string(ext).size()) == ext) {
                    std::filesystem::remove(entry.path());
                }
            }
        } catch (...) {}
    }

    fmt::print(fg(fmt::color::green), "[+] 题目已删除: {} (TID: {})\n", p.title, p.display_id);
}

std::vector<Problem> ProblemManager::list_problems() {
    return db_->get_all_problems();
}

std::string ProblemManager::fetch_html(const std::string& url) {
    try {
        auto r = cpr::Get(cpr::Url{url}, cpr::Timeout{10000});
        if (r.status_code == 200) return r.text;
        fmt::print("[!] HTTP 状态码: {}\n", r.status_code);
    } catch (const std::exception& e) {
        fmt::print("[!] 网络异常: {}\n", e.what());
    }
    return "";
}

Problem ProblemManager::parse_html(const std::string& html, const std::string& url) {
    Problem p;
    p.id = "web_" + std::to_string(std::time(nullptr));
    p.source = "web";
    p.url = url;
    p.created_at = std::time(nullptr);

    // Extract <title>
    p.title = utils::extract_title_from_html(html);
    if (p.title.empty()) {
        p.title = "未命名题目";
    } else {
        // Remove common suffixes
        auto pos = p.title.find(" - ");
        if (pos != std::string::npos) p.title = p.title.substr(0, pos);
    }

    return p;
}

std::vector<Problem> ProblemManager::filter_problems(const std::vector<Problem>& problems,
                                                     const std::string& status_filter,
                                                     const std::string& difficulty_filter,
                                                     const std::string& source_filter) {
    std::vector<Problem> result = problems;
    
    if (status_filter == "review") {
        auto reviews = db_->get_due_reviews(std::time(nullptr));
        std::unordered_set<std::string> due_ids;
        for (const auto& r : reviews) due_ids.insert(r.problem_id);
        
        result.erase(std::remove_if(result.begin(), result.end(),
            [&](const Problem& p) {
                if (due_ids.find(p.id) == due_ids.end()) return true;
                if (difficulty_filter != "all" && p.difficulty != difficulty_filter) return true;
                if (source_filter != "all" && utils::canonical_source(p.source) != source_filter) return true;
                return false;
            }), result.end());
        return result;
    }

    result.erase(std::remove_if(result.begin(), result.end(),
        [&](const Problem& p) {
            if (difficulty_filter != "all" && p.difficulty != difficulty_filter) return true;
            if (source_filter != "all" && utils::canonical_source(p.source) != source_filter) return true;
            if (status_filter == "ac") return p.last_verdict != "AC";
            if (status_filter == "failed") return p.last_verdict == "AC" || p.last_verdict.empty() || p.last_verdict == "SKIPPED";
            if (status_filter == "unaudited") return !p.last_verdict.empty() && p.last_verdict != "SKIPPED";
            return false;
        }), result.end());
    return result;
}
} // namespace shuati
