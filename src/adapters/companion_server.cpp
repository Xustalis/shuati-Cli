#include "shuati/adapters/companion_server.hpp"
#include <fmt/core.h>
#include <fmt/color.h>
#include <nlohmann/json.hpp>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <random>

namespace shuati {

namespace {

// Path to the API key file. Created with a random UUID on first start.
std::filesystem::path key_file_path() {
    auto home = std::filesystem::path(std::getenv("HOME"sv));
    return home / ".shuati" / "companion.key";
}

// Generate a random 32-char hex token.
std::string generate_key() {
    std::random_device rd;
    std::uniform_int_distribution<uint64_t> dist(0, UINT64_MAX);
    return fmt::format("{:016x}{:016x}", dist(rd), dist(rd));
}

// Load existing key or create a new one and persist it.
std::string load_or_create_key() {
    auto path = key_file_path();
    auto dir = path.parent_path();
    if (!std::filesystem::exists(dir)) {
        std::filesystem::create_directories(dir);
    }
    if (std::filesystem::exists(path)) {
        std::ifstream f(path);
        std::string key;
        if (std::getline(f, key)) return key;
    }
    // First start — generate and persist
    std::string new_key = generate_key();
    std::ofstream f(path);
    f << new_key;
    // Chmod 0600 — owner read/write only (on POSIX)
    std::filesystem::permissions(path,
        std::filesystem::perms::owner_read | std::filesystem::perms::owner_write);
    fmt::print(fg(fmt::color::yellow),
        "  [Companion] API key generated. Save this for your browser extension:\n"
        "  {}\n"
        "  Key stored in ~/.shuati/companion.key\n",
        new_key);
    return new_key;
}

// Verify the Authorization: Bearer <key> header.
// Returns true if authorized, false otherwise.
bool check_auth(const httplib::Request& req) {
    static std::string api_key = load_or_create_key();
    auto auth_hdr = req.get_header_value("Authorization");
    if (auth_hdr.empty()) return false;
    // Expect "Bearer <key>"
    static const std::string prefix = "Bearer ";
    if (auth_hdr.rfind(prefix, 0) != 0) return false;
    return auth_hdr.substr(prefix.size()) == api_key;
}

}  // namespace

CompanionServer::CompanionServer(ProblemManager& pm, Database& db)
    : pm_(pm), db_(db) {}

CompanionServer::~CompanionServer() {
    stop();
}

void CompanionServer::start() {
    if (running_) return;

    server_ = std::make_unique<httplib::Server>();

    server_->Post("/", [this](const httplib::Request& req, httplib::Response& res) {
        // Require authentication
        if (!check_auth(req)) {
            res.status = 401;
            res.set_content("Unauthorized. Provide 'Authorization: Bearer <key>' header.", "text/plain");
            return;
        }
        handle_post(req, res);
    });

    running_ = true;
    worker_ = std::thread([this]() {
        try {
            fmt::print(fg(fmt::color::cyan), "  [Companion] Listening on http://127.0.0.1:10043\n");
            fmt::print(fg(fmt::color::cyan),
                "  [Companion] Browser extension must send 'Authorization: Bearer <key>' header.\n");
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
    // Guard: body size limit (1 MB) — httplib default is fine but be explicit
    if (req.body.size() > 1024 * 1024) {
        res.status = 413;
        res.set_content("Payload too large (max 1 MB)", "text/plain");
        return;
    }

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

        // All DB operations guarded by mutex to prevent "database is locked" from
        // concurrent writes between this HTTP handler thread and the main thread.
        std::lock_guard<std::mutex> lock(db_mutex_);

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
