#include <filesystem>  // MSVC workaround: include before other headers

#include "shuati/adapters/matiji_crawler.hpp"
#include "shuati/adapters/base_crawler.hpp"
#include <ctime>
#include <set>

namespace shuati {

namespace {

std::string normalize_text(std::string s) {
    s = utils::replace_all(s, "\r\n", "\n");
    s = utils::replace_all(s, "&nbsp;", " ");
    s = utils::replace_all(s, "&lt;", "<");
    s = utils::replace_all(s, "&gt;", ">");
    s = utils::replace_all(s, "&amp;", "&");
    return utils::trim(s);
}

std::string build_fallback_id_from_url(const std::string& url) {
    const auto h = std::hash<std::string>{}(url);
    return "mtj_url" + std::to_string(static_cast<unsigned long long>(h));
}

std::string extract_matiji_id_impl(const std::string& url) {
    std::string id = utils::extract_regex_group(url, R"((?:ojquestion|question)/(\d+))");
    if (!id.empty()) return id;
    id = utils::extract_regex_group(url, R"(qid=(\d+))");
    if (!id.empty()) return id;
    id = utils::extract_regex_group(url, R"(questionId=(\d+))");
    if (!id.empty()) return id;
    return "";
}

std::vector<TestCase> extract_samples_from_html(const std::string& html) {
    std::vector<TestCase> cases;
    std::set<std::pair<std::string, std::string>> uniq;

    const std::regex block_re(R"((?:输入样例|样例输入|Input(?: Example)?)[\s\S]{0,40}?(?:<pre[^>]*>|<code[^>]*>|<textarea[^>]*>)([\s\S]*?)(?:</pre>|</code>|</textarea>)[\s\S]{0,80}?(?:输出样例|样例输出|Output(?: Example)?)[\s\S]{0,40}?(?:<pre[^>]*>|<code[^>]*>|<textarea[^>]*>)([\s\S]*?)(?:</pre>|</code>|</textarea>))", std::regex::icase);

    auto begin = std::sregex_iterator(html.begin(), html.end(), block_re);
    auto end = std::sregex_iterator();

    for (auto it = begin; it != end; ++it) {
        auto in = normalize_text((*it)[1].str());
        auto out = normalize_text((*it)[2].str());
        if (in.empty()) continue;
        if (uniq.insert({in, out}).second) {
            cases.push_back({in, out, true});
        }
    }

    return cases;
}

} // namespace

class MatijiCrawlerImpl : public BaseCrawler {
public:
    bool can_handle(const std::string& url) const override {
        return utils::contains(url, "matiji.net") &&
               (utils::contains(url, "/exam/ojquestion") ||
                utils::contains(url, "/exam/ojquestionlist") ||
                utils::contains(url, "/exam/question"));
    }

    Problem fetch_problem(const std::string& url) override {
        Problem p;
        p.source = "Matiji";
        p.url = url;

        try {
            std::string html = http_get(url);
            p.title = normalize_text(extract_title(html));
            p.title = clean_title(p.title, {" - 马蹄集", "- 马蹄集", "| 马蹄集", "_马蹄集"});
            if (p.title.empty()) {
                p.title = utils::contains(url, "ojquestionlist") ? "Matiji OJ Question List" : "Matiji Problem";
            }

            std::string id = MatijiCrawler::extract_problem_id_from_url(url);
            if (id.empty()) {
                id = build_fallback_id_from_url(url);
            }
            p.id = generate_id("mtj", id);
        } catch (const std::exception& e) {
            p.title = fmt::format("Error: {}", e.what());
            p.id = generate_id("mtj", build_fallback_id_from_url(url));
        }

        return p;
    }

    std::vector<TestCase> fetch_test_cases(const std::string& url) override {
        try {
            return MatijiCrawler::parse_test_cases_from_html(http_get(url));
        } catch (...) {
            return {};
        }
    }
};

MatijiCrawler::MatijiCrawler() : impl_(std::make_unique<MatijiCrawlerImpl>()) {}
MatijiCrawler::~MatijiCrawler() = default;

bool MatijiCrawler::can_handle(const std::string& url) const {
    return impl_->can_handle(url);
}

Problem MatijiCrawler::fetch_problem(const std::string& url) {
    return impl_->fetch_problem(url);
}

std::vector<TestCase> MatijiCrawler::fetch_test_cases(const std::string& url) {
    return impl_->fetch_test_cases(url);
}


std::string MatijiCrawler::extract_problem_id_from_url(const std::string& url) {
    return extract_matiji_id_impl(url);
}

std::vector<TestCase> MatijiCrawler::parse_test_cases_from_html(const std::string& html) {
    return extract_samples_from_html(html);
}

} // namespace shuati
