#include "shuati/http_client.hpp"
#include <cpr/cpr.h>
#include <regex>
#include <iostream>

namespace shuati {

namespace {

// Blocklist: private / reserved / loopback IP ranges
// RFC 1918: 10.0.0.0/8, 172.16.0.0/12, 192.168.0.0/16
// RFC 3927: 169.254.0.0/16 (link-local)
// RFC 3330: 127.0.0.0/8 (loopback)
// Also blocks cloud metadata endpoints (169.254.169.254)
static bool is_private_ip(const std::string& host) {
    // Skip if no dots (not an IP literal)
    if (host.find('.') == std::string::npos) return false;

    // Quick dot-count check: skip hostnames with many dots
    if (std::count(host.begin(), host.end(), '.') > 3) {
        // Could be a malformed IP; treat as potentially private
        return true;
    }

    // Block obviously private / cloud-Metadata hostnames
    if (host == "localhost" ||
        host == "127.0.0.1" || host == "::1" ||
        host.rfind("169.254.") == 0 ||  // AWS/GCP metadata
        host == "0.0.0.0") {
        return true;
    }

    try {
        // Very lightweight CIDR check for the three private ranges.
        // Extract first octet(s) as integers.
        auto dot = host.find('.');
        int a = std::stoi(host.substr(0, dot));
        if (a == 10) return true;  // 10.0.0.0/8

        auto dot2 = host.find('.', dot + 1);
        int b = std::stoi(host.substr(dot + 1, dot2 - dot - 1));
        if (a == 172 && b >= 16 && b <= 31) return true;  // 172.16.0.0/12

        if (a == 192 && b == 168) return true;  // 192.168.0.0/16
    } catch (...) {
        // stoi failed — treat as potentially private
        return true;
    }
    return false;
}

// Returns true if the URL is safe to fetch (HTTPS, no private-IP host).
static bool validate_url(const std::string& url) {
    // Must start with https:// (force TLS)
    if (url.find("https://") != 0) return false;

    // Extract host: skip scheme, find first '/' after authority
    size_t authority_start = 8;  // strlen("https://")
    size_t host_end = url.find('/', authority_start);
    std::string host = (host_end == std::string::npos)
        ? url.substr(authority_start)
        : url.substr(authority_start, host_end - authority_start);

    // Strip userinfo (user:pass@host)
    auto at_pos = host.find('@');
    if (at_pos != std::string::npos) host = host.substr(at_pos + 1);

    // Strip port
    auto colon_pos = host.find(':');
    if (colon_pos != std::string::npos) host = host.substr(0, colon_pos);

    if (host.empty()) return false;
    return !is_private_ip(host);
}

}  // namespace

HttpResponse CprHttpClient::get(const std::string& url, const std::map<std::string, std::string>& headers, int timeout_ms) {
    if (!validate_url(url)) {
        return HttpResponse{0, {}, {}, "SSRF blocked: invalid or non-HTTPS URL"};
    }

    cpr::Header cpr_headers;
    for (const auto& [k, v] : headers) {
        cpr_headers.insert({k, v});
    }

    auto r = cpr::Get(
        cpr::Url{url},
        cpr_headers,
        cpr::Timeout{timeout_ms}
    );

    return HttpResponse{
        static_cast<int>(r.status_code),
        r.text,
        {},
        r.error.message
    };
}

HttpResponse CprHttpClient::post(const std::string& url, const std::string& body, const std::map<std::string, std::string>& headers, int timeout_ms) {
    if (!validate_url(url)) {
        return HttpResponse{0, {}, {}, "SSRF blocked: invalid or non-HTTPS URL"};
    }

    cpr::Header cpr_headers;
    for (const auto& [k, v] : headers) {
        cpr_headers.insert({k, v});
    }

    auto r = cpr::Post(
        cpr::Url{url},
        cpr_headers,
        cpr::Body{body},
        cpr::Timeout{timeout_ms}
    );

    return HttpResponse{
        static_cast<int>(r.status_code),
        r.text,
        {},
        r.error.message
    };
}

} // namespace shuati
