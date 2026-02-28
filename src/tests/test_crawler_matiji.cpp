#include <cassert>
#include <filesystem>
#include <iostream>

#include "shuati/adapters/matiji_crawler.hpp"
#include "shuati/database.hpp"

using namespace shuati;

void test_matiji_url_id_extract() {
  assert(MatijiCrawler::extract_problem_id_from_url(
             "https://www.matiji.net/exam/ojquestion/123") == "123");
  assert(MatijiCrawler::extract_problem_id_from_url(
             "https://www.matiji.net/exam/ojquestionlist?qid=456") == "456");
  assert(MatijiCrawler::extract_problem_id_from_url(
             "https://www.matiji.net/exam/ojquestion?questionId=789") == "789");
}

void test_matiji_case_parse() {
  std::string html = R"(
<div>输入样例</div><pre>1 2</pre>
<div>输出样例</div><pre>3</pre>
<div>输入样例</div><pre>1 2</pre>
<div>输出样例</div><pre>3</pre>
)";

  auto cases = MatijiCrawler::parse_test_cases_from_html(html);
  assert(cases.size() == 1);
  assert(cases[0].input == "1 2");
  assert(cases[0].output == "3");
}

void test_db_testcases_idempotent() {
  const std::string db_path = "test_matiji_crawler.db";
  std::filesystem::remove(db_path);

  Database db(db_path);

  const bool first = db.add_test_case("mtj_1", "1 2", "3", true);
  const bool second = db.add_test_case("mtj_1", "1 2", "3", true);
  const bool third = db.add_test_case("mtj_1", "2 3", "5", true);

  assert(first);
  assert(!second);
  assert(third);

  auto tc = db.get_test_cases("mtj_1");
  assert(tc.size() == 2);

  db.clear_test_cases("mtj_1");
  tc = db.get_test_cases("mtj_1");
  assert(tc.empty());

  std::filesystem::remove(db_path);
}

int main() {
  test_matiji_url_id_extract();
  test_matiji_case_parse();
  test_db_testcases_idempotent();
  std::cout << "Matiji crawler tests passed!\n";
  return 0;
}
