#include "shuati/adapters/companion_server.hpp"
#include <fmt/core.h>
#include <fmt/color.h>
#include <nlohmann/json.hpp>
#include <chrono>

namespace shuati {

CompanionServer::CompanionServer(ProblemManager& pm, Database& db) 
    : pm_(pm), db_(db) {}

CompanionServer::~CompanionServer() {
    stop();
}

void CompanionServer::start() {
    if (running_) return;
    
    server_ = std::make_unique<httplib::Server>();
    
    server_->Post("/", [this](const httplib::Request& req, httplib::Response& res) {
        handle_post(req, res);
    });

    running_ = true;
    worker_ = std::thread([this]() {
        try {
            fmt::print(fg(fmt::color::cyan), "  [Companion] Listening on port 10043...\n");
            // listen is blocking
            server_->listen("127.0.0.1", 10043);
        } catch (...) {
            fmt::print(fg(fmt::color::red), "  [Companion] Failed to start server\n");
        }
        running_ = false;
    });
}

void CompanionServer::stop() {
    if (server_) {
        server_->stop(); // Breaks the blocking listen loop
    }
    
    if (worker_.joinable()) {
        worker_.join();
    }
    
    server_.reset();
    running_ = false;
}

void CompanionServer::handle_post(const httplib::Request& req, httplib::Response& res) {
    try {
        auto json = nlohmann::json::parse(req.body);
        
        std::string name = json.value("name", "Untitled");
        std::string group = json.value("group", "Unknown");
        std::string url = json.value("url", "");
        
        fmt::print(fg(fmt::color::green), "\n[Companion] Received problem: {}\n", name);
        
        // Generate ID logic (simplified)
        std::string id = "cp_" + std::to_string(std::hash<std::string>{}(url) % 100000);
        
        Problem p;
        p.id = id;
        p.title = name;
        p.source = group;
        p.url = url;
        p.created_at = std::time(nullptr);
        
        // Check if exists
        try {
            auto existing = db_.get_problem(id);
            if (!existing.id.empty()) {
                fmt::print(fg(fmt::color::yellow), "  Problem already exists.\n");
                res.status = 200;
                return;
            }
        } catch (...) {}

        // Save problem
        db_.add_problem(p);
        
        // Extract tests
        if (json.contains("tests")) {
            for (const auto& test : json["tests"]) {
                std::string input = test.value("input", "");
                std::string output = test.value("output", "");
                db_.add_test_case(id, input, output, true);
            }
            fmt::print(fg(fmt::color::green), "  Saved with {} test cases.\n", json["tests"].size());
        }

        // Ideally we would notify the main thread or ProblemManager here
        res.status = 200;
    } catch (const std::exception& e) {
        fmt::print(fg(fmt::color::red), "  [Companion] Error parsing: {}\n", e.what());
        res.status = 500;
    }
}

} // namespace shuati
