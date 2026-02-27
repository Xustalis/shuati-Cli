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
        if (!ctx.list_filter.empty() && ctx.list_filter != "all") {
            if (ctx.list_filter == "review") {
                auto reviews = svc.db->get_due_reviews(std::time(nullptr));
                std::unordered_set<std::string> due_ids;
                for (const auto& r : reviews) due_ids.insert(r.problem_id);
                
                auto it = std::remove_if(problems.begin(), problems.end(), [&](const Problem& p) {
                    return due_ids.find(p.id) == due_ids.end();
                });
                problems.erase(it, problems.end());
                
                if (problems.empty()) {
                    std::cout << "今天没有需要复习的题目。" << std::endl;
                    return;
                }
            } else {
                auto it = std::remove_if(problems.begin(), problems.end(), [&](const Problem& p) {
                    if (ctx.list_filter == "ac") return p.last_verdict != "AC";
                    if (ctx.list_filter == "failed") return p.last_verdict == "AC" || p.last_verdict.empty();
                    if (ctx.list_filter == "unaudited") return !p.last_verdict.empty();
                    return false;
                });
                problems.erase(it, problems.end());
            }
        }

        if (problems.empty()) {
            std::cout << "没有符合条件的题目。" << std::endl;
            return;
        }

        // Header
        std::cout << pad_string("TID", 6)
                  << pad_string("ID", 20)
                  << pad_string("标题", 25)
                  << pad_string("难度", 8)
                  << pad_string("状态", 15)
                  << pad_string("最后检查", 15) << std::endl;
        std::cout << std::string(95, '-') << std::endl;

        for (auto& p : problems) {
            std::string title = shorten_utf8_lossy(p.title, 24);
            
            // Format Status with Color
            std::string status = "-";
            std::string status_display = "-";
            
            if (!p.last_verdict.empty()) {
                if (p.last_verdict == "AC") {
                    status = "\033[32mAC\033[0m"; // Green
                    status_display = "AC";
                } else {
                    std::string v = p.last_verdict;
                    if (v == "WA") v = "\033[31mWA\033[0m"; // Red
                    else if (v == "TLE") v = "\033[33mTLE\033[0m"; // Yellow
                    else if (v == "CE") v = "\033[33mCE\033[0m"; // Yellow
                    else v = "\033[35m" + v + "\033[0m"; // Magenta for others

                    status = v + " " + std::to_string(p.pass_count) + "/" + std::to_string(p.total_count);
                    status_display = p.last_verdict + " " + std::to_string(p.pass_count) + "/" + std::to_string(p.total_count);
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

            // Make ID clickable (View command)
            // URL scheme: x-shuati-view://<id> -> handled by terminal? 
            // Or just a file link? 
            // OSC 8 is usually for http/https or file://. 
            // Let's use a dummy scheme or just file:// if we had a web UI. 
            // Actually VSCode handles file:// links well. 
            // Let's just link to the problem verification file for now, or just leave it as text if no web UI.
            // User asked for "Click to view details". In a terminal, usually means opening a file or URL.
            // Maybe we can construct a command to run? No, OSC 8 doesn't run commands.
            // We can link to the test_report.json file.
            std::string report_url = "file:///" + (find_root_or_die() / ".shuati" / "problems" / p.id / "test_report.json").string();
            // Escape backslashes for Windows? 
            // Actually std::filesystem::path::string() on Windows uses backslashes, which might break URI.
            // Need forward slashes for URI.
            std::string uri_path = (find_root_or_die() / ".shuati" / "problems" / p.id / "test_report.json").string();
            std::replace(uri_path.begin(), uri_path.end(), '\\', '/');
            report_url = "file:///" + uri_path;

            std::string id_display = link(ensure_utf8(p.id.substr(0, 18)), report_url);

            // Print row
            // Note: pad_string calculates visual width, so we shouldn't pad the ANSI strings directly if they contain invisible chars.
            // We need to pad based on visible length, then append ANSI.
            // For Status, we calculated status_display (clean text) for width, but we want to print colored status.
            // pad_string currently takes string and returns string. 
            // We need to manually pad.
            
            size_t id_w = utf8_display_width(ensure_utf8(p.id.substr(0, 18)));
            std::string id_padding = (id_w < 20) ? std::string(20 - id_w, ' ') : "";

            size_t status_w = utf8_display_width(status_display);
            std::string status_padding = (status_w < 15) ? std::string(15 - status_w, ' ') : "";

            std::cout << pad_string(std::to_string(p.display_id), 6)
                      << id_display << id_padding
                      << pad_string(title, 25)
                      << pad_string(ensure_utf8(p.difficulty), 8)
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
