#include "shuati/adapters/luogu_crawler.hpp"

namespace shuati {

bool LuoguCrawler::can_handle(const std::string&) const { return false; }
Problem LuoguCrawler::fetch_problem(const std::string&) { return {}; }
std::vector<TestCase> LuoguCrawler::fetch_test_cases(const std::string&) { return {}; }

} // namespace shuati
