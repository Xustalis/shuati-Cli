#pragma once

#include <string>
#include <map>
#include <memory>

namespace shuati {

struct HttpResponse {
    int status_code;
    std::string text;
    std::map<std::string, std::string> headers;
    std::string error_message;
};

class IHttpClient {
public:
    virtual ~IHttpClient() = default;
    virtual HttpResponse get(const std::string& url, const std::map<std::string, std::string>& headers = {}, int timeout_ms = 10000) = 0;
    virtual HttpResponse post(const std::string& url, const std::string& body, const std::map<std::string, std::string>& headers = {}, int timeout_ms = 10000) = 0;
};

// Default implementation using CPR
class CprHttpClient : public IHttpClient {
public:
    HttpResponse get(const std::string& url, const std::map<std::string, std::string>& headers, int timeout_ms) override;
    HttpResponse post(const std::string& url, const std::string& body, const std::map<std::string, std::string>& headers, int timeout_ms) override;
};

} // namespace shuati
