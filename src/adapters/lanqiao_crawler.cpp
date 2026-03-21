#include <filesystem>  // MSVC workaround: include before other headers

#include "shuati/adapters/lanqiao_crawler.hpp"
#include "shuati/adapters/base_crawler.hpp"
#include <ctime>
#include <nlohmann/json.hpp>

namespace shuati {

class LanqiaoCrawlerImpl : public BaseCrawler {
public:
    explicit LanqiaoCrawlerImpl(std::shared_ptr<IHttpClient> client, std::string cookie = "")
        : BaseCrawler(client), cookie_(std::move(cookie)) {}

    bool can_handle(const std::string& url) const override {
        return utils::contains(url, "lanqiao.cn");
    }

    cpr::Header get_default_headers() const override {
        cpr::Header h{
            {"User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36"},
            {"Accept", "application/json, text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8"},
            {"Accept-Language", "zh-CN,zh;q=0.9,en;q=0.8"},
            {"Referer", "https://www.lanqiao.cn/"}
        };
        if (!cookie_.empty()) {
            h["Cookie"] = cookie_;
        }
        return h;
    }

    int get_default_timeout_ms() const override {
        return 15000; // 15 seconds - Lanqiao can be slow
    }

    Problem fetch_problem(const std::string& url) override {
        Problem p;
        p.source = "Lanqiao";
        p.url = url;

        // Extract ID from URL: /problems/1234/learning/ or /problems/1234/
        std::string id_str = extract_regex(url, R"(problems/(\d+))");
        if (id_str.empty()) {
            p.id = generate_id("lq");
            p.title = "蓝桥云课题目 (ID Unknown)";
            return p;
        }

        p.id = "lq_" + id_str;
        p.display_id = std::stoi(id_str);

        try {
            // Try the API approach first
            std::string api_url = fmt::format("https://www.lanqiao.cn/api/v2/problems/{}/", id_str);

            std::map<std::string, std::string> headers;
            for (const auto& kv : get_default_headers()) {
                headers[kv.first] = kv.second;
            }

            auto response = http_client_->get(api_url, headers, get_default_timeout_ms());

            if (response.status_code == 200) {
                // Parse JSON response
                try {
                    auto json = nlohmann::json::parse(response.text);
                    parse_api_response(json, p, id_str);
                    return p;
                } catch (...) {
                    // JSON parse failed, fall through to HTML
                }
            } else if (response.status_code == 401 || response.status_code == 403) {
                // Login required, inject friendly hint below
            }

            // API failed (likely 401/403), try HTML approach
            try {
                std::string html = http_get(url);

                p.title = extract_title(html);
                p.title = clean_title(p.title, {" - 蓝桥云课", " | 蓝桥"});

                if (p.title.empty() || p.title == "蓝桥账户中心") {
                    p.title = fmt::format("蓝桥云课 #{}", id_str);
                }

                // Build description, with login hint if no cookie configured
                if (cookie_.empty()) {
                    p.description = fmt::format(
                        "蓝桥云课题目 #{}\n\n"
                        "> ⚠️  蓝桥云课需要登录才能查看完整题目内容。\n"
                        "> 请运行以下命令配置登录凭据后重新拉取：\n"
                        ">\n"
                        ">     shuati login lanqiao\n"
                        ">\n"
                        "> 或直接访问 {} 查看题目。\n",
                        id_str, url);
                } else {
                    p.description = fmt::format(
                        "蓝桥云课题目 #{}\n\n"
                        "> ⚠️  已配置登录凭据但仍无法获取题目内容。\n"
                        "> Cookie 可能已过期，请运行 `shuati login lanqiao` 重新配置。\n"
                        "> 或直接访问 {} 查看题目。\n",
                        id_str, url);
                }

            } catch (const std::exception& e) {
                // Network error
                p.title = fmt::format("蓝桥云课 #{}", id_str);
                p.description = fmt::format(
                    "蓝桥云课题目 #{}\n\n"
                    "> 无法抓取题目内容: {}\n"
                    "> 蓝桥云课可能需要登录才能访问。请直接访问 {} 查看题目。\n",
                    id_str, e.what(), url);
            }

        } catch (const std::exception& e) {
            p.title = fmt::format("Error: {}", e.what());
        }

        return p;
    }

private:
    std::string cookie_; // Lanqiao session cookie

    void parse_api_response(const nlohmann::json& json, Problem& p, const std::string& id_str) {
        // Title
        std::string title = json.value("title", "");
        if (title.empty()) title = json.value("name", ""); // fallback to "name"
        
        if (!title.empty()) {
            p.title = fmt::format("LQ{}. {}", id_str, title);
        } else {
            p.title = fmt::format("蓝桥云课 #{}", id_str);
        }

        // Difficulty
        if (json.contains("difficulty")) {
            auto diff = json["difficulty"];
            if (diff.is_string()) {
                std::string ds = diff.get<std::string>();
                if (ds == "简单" || ds == "easy" || ds == "Easy") p.difficulty = "easy";
                else if (ds == "困难" || ds == "hard" || ds == "Hard") p.difficulty = "hard";
                else p.difficulty = "medium";
            } else if (diff.is_number_integer()) {
                int level = diff.get<int>();
                if (level == 1) p.difficulty = "easy";
                else if (level == 3) p.difficulty = "hard";
                else p.difficulty = "medium";
            }
        }

        // Tags
        if (json.contains("tags") && json["tags"].is_array()) {
            std::vector<std::string> tag_names;
            for (const auto& tag : json["tags"]) {
                if (tag.is_string()) {
                    tag_names.push_back(tag.get<std::string>());
                } else if (tag.is_object() && tag.contains("name")) {
                    tag_names.push_back(tag["name"].get<std::string>());
                }
            }
            p.tags = utils::join(tag_names, ",");
        }

        // Description — try known field names first, then scan inner nested objects (oj, question_answer)
        std::string desc;
        static const std::vector<std::string> CONTENT_KEYS = {
            "content", "description", "body", "detail", "problem_description",
            "statement", "problem", "introduction", "content_en", "html"
        };
        
        auto find_desc_in_obj = [&](const nlohmann::json& obj) {
            if (!obj.is_object()) return false;
            for (const auto& key : CONTENT_KEYS) {
                if (obj.contains(key) && obj[key].is_string()) {
                    desc = obj[key].get<std::string>();
                    if (!desc.empty()) return true;
                }
            }
            return false;
        };

        if (!find_desc_in_obj(json)) {
            if (json.contains("oj")) find_desc_in_obj(json["oj"]);
            if (desc.empty() && json.contains("question_answer")) find_desc_in_obj(json["question_answer"]);
        }

        // Deep fallback: recursively find any string with HTML paragraphs or long text
        if (desc.empty()) {
            std::function<void(const nlohmann::json&)> search_deep = [&](const nlohmann::json& j) {
                if (j.is_object()) {
                    for (auto it = j.begin(); it != j.end(); ++it) {
                        if (it.value().is_string()) {
                            std::string s = it.value().get<std::string>();
                            if (s.length() > desc.length() && (s.find("<p") != std::string::npos || s.find("样例") != std::string::npos || s.length() > 50)) {
                                desc = s; // Take the longest looking like HTML/Chinese text
                            }
                        } else if (it.value().is_object() || it.value().is_array()) {
                            search_deep(it.value());
                        }
                    }
                } else if (j.is_array()) {
                    for (const auto& item : j) search_deep(item);
                }
            };
            search_deep(json);
        }

        // If still empty, dump all top-level string keys to stderr for diagnosis
        if (desc.empty()) {
            fmt::print(stderr, "[DEBUG] Lanqiao API response keys:\n");
            for (auto it = json.begin(); it != json.end(); ++it) {
                fmt::print(stderr, "  {} ({}): ", it.key(), it.value().type_name());
                if (it.value().is_string()) {
                    auto s = it.value().get<std::string>();
                    fmt::print(stderr, "{}\n", s.substr(0, 120));
                } else {
                    fmt::print(stderr, "[non-string]\n");
                }
            }
        }

        if (!desc.empty()) {
            // Clean HTML if present
            desc = utils::replace_all(desc, "<br>", "\n");
            desc = utils::replace_all(desc, "<br/>", "\n");
            desc = utils::replace_all(desc, "<br />", "\n");
            desc = utils::replace_all(desc, "</p>", "\n\n");
            desc = utils::replace_all(desc, "</li>", "\n");
            desc = utils::replace_all(desc, "&nbsp;", " ");
            desc = utils::replace_all(desc, "&lt;", "<");
            desc = utils::replace_all(desc, "&gt;", ">");
            desc = utils::replace_all(desc, "&amp;", "&");
            desc = utils::strip_html_tags(desc);
            desc = utils::trim(desc);
            p.description = desc;
        } else {
            p.description = "题目内容请访问蓝桥云课查看";
        }
    }
};

// Public interface
LanqiaoCrawler::LanqiaoCrawler(std::shared_ptr<IHttpClient> client, std::string cookie)
    : impl_(std::make_unique<LanqiaoCrawlerImpl>(client, std::move(cookie))) {}
LanqiaoCrawler::~LanqiaoCrawler() = default;

bool LanqiaoCrawler::can_handle(const std::string& url) const {
    return impl_->can_handle(url);
}

Problem LanqiaoCrawler::fetch_problem(const std::string& url) {
    return impl_->fetch_problem(url);
}

std::vector<TestCase> LanqiaoCrawler::fetch_test_cases(const std::string& url) {
    return impl_->fetch_test_cases(url);
}

} // namespace shuati
