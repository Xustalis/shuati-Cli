#pragma once

#include "shuati/problem_manager.hpp"
#include "shuati/database.hpp"
#include <httplib.h>
#include <memory>
#include <atomic>

namespace shuati {

class CompanionServer {
public:
    CompanionServer(ProblemManager& pm, Database& db);
    void start();
    void stop();

private:
    std::unique_ptr<httplib::Server> server_;
    ProblemManager& pm_;
    Database& db_;
    
    void handle_post(const httplib::Request& req, httplib::Response& res);
};

} // namespace shuati
