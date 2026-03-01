#include "shuati/adapters/lanqiao_crawler.hpp"
#include "shuati/adapters/luogu_crawler.hpp"
#include "shuati/adapters/leetcode_crawler.hpp"
#include "shuati/adapters/codeforces_crawler.hpp"
#include "mock_http_client.hpp"
#include <cassert>
#include <iostream>
#include <memory>

using namespace shuati;

void test_lanqiao() {
    auto mock_client = std::make_shared<MockHttpClient>();
    LanqiaoCrawler crawler(mock_client);
    
    mock_client->set_handler([](const std::string& url) -> HttpResponse {
        if (url.find("api/v2/problems/348") != std::string::npos) {
            return {200, R"({
                "title": "Mock Lanqiao Problem",
                "content": "This is a mock problem description.",
                "difficulty": 1,
                "tags": [{"name": "dp"}]
            })", {}, ""};
        }
        return {404, "", {}, ""};
    });
    
    Problem p = crawler.fetch_problem("https://www.lanqiao.cn/problems/348/learning/");
    // Title should be formatted as "LQ348. Mock Lanqiao Problem"
    if (p.title.find("Mock Lanqiao Problem") == std::string::npos) {
        std::cerr << "Lanqiao Title mismatch: " << p.title << "\n";
        exit(1);
    }
    if (p.description.find("mock problem description") == std::string::npos) {
        std::cerr << "Lanqiao Description mismatch: " << p.description << "\n";
        exit(1);
    }
    if (p.difficulty != "简单") {
        std::cerr << "Lanqiao Difficulty mismatch: " << p.difficulty << "\n";
        exit(1);
    }
    if (p.tags != "dp") {
        std::cerr << "Lanqiao Tags mismatch: " << p.tags << "\n";
        exit(1);
    }
    std::cout << "Lanqiao Test Passed!\n";
}

void test_luogu() {
    auto mock_client = std::make_shared<MockHttpClient>();
    LuoguCrawler crawler(mock_client);
    
    // Luogu now parses <script id="lentille-context"> JSON
    mock_client->set_handler([](const std::string& url) -> HttpResponse {
        return {200, R"(
<html>
<head><title>P1001 A+B Problem - 洛谷</title></head>
<body>
<script id="lentille-context" type="application/json">{"data":{"problem":{"pid":"P1001","title":"A+B Problem","difficulty":1,"tags":[1],"contenu":{"description":"输入两个整数 a, b，输出它们的和。","formatI":"两个整数","formatO":"一个整数","hint":"","background":""},"samples":[["1 2","3"]]}}}</script>
</body>
</html>
)", {}, ""};
    });
    
    Problem p = crawler.fetch_problem("https://www.luogu.com.cn/problem/P1001");
    if (p.title.find("A+B Problem") == std::string::npos) {
        std::cerr << "Luogu Title mismatch: " << p.title << "\n";
        exit(1);
    }
    if (p.description.find("输入两个整数") == std::string::npos) {
        std::cerr << "Luogu Description mismatch: " << p.description << "\n";
        exit(1);
    }
    if (p.difficulty != "入门") {
        std::cerr << "Luogu Difficulty mismatch: " << p.difficulty << "\n";
        exit(1);
    }
    if (p.tags.find("模拟") == std::string::npos) {
        std::cerr << "Luogu Tags mismatch: " << p.tags << "\n";
        exit(1);
    }
    
    // Also check test cases
    auto cases = crawler.fetch_test_cases("https://www.luogu.com.cn/problem/P1001");
    if (cases.size() != 1) {
        std::cerr << "Luogu test case count mismatch: " << cases.size() << "\n";
        exit(1);
    }
    if (cases[0].input != "1 2" || cases[0].output != "3") {
        std::cerr << "Luogu test case content mismatch\n";
        exit(1);
    }
    
    std::cout << "Luogu Test Passed!\n";
}

void test_leetcode() {
    auto mock_client = std::make_shared<MockHttpClient>();
    LeetCodeCrawler crawler(mock_client);
    
    // LeetCode now uses GraphQL POST
    mock_client->set_post_handler([](const std::string& url, const std::string& body) -> HttpResponse {
        return {200, R"({
            "data": {
                "question": {
                    "questionFrontendId": "1",
                    "title": "Two Sum",
                    "translatedTitle": "",
                    "content": "<p>Given an array of integers, find two numbers such that they add up to a target.</p>",
                    "translatedContent": "",
                    "difficulty": "Easy",
                    "topicTags": [{"name": "Array", "translatedName": "数组"}],
                    "exampleTestcaseList": ["[2,7,11,15]\n9"]
                }
            }
        })", {}, ""};
    });
    
    Problem p = crawler.fetch_problem("https://leetcode.com/problems/two-sum/");
    if (p.title.find("Two Sum") == std::string::npos) {
        std::cerr << "LeetCode Title mismatch: " << p.title << "\n";
        exit(1);
    }
    if (p.description.find("array of integers") == std::string::npos) {
        std::cerr << "LeetCode Description mismatch: " << p.description << "\n";
        exit(1);
    }
    if (p.difficulty != "Easy") {
        std::cerr << "LeetCode Difficulty mismatch: " << p.difficulty << "\n";
        exit(1);
    }
    if (p.tags.find("Array") == std::string::npos && p.tags.find("数组") == std::string::npos) {
        std::cerr << "LeetCode Tags mismatch: " << p.tags << "\n";
        exit(1);
    }
    std::cout << "LeetCode Test Passed!\n";
}

void test_codeforces() {
    auto mock_client = std::make_shared<MockHttpClient>();
    CodeforcesCrawler crawler(mock_client);
    
    mock_client->set_handler([](const std::string& url) -> HttpResponse {
        if (url.find("api/contest.standings") != std::string::npos) {
            return {200, R"({
                "status": "OK",
                "result": {
                    "problems": [
                        {"contestId": 4, "index": "A", "name": "Watermelon", "rating": 800, "tags": ["brute force", "math"]}
                    ],
                    "rows": []
                }
            })", {}, ""};
        }
        // HTML page
        return {200, R"(
<html>
<head><title>Problem - 4A - Codeforces</title></head>
<body>
<div class="problem-statement">
<div class="header"><div class="title">A. Watermelon</div></div>
<div>
<p>One hot summer day Pete and his friend Billy decided to buy a watermelon.</p>
</div>
<div class="input-specification"><div class="section-title">Input</div><p>The first line contains integer w.</p></div>
<div class="output-specification"><div class="section-title">Output</div><p>Print YES or NO.</p></div>
</div>
<div class="sample-test">
<div class="input"><div class="title">Input</div><pre>8</pre></div>
<div class="output"><div class="title">Output</div><pre>YES</pre></div>
</div>
</body>
</html>
)", {}, ""};
    });
    
    Problem p = crawler.fetch_problem("https://codeforces.com/problemset/problem/4/A");
    if (p.title.find("Watermelon") == std::string::npos) {
        std::cerr << "Codeforces Title mismatch: " << p.title << "\n";
        exit(1);
    }
    if (p.difficulty.empty()) {
        std::cerr << "Codeforces Difficulty is empty\n";
        exit(1);
    }
    if (p.tags.find("math") == std::string::npos) {
        std::cerr << "Codeforces Tags mismatch: " << p.tags << "\n";
        exit(1);
    }
    
    // Test cases
    auto cases = crawler.fetch_test_cases("https://codeforces.com/problemset/problem/4/A");
    if (cases.empty()) {
        std::cerr << "Codeforces should have at least one test case\n";
        exit(1);
    }
    
    std::cout << "Codeforces Test Passed!\n";
}

int main() {
    try {
        test_lanqiao();
        test_luogu();
        test_leetcode();
        test_codeforces();
        std::cout << "All tests passed!\n";
    } catch (const std::exception& e) {
        std::cerr << "Test failed: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
