#include <iostream>
#include <filesystem>
#include <cassert>
#include <vector>
#include <string>
#include "shuati/database.hpp"
#include "shuati/memory_manager.hpp"

using namespace shuati;

void test_memory_parsing() {
    std::string db_path = "test_memory.db";
    if (std::filesystem::exists(db_path)) {
        std::filesystem::remove(db_path);
    }
    

    
    // 1. Simluate AI response with hidden block
    std::string ai_response = R"(
Hello user, here is your hint.

<!-- SYSTEM_OP: UPDATE_MEMORY
{
  "new_mistake": "Sliding Window Off-by-one",
  "reinforce_mastery": "Hash Map"
}
-->
Hope this helps!
)";

    {
        Database db(db_path);
        MemoryManager mm(db);

        std::cout << "Testing AI response parsing...\n";
        bool updated = mm.update_memory_from_response(ai_response);
        if (!updated) {
            std::cerr << "Failed to parse AI response!\n";
            exit(1);
        }
        
        // 2. Verify DB state
        auto mistakes = db.get_all_memory_mistakes();
        if (mistakes.size() != 1) {
            std::cerr << "Expected 1 mistake, got " << mistakes.size() << "\n";
            exit(1);
        }
        if (mistakes[0].pattern != "Sliding Window Off-by-one") {
            std::cerr << "Mistake pattern mismatch: " << mistakes[0].pattern << "\n";
            exit(1);
        }
        if (mistakes[0].frequency != 1) {
            std::cerr << "Frequency mismatch: " << mistakes[0].frequency << "\n";
            exit(1);
        }
        
        auto mastery = db.get_all_mastery();
        if (mastery.size() != 1) {
            std::cerr << "Expected 1 mastery, got " << mastery.size() << "\n";
            exit(1);
        }
        if (mastery[0].skill != "Hash Map") {
            std::cerr << "Mastery skill mismatch: " << mastery[0].skill << "\n";
            exit(1);
        }
        // Verify confidence (First call: 50.0)
        if (std::abs(mastery[0].confidence - 50.0) > 0.001) {
             std::cerr << "Mastery confidence mismatch (1st). Expected 50.0, got " << mastery[0].confidence << "\n";
             exit(1);
        }
        
        // 3. Update again (same mistake, same mastery)
        std::cout << "Testing mistake frequency & mastery update...\n";
        mm.update_memory_from_response(ai_response);
        mistakes = db.get_all_memory_mistakes();
        if (mistakes[0].frequency != 2) {
            std::cerr << "Frequency update failed, expected 2, got " << mistakes[0].frequency << "\n";
            exit(1);
        }
        
        // Check mastery update (50 -> 60)
        mastery = db.get_all_mastery();
        if (std::abs(mastery[0].confidence - 60.0) > 0.001) {
             std::cerr << "Mastery confidence mismatch (2nd). Expected 60.0, got " << mastery[0].confidence << "\n";
             exit(1);
        }
        
        // 4. Get relevant context
        std::cout << "Testing context retrieval...\n";
        db.upsert_mastery("Hash Map", 90.0);
        std::string ctx = mm.get_relevant_context("array");
        std::cout << "Context:\n" << ctx << "\n";
        
        if (ctx.find("Sliding Window Off-by-one") == std::string::npos) {
            std::cerr << "Context missing mistake pattern\n";
            exit(1);
        }
        if (ctx.find("Hash Map") == std::string::npos) {
            std::cerr << "Context missing mastered skill\n";
            exit(1);
        }
    } // db destructed here and file released

    std::cout << "test_memory (parsing & DB) passed!\n";
}

int main() {
    try {
        test_memory_parsing();
    } catch (const std::exception& e) {
        std::cerr << "Test threw exception: " << e.what() << "\n";
        return 1;
    }
    
    // Try to remove db file
    try {
        std::filesystem::remove("test_memory.db");
    } catch(...) {}
    
    return 0;
}
