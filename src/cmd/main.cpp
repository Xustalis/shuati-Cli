#include <iostream>
#include <string>
#include <fstream>
#include <filesystem>
#include <chrono>
#include <ctime>
#include <algorithm>
#include <vector>
#include <sstream>
#include <regex>
#include <cstdlib>
#include <cerrno>
#include <optional>
#include <random>
#include <unordered_map>
#include <unordered_set>

#include <CLI/CLI.hpp>
#include <fmt/core.h>
#include <fmt/color.h>
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <replxx.hxx>
#include <nlohmann/json.hpp>

#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#include <shellapi.h>
#endif

#include "shuati/config.hpp"
#include "shuati/database.hpp"
#include "shuati/problem_manager.hpp"
#include "shuati/mistake_analyzer.hpp"
#include "shuati/ai_coach.hpp"
#include "shuati/memory_manager.hpp"
#include "shuati/sm2_algorithm.hpp"
#include "shuati/judge.hpp"
#include "shuati/compiler_doctor.hpp"
#include "shuati/version.hpp"
#include "shuati/logger.hpp"
#include "shuati/utils/encoding.hpp"
#include "shuati/boot_guard.hpp"

// Header separation fixes ODR violations
#include "shuati/adapters/leetcode_crawler.hpp"
#include "shuati/adapters/codeforces_crawler.hpp"
#include "shuati/adapters/luogu_crawler.hpp"
#include "shuati/adapters/lanqiao_crawler.hpp"
#include "shuati/adapters/companion_server.hpp"

namespace fs = std::filesystem;
using namespace shuati;
using shuati::utils::ensure_utf8;
using shuati::utils::shorten_utf8_lossy;

// ─── Helpers ──────────────────────────────────────────

#ifndef SHUATI_VERSION
#define SHUATI_VERSION "dev"
#endif

static fs::path find_root_or_die() {
    auto root = Config::find_root();
    if (root.empty()) {
        fmt::print(fg(fmt::color::red), "[!] 未找到项目根目录。请先运行: shuati init\n");
        throw std::runtime_error("Root not found");
    }
    return root;
}

#ifdef _WIN32
static std::string executable_path_utf8() {
    std::wstring wpath;
    wpath.resize(32768);
    DWORD n = GetModuleFileNameW(nullptr, wpath.data(), (DWORD)wpath.size());
    if (n == 0) return {};
    wpath.resize(n);
    return shuati::utils::wide_to_utf8(wpath);
}
#else
static std::string executable_path_utf8() {
    return {};
}
#endif

enum class SimpleType { Int, Float, String };

struct ParamSpec {
    std::string name;
    SimpleType type = SimpleType::String;
    std::optional<long long> min_i;
    std::optional<long long> max_i;
    std::vector<std::string> normals;
};

struct InputSpec {
    std::vector<ParamSpec> params;
};

struct PlannedCase {
    TestCase tc;
    std::string name;
    std::string kind;
    int priority = 0;
    bool has_oracle = false;
    std::vector<std::pair<std::string, std::string>> coverage_marks;
};

static std::vector<std::string> split_ws(const std::string& s) {
    std::istringstream iss(s);
    std::vector<std::string> out;
    for (std::string tok; iss >> tok;) out.push_back(tok);
    return out;
}

static bool parse_i64(const std::string& s, long long& out) {
    if (s.empty()) return false;
    char* end = nullptr;
    errno = 0;
    long long v = std::strtoll(s.c_str(), &end, 10);
    if (errno != 0) return false;
    if (end == s.c_str() || *end != '\0') return false;
    out = v;
    return true;
}

static std::string join_tokens(const std::vector<std::string>& tokens) {
    std::string out;
    for (size_t i = 0; i < tokens.size(); i++) {
        if (i) out.push_back(' ');
        out += tokens[i];
    }
    out.push_back('\n');
    return out;
}

static std::string trim_copy(const std::string& s) {
    size_t b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return "";
    size_t e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}


static InputSpec infer_spec_from_db_cases(const std::vector<std::pair<std::string, std::string>>& db_cases) {
    InputSpec spec;
    if (db_cases.empty()) return spec;
    auto tokens = split_ws(db_cases.front().first);
    for (size_t i = 0; i < tokens.size(); i++) {
        ParamSpec p;
        p.name = fmt::format("p{}", i + 1);
        long long v = 0;
        if (parse_i64(tokens[i], v)) {
            p.type = SimpleType::Int;
            p.normals.push_back(tokens[i]);
        } else {
            p.type = SimpleType::String;
            p.normals.push_back(tokens[i]);
        }
        spec.params.push_back(std::move(p));
    }
    return spec;
}

static InputSpec parse_spec_json_or_fallback(const std::string& raw, const InputSpec& fallback) {
    auto l = raw.find('{');
    auto r = raw.rfind('}');
    if (l == std::string::npos || r == std::string::npos || r <= l) return fallback;
    try {
        auto j = nlohmann::json::parse(raw.substr(l, r - l + 1));
        if (!j.contains("params") || !j["params"].is_array()) return fallback;

        InputSpec spec;
        for (auto& pj : j["params"]) {
            if (!pj.is_object()) continue;
            ParamSpec p;
            if (pj.contains("name")) p.name = pj["name"].get<std::string>();
            if (p.name.empty()) p.name = fmt::format("p{}", spec.params.size() + 1);

            std::string t = pj.value("type", "string");
            if (t == "int" || t == "integer" || t == "long") p.type = SimpleType::Int;
            else if (t == "float" || t == "double" || t == "real") p.type = SimpleType::Float;
            else p.type = SimpleType::String;

            if (pj.contains("min") && pj["min"].is_number_integer()) p.min_i = pj["min"].get<long long>();
            if (pj.contains("max") && pj["max"].is_number_integer()) p.max_i = pj["max"].get<long long>();
            if (pj.contains("normals") && pj["normals"].is_array()) {
                for (auto& v : pj["normals"]) {
                    if (v.is_string()) p.normals.push_back(v.get<std::string>());
                    else if (v.is_number_integer()) p.normals.push_back(std::to_string(v.get<long long>()));
                    else if (v.is_number_float()) p.normals.push_back(fmt::format("{}", v.get<double>()));
                }
            }
            spec.params.push_back(std::move(p));
        }

        if (spec.params.empty()) return fallback;
        return spec;
    } catch (...) {
        return fallback;
    }
}

static std::string build_problem_text(const Problem& prob) {
    std::string desc;
    desc += fmt::format("题目: {}\n", ensure_utf8(prob.title));
    desc += fmt::format("ID: {}\n", prob.id);
    desc += fmt::format("来源: {}\n", ensure_utf8(prob.source));
    desc += fmt::format("难度: {}\n", ensure_utf8(prob.difficulty));
    desc += fmt::format("URL: {}\n\n", ensure_utf8(prob.url));

    if (!prob.content_path.empty() && fs::exists(prob.content_path)) {
        std::ifstream f(prob.content_path, std::ios::in | std::ios::binary);
        std::string body(std::istreambuf_iterator<char>(f), {});
        if (!body.empty()) {
            desc += body.substr(0, 9000);
        }
    }
    return desc;
}

static InputSpec infer_spec_with_ai(AICoach* ai, const std::string& problem_text, const InputSpec& fallback) {
    if (!ai) return fallback;
    std::string sys =
        "你是测试点生成器。请从题面中抽取输入参数列表，并给出每个参数的类型与取值范围。"
        "只输出严格 JSON，不要输出解释或 Markdown。"
        "JSON 格式：{\"params\":[{\"name\":\"...\",\"type\":\"int|float|string\",\"min\":1,\"max\":100,\"normals\":[\"...\"]}]}。"
        "若范围未知，可省略 min/max，但必须提供 normals。";
    std::string user = problem_text.substr(0, 12000);
    auto raw = ai->chat_sync(sys, user);
    return parse_spec_json_or_fallback(raw, fallback);
}

static std::vector<PlannedCase> build_planned_cases(const InputSpec& spec,
                                                    const std::vector<std::pair<std::string, std::string>>& db_cases,
                                                    int max_cases,
                                                    unsigned int seed) {
    std::vector<PlannedCase> out;

    for (size_t i = 0; i < db_cases.size(); i++) {
        PlannedCase pc;
        pc.tc.input = db_cases[i].first;
        pc.tc.output = db_cases[i].second;
        pc.tc.is_sample = true;
        pc.name = fmt::format("样例 {}", i + 1);
        pc.kind = "sample";
        pc.priority = 1000;
        pc.has_oracle = !trim_copy(pc.tc.output).empty();
        out.push_back(std::move(pc));
    }

    if (spec.params.empty()) return out;

    std::mt19937 rng(seed ? seed : static_cast<unsigned int>(std::time(nullptr)));

    std::vector<std::string> baseline;
    baseline.reserve(spec.params.size());
    for (const auto& p : spec.params) {
        if (!p.normals.empty()) baseline.push_back(p.normals.front());
        else if (p.type == SimpleType::Int) baseline.push_back("1");
        else baseline.push_back("x");
    }

    auto add_case = [&](std::vector<std::string> tokens,
                        std::string name,
                        std::string kind,
                        int priority,
                        std::vector<std::pair<std::string, std::string>> marks) {
        if ((int)out.size() >= max_cases) return;
        PlannedCase pc;
        pc.tc.input = join_tokens(tokens);
        pc.tc.output = "";
        pc.tc.is_sample = false;
        pc.name = std::move(name);
        pc.kind = std::move(kind);
        pc.priority = priority;
        pc.has_oracle = false;
        pc.coverage_marks = std::move(marks);
        out.push_back(std::move(pc));
    };

    auto make_int_values = [&](const ParamSpec& p, long long typical) {
        long long mn = p.min_i.value_or(typical - 10);
        long long mx = p.max_i.value_or(typical + 10);
        if (mn > mx) std::swap(mn, mx);
        long long critical = (mn == mx) ? mn : (mn + 1);
        long long common = typical;
        long long mid = (mn + mx) / 2;
        return std::vector<std::pair<std::string, long long>>{
            {"min", mn},
            {"max", mx},
            {"critical", critical},
            {"typical", mid},
            {"common", common},
        };
    };

    for (size_t i = 0; i < spec.params.size(); i++) {
        const auto& p = spec.params[i];
        std::vector<std::string> tokens = baseline;
        long long typical = 1;
        parse_i64(tokens[i], typical);

        if (p.type == SimpleType::Int) {
            auto vals = make_int_values(p, typical);
            std::unordered_set<long long> seen;
            for (auto& kv : vals) {
                if (!seen.insert(kv.second).second) continue;
                tokens[i] = std::to_string(kv.second);
                add_case(tokens,
                         fmt::format("{}={}", p.name, kv.first),
                         kv.first,
                         kv.first == "min" || kv.first == "max" ? 900 : 800,
                         {{p.name, kv.first}});
            }
        } else {
            std::vector<std::string> candidates;
            if (!p.normals.empty()) candidates.push_back(p.normals.front());
            candidates.push_back("x");
            candidates.push_back("abc");
            std::unordered_set<std::string> seen;
            for (const auto& v : candidates) {
                if (!seen.insert(v).second) continue;
                tokens[i] = v;
                add_case(tokens,
                         fmt::format("{}=str", p.name),
                         "normal",
                         700,
                         {{p.name, "normal"}});
            }
        }
    }

    if ((int)out.size() < max_cases) {
        std::vector<std::string> all_min = baseline;
        std::vector<std::string> all_max = baseline;
        std::vector<std::pair<std::string, std::string>> marks_min;
        std::vector<std::pair<std::string, std::string>> marks_max;
        for (size_t i = 0; i < spec.params.size(); i++) {
            const auto& p = spec.params[i];
            long long typical = 1;
            parse_i64(baseline[i], typical);
            if (p.type == SimpleType::Int) {
                long long mn = p.min_i.value_or(typical - 10);
                long long mx = p.max_i.value_or(typical + 10);
                if (mn > mx) std::swap(mn, mx);
                all_min[i] = std::to_string(mn);
                all_max[i] = std::to_string(mx);
                marks_min.push_back({p.name, "min"});
                marks_max.push_back({p.name, "max"});
            }
        }
        add_case(all_min, "组合: 全最小", "combo", 850, marks_min);
        add_case(all_max, "组合: 全最大", "combo", 850, marks_max);
    }

    if ((int)out.size() < max_cases && spec.params.size() >= 2) {
        std::uniform_int_distribution<int> pick(0, 1);
        for (size_t a = 0; a < spec.params.size() && (int)out.size() < max_cases; a++) {
            for (size_t b = a + 1; b < spec.params.size() && (int)out.size() < max_cases; b++) {
                auto tokens = baseline;
                std::vector<std::pair<std::string, std::string>> marks;
                for (size_t i : {a, b}) {
                    const auto& p = spec.params[i];
                    long long typical = 1;
                    parse_i64(baseline[i], typical);
                    if (p.type != SimpleType::Int) continue;
                    long long mn = p.min_i.value_or(typical - 10);
                    long long mx = p.max_i.value_or(typical + 10);
                    if (mn > mx) std::swap(mn, mx);
                    bool use_min = pick(rng) == 0;
                    tokens[i] = std::to_string(use_min ? mn : mx);
                    marks.push_back({p.name, use_min ? "min" : "max"});
                }
                if (!marks.empty()) add_case(tokens, "组合: 两参边界", "combo", 820, marks);
            }
        }
    }

    std::unordered_set<std::string> uniq;
    std::vector<PlannedCase> dedup;
    dedup.reserve(out.size());
    for (auto& c : out) {
        std::string key = c.tc.input + "\n----\n" + c.tc.output;
        if (!uniq.insert(key).second) continue;
        dedup.push_back(std::move(c));
    }
    return dedup;
}

static bool fill_expected_with_ai(AICoach* ai,
                                  const std::string& problem_text,
                                  std::vector<PlannedCase>& cases) {
    if (!ai) return false;
    std::vector<size_t> idx;
    for (size_t i = 0; i < cases.size(); i++) {
        if (trim_copy(cases[i].tc.output).empty()) idx.push_back(i);
    }
    if (idx.empty()) return true;

    size_t pos = 0;
    while (pos < idx.size()) {
        size_t end = std::min(idx.size(), pos + 8);
        std::string sys =
            "你是在线评测器。给定题面与若干输入，请输出每个输入对应的标准输出。"
            "只输出严格 JSON 数组（字符串数组），数组长度必须等于输入个数，顺序必须一致。"
            "不要输出任何解释，不要输出 Markdown。";

        std::string user;
        user += "题面：\n";
        user += problem_text.substr(0, 9000);
        user += "\n\n输入列表：\n";
        for (size_t k = pos; k < end; k++) {
            user += fmt::format("Input {}:\n```\n{}\n```\n", (k - pos) + 1, cases[idx[k]].tc.input);
        }

        auto raw = ai->chat_sync(sys, user);
        auto l = raw.find('[');
        auto r = raw.rfind(']');
        if (l == std::string::npos || r == std::string::npos || r <= l) return false;
        try {
            auto j = nlohmann::json::parse(raw.substr(l, r - l + 1));
            if (!j.is_array() || j.size() != (end - pos)) return false;
            for (size_t k = pos; k < end; k++) {
                auto& v = j[k - pos];
                if (v.is_string()) cases[idx[k]].tc.output = v.get<std::string>();
                else cases[idx[k]].tc.output = v.dump();
                cases[idx[k]].has_oracle = true;
            }
        } catch (...) {
            return false;
        }

        pos = end;
    }

    return true;
}

struct Services {
    std::shared_ptr<Database>       db;
    std::shared_ptr<ProblemManager> pm;
    std::shared_ptr<MistakeAnalyzer> ma;
    std::unique_ptr<MemoryManager>  mm;
    std::unique_ptr<AICoach>        ai;
    std::unique_ptr<CompanionServer> companion;
    std::unique_ptr<Judge>          judge;
    Config                          cfg;

    static Services load(const fs::path& root) {
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
            s.pm->register_crawler(std::make_unique<LanqiaoCrawler>());
            
            s.ma  = std::make_shared<MistakeAnalyzer>(s.db);
            s.mm  = std::make_unique<MemoryManager>(*s.db); // Database& constructor
            s.ai  = std::make_unique<AICoach>(s.cfg, s.mm.get());
            s.companion = std::make_unique<CompanionServer>(*s.pm, *s.db);
            s.judge = std::make_unique<Judge>();
        } catch (const std::exception& e) {
            fmt::print(fg(fmt::color::red), "[!] 服务加载失败: {}\n", e.what());
            throw;
        }

        
        // Environment Check
        std::vector<std::string> missing;
        if (!CompilerDoctor::check_environment(missing)) {
             fmt::print(fg(fmt::color::yellow), "[!] 警告: 未检测到以下环境工具，可能无法运行代码:\n");
             for (const auto& t : missing) {
                 fmt::print("    - {}\n", t);
             }
             fmt::print("\n");
        }
        
        return s;
    }
};

// ─── Command Context ──────────────────────────────────

struct CommandContext {
    std::string pull_url;
    std::string new_title, new_tags, new_diff = "medium";
    std::string solve_pid;
    std::string submit_pid, submit_file;
    int submit_quality = -1;
    std::string hint_pid, hint_file;
    std::string cfg_key, cfg_model;
    bool cfg_show = false;
    int test_max_cases = 30;
    std::string test_oracle = "auto";
    bool test_ui = false;
};

// ─── Test Command Implementation ──────────────────────

void cmd_test(CommandContext& ctx) {
    try {
        auto svc = Services::load(find_root_or_die());
        // 1. Resolve Problem
        Problem prob = svc.pm->get_problem(ctx.solve_pid);
        if (prob.id.empty()) { fmt::print(fg(fmt::color::red), "[!] 题目不存在。\n"); return; }
        
        // 2. Prepare Environment
        fs::path prob_dir = fs::path(".shuati") / "problems" / prob.id;
        fs::create_directories(prob_dir / "data");
        fs::create_directories(prob_dir / "validator");
        fs::create_directories(prob_dir / "debug");
        fs::create_directories(prob_dir / "temp");

        std::string ext = svc.cfg.language == "python" ? ".py" : ".cpp";
        std::string src_file = "solution_" + prob.id + ext; // User solution in root
        if (!fs::exists(src_file)) { fmt::print(fg(fmt::color::red), "[!] 找不到代码文件: {}\n", src_file); return; }

        // Compile User Solution
        fmt::print("[*] 正在编译用户代码...\n");
        std::string user_exe;
        try {
            user_exe = svc.judge->prepare(src_file, svc.cfg.language);
        } catch (const std::exception& e) {
            fmt::print(fg(fmt::color::red), "[Compile Error]\n{}\n", e.what());
            return;
        }

        // 3. Mode Decision
        // Check for static cases in .shuati/problems/<id>/data/*.in
        std::vector<fs::path> static_inputs;
        if (fs::exists(prob_dir / "data")) {
            for (const auto& entry : fs::directory_iterator(prob_dir / "data")) {
                if (entry.path().extension() == ".in") {
                    static_inputs.push_back(entry.path());
                }
            }
        }
        std::sort(static_inputs.begin(), static_inputs.end());

        if (!static_inputs.empty() && ctx.test_oracle != "ai") {
            // === Static Mode ===
            fmt::print(fg(fmt::color::cyan), "=== 静态测试模式 ({} cases) ===\n", static_inputs.size());
            int ac_cnt = 0;
            for (const auto& in_path : static_inputs) {
                fs::path out_path = in_path;
                out_path.replace_extension(".out");
                fs::path my_out = prob_dir / "temp" / in_path.filename().replace_extension(".my.out");
                
                auto start = std::chrono::steady_clock::now();
                auto res = svc.judge->run_process_redirect(user_exe, in_path.string(), my_out.string(), 2000, 256*1024);
                
                std::string verdict;
                bool pass = false;
                if (res.exit_code != 0) verdict = "RE";
                else if (res.tle) verdict = "TLE";
                else if (res.mle) verdict = "MLE";
                else {
                    if (fs::exists(out_path)) {
                        if (svc.judge->stream_file_diff(out_path.string(), my_out.string())) {
                            verdict = "AC"; pass = true;
                        } else verdict = "WA";
                    } else {
                        verdict = "OK"; // No output to compare
                        pass = true;
                    }
                }
                
                if (pass) ac_cnt++;
                fmt::print("[{}] {} ({}ms, {}KB)\n", verdict, in_path.filename().string(), res.time_ms, res.memory_kb);
                if (!pass && verdict == "WA") {
                     fmt::print(fg(fmt::color::yellow), "    Diff failed. See {}\n", my_out.string());
                }
            }
            fmt::print("\nResult: {} / {}\n", ac_cnt, static_inputs.size());
        } else {
            // === Dynamic Stress Mode ===
            fmt::print(fg(fmt::color::magenta), "=== 动态对拍模式 (Stress Testing) ===\n");
            
            // A. Check Scripts
            fs::path gen_py = prob_dir / "validator" / "gen.py";
            fs::path sol_py = prob_dir / "validator" / "sol.py";
            
            if (!fs::exists(gen_py) || !fs::exists(sol_py)) {
                if (!svc.cfg.ai_enabled || !svc.ai || !svc.ai->enabled()) {
                    fmt::print(fg(fmt::color::red), "[!] 缺少对拍脚本且 AI 未启用。请手动配置 validator/gen.py 和 validator/sol.py\n");
                    return;
                }
                fmt::print("[*] 正在生成对拍脚本 (gen.py, sol.py)...\n");
                std::string desc = build_problem_text(prob);
                auto scripts = svc.ai->generate_test_scripts(desc);
                
                if (scripts.first.empty() || scripts.second.empty()) {
                    fmt::print(fg(fmt::color::red), "[!] AI 生成脚本失败。\n");
                    fmt::print(fg(fmt::color::yellow), "    可能原因：题目描述过短或不明确，AI 无法推断输入格式。\n");
                    fmt::print("    建议：1. 编辑 .shuati/problems/{}/problem.md 补充描述\n", prob.id);
                    fmt::print("          2. 或手动创建 validator/gen.py 和 validator/sol.py\n");
                    return;
                }

                { std::ofstream fgen(gen_py); fgen << scripts.first; }
                { std::ofstream fsol(sol_py); fsol << scripts.second; }
                fmt::print(fg(fmt::color::green), "[+] 脚本已保存至 validator/\n");
            }

            // B. Verify Oracle (Strict Check)
            auto db_cases = svc.db->get_test_cases(prob.id);
            std::string sample_in_content, sample_out_answer;
            bool has_sample = false;

            if (!db_cases.empty()) {
                sample_in_content = db_cases[0].first;
                sample_out_answer = db_cases[0].second;
                has_sample = true;
            } else if (!static_inputs.empty()) {
                try {
                    std::ifstream in(static_inputs[0], std::ios::in | std::ios::binary);
                    sample_in_content.assign(std::istreambuf_iterator<char>(in), {});
                    fs::path exp_path = static_inputs[0];
                    exp_path.replace_extension(".out");
                    if (fs::exists(exp_path)) {
                        std::ifstream out(exp_path, std::ios::in | std::ios::binary);
                        sample_out_answer.assign(std::istreambuf_iterator<char>(out), {});
                        has_sample = true;
                    }
                } catch (...) {}
            }

            if (has_sample) {
                fmt::print("[*] 正在验算 AI 标程...\n");
                fs::path sample_in = prob_dir / "temp" / "sample_verify.in";
                fs::path sample_std = prob_dir / "temp" / "sample_verify.std";
                fs::path sample_actual = prob_dir / "temp" / "sample_verify.out";
                
                { std::ofstream out(sample_in); out << sample_in_content; }
                if (!sample_out_answer.empty()) {
                     std::ofstream out(sample_std); out << sample_out_answer;
                }

                auto res = svc.judge->run_process_redirect("python \"" + sol_py.string() + "\"", sample_in.string(), sample_actual.string(), 5000, 512*1024);
                if (res.exit_code != 0 || res.tle) {
                     fmt::print(fg(fmt::color::red), "[!] AI 标程运行样例失败 (RE/TLE)。请检查 validator/sol.py\n");
                     return;
                }

                if (!sample_out_answer.empty()) {
                    if (!svc.judge->stream_file_diff(sample_std.string(), sample_actual.string())) {
                         fmt::print(fg(fmt::color::red) | fmt::emphasis::bold, "[CRITICAL] AI 标程输出与样例不符！拒绝执行对拍。\n");
                         fmt::print("Input: {}\n", sample_in.string());
                         fmt::print("Expected: {}\n", sample_std.string());
                         fmt::print("Actual: {}\n", sample_actual.string());
                         return; 
                    }
                }
                fmt::print(fg(fmt::color::green), "[Pass] AI 标程通过样例验算。\n");
            } else {
                fmt::print(fg(fmt::color::yellow), "[Warn] 无样例可供验算，风险自负。\n");
            }

            // C. Stress Loop
            int rounds = ctx.test_max_cases > 0 ? ctx.test_max_cases : 100;
            fmt::print("[*] 开始 {} 轮对拍...\n", rounds);

            fs::path cur_in = prob_dir / "temp" / "cur.in";
            fs::path cur_exp = prob_dir / "temp" / "cur.exp";
            fs::path cur_out = prob_dir / "temp" / "cur.out";

            for (int i = 1; i <= rounds; i++) {
                 // 1. Gen
                 auto r_gen = svc.judge->run_process_redirect("python \"" + gen_py.string() + "\"", "", cur_in.string(), 5000, 512*1024);
                 if (r_gen.exit_code != 0) {
                     fmt::print(fg(fmt::color::red), "[!] 生成器错误 (Case {})\n", i);
                     break;
                 }

                 // 2. Oracle
                 auto r_sol = svc.judge->run_process_redirect("python \"" + sol_py.string() + "\"", cur_in.string(), cur_exp.string(), 5000, 512*1024);
                 if (r_sol.exit_code != 0) {
                     fmt::print(fg(fmt::color::red), "[!] 标程错误 (Case {})\n", i);
                     break; 
                 }

                 // 3. User
                 auto r_user = svc.judge->run_process_redirect(user_exe, cur_in.string(), cur_out.string(), 2000, 256*1024);
                 
                 if (r_user.exit_code != 0) {
                     fmt::print(fg(fmt::color::red), "[RE] Case {}\n", i);
                     fs::copy_file(cur_in, prob_dir / "debug" / "fail.in", fs::copy_options::overwrite_existing);
                     break;
                 } 
                 if (r_user.tle) {
                     fmt::print(fg(fmt::color::red), "[TLE] Case {}\n", i);
                     fs::copy_file(cur_in, prob_dir / "debug" / "fail.in", fs::copy_options::overwrite_existing);
                     break;
                 }

                 // 4. Diff
                 if (!svc.judge->stream_file_diff(cur_exp.string(), cur_out.string())) {
                     fmt::print(fg(fmt::color::red) | fmt::emphasis::bold, "[WA] Case {}\n", i);
                     fmt::print("    Data saved to .shuati/problems/{}/debug/\n", prob.id);
                     fs::copy_file(cur_in, prob_dir / "debug" / "fail.in", fs::copy_options::overwrite_existing);
                     fs::copy_file(cur_exp, prob_dir / "debug" / "fail.exp", fs::copy_options::overwrite_existing);
                     fs::copy_file(cur_out, prob_dir / "debug" / "fail.out", fs::copy_options::overwrite_existing);
                     
                     if (svc.cfg.ai_enabled && svc.ai && svc.ai->enabled()) {
                         fmt::print("[*] 正在请求 AI 诊断...\n");
                         std::string in_txt, exp_txt, out_txt;
                         { std::ifstream f(cur_in); in_txt.assign(std::istreambuf_iterator<char>(f), {}); }
                         { std::ifstream f(cur_exp); exp_txt.assign(std::istreambuf_iterator<char>(f), {}); }
                         { std::ifstream f(cur_out); out_txt.assign(std::istreambuf_iterator<char>(f), {}); }
                         
                         std::string fail_log = "Input:\n" + in_txt.substr(0, 1000) + "\nExpected:\n" + exp_txt.substr(0, 1000) + "\nActual:\n" + out_txt.substr(0, 1000);
                         std::string user_code;
                         { std::ifstream f(src_file); user_code.assign(std::istreambuf_iterator<char>(f), {}); }
                         
                         auto mistakes = svc.ma->get_mistakes(prob.id);
                         std::string history; // ...
                         
                         auto diag = svc.ai->diagnose(build_problem_text(prob), user_code, fail_log, history);
                         fmt::print(fg(fmt::color::yellow), "{}\n", diag);
                     }
                     return; // Stop on first error
                 }

                 if (i % 10 == 0) fmt::print(".");
                 if (i % 50 == 0) fmt::print(" {}\n", i);
                 std::cout.flush();
            }
            fmt::print(fg(fmt::color::green), "\n[+] 对拍完成，未发现错误。\n");
        }
        
        svc.judge->cleanup_prepared(user_exe, svc.cfg.language);

    } catch (const std::exception& e) {
        fmt::print(fg(fmt::color::red), "[!] 错误: {}\n", e.what());
    }
}

// ─── Command Setup ────────────────────────────────────

void setup_commands(CLI::App& app, CommandContext& ctx) {
    app.require_subcommand(1);


    // ── init ──
    app.add_subcommand("init", "在当前目录初始化项目")->callback([]() {
        if (fs::exists(Config::DIR_NAME)) {
            fmt::print("[!] 项目已初始化。\n");
            return;
        }
        auto dir = fs::current_path() / Config::DIR_NAME;
        fs::create_directories(dir / "templates");
        fs::create_directories(dir / "problems");
        
        Config cfg;
        cfg.save(Config::config_path(fs::current_path()));
        
        Database db(Config::db_path(fs::current_path()).string());
        db.init_schema();
        
        fmt::print(fg(fmt::color::green_yellow), "[+] 初始化成功: {}\n", dir.string());
        fmt::print("    请设置 API Key: config --api-key <YOUR_KEY>\n");
    });

    // ── info ──
    app.set_version_flag("-v,--version", fmt::format("shuati version {}", current_version().to_string()), "显示版本信息");

    app.add_subcommand("info", "显示可执行文件与项目环境信息")->callback([]() {
        try {
            std::string exe = executable_path_utf8();
            if (exe.empty()) exe = fs::absolute(fs::path("shuati")).string();
            fmt::print("Version: {}\n", current_version().to_string());
            fmt::print("Exe:     {}\n", ensure_utf8(exe));
            fmt::print("CWD:     {}\n", ensure_utf8(fs::current_path().string()));

            auto root = Config::find_root();
            if (root.empty()) {
                fmt::print("Root:    (not found)\n");
                return;
            }
            fmt::print("Root:    {}\n", ensure_utf8(root.string()));
            fmt::print("Config:  {}\n", ensure_utf8(Config::config_path(root).string()));
            fmt::print("DB:      {}\n", ensure_utf8(Config::db_path(root).string()));
        } catch (const std::exception& e) {
            fmt::print(fg(fmt::color::red), "[!] 错误: {}\n", e.what());
        }
    });

    // ── pull ──
    auto pull_cmd = app.add_subcommand("pull", "从 URL 拉取题目");
    pull_cmd->add_option("url", ctx.pull_url, "题目链接")->required();
    pull_cmd->callback([&]() {
        try {
            auto svc = Services::load(find_root_or_die());
            svc.pm->pull_problem(ctx.pull_url);
        } catch (const std::exception& e) {
            fmt::print(fg(fmt::color::red), "[!] 错误: {}\n", e.what());
        }
    });

    // ── new ──
    auto new_cmd = app.add_subcommand("new", "创建本地题目");
    new_cmd->add_option("title", ctx.new_title, "标题")->required();
    new_cmd->add_option("-t,--tags", ctx.new_tags, "标签 (逗号分隔)");
    new_cmd->add_option("-d,--difficulty", ctx.new_diff, "难度 (easy/medium/hard)");
    new_cmd->callback([&]() {
        try {
            auto svc = Services::load(find_root_or_die());
            std::string pid = svc.pm->create_local(ctx.new_title, ctx.new_tags, ctx.new_diff);
            
            // Create directory structure
            fs::path prob_dir = fs::path(".shuati") / "problems" / pid;
            fs::create_directories(prob_dir / "data");
            fs::create_directories(prob_dir / "validator");
            fs::create_directories(prob_dir / "debug");
            fs::create_directories(prob_dir / "temp");

            // Create initial files
            std::ofstream(prob_dir / "problem.md") << "# " << ctx.new_title << "\n\nDescription here.\n";
            
            fmt::print(fg(fmt::color::green), "[+] 本地题目创建成功: {} ({})\n", ctx.new_title, pid);
            fmt::print("    目录: {}\n", prob_dir.string());
            fmt::print("    请在 data/ 放入测试用例，或在 validator/ 放入 gen.py/sol.py\n");
            
        } catch (const std::exception& e) {
            fmt::print(fg(fmt::color::red), "[!] 错误: {}\n", e.what());
        }
    });

    // ── solve ──
    auto solve_cmd = app.add_subcommand("solve", "开始解决一道题 (支持 ID 或 TID)");
    solve_cmd->add_option("id", ctx.solve_pid, "题目 ID 或 TID");
    solve_cmd->callback([&]() {
        try {
            auto root = find_root_or_die();
            auto svc = Services::load(root);
            
            Problem prob;
            if (ctx.solve_pid.empty()) {
                // Interactive Selection
                auto problems = svc.pm->list_problems();
                if (problems.empty()) { fmt::print("题库为空。\n"); return; }
                
                using namespace ftxui;
                std::vector<std::string> entries;
                for (const auto& p : problems) {
                    entries.push_back(fmt::format("{:<4} {:<20} {}", p.display_id, p.id.substr(0, 18), ensure_utf8(p.title)));
                }
                int selected = 0;
                auto menu = Menu(&entries, &selected);
                auto screen = ScreenInteractive::TerminalOutput();
                
                auto component = CatchEvent(menu, [&](Event event) {
                    if (event == Event::Return) {
                        screen.ExitLoopClosure()();
                        return true;
                    }
                    if (event == Event::Escape || event == Event::Character('q')) {
                        selected = -1; 
                        screen.ExitLoopClosure()();
                        return true;
                    }
                    return false;
                });

                auto renderer = Renderer(component, [&] {
                    return window(text("选择题目 (Enter 确认, q 退出)"), menu->Render() | vscroll_indicator | frame | size(HEIGHT, LESS_THAN, 20)) | border;
                });
                
                screen.Loop(renderer);
                if (selected >= 0 && selected < problems.size()) {
                    prob = problems[selected];
                } else {
                    return;
                }
            } else {
                try {
                    int tid = std::stoi(ctx.solve_pid);
                    prob = svc.db->get_problem_by_display_id(tid);
                } catch (...) {
                    prob = svc.db->get_problem(ctx.solve_pid);
                }
            }

            if (prob.id.empty()) {
                fmt::print(fg(fmt::color::red), "[!] 未找到题目: {}\n", ctx.solve_pid);
                return;
            }

            fmt::print(fg(fmt::color::cyan), "=== 开始练习: {} ===\n", ensure_utf8(prob.title));
            if (fs::exists(prob.content_path)) {
                fmt::print("题目描述: {}\n", prob.content_path);
            }

            std::string ext = svc.cfg.language == "python" ? ".py" : ".cpp";
            std::string filename = "solution_" + prob.id + ext;
            if (!fs::exists(filename)) {
                std::ofstream out(filename, std::ios::out | std::ios::binary);
                if (svc.cfg.language == "python") {
                    out << "import sys\n\n\ndef main():\n    pass\n\n\nif __name__ == \"__main__\":\n    main()\n";
                } else {
                    out << "#include <iostream>\n\nint main() {\n    std::ios::sync_with_stdio(false);\n    std::cin.tie(nullptr);\n\n    return 0;\n}\n";
                }
                fmt::print(fg(fmt::color::green), "[+] 代码文件已就绪: {}\n", filename);
            }
            
            if (!svc.cfg.editor.empty()) {
                std::string cmd = fmt::format("{} \"{}\"", svc.cfg.editor, filename);
                if (std::system(cmd.c_str()) != 0) {}
            }
        } catch (const std::exception& e) {
            fmt::print(fg(fmt::color::red), "[!] 错误: {}\n", e.what());
        }
    });

    // ── list ──
    app.add_subcommand("list", "列出所有题目")->callback([]() {
        try {
            auto svc = Services::load(find_root_or_die());
            auto problems = svc.pm->list_problems();
            if (problems.empty()) {
                fmt::print("题库为空。\n");
                return;
            }
            fmt::print("{:<4} {:<20} {:<30} {:<8} {}\n", "TID", "ID", "标题", "难度", "来源");
            fmt::print("{}\n", std::string(80, '-'));
            for (auto& p : problems) {
                std::string title = shorten_utf8_lossy(p.title, 25);
                fmt::print("{:<4} {:<20} {:<30} {:<8} {}\n",
                           p.display_id,
                           ensure_utf8(p.id.substr(0, 18)),
                           title,
                           ensure_utf8(p.difficulty),
                           ensure_utf8(p.source));
            }
        } catch (const std::exception& e) {
            fmt::print(fg(fmt::color::red), "[!] 错误: {}\n", e.what());
        }
    });

    // ── delete ──
    // ── delete ──
    auto delete_cmd = app.add_subcommand("delete", "删除题目 (支持 TID)");
    delete_cmd->add_option("id", ctx.solve_pid, "题目 ID 或 TID");
    delete_cmd->callback([&]() {
        try {
            auto svc = Services::load(find_root_or_die());
            
            if (ctx.solve_pid.empty()) {
                // Interactive Selection
                auto problems = svc.pm->list_problems();
                if (problems.empty()) { fmt::print("题库为空。\n"); return; }
                
                using namespace ftxui;
                std::vector<std::string> entries;
                for (const auto& p : problems) {
                    entries.push_back(fmt::format("{:<4} {:<20} {}", p.display_id, p.id.substr(0, 18), ensure_utf8(p.title)));
                }
                int selected = 0;
                auto menu = Menu(&entries, &selected);
                auto screen = ScreenInteractive::TerminalOutput();

                auto component = CatchEvent(menu, [&](Event event) {
                    if (event == Event::Return) {
                        screen.ExitLoopClosure()();
                        return true;
                    }
                    if (event == Event::Escape || event == Event::Character('q')) {
                        selected = -1;
                        screen.ExitLoopClosure()();
                        return true;
                    }
                    return false;
                });

                auto renderer = Renderer(component, [&] {
                    return window(text("选择要删除的题目 (Enter 确认, q 退出)"), menu->Render() | vscroll_indicator | frame | size(HEIGHT, LESS_THAN, 20)) | border;
                });
                
                screen.Loop(renderer);
                if (selected >= 0 && selected < problems.size()) {
                    ctx.solve_pid = problems[selected].id;
                } else {
                    return;
                }
            }

            // Confirm deletion
            fmt::print(fg(fmt::color::yellow), "确定要删除题目 '{}' 吗? [y/N] ", ctx.solve_pid);
            char confirm; std::cin >> confirm;
            if (confirm != 'y' && confirm != 'Y') {
                fmt::print("操作取消。\n");
                return;
            }

            try {
                int tid = std::stoi(ctx.solve_pid);
                svc.pm->delete_problem(tid);
            } catch (...) {
                svc.pm->delete_problem(ctx.solve_pid);
            }
            fmt::print(fg(fmt::color::green), "[+] 题目已删除。\n");
        } catch (const std::exception& e) {
            fmt::print(fg(fmt::color::red), "[!] 错误: {}\n", e.what());
        }
    });

    // ── submit ──
    auto submit_cmd = app.add_subcommand("submit", "提交并记录心得");
    submit_cmd->add_option("id", ctx.submit_pid, "题目 ID")->required();
    submit_cmd->add_option("-q,--quality", ctx.submit_quality, "掌握程度 (0-5)");
    submit_cmd->callback([&]() {
        try {
            auto svc = Services::load(find_root_or_die());
            
            if (ctx.submit_quality < 0 || ctx.submit_quality > 5) {
                fmt::print("掌握程度 (0=不会, 5=秒杀): ");
                std::string val; std::getline(std::cin, val);
                try { ctx.submit_quality = std::stoi(val); } catch (...) { ctx.submit_quality = 3; }
            }

            if (ctx.submit_quality < 3) {
                 fmt::print("记录一个错误类型 (逻辑/细节/算法/TLE): ");
                 std::string type, desc;
                 std::getline(std::cin, type);
                 fmt::print("详细描述 (或按空格跳过): ");
                 std::getline(std::cin, desc);
                 svc.ma->log_mistake(ctx.submit_pid, type, desc);
            }

            auto review = svc.db->get_review(ctx.submit_pid);
            review = SM2Algorithm::update(review, ctx.submit_quality);
            svc.db->upsert_review(review);
            fmt::print(fg(fmt::color::green), "[+] 记录成功。下次复习间隔: {} 天。\n", review.interval);
        } catch (const std::exception& e) {
            fmt::print(fg(fmt::color::red), "[!] 错误: {}\n", e.what());
        }
    });

    // ── test ──
    auto test_cmd = app.add_subcommand("test", "运行测试用例");
    test_cmd->add_option("id", ctx.solve_pid, "题目 ID")->required();
    test_cmd->add_option("--max", ctx.test_max_cases, "最多生成测试点数 (含样例)");
    test_cmd->add_option("--oracle", ctx.test_oracle, "预期输出生成方式: auto/ai/none")
        ->check(CLI::IsMember({"auto", "ai", "none"}));
    test_cmd->add_flag("--ui", ctx.test_ui, "交互查看每个测试点输入输出详情");
    test_cmd->callback([&]() { cmd_test(ctx); });


    // ── hint ──
    auto hint_cmd = app.add_subcommand("hint", "获取 AI 启发式提示");
    hint_cmd->add_option("id", ctx.hint_pid, "题目 ID")->required();
    hint_cmd->add_option("-f,--file", ctx.hint_file, "当前代码文件");
    hint_cmd->callback([&]() {
        try {
            auto svc = Services::load(find_root_or_die());
            auto prob = svc.pm->get_problem(ctx.hint_pid);
            if (prob.id.empty()) { fmt::print(fg(fmt::color::red), "[!] 题目不存在。\n"); return; }
            
            std::string desc;
            desc += fmt::format("题目: {}\n", ensure_utf8(prob.title));
            desc += fmt::format("ID: {}\n", prob.id);
            desc += fmt::format("来源: {}\n", ensure_utf8(prob.source));
            desc += fmt::format("难度: {}\n", ensure_utf8(prob.difficulty));
            desc += fmt::format("URL: {}\n\n", ensure_utf8(prob.url));

            if (fs::exists(prob.content_path)) {
                std::ifstream f(prob.content_path);
                std::string body(std::istreambuf_iterator<char>(f), {});
                if (!body.empty()) {
                    desc += "题面/笔记文件内容:\n";
                    desc += body.substr(0, 6000);
                    desc += "\n\n";
                }
            }

            auto cases = svc.db->get_test_cases(prob.id);
            if (!cases.empty()) {
                desc += "测试用例(最多展示 3 个):\n";
                for (size_t i = 0; i < cases.size() && i < 3; i++) {
                    desc += fmt::format("Case {} Input:\n{}\nCase {} Expected:\n{}\n\n",
                                        i + 1,
                                        cases[i].first.substr(0, 800),
                                        i + 1,
                                        cases[i].second.substr(0, 800));
                }
            }
            
            if (ctx.hint_file.empty()) ctx.hint_file = "solution_" + prob.id + (svc.cfg.language == "python" ? ".py" : ".cpp");
            std::string code = "";
            if (fs::exists(ctx.hint_file)) {
                std::ifstream f(ctx.hint_file);
                code.assign(std::istreambuf_iterator<char>(f), {});
            }

            fmt::print(fg(fmt::color::yellow), "[教练] 思考中 (按 Ctrl+C 中止)...\n\n");
            
            // Streaming output callback
            auto print_chunk = [](std::string chunk) {
                std::cout << chunk << std::flush;
            };

            svc.ai->analyze(desc, code, print_chunk);
            std::cout << "\n\n";
        } catch (const std::exception& e) {
             fmt::print(fg(fmt::color::red), "[!] 错误: {}\n", e.what());
        }
    });

    // ── config ──
    auto cfg_cmd = app.add_subcommand("config", "配置工具");
    cfg_cmd->add_flag("--show", ctx.cfg_show, "显示当前配置");
    cfg_cmd->add_option("--api-key", ctx.cfg_key, "设置 API Key");
    cfg_cmd->add_option("--model", ctx.cfg_model, "设置模型 (默认 deepseek-chat)");
    cfg_cmd->add_option("--language", ctx.new_diff, "设置默认语言 (cpp/python)"); // Reuse variable or add new one? reused just for parsing, need separate field
    cfg_cmd->callback([&]() {
        try {
            auto root = find_root_or_die();
            auto cfg_path = Config::config_path(root);
            auto cfg = Config::load(cfg_path);

            // Handle positional args or mixed flags logic if complex, but here flags are enough
            // We need a proper context field for language since new_diff is for 'new' command
            // Let's assume user uses flags. 
            // NOTE: ctx.new_diff is used above for --language just to capture string, dirty but works if no conflict in subcommands.
            // Better to add ctx.cfg_lang.

            if (ctx.cfg_show) {
                fmt::print(fg(fmt::color::cyan) | fmt::emphasis::bold, "当前配置:\n");
                fmt::print("  API Key:      {}\n", cfg.api_key.empty() ? "(未设置)" : "******" + cfg.api_key.substr(std::max(0, (int)cfg.api_key.length()-4)));
                fmt::print("  Model:        {}\n", cfg.model);
                fmt::print("  Language:     {}\n", cfg.language);
                fmt::print("  Editor:       {}\n", cfg.editor.empty() ? "(未设置)" : cfg.editor);
                fmt::print("  AI Enabled:   {}\n", cfg.ai_enabled ? "Yes" : "No");
                return;
            }

            bool changed = false;
            if (!ctx.cfg_key.empty()) {
                if (ctx.cfg_key.length() < 10) {
                     fmt::print(fg(fmt::color::red), "[!] API Key 格式似乎不正确。\n");
                } else {
                    cfg.api_key = ctx.cfg_key;
                    changed = true;
                }
            }
            if (!ctx.cfg_model.empty()) {
                cfg.model = ctx.cfg_model;
                changed = true;
            }
            if (!ctx.new_diff.empty() && ctx.new_diff != "medium") { // Hack: new_diff defaults to medium? No, main.cpp line 86 default is medium
                // Wait, new_diff default is "medium", so if user doesn't set it, it's "medium".
                // This reuse is risky. Let's rely on standard CLI check or empty check if we remove default in struct?
                // Context struct: std::string new_diff = "medium"; 
                // We should add cfg_lang to struct.
                std::string lang = ctx.new_diff;
                if (lang == "cpp" || lang == "c++" || lang == "python" || lang == "py") {
                    cfg.language = (lang == "py") ? "python" : (lang == "c++" ? "cpp" : lang);
                    changed = true;
                    // Reset to avoid side effects if reused
                    ctx.new_diff = "medium"; 
                } else if (lang != "medium") {
                    fmt::print(fg(fmt::color::yellow), "[!] 未知语言: {} (仅支持 cpp/python)\n", lang);
                }
            }
            
            if (changed) {
                cfg.save(cfg_path);
                fmt::print(fg(fmt::color::green), "[+] 配置已保存。\n");
            } else {
                if (!ctx.cfg_show) fmt::print("使用 --show 查看配置，或使用 --api-key/--model/--language 修改。\n");
            }
        } catch (...) { fmt::print(fg(fmt::color::red), "[!] 操作失败\n"); }
    });

    app.add_subcommand("exit", "退出程序")->callback([]() { exit(0); });
}

// ─── REPL Loop ────────────────────────────────────────

void run_repl() {
    using Replxx = replxx::Replxx;
    Replxx rx;

    auto root = Config::find_root();
    std::unique_ptr<Services> global_svc;
    if (!root.empty()) {
        try {
            global_svc = std::make_unique<Services>(Services::load(root));
            if (global_svc->companion) global_svc->companion->start();
        } catch (...) {}
    }

    // Set up completion
    rx.set_completion_callback([&](std::string const& input, int& contextLen) {
        std::vector<Replxx::Completion> completions;
        std::vector<std::string> cmds = {"init", "info", "pull", "new", "solve", "list", "delete", "submit", "test", "hint", "config", "exit"};
        
        // Command completion
        size_t last_space = input.rfind(' ');
        if (last_space == std::string::npos) {
            for (auto& c : cmds) if (c.find(input) == 0) completions.push_back({c});
            contextLen = input.length();
        } else {
            // Context-aware completion
            std::string cmd = input.substr(0, last_space);
            std::string prefix = input.substr(last_space + 1);
            
            if (cmd == "solve" || cmd == "delete" || cmd == "submit" || cmd == "hint" || cmd == "test") {
                if (global_svc && global_svc->pm) {
                    auto problems = global_svc->pm->list_problems();
                    for (const auto& p : problems) {
                        std::string label = std::to_string(p.display_id);
                        // Match against ID or Title
                        if (std::to_string(p.display_id).find(prefix) == 0 || p.title.find(prefix) == 0) {
                             completions.push_back({label});
                        }
                    }
                }
            }
            contextLen = prefix.length();
        }
        return completions;
    });

    fmt::print(fg(fmt::color::cyan) | fmt::emphasis::bold, 
        R"(
  ███████╗██╗  ██╗██╗   ██╗ █████╗ ████████╗██╗
  ██╔════╝██║  ██║██║   ██║██╔══██╗╚══██╔══╝██║
  ███████╗███████║██║   ██║███████║   ██║   ██║
  ╚════██║██╔══██║██║   ██║██╔══██║   ██║   ██║
  ███████║██║  ██║╚██████╔╝██║  ██║   ██║   ██║
  ╚══════╝╚═╝  ╚═╝ ╚═════╝ ╚═╝  ╚═╝   ╚═╝   ╚═╝
        )" "\n");
    fmt::print("  输入 'help' 查看命令, 'exit' 退出\n\n");

    while (true) {
        // ... (existing loop code, but "input" variable declaration is handled by replxx)
        const char* input = rx.input("shuati > ");
        if (input == nullptr) break;
        std::string line(input);
        if (line.empty()) continue;
        
        rx.history_add(line);

        std::stringstream ss(line);
        std::string word;
        std::vector<std::string> args;
        args.push_back("shuati"); 
        while (ss >> word) args.push_back(word);

        if (args.size() == 1) continue;
        if (args[1] == "exit" || args[1] == "quit") break;
        
        // ─── Shell Command Emulation ───
        if (args[1] == "ls" || args[1] == "dir") {
            try {
                auto cwd = fs::current_path();
                fmt::print(fg(fmt::color::cyan) | fmt::emphasis::bold, "目录: {}\n\n", cwd.string());
                
                std::vector<fs::directory_entry> entries;
                for (const auto& entry : fs::directory_iterator(cwd)) {
                    entries.push_back(entry);
                }
                
                std::sort(entries.begin(), entries.end(), [](const auto& a, const auto& b) {
                    if (a.is_directory() != b.is_directory()) return a.is_directory();
                    return a.path().filename().string() < b.path().filename().string();
                });
                
                for (const auto& entry : entries) {
                    std::string name = entry.path().filename().string();
                    if (entry.is_directory()) {
                        fmt::print(fg(fmt::color::blue) | fmt::emphasis::bold, "  [DIR]  {}/\n", ensure_utf8(name));
                    } else {
                        auto size = entry.file_size();
                        std::string size_str;
                        if (size < 1024) size_str = fmt::format("{} B", size);
                        else if (size < 1024 * 1024) size_str = fmt::format("{:.1f} KB", size / 1024.0);
                        else size_str = fmt::format("{:.1f} MB", size / (1024.0 * 1024.0));
                        fmt::print("  [FILE] {:<40} {}\n", ensure_utf8(name), size_str);
                    }
                }
                fmt::print("\n");
            } catch (const std::exception& e) {
                fmt::print(fg(fmt::color::red), "[!] 错误: {}\n", e.what());
            }
            continue;
        }
        
        if (args[1] == "cd") {
            if (args.size() < 3) {
                fmt::print(fg(fmt::color::yellow), "用法: cd <目录>\n");
                continue;
            }
            try {
                fs::path target = args[2];
                if (fs::exists(target) && fs::is_directory(target)) {
                    fs::current_path(target);
                    fmt::print(fg(fmt::color::green), "[+] 已切换到: {}\n", fs::current_path().string());
                    BootGuard::record_history(target.string());
                    
                    // Reload services if we're in a valid project directory
                    auto new_root = Config::find_root();
                    if (!new_root.empty() && global_svc) {
                        try {
                            *global_svc = Services::load(new_root);
                            fmt::print(fg(fmt::color::cyan), "[*] 已重新加载项目上下文\n");
                        } catch (...) {}
                    }
                } else {
                    fmt::print(fg(fmt::color::red), "[!] 目录不存在: {}\n", target.string());
                }
            } catch (const std::exception& e) {
                fmt::print(fg(fmt::color::red), "[!] 错误: {}\n", e.what());
            }
            continue;
        }
        
        if (args[1] == "pwd") {
            fmt::print("{}\n", fs::current_path().string());
            continue;
        }
        
        if (args[1] == "cls" || args[1] == "clear") {
#ifdef _WIN32
            std::system("cls");
#else
            std::system("clear");
#endif
            continue;
        }
        
        if (args[1] == "help") { 
             fmt::print(fg(fmt::color::white) | fmt::emphasis::bold, "{:<10} {:<35} {}\n", "命令", "说明", "用法");
             fmt::print("{}\n", std::string(80, '-'));
             
             // Shell commands
             fmt::print(fg(fmt::color::yellow) | fmt::emphasis::bold, "Shell 命令:\n");
             fmt::print("{:<10} {:<35} {}\n", "ls/dir", "列出当前目录文件", "ls");
             fmt::print("{:<10} {:<35} {}\n", "cd", "切换目录 (自动重载项目)", "cd <目录>");
             fmt::print("{:<10} {:<35} {}\n", "pwd", "显示当前目录", "pwd");
             fmt::print("{:<10} {:<35} {}\n", "cls/clear", "清屏", "cls");
             fmt::print("\n");
             // Shuati commands
             fmt::print(fg(fmt::color::cyan) | fmt::emphasis::bold, "Shuati 命令:\n");
             fmt::print("{:<10} {:<35} {}\n", "init", "初始化项目结构", "init");
             fmt::print("{:<10} {:<35} {}\n", "info", "显示可执行文件与环境信息", "info");
             fmt::print("{:<10} {:<35} {}\n", "pull", "从 URL 拉取题目", "pull <url>");
             fmt::print("{:<10} {:<35} {}\n", "new", "创建本地题目", "new <title>");
             fmt::print("{:<10} {:<35} {}\n", "solve", "开始做题 (支持交互选择)", "solve [id]");
             fmt::print("{:<10} {:<35} {}\n", "list", "列出所有题目", "list");
             fmt::print("{:<10} {:<35} {}\n", "delete", "删除题目", "delete <id>");
             fmt::print("{:<10} {:<35} {}\n", "submit", "提交记录与心得", "submit <id>");
             fmt::print("{:<10} {:<35} {}\n", "test", "运行测试用例", "test <id>");
             fmt::print("{:<10} {:<35} {}\n", "hint", "获取 AI 提示", "hint <id>");
             fmt::print("{:<10} {:<35} {}\n", "config", "配置工具", "config [--show]");
             fmt::print("{:<10} {:<35} {}\n", "exit", "退出", "exit");
             fmt::print("\n");
             continue; 
        }

        CLI::App app{"REPL"};
        app.allow_extras();
        CommandContext ctx;
        setup_commands(app, ctx);

        std::vector<const char*> argv;
        for (const auto& a : args) argv.push_back(a.c_str());

        try {
            app.parse(argv.size(), const_cast<char**>(argv.data()));
        } catch (const CLI::ParseError& e) {
            app.exit(e);
        } catch (const std::exception& e) {
            fmt::print(fg(fmt::color::red), "[!] 运行时错误: {}\n", e.what());
        }
    }
    
    if (global_svc && global_svc->companion) global_svc->companion->stop();
}

// ─── Main ─────────────────────────────────────────────

int main(int argc, char** argv) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif

    // Initialize Logger
    auto root = Config::find_root();
    if (!root.empty()) {
        Logger::instance().init((Config::data_dir(root) / "logs" / "shuati.log").string());
    } else {
        // Fallback or dry run, maybe init in current dir if .shuati exists?
        // For now, only log if project is initialized to avoid cluttering random dirs.
    }
    
    Logger::instance().info("Session started. Version: {}", current_version().to_string());

    // ─── Boot Guard: Smart Startup Check ───
    if (argc == 1) {
        if (!BootGuard::check()) return 0;
    }

    if (argc == 1) {
        run_repl();
    } else {
        CLI::App app{"shuati CLI"};
        CommandContext ctx;
        setup_commands(app, ctx);

#ifdef _WIN32
        int wargc;
        LPWSTR* wargv = CommandLineToArgvW(GetCommandLineW(), &wargc);
        if (wargv) {
            std::vector<std::string> args;
            for (int i = 0; i < wargc; i++) {
                args.push_back(shuati::utils::wide_to_utf8(wargv[i]));
            }
            LocalFree(wargv);
            
            std::vector<const char*> c_args;
            for (const auto& arg : args) {
                c_args.push_back(arg.c_str());
            }

            try {
                app.parse(c_args.size(), const_cast<char**>(c_args.data()));
            } catch (const CLI::ParseError& e) {
                return app.exit(e);
            }
        } else {
            CLI11_PARSE(app, argc, argv);
        }
#else
        CLI11_PARSE(app, argc, argv);
#endif
    }
    return 0;
}
