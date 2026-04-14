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
        auto db = std::make_shared<Database>(Config::db_path(root).string());
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
    auto root = find_root_or_die();
    // Defense-in-depth: sanitize both fields even though they were sanitized at DB write time.
    std::string safe_id = sanitize_filename(prob.id);
    std::string safe_src = canonical_source(sanitize_filename(prob.source));
    auto dir = root / ".shuati" / "problems" / safe_src / safe_id;
    if (!std::filesystem::exists(dir)) {
        std::filesystem::create_directories(dir);
    }
    return (dir / ("solution" + ext)).string();
}

std::string find_solution_file(const Problem& prob, const std::string& language) {
    namespace fs = std::filesystem;
    std::string ext = (language == "python" || language == "py") ? ".py" : ".cpp";
    
    auto root = find_root_or_die();
    
    // 1. Canonical path (new source-based structure)
    auto canonical = root / ".shuati" / "problems" / canonical_source(prob.source) / prob.id / ("solution" + ext);
    if (fs::exists(canonical)) return canonical.string();
    
    // 2. Fallbacks (old UUID structure)
    auto fallback = root / ".shuati" / "problems" / prob.id / ("solution" + ext);
    if (fs::exists(fallback)) return fallback.string();
    
    // 3. Old display_id structure (also relative to root, to prevent cwd dependence)
    std::string prefix = std::to_string(prob.display_id) + "_";
    try {
        if (fs::exists(root)) {
            for (const auto& entry : fs::directory_iterator(root)) {
                std::string name = entry.path().filename().string();
                if (name.rfind(prefix, 0) == 0 && name.size() > ext.size() &&
                    name.substr(name.size() - ext.size()) == ext) {
                    return entry.path().string();
                }
            }
        }
    } catch (...) {}
    
    return "";
}

} // namespace cmd
} // namespace shuati
