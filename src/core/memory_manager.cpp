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
                std::string pattern = j["new_mistake"];
                // Extract tags from where? For now, let's use a placeholder or empty
                // In a perfect world, we'd pass current problem tags here too.
                // For now, we rely on AI not providing tags in this JSON, or we assume global.
                // Let's just use empty tags for now as the schema allows it.
                if (!pattern.empty()) {
                    db_.upsert_memory_mistake("", pattern, ""); 
                }
            }
            
            if (j.contains("reinforce_mastery") && j["reinforce_mastery"].is_string()) {
                std::string skill = j["reinforce_mastery"];
                 if (!skill.empty()) {
                     // Optimization: Use direct lookup instead of scanning all mastery items
                     auto current = db_.get_mastery(skill);
                     double conf = 50.0;
                     if (current) {
                         conf = std::min(100.0, current->confidence + 10.0);
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
