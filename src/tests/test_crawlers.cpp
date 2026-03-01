#include "shuati/adapters/lanqiao_crawler.hpp"
#include "shuati/adapters/luogu_crawler.hpp"
#include "shuati/adapters/leetcode_crawler.hpp"
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
                "title": "Mock Lanqiao",
                "content": "Mock Description",
                "difficulty": 10,
                "tags": [{"name": "dp"}]
            })", {}};
        }
        return {404, "", {}};
    });
    
    Problem p = crawler.fetch_problem("https://www.lanqiao.cn/problems/348/learning/");
    if (p.title != "Mock Lanqiao") {
        std::cerr << "Lanqiao Title mismatch: " << p.title << "\n";
        exit(1);
    }
    if (p.description != "Mock Description") {
        std::cerr << "Lanqiao Description mismatch\n";
        exit(1);
    }
    if (p.difficulty != "easy") {
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
    
    mock_client->set_handler([](const std::string& url) -> HttpResponse {
        return {200, R"(window._feInjection = JSON.parse(decodeURIComponent("%7B%22currentData%22%3A%7B%22problem%22%3A%7B%22title%22%3A%22Mock%20Luogu%22%2C%22description%22%3A%22Mock%20Desc%22%2C%22difficulty%22%3A2%2C%22tags%22%3A%5B%7B%22name%22%3A%22Greedy%22%7D%5D%7D%7D%7D"));)", {}};
    });
    
    Problem p = crawler.fetch_problem("https://www.luogu.com.cn/problem/P1001");
    if (p.title != "Mock Luogu") {
        std::cerr << "Luogu Title mismatch: " << p.title << "\n";
        exit(1);
    }
    if (p.description != "Mock Desc") {
        std::cerr << "Luogu Description mismatch\n";
        exit(1);
    }
    if (p.difficulty != "medium") {
        std::cerr << "Luogu Difficulty mismatch: " << p.difficulty << "\n";
        exit(1);
    }
    if (p.tags != "Greedy") {
        std::cerr << "Luogu Tags mismatch: " << p.tags << "\n";
        exit(1);
    }
    std::cout << "Luogu Test Passed!\n";
}

void test_leetcode() {
    auto mock_client = std::make_shared<MockHttpClient>();
    LeetCodeCrawler crawler(mock_client);
    
    mock_client->set_handler([](const std::string& url) -> HttpResponse {
        return {200, R"({
            "data": {
                "question": {
                    "questionFrontendId": "1",
                    "title": "Two Sum",
                    "content": "Find two numbers...",
                    "difficulty": "Easy",
                    "topicTags": [{"name": "Array"}]
                }
            }
        })", {}};
    });
    
    Problem p = crawler.fetch_problem("https://leetcode.com/problems/two-sum/");
    if (p.title != "Two Sum") {
        std::cerr << "LeetCode Title mismatch: " << p.title << "\n";
        exit(1);
    }
    if (p.description != "Find two numbers...") {
        std::cerr << "LeetCode Description mismatch\n";
        exit(1);
    }
    if (p.difficulty != "easy") {
        std::cerr << "LeetCode Difficulty mismatch: " << p.difficulty << "\n";
        exit(1);
    }
    if (p.tags != "Array") {
        std::cerr << "LeetCode Tags mismatch: " << p.tags << "\n";
        exit(1);
    }
    std::cout << "LeetCode Test Passed!\n";
}

int main() {
    try {
        test_lanqiao();
        test_luogu();
        test_leetcode();
        std::cout << "All tests passed!\n";
    } catch (const std::exception& e) {
        std::cerr << "Test failed: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
