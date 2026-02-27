#pragma once

#include <string>
#include <vector>
#include "shuati/types.hpp"

namespace shuati {

// Strategy pattern interface for different OJ platforms
class ICrawler {
public:
    virtual ~ICrawler() = default;
    
    // Check if this crawler can handle the given URL
    virtual bool can_handle(const std::string& url) const = 0;
    
    // Fetch problem metadata (title, description, tags, difficulty)
    virtual Problem fetch_problem(const std::string& url) = 0;
    
    // Fetch test cases (samples or full if available)
    virtual std::vector<TestCase> fetch_test_cases(const std::string& url) = 0;
};

} // namespace shuati
