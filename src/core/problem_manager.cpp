#include "problem_manager.hpp"
#include <cpr/cpr.h>
#include <fmt/core.h>
#include <regex>
#include <fstream>
#include <ctime>
#include "config.hpp"

namespace shuati {

ProblemManager::ProblemManager(std::shared_ptr<Database> db) : db_(db) {}

void ProblemManager::pull_problem(const std::string& url) {
    if (db_->problem_exists(url)) {
        fmt::print("[!] Problem already exists.\n");
        return;
    }

    fmt::print("[*] Fetching {}...\n", url);
    std::string html = fetch_html(url);
    if (html.empty()) {
        fmt::print("[!] Failed to fetch content.\n");
        return;
    }

    Problem p = parse_html(html, url);

    // Save to markdown file in current directory
    std::string filename = p.id + ".md";
    std::ofstream out(filename);
    out << "# " << p.title << "\n\n"
        << "Source: " << url << "\n\n"
        << "---\n\n"
        << "(Write your notes here)\n";
    out.close();

    p.content_path = std::filesystem::absolute(filename).string();
    db_->add_problem(p);

    // Auto-schedule first review (now + 1 day)
    ReviewItem r;
    r.problem_id = p.id;
    r.next_review = std::time(nullptr) + 86400;
    r.interval = 1;
    r.ease_factor = 2.5;
    r.repetitions = 0;
    db_->upsert_review(r);

    fmt::print("[+] Problem '{}' saved -> {}\n", p.title, filename);
}

void ProblemManager::create_local(const std::string& title, const std::string& tags, const std::string& difficulty) {
    Problem p;
    p.id = "local_" + std::to_string(std::time(nullptr));
    p.source = "local";
    p.title = title;
    p.tags = tags;
    p.difficulty = difficulty;
    p.created_at = std::time(nullptr);

    std::string filename = p.id + ".md";
    std::ofstream out(filename);
    out << "# " << title << "\n\n"
        << "Tags: " << tags << "\n"
        << "Difficulty: " << difficulty << "\n\n"
        << "## Description\n\n"
        << "(Write problem description here)\n\n"
        << "## Input / Output\n\n"
        << "## Examples\n\n";
    out.close();

    p.content_path = std::filesystem::absolute(filename).string();
    db_->add_problem(p);

    ReviewItem r;
    r.problem_id = p.id;
    r.next_review = std::time(nullptr) + 86400;
    db_->upsert_review(r);

    fmt::print("[+] Created '{}' -> {}\n", title, filename);
}

Problem ProblemManager::get_problem(const std::string& id) {
    return db_->get_problem(id);
}

std::vector<Problem> ProblemManager::list_problems() {
    return db_->get_all_problems();
}

std::string ProblemManager::fetch_html(const std::string& url) {
    try {
        auto r = cpr::Get(cpr::Url{url}, cpr::Timeout{10000});
        if (r.status_code == 200) return r.text;
        fmt::print("[!] HTTP {}\n", r.status_code);
    } catch (const std::exception& e) {
        fmt::print("[!] Network error: {}\n", e.what());
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
        // Clean up common suffixes
        auto pos = p.title.find(" - ");
        if (pos != std::string::npos) p.title = p.title.substr(0, pos);
    } else {
        p.title = "Untitled";
    }

    return p;
}

} // namespace shuati
