#include "shuati/adapters/codeforces_crawler.hpp"

namespace shuati {

bool CodeforcesCrawler::can_handle(const std::string&) const { return false; }
Problem CodeforcesCrawler::fetch_problem(const std::string&) { return {}; }
std::vector<TestCase> CodeforcesCrawler::fetch_test_cases(const std::string&) { return {}; }

} // namespace shuati
