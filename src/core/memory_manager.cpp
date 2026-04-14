#include "shuati/memory_manager.hpp"
#include <nlohmann/json.hpp>
#include <fmt/core.h>
#include <algorithm>
#include <sstream>
#include <regex>
#include <iostream>

namespace shuati {

MemoryManager::MemoryManager(Database& db) : db_(db) {}

std::string MemoryManager::get_relevant_context(const std::string& tags) {
    auto mistakes = db_.get_all_memory_mistakes();
    auto mastery = db_.get_all_mastery();
    auto profile = db_.get_user_profile();
    
    std::stringstream ss;
    ss << "## User Profile\n";
    ss << "- Rating: " << profile.elo_rating << "\n";
    if (!profile.preferences.empty() && profile.preferences != "{}") {
         ss << "- Preferences: " << profile.preferences << "\n";
    }
    ss << "\n";

    // Filter mistakes relevant to tags (simple substring match for now)
    // Or just show top global mistakes if no specific tags matches
    // For V1 iteration, we just show top 3 global mistakes to keep it simple and robust
    if (!mistakes.empty()) {
        ss << "## Frequent Mistakes (Please Check)\n";
        int count = 0;
        for (const auto& m : mistakes) {
            if (count >= 3) break;
            ss << "- " << m.pattern << " (freq: " << m.frequency << ")\n";
            count++;
        }
        ss << "\n";
    }
    
    if (!mastery.empty()) {
        ss << "## Mastered Skills (Skip Basic Explanations)\n";
        int count = 0;
        for (const auto& m : mastery) {
            if (count >= 3) break;
            if (m.confidence > 80.0) {
                 ss << "- " << m.skill << "\n";
                 count++;
            }
        }
        ss << "\n";
    }
    
    return ss.str();
}

bool MemoryManager::update_memory_from_response(const std::string& ai_response) {
    // Regex to find the hidden block
    // <!-- SYSTEM_OP: UPDATE_MEMORY ... -->
    std::regex re(R"(<!--\s*SYSTEM_OP:\s*UPDATE_MEMORY\s*([\s\S]*?)\s*-->)");
    std::smatch match;
    
    if (std::regex_search(ai_response, match, re) && match.size() > 1) {
        std::string json_str = match[1].str();
        try {
            auto j = nlohmann::json::parse(json_str);
            
            if (j.contains("new_mistake") && j["new_mistake"].is_string()) {
                std::string pattern = j["new_mistake"].get<std::string>();
                // Validate: non-empty, max 500 chars, no control chars
                if (!pattern.empty() && pattern.size() <= 500 &&
                    pattern.find('\0') == std::string::npos) {
                    std::string tags = j.value("tags", "");
                    std::string example_id = j.value("example_id", "");
                    if (tags.size() > 200) tags = tags.substr(0, 200);
                    if (example_id.size() > 100) example_id = example_id.substr(0, 100);
                    db_.upsert_memory_mistake(tags, pattern, example_id); 
                }
            }
            
            if (j.contains("reinforce_mastery") && j["reinforce_mastery"].is_string()) {
                std::string skill = j["reinforce_mastery"].get<std::string>();
                // Validate: non-empty, max 200 chars, no control chars
                if (!skill.empty() && skill.size() <= 200 &&
                    skill.find('\0') == std::string::npos) {
                     // Optimization: Use direct lookup instead of scanning all mastery items
                     auto current = db_.get_mastery(skill);
                     double conf = 50.0;
                     if (current) {
                         // Diminishing returns: the closer to 100, the smaller the gain.
                         // This prevents trivial problems from instantly maxing out mastery.
                         double gap = 100.0 - current->confidence;
                         double gain = std::max(1.0, 5.0 * (gap / 100.0));
                         conf = std::min(100.0, current->confidence + gain);
                     }
                    db_.upsert_mastery(skill, conf);
                }
            }
            return true;
        } catch (const nlohmann::json::parse_error& e) {
            std::cerr << "Failed to parse memory update JSON: " << e.what() << "\n";
            return false;
        } catch (const std::exception& e) {
            std::cerr << "Database error during memory update: " << e.what() << "\n";
            return false;
        }
    }
    return false;
}

} // namespace shuati
