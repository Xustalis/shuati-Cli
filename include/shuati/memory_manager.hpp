#pragma once

#include <string>
#include <vector>
#include <memory>
#include "shuati/database.hpp"
#include "shuati/types.hpp"

namespace shuati {

class MemoryManager {
public:
    explicit MemoryManager(Database& db);
    
    // Retrieves relevant context for the AI System Prompt based on tags and current problem
    // Returns a formatted string ready to be injected into system prompt
    std::string get_relevant_context(const std::string& tags);

    // Updates memory based on AI's structured output (hidden block)
    // Returns true if memory was updated
    bool update_memory_from_response(const std::string& ai_response);

private:
    Database& db_;
};

} // namespace shuati
