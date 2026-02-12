#include "shuati/database.hpp"
#include <fmt/core.h>

#ifdef _WIN32
#include <windows.h>
#endif

namespace shuati {

namespace {
#ifdef _WIN32
static bool is_valid_utf8(const std::string& s) {
    size_t i = 0;
    while (i < s.size()) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        if (c <= 0x7F) {
            i++;
            continue;
        }

        size_t need = 0;
        if ((c & 0xE0) == 0xC0) need = 2;
        else if ((c & 0xF0) == 0xE0) need = 3;
        else if ((c & 0xF8) == 0xF0) need = 4;
        else return false;

        if (i + need > s.size()) return false;
        for (size_t j = 1; j < need; j++) {
            unsigned char cc = static_cast<unsigned char>(s[i + j]);
            if ((cc & 0xC0) != 0x80) return false;
        }
        i += need;
    }
    return true;
}

static std::string acp_to_utf8(const std::string& s) {
    if (s.empty()) return {};
    int wlen = MultiByteToWideChar(CP_ACP, MB_ERR_INVALID_CHARS, s.data(), (int)s.size(), nullptr, 0);
    if (wlen <= 0) return {};
    std::wstring w((size_t)wlen, L'\0');
    MultiByteToWideChar(CP_ACP, 0, s.data(), (int)s.size(), w.data(), wlen);
    int ulen = WideCharToMultiByte(CP_UTF8, 0, w.data(), wlen, nullptr, 0, nullptr, nullptr);
    if (ulen <= 0) return {};
    std::string out((size_t)ulen, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.data(), wlen, out.data(), ulen, nullptr, nullptr);
    return out;
}

static std::string ensure_utf8_lossy(const std::string& s) {
    if (is_valid_utf8(s)) return s;
    auto converted = acp_to_utf8(s);
    if (is_valid_utf8(converted)) return converted;
    std::string out;
    out.reserve(s.size());
    for (unsigned char c : s) out.push_back((c >= 0x20 && c <= 0x7E) ? (char)c : '?');
    return out;
}
#else
static std::string ensure_utf8_lossy(const std::string& s) { return s; }
#endif

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

} // namespace shuati
