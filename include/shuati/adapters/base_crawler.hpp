#pragma once

#include "shuati/crawler.hpp"
#include "shuati/utils/string_utils.hpp"
#include <cpr/cpr.h>
#include <fmt/core.h>
#include <regex>
#include <ctime>
#include "shuati/http_client.hpp"

namespace shuati {

// Abstract base class providing common functionality for web crawlers
class BaseCrawler : public ICrawler {
public:
    explicit BaseCrawler(std::shared_ptr<IHttpClient> client = nullptr) 
        : http_client_(client ? client : std::make_shared<CprHttpClient>()) {}
    
    virtual ~BaseCrawler() = default;

    // Default empty test cases implementation - made public for Impl classes
    virtual std::vector<TestCase> fetch_test_cases(const std::string& url) override {
        (void)url;
        return {};
    }

protected:
    std::shared_ptr<IHttpClient> http_client_;

    // HTTP request helpers with common configuration
    virtual cpr::Header get_default_headers() const {
        return cpr::Header{
            {"User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36"}
        };
    }

    virtual int get_default_timeout_ms() const {
        return 10000; // 10 seconds
    }

    // Perform HTTP GET request
    virtual std::string http_get(const std::string& url) const {
        std::map<std::string, std::string> headers;
        for (const auto& kv : get_default_headers()) {
            headers[kv.first] = kv.second;
        }

        auto response = http_client_->get(
            url,
            headers,
            get_default_timeout_ms()
        );

        if (response.status_code == 0) {
            throw std::runtime_error(fmt::format("Network Error: Could not reach {} ({})", url, response.error_message));
        } else if (response.status_code != 200) {
            throw std::runtime_error(fmt::format("HTTP {} ({})", response.status_code, response.error_message));
        }
        return response.text;
    }

    // Perform HTTP POST request with JSON body
    virtual std::string http_post_json(const std::string& url, const std::string& json_body) const {
        std::map<std::string, std::string> headers;
        headers["Content-Type"] = "application/json";
        headers["User-Agent"] = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36";

        auto response = http_client_->post(
            url,
            json_body,
            headers,
            get_default_timeout_ms()
        );

        if (response.status_code == 0) {
            throw std::runtime_error(fmt::format("Network Error: Could not reach {} ({})", url, response.error_message));
        } else if (response.status_code != 200) {
            throw std::runtime_error(fmt::format("HTTP {} ({})", response.status_code, response.error_message));
        }
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
