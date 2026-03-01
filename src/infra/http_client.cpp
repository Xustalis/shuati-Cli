#include "shuati/http_client.hpp"
#include <cpr/cpr.h>

namespace shuati {

HttpResponse CprHttpClient::get(const std::string& url, const std::map<std::string, std::string>& headers, int timeout_ms) {
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
        {} // headers map conversion if needed
    };
}

HttpResponse CprHttpClient::post(const std::string& url, const std::string& body, const std::map<std::string, std::string>& headers, int timeout_ms) {
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
        {}
    };
}

} // namespace shuati
