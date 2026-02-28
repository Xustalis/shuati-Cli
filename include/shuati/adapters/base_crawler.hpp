#pragma once

#include "shuati/crawler.hpp"
#include "shuati/utils/string_utils.hpp"
#include <cpr/cpr.h>
#include <fmt/core.h>
#include <regex>
#include <ctime>
#include <atomic>
#include <cstdlib>
#include <algorithm>
#include <chrono>
#include <mutex>
#include <random>
#include <thread>

namespace shuati {

struct CrawlerHttpPolicy {
    int timeout_ms = 10000;
    int max_retries = 2;
    int min_interval_ms = 200;
    int backoff_base_ms = 300;
};

// Abstract base class providing common functionality for web crawlers
class BaseCrawler : public ICrawler {
public:
    virtual ~BaseCrawler() = default;

    // Default empty test cases implementation - made public for Impl classes
    virtual std::vector<TestCase> fetch_test_cases(const std::string& url) override {
        (void)url;
        return {};
    }

protected:
    static CrawlerHttpPolicy load_policy_from_env() {
        CrawlerHttpPolicy p;
        if (const char* v = std::getenv("SHUATI_CRAWLER_TIMEOUT_MS")) {
            p.timeout_ms = std::max(1000, std::atoi(v));
        }
        if (const char* v = std::getenv("SHUATI_CRAWLER_MAX_RETRIES")) {
            p.max_retries = std::clamp(std::atoi(v), 0, 8);
        }
        if (const char* v = std::getenv("SHUATI_CRAWLER_MIN_INTERVAL_MS")) {
            p.min_interval_ms = std::max(0, std::atoi(v));
        }
        if (const char* v = std::getenv("SHUATI_CRAWLER_BACKOFF_BASE_MS")) {
            p.backoff_base_ms = std::max(50, std::atoi(v));
        }
        return p;
    }

    // HTTP request helpers with common configuration
    virtual cpr::Header get_default_headers() const {
        const auto uas = user_agents();
        const size_t idx = ua_index().fetch_add(1) % uas.size();
        return cpr::Header{
            {"User-Agent", uas[idx]}
        };
    }

    virtual cpr::Timeout get_default_timeout() const {
        static const CrawlerHttpPolicy policy = load_policy_from_env();
        return cpr::Timeout{policy.timeout_ms};
    }

    virtual std::vector<std::string> get_proxy_pool() const {
        std::vector<std::string> proxies;
        const char* raw = std::getenv("SHUATI_CRAWLER_PROXIES");
        if (!raw || !*raw) return proxies;
        for (const auto& item : utils::split(std::string(raw), ',')) {
            auto proxy = utils::trim(item);
            if (!proxy.empty()) proxies.push_back(proxy);
        }
        return proxies;
    }

    static void throttle_before_request() {
        static std::mutex mtx;
        static auto last = std::chrono::steady_clock::now() - std::chrono::seconds(1);
        static const CrawlerHttpPolicy policy = load_policy_from_env();

        std::lock_guard<std::mutex> lk(mtx);
        auto now = std::chrono::steady_clock::now();
        auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - last).count();
        if (elapsed_ms < policy.min_interval_ms) {
            std::this_thread::sleep_for(std::chrono::milliseconds(policy.min_interval_ms - elapsed_ms));
        }
        last = std::chrono::steady_clock::now();
    }

    static int backoff_with_jitter(int attempt) {
        static const CrawlerHttpPolicy policy = load_policy_from_env();
        static thread_local std::mt19937 rng{std::random_device{}()};
        std::uniform_int_distribution<int> jitter(0, 120);
        int base = policy.backoff_base_ms * (1 << std::min(attempt, 10));
        return std::min(10000, base + jitter(rng));
    }

    static bool should_retry_status(int status_code) {
        return status_code == 403 || status_code == 408 || status_code == 429 || status_code >= 500;
    }

    template <typename RequestFn>
    cpr::Response request_with_retry(RequestFn&& fn, const std::string& url) const {
        static const CrawlerHttpPolicy policy = load_policy_from_env();
        cpr::Response last;
        auto proxies = get_proxy_pool();

        for (int attempt = 0; attempt <= policy.max_retries; ++attempt) {
            throttle_before_request();
            cpr::Header headers = get_default_headers();

            if (!proxies.empty()) {
                const auto& proxy = proxies[attempt % proxies.size()];
                last = fn(headers, proxy);
            } else {
                last = fn(headers, "");
            }

            if (last.error.code == cpr::ErrorCode::OK && last.status_code >= 200 && last.status_code < 300) {
                return last;
            }

            if (attempt < policy.max_retries && (last.error.code != cpr::ErrorCode::OK || should_retry_status(last.status_code))) {
                std::this_thread::sleep_for(std::chrono::milliseconds(backoff_with_jitter(attempt)));
                continue;
            }
            break;
        }

        throw std::runtime_error(fmt::format(
            "HTTP request failed (url={}, status={}, error={})",
            url,
            last.status_code,
            static_cast<int>(last.error.code)
        ));
    }

    static std::atomic<size_t>& ua_index() {
        static std::atomic<size_t> idx{0};
        return idx;
    }

    static const std::vector<std::string>& user_agents() {
        static const std::vector<std::string> uas = {
            "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 Chrome/124.0 Safari/537.36",
            "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 Chrome/123.0 Safari/537.36",
            "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/605.1.15 Version/17.0 Safari/605.1.15",
            "Mozilla/5.0 (Windows NT 10.0; Win64; x64; rv:124.0) Gecko/20100101 Firefox/124.0"
        };
        return uas;
    }

    // Perform HTTP GET request
    virtual std::string http_get(const std::string& url) const {
        auto response = request_with_retry([&](const cpr::Header& headers, const std::string& proxy) {
            if (!proxy.empty()) {
                return cpr::Get(
                    cpr::Url{url},
                    headers,
                    cpr::Proxies{{"http", proxy}, {"https", proxy}},
                    get_default_timeout()
                );
            }
            return cpr::Get(cpr::Url{url}, headers, get_default_timeout());
        }, url);
        return response.text;
    }

    // Perform HTTP POST request with JSON body
    virtual std::string http_post_json(const std::string& url, const std::string& json_body) const {
        auto response = request_with_retry([&](cpr::Header headers, const std::string& proxy) {
            headers["Content-Type"] = "application/json";
            if (!proxy.empty()) {
                return cpr::Post(
                    cpr::Url{url},
                    headers,
                    cpr::Proxies{{"http", proxy}, {"https", proxy}},
                    cpr::Body{json_body},
                    get_default_timeout()
                );
            }
            return cpr::Post(cpr::Url{url}, headers, cpr::Body{json_body}, get_default_timeout());
        }, url);
        return response.text;
    }

    // Common HTML parsing helpers
    virtual std::string extract_title(const std::string& html) const {
        return utils::extract_title_from_html(html);
    }

    virtual std::string extract_regex(const std::string& text, const std::string& pattern, size_t group = 1) const {
        return utils::extract_regex_group(text, pattern, group);
    }

    // Generate unique problem ID with prefix
    virtual std::string generate_id(const std::string& prefix, const std::string& identifier = "") const {
        if (!identifier.empty()) {
            return fmt::format("{}_{}", prefix, identifier);
        }
        return fmt::format("{}_{}", prefix, std::time(nullptr));
    }

    // Clean up title by removing common suffixes
    virtual std::string clean_title(std::string title, const std::vector<std::string>& suffixes_to_remove = {}) const {
        for (const auto& suffix : suffixes_to_remove) {
            auto pos = title.find(suffix);
            if (pos != std::string::npos) {
                title = title.substr(0, pos);
            }
        }
        return utils::trim(title);
    }
};

// Simple HTML-based crawler for platforms that only need basic parsing
class SimpleHtmlCrawler : public BaseCrawler {
public:
    SimpleHtmlCrawler(std::string name, std::string url_pattern, std::string id_prefix)
        : name_(std::move(name))
        , url_pattern_(std::move(url_pattern))
        , id_prefix_(std::move(id_prefix)) {}

    bool can_handle(const std::string& url) const override {
        return utils::contains(url, url_pattern_);
    }

    Problem fetch_problem(const std::string& url) override {
        Problem p;
        p.source = name_;
        p.url = url;

        try {
            std::string html = http_get(url);
            p.title = extract_title(html);
            if (p.title.empty()) {
                p.title = name_ + " Problem";
            }
            p.title = clean_title(p.title, get_title_suffixes());
            p.id = extract_id(url, html);
        } catch (const std::exception& e) {
            p.title = fmt::format("Error: {}", e.what());
        }

        return p;
    }

protected:
    // Override to provide custom title cleanup suffixes
    virtual std::vector<std::string> get_title_suffixes() const {
        return {};
    }

    // Override to extract ID from URL or HTML
    virtual std::string extract_id(const std::string& url, const std::string& html) {
        (void)html; // Unused by default
        (void)url;
        return generate_id(id_prefix_);
    }

    std::string name_;
    std::string url_pattern_;
    std::string id_prefix_;
};

} // namespace shuati
