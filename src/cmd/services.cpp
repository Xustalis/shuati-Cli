#include "commands.hpp"
#include "shuati/compiler_doctor.hpp"
#include "shuati/utils/encoding.hpp"
#include "shuati/adapters/leetcode_crawler.hpp"
#include "shuati/adapters/codeforces_crawler.hpp"
#include "shuati/adapters/luogu_crawler.hpp"
#include "shuati/adapters/lanqiao_crawler.hpp"
#include <fmt/core.h>
#include <fmt/color.h>
#include <iostream>

#ifdef _WIN32
#include <windows.h>
#endif

namespace shuati {
namespace cmd {

namespace fs = std::filesystem;

fs::path find_root_or_die() {
    auto root = Config::find_root();
    if (root.empty()) {
        fmt::print(fg(fmt::color::red), "[!] 未找到项目根目录。请先运行: shuati init\n");
        throw std::runtime_error("Root not found");
    }
    return root;
}

#ifdef _WIN32
std::string executable_path_utf8() {
    std::wstring wpath;
    wpath.resize(32768);
    DWORD n = GetModuleFileNameW(nullptr, wpath.data(), (DWORD)wpath.size());
    if (n == 0) return {};
    wpath.resize(n);
    return shuati::utils::wide_to_utf8(wpath);
}
#else
std::string executable_path_utf8() {
    return "shuati"; // Placeholder for linux/mac if not strictly needed
}
#endif

Services Services::load(const fs::path& root, bool skip_doctor) {
    Services s;
    try {
        s.cfg = Config::load(Config::config_path(root));
        
        // Simple caching for Database to avoid overhead in REPL
        static std::weak_ptr<Database> db_cache;
        static std::string db_cache_path;
        
        std::shared_ptr<Database> db = db_cache.lock();
        std::string current_db_path = Config::db_path(root).string();
        
        if (!db || db_cache_path != current_db_path) {
            db = std::make_shared<Database>(current_db_path);
            db_cache = db;
            db_cache_path = current_db_path;
        }
        s.db = db;
        s.pm  = std::make_shared<ProblemManager>(s.db);
        
        // Register crawlers
        s.pm->register_crawler(std::make_unique<LeetCodeCrawler>());
        s.pm->register_crawler(std::make_unique<CodeforcesCrawler>());
        s.pm->register_crawler(std::make_unique<LuoguCrawler>());
        s.pm->register_crawler(std::make_unique<LanqiaoCrawler>(nullptr, s.cfg.lanqiao_cookie));
        
        s.ma  = std::make_shared<MistakeAnalyzer>(s.db);
        s.mm  = std::make_unique<MemoryManager>(*s.db); // Database& constructor
        s.ai  = std::make_unique<AICoach>(s.cfg, s.mm.get());
        s.companion = std::make_unique<CompanionServer>(*s.pm, *s.db);
        s.judge = std::make_unique<Judge>();
    } catch (const std::exception& e) {
        fmt::print(fg(fmt::color::red), "[!] 服务加载失败: {}\n", e.what());
        throw;
    }

    // Environment Check (skip if requested, since it can be very slow)
    if (!skip_doctor) {
        std::vector<std::string> missing;
        if (!CompilerDoctor::check_environment(missing)) {
             fmt::print(fg(fmt::color::yellow), "[!] 警告: 未检测到以下环境工具，可能无法运行代码:\n");
             for (const auto& t : missing) {
                 fmt::print("    - {}\n", t);
             }
             fmt::print("\n");
        }
    }
    
    return s;
}

std::string make_solution_filename(const Problem& prob, const std::string& language) {
    std::string ext = (language == "python" || language == "py") ? ".py" : ".cpp";
    
    // Sanitize title for filename: keep CJK chars, alphanumeric, underscore
    std::string safe_title;
    std::string title = prob.title;
    
    // Limit title length
    if (title.size() > 40) {
        // Truncate at UTF-8 boundary
        size_t len = 40;
        while (len > 0) {
            unsigned char c = static_cast<unsigned char>(title[len]);
            if ((c & 0xC0) != 0x80) break;
            len--;
        }
        title = title.substr(0, len);
    }
    
    for (size_t i = 0; i < title.size(); ) {
        unsigned char c = static_cast<unsigned char>(title[i]);
        if (c < 0x80) {
            // ASCII
            if (std::isalnum(c) || c == '_') {
                safe_title += static_cast<char>(c);
            } else if (c == ' ' || c == '-') {
                safe_title += '_';
            }
            i++;
        } else {
            // Multi-byte UTF-8 (CJK etc.) - keep as-is
            size_t bytes = 1;
            if ((c & 0xE0) == 0xC0) bytes = 2;
            else if ((c & 0xF0) == 0xE0) bytes = 3;
            else if ((c & 0xF8) == 0xF0) bytes = 4;
            safe_title += title.substr(i, bytes);
            i += bytes;
        }
    }
    
    // Remove trailing underscores
    while (!safe_title.empty() && safe_title.back() == '_') safe_title.pop_back();
    
    if (safe_title.empty()) safe_title = prob.id;
    
    // Format: TID_title.ext (e.g., "3_两数之和.cpp")
    return std::to_string(prob.display_id) + "_" + safe_title + ext;
}

std::string find_solution_file(const Problem& prob, const std::string& language) {
    namespace fs = std::filesystem;
    std::string ext = (language == "python" || language == "py") ? ".py" : ".cpp";
    
    // Try new naming first
    std::string new_name = make_solution_filename(prob, language);
    if (fs::exists(shuati::utils::utf8_path(new_name))) return new_name;
    
    // Fallback: old naming (solution_ID.ext)
    std::string old_name = "solution_" + prob.id + ext;
    if (fs::exists(shuati::utils::utf8_path(old_name))) return old_name;
    
    // Not found — return new name for creation
    return "";
}

} // namespace cmd
} // namespace shuati
