#include "problem_manager.hpp"
#include <cpr/cpr.h>
#include <fmt/core.h>
#include <regex>
#include <fstream>
#include <ctime>
#include "config.hpp"
#include <fmt/color.h>

namespace shuati {

ProblemManager::ProblemManager(std::shared_ptr<Database> db) : db_(db) {}

void ProblemManager::pull_problem(const std::string& url) {
    if (db_->problem_exists(url)) {
        fmt::print(fg(fmt::color::yellow), "[!] 题目已存在。\n");
        return;
    }

    fmt::print("[*] 正在拉取 {}...\n", url);
    std::string html = fetch_html(url);
    if (html.empty()) {
        fmt::print(fg(fmt::color::red), "[!] 拉取失败 (网络错误或无效 URL)。\n");
        return;
    }

    Problem p = parse_html(html, url);

    // Validate parsing result - don't save if title is generic and HTML was empty
    if (p.title == "未命名题目" && html.empty()) {
        fmt::print(fg(fmt::color::red), "[!] HTML 解析失败，无法提取题目信息。\n");
        return;
    }

    std::string problems_dir = Config::DIR_NAME + std::string("/problems/");
    if (!std::filesystem::exists(problems_dir)) std::filesystem::create_directories(problems_dir);
    std::string filename = problems_dir + p.id + ".md";
    std::ofstream out(filename);
    out << "# " << p.title << "\n\n"
        << "来源: " << url << "\n\n"
        << "## 题目描述\n\n"
        << "(在此处补充题目描述)\n\n"
        << "## 笔记\n\n";
    out.close();

    p.content_path = std::filesystem::absolute(filename).string();
    db_->add_problem(p);

    ReviewItem r;
    r.problem_id = p.id;
    r.next_review = std::time(nullptr) + 86400; // 1 day
    r.interval = 1;
    r.ease_factor = 2.5;
    r.repetitions = 0;
    db_->upsert_review(r);

    fmt::print(fg(fmt::color::green), "[+] 题目 '{}' 已保存至 {}\n", p.title, filename);
    fmt::print("    使用 shuati solve {} 开始练习\n", p.id);
}

void ProblemManager::create_local(const std::string& title, const std::string& tags, const std::string& difficulty) {
    Problem p;
    p.id = "local_" + std::to_string(std::time(nullptr));
    p.source = "local";
    p.title = title;
    p.tags = tags;
    p.difficulty = difficulty;
    p.created_at = std::time(nullptr);

    std::string problems_dir = Config::DIR_NAME + std::string("/problems/");
    if (!std::filesystem::exists(problems_dir)) std::filesystem::create_directories(problems_dir);
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

    fmt::print(fg(fmt::color::green), "[+] 本地题目 '{}' 已创建: {}\n", title, filename);
}


Problem ProblemManager::get_problem(const std::string& id) {
    // Try parsing as integer first
    try {
        int tid = std::stoi(id);
        auto p = db_->get_problem_by_display_id(tid);
        if (!p.id.empty()) return p;
    } catch (...) {}
    
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

    // Delete problem file
    if (std::filesystem::exists(p.content_path)) {
        std::filesystem::remove(p.content_path);
    }
    
    // Delete solution file(s)
    for (const auto& ext : {".cpp", ".py"}) {
        std::string sol = "solution_" + p.id + ext;
        if (std::filesystem::exists(sol)) std::filesystem::remove(sol);
    }

    // Delete from DB
    // We need delete_problem(string id) in DB too? Or map back to TID?
    // DB only has delete_problem(int tid).
    // Let's use tid if available, or add delete_problem(string id) to DB.
    // Actually DB::delete_problem(int tid) calls internal logic using UUID.
    // So we need DB to support delete by UUID or just map here.
    if (p.display_id > 0) db_->delete_problem(p.display_id);
    
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
    std::regex re("<title>(.*?)</title>", std::regex::icase);
    std::smatch m;
    if (std::regex_search(html, m, re)) {
        p.title = m[1].str();
        auto pos = p.title.find(" - ");
        if (pos != std::string::npos) p.title = p.title.substr(0, pos);
    } else {
        p.title = "未命名题目";
    }

    return p;
}

} // namespace shuati
