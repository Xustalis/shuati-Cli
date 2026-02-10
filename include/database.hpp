#pragma once

#include <SQLiteCpp/SQLiteCpp.h>
#include <string>
#include <memory>
#include <filesystem>
#include <vector>
#include <ctime>

namespace shuati {

struct Problem {
    int display_id = 0;   // 1-based numeric ID (TID)
    std::string id;       // UUID or original string ID
    std::string source;   // "web", "local"
    std::string title;
    std::string url;
    std::string content_path;
    std::string tags;      // comma-separated
    std::string difficulty; // "easy", "medium", "hard"
    long long created_at = 0;
};

struct Mistake {
    int id = 0;
    std::string problem_id;
    std::string type;        // "Logic", "Boundary", "Greedy", "DP", "TLE"
    std::string description;
    long long timestamp = 0;
};

struct ReviewItem {
    std::string problem_id;
    std::string title;
    long long next_review = 0;
    int interval = 1;       // days
    double ease_factor = 2.5;
    int repetitions = 0;
};

class Database {
public:
    explicit Database(const std::string& db_path);
    ~Database() = default;

    void init_schema();

    // Problem CRUD
    void add_problem(const Problem& p);
    bool problem_exists(const std::string& url);
    std::vector<Problem> get_all_problems();
    Problem get_problem(const std::string& id);
    Problem get_problem_by_display_id(int tid);
    void delete_problem(int tid);

    // Mistake CRUD
    void log_mistake(const std::string& problem_id, const std::string& type, const std::string& desc);
    std::vector<Mistake> get_mistakes_for(const std::string& problem_id);
    std::vector<std::pair<std::string, int>> get_mistake_stats();

    // Review / Spaced Repetition
    void upsert_review(const ReviewItem& r);
    std::vector<ReviewItem> get_due_reviews(long long now);
    ReviewItem get_review(const std::string& problem_id);

private:
    std::unique_ptr<SQLite::Database> db_;
};

} // namespace shuati
