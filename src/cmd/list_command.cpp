#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <filesystem>
#include <thread>
#include <future>
#include <chrono>

#include "shuati/database.hpp"
#include "shuati/problem_manager.hpp"
#include "shuati/judge.hpp"
#include "shuati/ai_coach.hpp"
#include "shuati/config.hpp"

#include "commands.hpp"
#include "shuati/utils/encoding.hpp"

namespace shuati {
namespace cmd {

using shuati::utils::ensure_utf8;
using shuati::utils::shorten_utf8_lossy;
using shuati::utils::pad_string;
using shuati::utils::utf8_display_width;

// Helper for OSC 8 hyperlinks
std::string link(const std::string& text, const std::string& url) {
    // OSC 8 ; params ; url ST text OSC 8 ;; ST
    return "\033]8;;" + url + "\033\\" + text + "\033]8;;\033\\";
}

void cmd_list(CommandContext& ctx) {
    try {
        auto svc = Services::load(find_root_or_die());
        auto problems = svc.pm->list_problems();
        if (problems.empty()) {
            std::cout << "题库为空。" << std::endl;
            return;
        }

        // Filtering
        std::string status_filter = ctx.list_filter.empty() ? "all" : ctx.list_filter;
        std::string diff_filter = ctx.list_difficulty.empty() ? "all" : ctx.list_difficulty;
        std::string source_filter = ctx.list_source.empty() ? "all" : ctx.list_source;
        
        problems = svc.pm->filter_problems(problems, status_filter, diff_filter, source_filter);

        if (problems.empty()) {
            std::cout << "没有符合条件的题目。" << std::endl;
            return;
        }

        // Header
        std::cout << pad_string("TID", 6)
                  << pad_string("ID", 20)
                  << pad_string("标题", 25)
                  << pad_string("难度", 8)
                  << pad_string("来源", 10)
                  << pad_string("状态", 15)
                  << pad_string("最后检查", 15) << std::endl;
        std::cout << std::string(105, '-') << std::endl;

        // Cache root path for link generation
        auto root_path = find_root_or_die();

        for (auto& p : problems) {
            std::string title = shorten_utf8_lossy(p.title, 24);
            
            // Format Status with Color
            std::string status = "-";
            std::string status_display = "-";
            
            if (!p.last_verdict.empty()) {
                if (p.last_verdict == "AC") {
                    status = "AC";
                    status_display = "AC";
                } else {
                    std::string v = p.last_verdict;
                    status = v + " " + std::to_string(p.pass_count) + "/" + std::to_string(p.total_count);
                    status_display = status;
                }
            }
            
            // Format Time
            std::string time_str = "-";
            if (p.last_checked_at > 0) {
                std::time_t t = p.last_checked_at;
                std::tm tm{};
                #ifdef _WIN32
                localtime_s(&tm, &t);
                #else
                localtime_r(&t, &tm);
                #endif
                char buf[32];
                std::strftime(buf, sizeof(buf), "%Y-%m-%d", &tm);
                time_str = buf;
            }

            // Generate OSC 8 hyperlink to test report file
            std::string uri_path = (root_path / ".shuati" / "problems" / canonical_source(p.source) / p.id / "test_report.json").string();
            std::replace(uri_path.begin(), uri_path.end(), '\\', '/');
            std::string report_url = "file:///" + uri_path;

            std::string id_display = link(ensure_utf8(p.id.substr(0, 18)), report_url);

            // Pad based on visible width (ANSI codes are invisible)
            size_t id_w = utf8_display_width(ensure_utf8(p.id.substr(0, 18)));
            std::string id_padding = (id_w < 20) ? std::string(20 - id_w, ' ') : "";

            size_t status_w = utf8_display_width(status_display);
            std::string status_padding = (status_w < 15) ? std::string(15 - status_w, ' ') : "";

            std::cout << pad_string(std::to_string(p.display_id), 6)
                      << id_display << id_padding
                      << pad_string(title, 25)
                      << pad_string(ensure_utf8(p.difficulty), 8)
                      << pad_string(ensure_utf8(canonical_source(p.source)), 10)
                      << status << status_padding
                      << pad_string(time_str, 15)
                      << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "[!] 错误: " << e.what() << std::endl;
    }
}

} // namespace cmd
} // namespace shuati
