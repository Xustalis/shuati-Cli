#include "shuati/database.hpp"
#include "shuati/utils/encoding.hpp"
#include <fmt/core.h>

#ifdef _WIN32
#include <windows.h>
#endif

namespace shuati {

using utils::ensure_utf8_lossy;

namespace {

static std::string safe_column_text(SQLite::Column col) {
    try {
        return ensure_utf8_lossy(col.getString());
    } catch (...) {
        try {
            const void* b = col.getBlob();
            int n = col.getBytes();
            if (!b || n <= 0) return {};
            return ensure_utf8_lossy(std::string((const char*)b, (size_t)n));
        } catch (...) {
            return {};
        }
    }
}
} // namespace

Database::Database(const std::string& db_path) {
    std::filesystem::path p(db_path);
    if (p.has_parent_path())
        std::filesystem::create_directories(p.parent_path());

    db_ = std::make_unique<SQLite::Database>(db_path, SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE);
    init_schema();
}

void Database::init_schema() {
    // Migration: Drop any existing views that may reference old columns
    try { db_->exec("DROP VIEW IF EXISTS problem_stats"); } catch (...) {}
    try { db_->exec("DROP VIEW IF EXISTS problem_summary"); } catch (...) {}
    
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
    
    // Migration: Check for old schema artifacts and clean up
    // If there's a view or trigger referencing pass_count, drop it
    try {
        // Check for any views referencing pass_count
        SQLite::Statement check_view(*db_, "SELECT name FROM sqlite_master WHERE type='view' AND sql LIKE '%pass_count%'");
        while (check_view.executeStep()) {
            std::string view_name = check_view.getColumn(0).getText();
            db_->exec("DROP VIEW IF EXISTS " + view_name);
        }
    } catch (...) {}
    
    try {
        // Check for any triggers referencing pass_count
        SQLite::Statement check_trigger(*db_, "SELECT name FROM sqlite_master WHERE type='trigger' AND sql LIKE '%pass_count%'");
        while (check_trigger.executeStep()) {
            std::string trigger_name = check_trigger.getColumn(0).getText();
            db_->exec("DROP TRIGGER IF EXISTS " + trigger_name);
        }
    } catch (...) {}

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
        
    // V3 Memory System
    db_->exec(
        "CREATE TABLE IF NOT EXISTS memory_mistakes ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  tags TEXT,"
        "  pattern TEXT,"
        "  frequency INTEGER DEFAULT 1,"
        "  last_seen INTEGER,"
        "  example_id TEXT,"
        "  UNIQUE(tags, pattern)" 
        ")");

    db_->exec(
        "CREATE TABLE IF NOT EXISTS memory_mastery ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  skill TEXT UNIQUE,"
        "  confidence REAL,"
        "  last_verified INTEGER"
        ")");
        
    db_->exec(
        "CREATE TABLE IF NOT EXISTS user_profile ("
        "  id INTEGER PRIMARY KEY CHECK (id = 1),"
        "  elo_rating INTEGER DEFAULT 1200,"
        "  preferences TEXT DEFAULT '{}'"
        ")");
    
    db_->exec("INSERT OR IGNORE INTO user_profile (id, elo_rating, preferences) VALUES (1, 1200, '{}')");
}

// ---- Problem ----

void Database::add_problem(const Problem& p) {
    SQLite::Statement q(*db_,
        "INSERT OR REPLACE INTO problems (id,source,title,url,content_path,tags,difficulty,created_at) "
        "VALUES (?,?,?,?,?,?,?,?)");
    q.bind(1, ensure_utf8_lossy(p.id));
    q.bind(2, ensure_utf8_lossy(p.source));
    q.bind(3, ensure_utf8_lossy(p.title));
    q.bind(4, ensure_utf8_lossy(p.url));
    q.bind(5, ensure_utf8_lossy(p.content_path));
    q.bind(6, ensure_utf8_lossy(p.tags));
    q.bind(7, ensure_utf8_lossy(p.difficulty));
    q.bind(8, static_cast<int64_t>(p.created_at ? p.created_at : std::time(nullptr)));
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
        p.id = safe_column_text(q.getColumn(1));
        p.source = safe_column_text(q.getColumn(2));
        p.title = safe_column_text(q.getColumn(3));
        p.url = safe_column_text(q.getColumn(4));
        p.content_path = safe_column_text(q.getColumn(5));
        p.tags = safe_column_text(q.getColumn(6));
        p.difficulty = safe_column_text(q.getColumn(7));
        p.created_at = q.getColumn(8).getInt64();
        out.push_back(p);
    }
    return out;
}

Problem Database::get_problem(const std::string& id) {
    SQLite::Statement q(*db_, "SELECT rowid, id,source,title,url,content_path,tags,difficulty,created_at FROM problems WHERE id=?");
    q.bind(1, ensure_utf8_lossy(id));
    if (q.executeStep()) {
        Problem p;
        p.display_id = q.getColumn(0).getInt();
        p.id = safe_column_text(q.getColumn(1));
        p.source = safe_column_text(q.getColumn(2));
        p.title = safe_column_text(q.getColumn(3));
        p.url = safe_column_text(q.getColumn(4));
        p.content_path = safe_column_text(q.getColumn(5));
        p.tags = safe_column_text(q.getColumn(6));
        p.difficulty = safe_column_text(q.getColumn(7));
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
        p.id = safe_column_text(q.getColumn(1));
        p.source = safe_column_text(q.getColumn(2));
        p.title = safe_column_text(q.getColumn(3));
        p.url = safe_column_text(q.getColumn(4));
        p.content_path = safe_column_text(q.getColumn(5));
        p.tags = safe_column_text(q.getColumn(6));
        p.difficulty = safe_column_text(q.getColumn(7));
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
        if (q.executeStep()) uuid = safe_column_text(q.getColumn(0));
    }
    
    if (uuid.empty()) return;

    db_->exec("BEGIN TRANSACTION");
    try {
        SQLite::Statement q1(*db_, "DELETE FROM mistakes WHERE problem_id=?"); q1.bind(1, uuid); q1.exec();
        SQLite::Statement q2(*db_, "DELETE FROM reviews WHERE problem_id=?"); q2.bind(1, uuid); q2.exec();
        SQLite::Statement q_tc(*db_, "DELETE FROM test_cases WHERE problem_id=?"); q_tc.bind(1, uuid); q_tc.exec();
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
    q.bind(1, ensure_utf8_lossy(pid));
    q.bind(2, ensure_utf8_lossy(type));
    q.bind(3, ensure_utf8_lossy(desc));
    q.bind(4, static_cast<int64_t>(std::time(nullptr)));
    q.exec();
}

std::vector<Mistake> Database::get_mistakes_for(const std::string& pid) {
    std::vector<Mistake> out;
    SQLite::Statement q(*db_, "SELECT id,problem_id,type,description,timestamp FROM mistakes WHERE problem_id=? ORDER BY timestamp DESC");
    q.bind(1, ensure_utf8_lossy(pid));
    while (q.executeStep()) {
        out.push_back({
            q.getColumn(0).getInt(), safe_column_text(q.getColumn(1)),
            safe_column_text(q.getColumn(2)), safe_column_text(q.getColumn(3)),
            q.getColumn(4).getInt64()
        });
    }
    return out;
}

std::vector<std::pair<std::string, int>> Database::get_mistake_stats() {
    std::vector<std::pair<std::string, int>> out;
    SQLite::Statement q(*db_, "SELECT type, count(*) as cnt FROM mistakes GROUP BY type ORDER BY cnt DESC");
    while (q.executeStep()) {
        out.emplace_back(safe_column_text(q.getColumn(0)), q.getColumn(1).getInt());
    }
    return out;
}

// ---- Review / SM-2 ----

void Database::upsert_review(const ReviewItem& r) {
    SQLite::Statement q(*db_,
        "INSERT OR REPLACE INTO reviews (problem_id,next_review,interval,ease_factor,repetitions) "
        "VALUES (?,?,?,?,?)");
    q.bind(1, ensure_utf8_lossy(r.problem_id));
    q.bind(2, static_cast<int64_t>(r.next_review));
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
    q.bind(1, static_cast<int64_t>(now));
    while (q.executeStep()) {
        out.push_back({
            safe_column_text(q.getColumn(0)), safe_column_text(q.getColumn(1)),
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
    q.bind(1, ensure_utf8_lossy(pid));
    if (q.executeStep()) {
        return {
            safe_column_text(q.getColumn(0)), "",
            q.getColumn(2).getInt64(), q.getColumn(3).getInt(),
            q.getColumn(4).getDouble(), q.getColumn(5).getInt()
        };
    }
    return {pid, "", 0, 1, 2.5, 0};
}

// ---- Test Cases ----

void Database::add_test_case(const std::string& problem_id, const std::string& input, const std::string& output, bool is_sample) {
    SQLite::Statement q(*db_, "INSERT INTO test_cases (problem_id,input,output,is_sample) VALUES (?,?,?,?)");
    q.bind(1, ensure_utf8_lossy(problem_id));
    q.bind(2, ensure_utf8_lossy(input));
    q.bind(3, ensure_utf8_lossy(output));
    q.bind(4, is_sample ? 1 : 0);
    q.exec();
}

std::vector<std::pair<std::string, std::string>> Database::get_test_cases(const std::string& problem_id) {
    std::vector<std::pair<std::string, std::string>> out;
    SQLite::Statement q(*db_, "SELECT input, output FROM test_cases WHERE problem_id=? ORDER BY is_sample DESC, id ASC");
    q.bind(1, ensure_utf8_lossy(problem_id));
    while (q.executeStep()) {
        out.emplace_back(safe_column_text(q.getColumn(0)), safe_column_text(q.getColumn(1)));
    }
    return out;
}

// ---- Memory System (V3) ----

// Mistakes
void Database::upsert_memory_mistake(const std::string& tags, const std::string& pattern, const std::string& example_id) {
    // Try to update existing first (increment frequency)
    // Using tags + pattern as unique key logic
    SQLite::Statement q_check(*db_, "SELECT id, frequency FROM memory_mistakes WHERE pattern=?");
    q_check.bind(1, ensure_utf8_lossy(pattern));
    
    bool found = false;
    int id = 0;
    int freq = 0;
    if (q_check.executeStep()) {
        id = q_check.getColumn(0).getInt();
        freq = q_check.getColumn(1).getInt();
        found = true;
    }
    
    if (found) {
        SQLite::Statement q(*db_, "UPDATE memory_mistakes SET frequency=?, last_seen=?, example_id=? WHERE id=?");
        q.bind(1, freq + 1);
        q.bind(2, static_cast<int64_t>(std::time(nullptr)));
        q.bind(3, ensure_utf8_lossy(example_id));
        q.bind(4, id);
        q.exec();
    } else {
        SQLite::Statement q(*db_, "INSERT INTO memory_mistakes (tags, pattern, frequency, last_seen, example_id) VALUES (?, ?, 1, ?, ?)");
        q.bind(1, ensure_utf8_lossy(tags));
        q.bind(2, ensure_utf8_lossy(pattern));
        q.bind(3, static_cast<int64_t>(std::time(nullptr)));
        q.bind(4, ensure_utf8_lossy(example_id));
        q.exec();
    }
}

std::vector<MemoryMistake> Database::get_all_memory_mistakes() {
    std::vector<MemoryMistake> out;
    SQLite::Statement q(*db_, "SELECT id, tags, pattern, frequency, last_seen, example_id FROM memory_mistakes ORDER BY frequency DESC, last_seen DESC");
    while (q.executeStep()) {
        MemoryMistake m;
        m.id = q.getColumn(0).getInt();
        m.tags = safe_column_text(q.getColumn(1));
        m.pattern = safe_column_text(q.getColumn(2));
        m.frequency = q.getColumn(3).getInt();
        m.last_seen = q.getColumn(4).getInt64();
        m.example_id = safe_column_text(q.getColumn(5));
        out.push_back(m);
    }
    return out;
}

// Mastery
void Database::upsert_mastery(const std::string& skill, double confidence) {
    SQLite::Statement q(*db_, 
        "INSERT INTO memory_mastery (skill, confidence, last_verified) VALUES (?, ?, ?) "
        "ON CONFLICT(skill) DO UPDATE SET confidence=?, last_verified=?"
    );
    // Bind 1,2,3 for INSERT
    q.bind(1, ensure_utf8_lossy(skill));
    q.bind(2, confidence);
    q.bind(3, static_cast<int64_t>(std::time(nullptr)));
    
    // Bind 4,5 for UPDATE
    q.bind(4, confidence);
    q.bind(5, static_cast<int64_t>(std::time(nullptr)));
    q.exec();
}

std::optional<Mastery> Database::get_mastery(const std::string& skill) {
    SQLite::Statement q(*db_, "SELECT id, skill, confidence, last_verified FROM memory_mastery WHERE skill=?");
    q.bind(1, ensure_utf8_lossy(skill));
    if (q.executeStep()) {
        Mastery m;
        m.id = q.getColumn(0).getInt();
        m.skill = safe_column_text(q.getColumn(1));
        m.confidence = q.getColumn(2).getDouble();
        m.last_verified = q.getColumn(3).getInt64();
        return m;
    }
    return std::nullopt;
}

std::vector<Mastery> Database::get_all_mastery() {
    std::vector<Mastery> out;
    SQLite::Statement q(*db_, "SELECT id, skill, confidence, last_verified FROM memory_mastery ORDER BY confidence DESC");
    while (q.executeStep()) {
        Mastery m;
        m.id = q.getColumn(0).getInt();
        m.skill = safe_column_text(q.getColumn(1));
        m.confidence = q.getColumn(2).getDouble();
        m.last_verified = q.getColumn(3).getInt64();
        out.push_back(m);
    }
    return out;
}

// User Profile
void Database::update_user_profile(int elo, const std::string& preferences) {
    if (elo > 0) {
        SQLite::Statement q(*db_, "UPDATE user_profile SET elo_rating=? WHERE id=1");
        q.bind(1, elo);
        q.exec();
    }
    if (!preferences.empty()) {
        SQLite::Statement q(*db_, "UPDATE user_profile SET preferences=? WHERE id=1");
        q.bind(1, ensure_utf8_lossy(preferences));
        q.exec();
    }
}

UserProfile Database::get_user_profile() {
    UserProfile p;
    SQLite::Statement q(*db_, "SELECT elo_rating, preferences FROM user_profile WHERE id=1");
    if (q.executeStep()) {
        p.elo_rating = q.getColumn(0).getInt();
        p.preferences = safe_column_text(q.getColumn(1));
    }
    return p;
}

} // namespace shuati
