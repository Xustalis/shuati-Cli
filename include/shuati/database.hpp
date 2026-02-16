#pragma once

#include <SQLiteCpp/SQLiteCpp.h>
#include <string>
#include <memory>
#include <filesystem>
#include <vector>
#include <ctime>
#include <optional>
#include "shuati/types.hpp"

namespace shuati {

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

    // Problem Status
    void update_problem_status(const std::string& id, const std::string& verdict, int pass_count, int total_count);

    // Review / Spaced Repetition
    void upsert_review(const ReviewItem& r);
    std::vector<ReviewItem> get_due_reviews(long long now);
    ReviewItem get_review(const std::string& problem_id);

    // Test Cases
    void add_test_case(const std::string& problem_id, const std::string& input, const std::string& output, bool is_sample = true);
    std::vector<std::pair<std::string, std::string>> get_test_cases(const std::string& problem_id);

    // Memory System (V3)
    // Mistakes (Abstract Patterns)
    void upsert_memory_mistake(const std::string& tags, const std::string& pattern, const std::string& example_id);
    std::vector<MemoryMistake> get_all_memory_mistakes();
    
    // Mastery
    void upsert_mastery(const std::string& skill, double confidence);
    std::optional<Mastery> get_mastery(const std::string& skill);
    std::vector<Mastery> get_all_mastery();

    // User Profile
    void update_user_profile(int elo, const std::string& preferences);
    UserProfile get_user_profile();

private:
    std::unique_ptr<SQLite::Database> db_;
};

} // namespace shuati
