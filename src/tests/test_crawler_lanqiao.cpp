#include <cassert>
#include <iostream>

#include "shuati/adapters/lanqiao_crawler.hpp"

using namespace shuati;

int main() {
  std::string html = R"(
<title>蓝桥账户中心 - 蓝桥云课</title>
<meta name="keywords" content="模拟,字符串">
<meta name="description" content="给定用户名，输出规范化账号。">
<div>难度：中等</div>
)";

  auto p = LanqiaoCrawler::parse_problem_from_html(
      "https://www.lanqiao.cn/problems/348/learning/", html);

  assert(p.id == "lq_348");
  assert(p.title == "蓝桥账户中心");
  assert(p.tags.find("模拟") != std::string::npos);
  assert(!p.difficulty.empty());
  assert(p.description.find("规范化账号") != std::string::npos);

  std::cout << "Lanqiao crawler metadata tests passed!\n";
  return 0;
}
