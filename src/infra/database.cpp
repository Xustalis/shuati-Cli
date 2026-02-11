#include "shuati/database.hpp"
#include <fmt/core.h>

namespace shuati {

Database::Database(const std::string& db_path) {
    std::filesystem::path p(db_path);
    if (p.has_parent_path())
        std::filesystem::create_directories(p.parent_path());

    db_ = std::make_unique<SQLite::Database>(db_path, SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE);
    init_schema();
}

void Database::init_schema() {
    db_->exec(
        "CREATE TABLE IF NOT EXISTS problems ("
        "  id TEXT PRIMARY KEY,"
        "  source TEXT,"
        "  title TEXT,"
        "  url TEXT,"
        "  content_path TEXT,"
        "  tags TEXT DEFAULT '',"
        "  difficulty TEXT DEFAULT 'medium',"
        "  created_at INTEGER"
        ")");

    db_->exec(
        "CREATE TABLE IF NOT EXISTS mistakes ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  problem_id TEXT,"
        "  type TEXT,"
        "  description TEXT,"
        "  timestamp INTEGER,"
        "  FOREIGN KEY(problem_id) REFERENCES problems(id)"
        ")");

    db_->exec(
        "CREATE TABLE IF NOT EXISTS reviews ("
        "  problem_id TEXT PRIMARY KEY,"
        "  next_review INTEGER,"
        "  interval INTEGER DEFAULT 1,"
        "  ease_factor REAL DEFAULT 2.5,"
        "  repetitions INTEGER DEFAULT 0,"
        "  FOREIGN KEY(problem_id) REFERENCES problems(id)"
        ")");
    
    db_->exec(
        "CREATE TABLE IF NOT EXISTS test_cases ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  problem_id TEXT NOT NULL,"
        "  input TEXT,"
        "  output TEXT,"
        "  is_sample INTEGER DEFAULT 1,"
        "  FOREIGN KEY(problem_id) REFERENCES problems(id)"
        ")");
}

// ---- Problem ----

void Database::add_problem(const Problem& p) {
    SQLite::Statement q(*db_,
        "INSERT OR REPLACE INTO problems (id,source,title,url,content_path,tags,difficulty,created_at) "
        "VALUES (?,?,?,?,?,?,?,?)");
    q.bind(1, p.id);
    q.bind(2, p.source);
    q.bind(3, p.title);
    q.bind(4, p.url);
    q.bind(5, p.content_path);
    q.bind(6, p.tags);
    q.bind(7, p.difficulty);
    q.bind(8, p.created_at ? p.created_at : (long long)std::time(nullptr));
    q.exec();
}

bool Database::problem_exists(const std::string& url) {
    SQLite::Statement q(*db_, "SELECT count(*) FROM problems WHERE url=?");
    q.bind(1, url);
    return q.executeStep() && q.getColumn(0).getInt() > 0;
}

std::vector<Problem> Database::get_all_problems() {
    std::vector<Problem> out;
    // Fetch rowid as display_id
    SQLite::Statement q(*db_, "SELECT rowid, id,source,title,url,content_path,tags,difficulty,created_at FROM problems ORDER BY created_at DESC");
    while (q.executeStep()) {
        Problem p;
        p.display_id = q.getColumn(0).getInt();
        p.id = q.getColumn(1).getString();
        p.source = q.getColumn(2).getString();
        p.title = q.getColumn(3).getString();
        p.url = q.getColumn(4).getString();
        p.content_path = q.getColumn(5).getString();
        p.tags = q.getColumn(6).getString();
        p.difficulty = q.getColumn(7).getString();
        p.created_at = q.getColumn(8).getInt64();
        out.push_back(p);
    }
    return out;
}

Problem Database::get_problem(const std::string& id) {
    SQLite::Statement q(*db_, "SELECT rowid, id,source,title,url,content_path,tags,difficulty,created_at FROM problems WHERE id=?");
    q.bind(1, id);
    if (q.executeStep()) {
        Problem p;
        p.display_id = q.getColumn(0).getInt();
        p.id = q.getColumn(1).getString();
        p.source = q.getColumn(2).getString();
        p.title = q.getColumn(3).getString();
        p.url = q.getColumn(4).getString();
        p.content_path = q.getColumn(5).getString();
        p.tags = q.getColumn(6).getString();
        p.difficulty = q.getColumn(7).getString();
        p.created_at = q.getColumn(8).getInt64();
        return p;
    }
    return {};
}

Problem Database::get_problem_by_display_id(int tid) {
    SQLite::Statement q(*db_, "SELECT rowid, id,source,title,url,content_path,tags,difficulty,created_at FROM problems WHERE rowid=?");
    q.bind(1, tid);
    if (q.executeStep()) {
        Problem p;
        p.display_id = q.getColumn(0).getInt();
        p.id = q.getColumn(1).getString();
        p.source = q.getColumn(2).getString();
        p.title = q.getColumn(3).getString();
        p.url = q.getColumn(4).getString();
        p.content_path = q.getColumn(5).getString();
        p.tags = q.getColumn(6).getString();
        p.difficulty = q.getColumn(7).getString();
        p.created_at = q.getColumn(8).getInt64();
        return p;
    }
    return {};
}

void Database::delete_problem(int tid) {
    // Get UUID first to delete related records
    std::string uuid;
    {
        SQLite::Statement q(*db_, "SELECT id FROM problems WHERE rowid=?");
        q.bind(1, tid);
        if (q.executeStep()) uuid = q.getColumn(0).getString();
    }
    
    if (uuid.empty()) return;

    db_->exec("BEGIN TRANSACTION");
    try {
        SQLite::Statement q1(*db_, "DELETE FROM mistakes WHERE problem_id=?"); q1.bind(1, uuid); q1.exec();
        SQLite::Statement q2(*db_, "DELETE FROM reviews WHERE problem_id=?"); q2.bind(1, uuid); q2.exec();
        SQLite::Statement q3(*db_, "DELETE FROM problems WHERE id=?"); q3.bind(1, uuid); q3.exec();
        db_->exec("COMMIT");
    } catch (...) {
        db_->exec("ROLLBACK");
        throw;
    }
}

// ---- Mistake ----

void Database::log_mistake(const std::string& pid, const std::string& type, const std::string& desc) {
    SQLite::Statement q(*db_, "INSERT INTO mistakes (problem_id,type,description,timestamp) VALUES (?,?,?,?)");
    q.bind(1, pid);
    q.bind(2, type);
    q.bind(3, desc);
    q.bind(4, (long long)std::time(nullptr));
    q.exec();
}

std::vector<Mistake> Database::get_mistakes_for(const std::string& pid) {
    std::vector<Mistake> out;
    SQLite::Statement q(*db_, "SELECT id,problem_id,type,description,timestamp FROM mistakes WHERE problem_id=? ORDER BY timestamp DESC");
    q.bind(1, pid);
    while (q.executeStep()) {
        out.push_back({
            q.getColumn(0).getInt(), q.getColumn(1).getString(),
            q.getColumn(2).getString(), q.getColumn(3).getString(),
            q.getColumn(4).getInt64()
        });
    }
    return out;
}

std::vector<std::pair<std::string, int>> Database::get_mistake_stats() {
    std::vector<std::pair<std::string, int>> out;
    SQLite::Statement q(*db_, "SELECT type, count(*) as cnt FROM mistakes GROUP BY type ORDER BY cnt DESC");
    while (q.executeStep()) {
        out.emplace_back(q.getColumn(0).getString(), q.getColumn(1).getInt());
    }
    return out;
}

// ---- Review / SM-2 ----

void Database::upsert_review(const ReviewItem& r) {
    SQLite::Statement q(*db_,
        "INSERT OR REPLACE INTO reviews (problem_id,next_review,interval,ease_factor,repetitions) "
        "VALUES (?,?,?,?,?)");
    q.bind(1, r.problem_id);
    q.bind(2, r.next_review);
    q.bind(3, r.interval);
    q.bind(4, r.ease_factor);
    q.bind(5, r.repetitions);
    q.exec();
}

std::vector<ReviewItem> Database::get_due_reviews(long long now) {
    std::vector<ReviewItem> out;
    SQLite::Statement q(*db_,
        "SELECT r.problem_id, p.title, r.next_review, r.interval, r.ease_factor, r.repetitions "
        "FROM reviews r JOIN problems p ON r.problem_id=p.id "
        "WHERE r.next_review <= ? ORDER BY r.next_review ASC");
    q.bind(1, now);
    while (q.executeStep()) {
        out.push_back({
            q.getColumn(0).getString(), q.getColumn(1).getString(),
            q.getColumn(2).getInt64(), q.getColumn(3).getInt(),
            q.getColumn(4).getDouble(), q.getColumn(5).getInt()
        });
    }
    return out;
}

ReviewItem Database::get_review(const std::string& pid) {
    SQLite::Statement q(*db_,
        "SELECT problem_id, '', next_review, interval, ease_factor, repetitions "
        "FROM reviews WHERE problem_id=?");
    q.bind(1, pid);
    if (q.executeStep()) {
        return {
            q.getColumn(0).getString(), "",
            q.getColumn(2).getInt64(), q.getColumn(3).getInt(),
            q.getColumn(4).getDouble(), q.getColumn(5).getInt()
        };
    }
    return {pid, "", 0, 1, 2.5, 0};
}

// ---- Test Cases ----

void Database::add_test_case(const std::string& problem_id, const std::string& input, const std::string& output, bool is_sample) {
    SQLite::Statement q(*db_, "INSERT INTO test_cases (problem_id,input,output,is_sample) VALUES (?,?,?,?)");
    q.bind(1, problem_id);
    q.bind(2, input);
    q.bind(3, output);
    q.bind(4, is_sample ? 1 : 0);
    q.exec();
}

std::vector<std::pair<std::string, std::string>> Database::get_test_cases(const std::string& problem_id) {
    std::vector<std::pair<std::string, std::string>> out;
    SQLite::Statement q(*db_, "SELECT input, output FROM test_cases WHERE problem_id=? ORDER BY is_sample DESC, id ASC");
    q.bind(1, problem_id);
    while (q.executeStep()) {
        out.emplace_back(q.getColumn(0).getString(), q.getColumn(1).getString());
    }
    return out;
}

} // namespace shuati
