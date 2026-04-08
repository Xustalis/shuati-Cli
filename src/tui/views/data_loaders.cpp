#include "all_views.hpp"
#include "common_widgets.hpp"
#include "shuati/config.hpp"
#include "shuati/tui_views.hpp"
#include "../../cmd/commands.hpp"
#include <ctime>
#include <unordered_set>

namespace shuati {
namespace tui {
namespace views {

void load_config_state(AppState& state) {
    auto root = Config::find_root();
    if (root.empty()) {
        state.config_state.status_msg = "\xe6\x9c\xaa\xe6\x89\xbe\xe5\x88\xb0 .shuati \xe9\xa1\xb9\xe7\x9b\xae\xef\xbc\x8c\xe8\xaf\xb7\xe5\x85\x88\xe8\xbf\x90\xe8\xa1\x8c /init";
        state.config_state.loaded = true;
        return;
    }
    auto cfg = Config::load(Config::config_path(root));
    state.config_state.language       = cfg.language;
    state.config_state.editor         = cfg.editor;
    state.config_state.api_key        = cfg.api_key;
    state.config_state.model          = cfg.model;
    state.config_state.ui_mode        = cfg.ui_mode;
    state.config_state.autostart_repl = cfg.autostart_repl;
    state.config_state.max_tokens     = cfg.max_tokens;
    state.config_state.ai_enabled     = cfg.ai_enabled;
    state.config_state.template_enabled = cfg.template_enabled;
    state.config_state.lanqiao_cookie = cfg.lanqiao_cookie;
    state.config_state.focused_field  = 0;
    state.config_state.status_msg.clear();
    state.config_state.loaded         = true;
}

void save_config_state(AppState& state) {
    auto root = Config::find_root();
    if (root.empty()) {
        state.config_state.status_msg = "\xe6\x9c\xaa\xe6\x89\xbe\xe5\x88\xb0 .shuati \xe9\xa1\xb9\xe7\x9b\xae";
        return;
    }
    auto cfg = Config::load(Config::config_path(root));
    cfg.language       = state.config_state.language;
    cfg.editor         = state.config_state.editor;
    cfg.api_key        = state.config_state.api_key;
    cfg.model          = state.config_state.model;
    cfg.ui_mode        = state.config_state.ui_mode;
    cfg.autostart_repl = state.config_state.autostart_repl;
    cfg.max_tokens     = state.config_state.max_tokens;
    cfg.ai_enabled     = state.config_state.ai_enabled;
    cfg.template_enabled = state.config_state.template_enabled;
    cfg.lanqiao_cookie = state.config_state.lanqiao_cookie;
    cfg.save(Config::config_path(root));
    state.config_state.status_msg = "\xe9\x85\x8d\xe7\xbd\xae\xe5\xb7\xb2\xe4\xbf\x9d\xe5\xad\x98\xe3\x80\x82";
}

void load_list_state(AppState& state,
                     const std::string& status_filter,
                     const std::string& difficulty_filter,
                     const std::string& source_filter) {
    if (state.list_state.loaded &&
        state.list_state.status_filter == status_filter &&
        state.list_state.difficulty_filter == difficulty_filter &&
        state.list_state.source_filter == source_filter)
        return;

    auto root = Config::find_root();
    state.list_state.status_filter = status_filter;
    state.list_state.difficulty_filter = difficulty_filter;
    state.list_state.source_filter = source_filter;
    if (root.empty()) {
        state.list_state.error = "\xe6\x9c\xaa\xe6\x89\xbe\xe5\x88\xb0 .shuati \xe9\xa1\xb9\xe7\x9b\xae";
        state.list_state.rows.clear();
        state.list_state.loaded = true;
        return;
    }
    try {
        auto svc = cmd::Services::load(root);
        auto problems = svc.pm->filter_problems(
            svc.pm->list_problems(), status_filter,
            difficulty_filter, source_filter);
        auto current_time = std::time(nullptr);
        auto reviews = svc.db->get_due_reviews(current_time);
        std::unordered_set<std::string> due_ids;
        for (const auto& r : reviews) due_ids.insert(r.problem_id);

        std::string prev_id;
        if (state.list_state.selected <
            static_cast<int>(state.list_state.rows.size()))
            prev_id = state.list_state.rows[state.list_state.selected].id;

        state.list_state.rows.clear();
        for (const auto& p : problems) {
            ListState::Row row;
            row.tid        = p.display_id;
            row.id         = p.id;
            row.title      = p.title;
            row.difficulty = p.difficulty;
            row.source     = cmd::canonical_source(p.source);
            row.status     = p.last_verdict.empty() ? "-" : p.last_verdict;
            row.passed     = (p.last_verdict == "AC");
            row.review_due = (due_ids.count(p.id) > 0);
            {
                char buf[16] = {};
                std::time_t t = static_cast<std::time_t>(p.created_at);
                struct std::tm tm_val{};
#ifdef _WIN32
                localtime_s(&tm_val, &t);
#else
                localtime_r(&t, &tm_val);
#endif
                std::strftime(buf, sizeof(buf), "%Y-%m-%d", &tm_val);
                row.date = buf;
            }
            state.list_state.rows.push_back(std::move(row));
        }
        state.list_state.selected = 0;
        if (!prev_id.empty()) {
            for (int i = 0;
                 i < static_cast<int>(state.list_state.rows.size()); ++i) {
                if (state.list_state.rows[i].id == prev_id) {
                    state.list_state.selected = i;
                    break;
                }
            }
        }
        state.list_state.error.clear();
    } catch (const std::exception& e) {
        state.list_state.rows.clear();
        state.list_state.error = std::string("\xe6\x95\xb0\xe6\x8d\xae\xe5\xba\x93\xe9\x94\x99\xe8\xaf\xaf: ") + e.what();
    }
    state.list_state.loaded = true;
}

void load_dashboard_state(AppState& state) {
    auto root = Config::find_root();
    if (root.empty()) {
        state.dashboard_state.error = "\xe6\x9c\xaa\xe6\x89\xbe\xe5\x88\xb0 .shuati \xe9\xa1\xb9\xe7\x9b\xae";
        state.dashboard_state.loaded = true;
        return;
    }
    try {
        auto svc = cmd::Services::load(root);
        auto problems = svc.pm->list_problems();
        state.dashboard_state.total_problems = static_cast<int>(problems.size());
        state.dashboard_state.ac_count = 0;
        state.dashboard_state.wa_count = 0;
        int easy_count = 0, medium_count = 0, hard_count = 0;
        for (const auto& p : problems) {
            if (p.last_verdict == "AC") state.dashboard_state.ac_count++;
            else if (!p.last_verdict.empty()) state.dashboard_state.wa_count++;
            if (p.difficulty == "easy") easy_count++;
            else if (p.difficulty == "medium") medium_count++;
            else if (p.difficulty == "hard") hard_count++;
        }
        auto current_time = std::time(nullptr);
        auto reviews = svc.db->get_due_reviews(current_time);
        state.dashboard_state.pending_reviews = static_cast<int>(reviews.size());
        state.dashboard_state.difficulty_distribution.values = {
            easy_count, medium_count, hard_count};
        state.dashboard_state.difficulty_distribution.labels = {
            "Easy", "Medium", "Hard"};
        state.dashboard_state.difficulty_distribution.max_value =
            std::max({easy_count, medium_count, hard_count});
        state.dashboard_state.weekly_chart.values = {0,0,0,0,0,0,0};
        state.dashboard_state.weekly_chart.labels =
            {"\xe4\xb8\x80","\xe4\xba\x8c","\xe4\xb8\x89",
             "\xe5\x9b\x9b","\xe4\xba\x94","\xe5\x85\xad","\xe6\x97\xa5"};
        state.dashboard_state.weekly_chart.max_value = 1;
        state.dashboard_state.streak_days = 0;
        state.dashboard_state.today_solved = 0;
        state.dashboard_state.calculate_stats();
        state.dashboard_state.recent_activities.clear();
        state.dashboard_state.recent_activities.push_back(
            {"\xe5\x88\x9a\xe5\x88\x9a", "\xe5\x90\xaf\xe5\x8a\xa8",
             "shuati CLI \xe5\xb7\xb2\xe5\xb0\xb1\xe7\xbb\xaa", "info"});
        if (state.dashboard_state.pending_reviews > 0) {
            state.dashboard_state.recent_activities.push_back(
                {"--", "\xe5\xbe\x85\xe5\x8a\x9e",
                 "\xe6\x9c\x89 " +
                     std::to_string(state.dashboard_state.pending_reviews) +
                     " \xe9\x81\x93\xe9\xa2\x98\xe5\xbe\x85\xe5\xa4\x8d\xe4\xb9\xa0",
                 "hint"});
        }
        if (state.dashboard_state.ac_count > 0) {
            state.dashboard_state.recent_activities.push_back(
                {"--", "\xe6\x88\x90\xe5\xb0\xb1",
                 "\xe5\xb7\xb2\xe9\x80\x9a\xe8\xbf\x87 " +
                     std::to_string(state.dashboard_state.ac_count) +
                     " \xe9\x81\x93\xe9\xa2\x98",
                 "AC"});
        }
        state.dashboard_state.stat_cards = {
            {"\xe6\x80\xbb\xe9\xa2\x98\xe6\x95\xb0",
             std::to_string(state.dashboard_state.total_problems), "",
             "\xf0\x9f\x93\x9a", true},
            {"\xe5\xb7\xb2\xe9\x80\x9a\xe8\xbf\x87",
             std::to_string(state.dashboard_state.ac_count), "",
             "\xe2\x9c\x93", true},
            {"\xe5\xbe\x85\xe5\xa4\x8d\xe4\xb9\xa0",
             std::to_string(state.dashboard_state.pending_reviews), "",
             "\xf0\x9f\x93\x9d", state.dashboard_state.pending_reviews == 0},
            {"\xe8\xbf\x9e\xe7\xbb\xad\xe5\xa4\xa9\xe6\x95\xb0",
             std::to_string(state.dashboard_state.streak_days),
             "\xe5\xa4\xa9", "\xf0\x9f\x94\xa5", true},
            {"\xe4\xbb\x8a\xe6\x97\xa5\xe8\xa7\xa3\xe9\xa2\x98",
             std::to_string(state.dashboard_state.today_solved), "",
             "\xe2\x9a\xa1", true},
        };
        state.dashboard_state.error.clear();
        state.dashboard_state.loaded = true;
    } catch (const std::exception& e) {
        state.dashboard_state.error =
            std::string("\xe6\x95\xb0\xe6\x8d\xae\xe5\xba\x93\xe9\x94\x99\xe8\xaf\xaf: ") + e.what();
        state.dashboard_state.loaded = true;
    }
}

}  // namespace views
}  // namespace tui
}  // namespace shuati
