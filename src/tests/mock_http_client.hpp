#pragma once
#include "shuati/http_client.hpp"
#include <map>
#include <functional>

namespace shuati {

class MockHttpClient : public IHttpClient {
public:
    using Handler = std::function<HttpResponse(const std::string&)>;
    
    void set_handler(Handler h) { handler_ = h; }
    
    HttpResponse get(const std::string& url, const std::map<std::string, std::string>& headers, int timeout_ms) override {
        if (handler_) return handler_(url);
        return {404, "", {}};
    }
    
    HttpResponse post(const std::string& url, const std::string& body, const std::map<std::string, std::string>& headers, int timeout_ms) override {
        if (handler_) return handler_(url);
        return {404, "", {}};
    }

private:
    Handler handler_;
};

}
