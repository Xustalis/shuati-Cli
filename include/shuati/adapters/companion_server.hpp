#pragma once
#include "shuati/problem_manager.hpp"
#include "shuati/database.hpp"
#include <httplib.h>
#include <memory>
#include <thread>
#include <atomic>
#include <mutex>

namespace shuati {

class CompanionServer {
public:
    CompanionServer(ProblemManager& pm, Database& db);
    ~CompanionServer();

    void start();
    void stop();

private:
    std::unique_ptr<httplib::Server> server_;
    ProblemManager& pm_;
    Database& db_;
    std::thread worker_;
    std::atomic<bool> running_{false};
    std::mutex db_mutex_;  // Protects db_ writes from HTTP handler thread

    void handle_post(const httplib::Request& req, httplib::Response& res);
};

} // namespace shuati
