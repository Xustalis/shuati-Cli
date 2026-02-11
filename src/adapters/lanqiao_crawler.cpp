#include "shuati/adapters/lanqiao_crawler.hpp"

namespace shuati {

bool LanqiaoCrawler::can_handle(const std::string&) const { return false; }
Problem LanqiaoCrawler::fetch_problem(const std::string&) { return {}; }
std::vector<TestCase> LanqiaoCrawler::fetch_test_cases(const std::string&) { return {}; }

} // namespace shuati
