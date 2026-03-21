#include "shuati/utils/project_utils.hpp"
#include <fmt/core.h>
#include <fmt/color.h>
#include <algorithm>

namespace shuati {
namespace utils {

std::filesystem::path find_root_or_die() {
    auto root = Config::find_root();
    if (root.empty()) {
        fmt::print(fg(fmt::color::red), "[!] 未找到项目根目录。请先运行: shuati init\n");
        throw std::runtime_error("Root not found");
    }
    return root;
}

std::string canonical_source(const std::string& source) {
    std::string s = source;
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);
    if (s.find("leetcode") != std::string::npos) return "leetcode";
    if (s.find("codeforces") != std::string::npos) return "codeforces";
    if (s.find("luogu") != std::string::npos) return "luogu";
    if (s.find("lanqiao") != std::string::npos) return "lanqiao";
    if (s.find("local") != std::string::npos) return "local";
    if (s.empty()) return "local";
    return "other";
}

} // namespace utils
} // namespace shuati
