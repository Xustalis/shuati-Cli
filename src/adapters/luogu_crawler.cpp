#include <filesystem>  // MSVC workaround: include before other headers

#include "shuati/adapters/luogu_crawler.hpp"
#include "shuati/adapters/base_crawler.hpp"
#include <ctime>
#include <nlohmann/json.hpp>

namespace shuati {

// Luogu difficulty levels (0-7)
static std::string luogu_difficulty_str(int level) {
    switch (level) {
        case 0: return "暂无评定";
        case 1: return "入门";
        case 2: return "普及-";
        case 3: return "普及/提高-";
        case 4: return "普及+/提高";
        case 5: return "提高+/省选-";
        case 6: return "省选/NOI-";
        case 7: return "NOI/NOI+/CTSC";
        default: return fmt::format("Unknown({})", level);
    }
}

// Map known Luogu tag IDs to names
static std::string luogu_tag_name(int tag_id) {
    // Common Luogu algorithm tag IDs
    static const std::map<int, std::string> tag_map = {
        {1, "模拟"}, {2, "字符串"}, {3, "搜索"}, {4, "贪心"},
        {5, "动态规划"}, {6, "图论"}, {7, "数学"}, {8, "数据结构"},
        {9, "暴力"}, {10, "分治"}, {11, "递归"}, {12, "二分"},
        {13, "高精度"}, {14, "递推"}, {15, "枚举"}, {16, "位运算"},
        {17, "构造"}, {18, "博弈论"}, {19, "概率"}, {20, "组合数学"},
        {21, "几何"}, {22, "排序"}, {23, "排序算法"},
        {30, "树形DP"}, {31, "状压DP"}, {32, "区间DP"},
        {33, "线性DP"}, {34, "背包"}, {35, "数位DP"},
        {40, "BFS"}, {41, "DFS"}, {42, "最短路"}, {43, "最小生成树"},
        {44, "拓扑排序"}, {45, "二分图"}, {46, "网络流"},
        {50, "线段树"}, {51, "树状数组"}, {52, "并查集"},
        {53, "堆"}, {54, "栈"}, {55, "队列"}, {56, "哈希"},
        {60, "前缀和与差分"}, {61, "双指针"}, {62, "倍增"},
        {63, "离散化"}, {64, "单调栈"}, {65, "单调队列"},
        {113, "Special Judge"}, {333, "洛谷原创"},
    };
    auto it = tag_map.find(tag_id);
    if (it != tag_map.end()) return it->second;
    return fmt::format("tag_{}", tag_id);
}

class LuoguCrawlerImpl : public BaseCrawler {
public:
    using BaseCrawler::BaseCrawler;

    bool can_handle(const std::string& url) const override {
        return utils::contains(url, "luogu.com.cn");
    }

    cpr::Header get_default_headers() const override {
        return cpr::Header{
            {"User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36"},
            {"Accept", "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8"},
            {"Accept-Language", "zh-CN,zh;q=0.9,en;q=0.8"}
        };
    }

    Problem fetch_problem(const std::string& url) override {
        Problem p;
        p.source = "Luogu";
        p.url = url;

        // Extract ID from URL: problem/P1001
        std::string pid = extract_regex(url, R"(problem/(P\d+))");
        p.id = !pid.empty() ? generate_id("lg", pid) : generate_id("lg");
        if (!pid.empty()) p.display_id = 0;

        try {
            std::string html = http_get(url);

            // Parse embedded JSON from <script id="lentille-context">
            auto json_data = extract_lentille_json(html);

            if (!json_data.is_null() && json_data.contains("problem")) {
                auto& prob = json_data["problem"];

                // Title
                p.title = prob.value("title", "");
                if (p.title.empty() && prob.contains("contenu")) {
                    p.title = prob["contenu"].value("name", "");
                }
                if (!pid.empty() && !p.title.empty()) {
                    p.title = pid + " " + p.title;
                }

                // Difficulty
                if (prob.contains("difficulty")) {
                    p.difficulty = luogu_difficulty_str(prob["difficulty"].get<int>());
                }

                // Tags
                if (prob.contains("tags") && prob["tags"].is_array()) {
                    std::vector<std::string> tag_names;
                    for (const auto& t : prob["tags"]) {
                        if (t.is_number_integer()) {
                            tag_names.push_back(luogu_tag_name(t.get<int>()));
                        }
                    }
                    p.tags = utils::join(tag_names, ",");
                }

                // Description - prefer Chinese (contenu) over English (content)
                std::string desc;
                if (prob.contains("contenu") && !prob["contenu"].is_null()) {
                    auto& c = prob["contenu"];
                    desc = build_description(c);
                } else if (prob.contains("content") && !prob["content"].is_null()) {
                    auto& c = prob["content"];
                    desc = build_description(c);
                }

                if (!desc.empty()) {
                    p.description = desc;
                } else {
                    p.description = "无法解析题目内容";
                }

            } else {
                // Fallback: extract title from HTML
                p.title = extract_title(html);
                p.title = clean_title(p.title, {" - 洛谷"});
                p.description = "无法解析题目内容 (JSON未找到)";
            }

        } catch (const std::exception& e) {
            p.title = fmt::format("Error: {}", e.what());
        }

        return p;
    }

    std::vector<TestCase> fetch_test_cases(const std::string& url) override {
        std::vector<TestCase> cases;

        try {
            std::string html = http_get(url);
            auto json_data = extract_lentille_json(html);

            if (!json_data.is_null() && json_data.contains("problem")) {
                auto& prob = json_data["problem"];
                if (prob.contains("samples") && prob["samples"].is_array()) {
                    for (const auto& sample : prob["samples"]) {
                        if (sample.is_array() && sample.size() >= 2) {
                            TestCase tc;
                            tc.input = utils::trim(sample[0].get<std::string>());
                            tc.output = utils::trim(sample[1].get<std::string>());
                            tc.is_sample = true;
                            cases.push_back(tc);
                        }
                    }
                }
            }
        } catch (...) {
            // Ignore errors in test case extraction
        }

        return cases;
    }

private:
    // Extract JSON from <script id="lentille-context" type="application/json">...</script>
    nlohmann::json extract_lentille_json(const std::string& html) const {
        // Find the lentille-context script tag
        std::string marker = "id=\"lentille-context\"";
        auto pos = html.find(marker);
        if (pos == std::string::npos) return nlohmann::json();

        // Find the start of JSON content (after the closing >)
        auto json_start = html.find(">", pos);
        if (json_start == std::string::npos) return nlohmann::json();
        json_start++; // Skip >

        // Find </script>
        auto json_end = html.find("</script>", json_start);
        if (json_end == std::string::npos) return nlohmann::json();

        std::string json_str = html.substr(json_start, json_end - json_start);

        try {
            auto j = nlohmann::json::parse(json_str);
            if (j.contains("data")) {
                return j["data"];
            }
            return j;
        } catch (...) {
            return nlohmann::json();
        }
    }

    // Build a formatted problem description from the content object
    std::string build_description(const nlohmann::json& content) const {
        std::string desc;

        auto append_section = [&](const std::string& title, const std::string& key) {
            if (content.contains(key) && !content[key].is_null()) {
                std::string text = content[key].get<std::string>();
                if (!text.empty()) {
                    if (!desc.empty()) desc += "\n\n";
                    desc += "## " + title + "\n\n" + text;
                }
            }
        };

        // Background (if any)
        append_section("题目背景", "background");

        // Problem description
        append_section("题目描述", "description");

        // Input format
        append_section("输入格式", "formatI");

        // Output format
        append_section("输出格式", "formatO");

        // Hint / notes
        append_section("说明/提示", "hint");

        return desc;
    }
};

// Public interface
LuoguCrawler::LuoguCrawler(std::shared_ptr<IHttpClient> client)
    : impl_(std::make_unique<LuoguCrawlerImpl>(client)) {}
LuoguCrawler::~LuoguCrawler() = default;

bool LuoguCrawler::can_handle(const std::string& url) const {
    return impl_->can_handle(url);
}

Problem LuoguCrawler::fetch_problem(const std::string& url) {
    return impl_->fetch_problem(url);
}

std::vector<TestCase> LuoguCrawler::fetch_test_cases(const std::string& url) {
    return impl_->fetch_test_cases(url);
}

} // namespace shuati

