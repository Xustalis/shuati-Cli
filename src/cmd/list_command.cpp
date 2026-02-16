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

void cmd_list(CommandContext& ctx) {
    try {
        auto svc = Services::load(find_root_or_die());
        auto problems = svc.pm->list_problems();
        if (problems.empty()) {
            std::cout << "题库为空。" << std::endl;
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
            
            // Format Status
            std::string status = "-";
            if (!p.last_verdict.empty()) {
                if (p.last_verdict == "AC") {
                    status = "AC"; 
                } else {
                    status = p.last_verdict + " " + std::to_string(p.pass_count) + "/" + std::to_string(p.total_count);
                }
            }
            
            // Format Time
            std::string time_str = "-";
            if (p.last_checked_at > 0) {
                std::time_t t = p.last_checked_at;
                std::tm tm;
                #ifdef _WIN32
                localtime_s(&tm, &t);
                #else
                localtime_r(&t, &tm);
                #endif
                char buf[32];
                std::strftime(buf, sizeof(buf), "%Y-%m-%d", &tm);
                time_str = buf;
            }

            // Print row
            std::cout << pad_string(std::to_string(p.display_id), 6)
                      << pad_string(ensure_utf8(p.id.substr(0, 18)), 20)
                      << pad_string(title, 25)
                      << pad_string(ensure_utf8(p.difficulty), 8)
                      << pad_string(status, 15)
                      << pad_string(time_str, 15)
                      << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "[!] 错误: " << e.what() << std::endl;
    }
}

} // namespace cmd
} // namespace shuati
