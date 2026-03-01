import unittest
from unittest.mock import MagicMock, patch
from lanqiao_scraper import LanqiaoScraper, ProblemData
import json

class TestLanqiaoScraper(unittest.TestCase):
    def setUp(self):
        self.scraper = LanqiaoScraper(cookie="test_cookie")
        self.mock_response = {
            "title": "Mock Problem",
            "content": "This is a mock description.",
            "difficulty": 10,
            "tags": ["dp", "greedy"]
        }
        
    @patch('requests.Session.get')
    def test_fetch_problem_success(self, mock_get):
        # Mock successful response
        mock_resp = MagicMock()
        mock_resp.status_code = 200
        mock_resp.json.return_value = self.mock_response
        mock_resp.content = b'test content'
        mock_get.return_value = mock_resp
        
        problem = self.scraper.fetch_problem("https://www.lanqiao.cn/problems/348/learning/")
        
        self.assertIsNotNone(problem)
        self.assertEqual(problem.title, "Mock Problem")
        self.assertEqual(problem.description, "This is a mock description.")
        self.assertEqual(problem.difficulty, "简单") # 10 <= 20
        self.assertEqual(problem.tags, ["dp", "greedy"])
        
    @patch('requests.Session.get')
    def test_fetch_problem_401(self, mock_get):
        # Mock 401 response
        mock_resp = MagicMock()
        mock_resp.status_code = 401
        mock_get.return_value = mock_resp
        
        problem = self.scraper.fetch_problem("https://www.lanqiao.cn/problems/348/learning/")
        
        self.assertIsNone(problem)

    def test_extract_id(self):
        url = "https://www.lanqiao.cn/problems/348/learning/"
        self.assertEqual(self.scraper.extract_id(url), "348")
        
        url = "https://www.lanqiao.cn/problems/12345"
        self.assertEqual(self.scraper.extract_id(url), "12345")
        
    def test_generate_markdown(self):
        p = ProblemData("348", "Test", "Desc", "简单", ["tag1"])
        md = self.scraper.generate_markdown(p)
        self.assertIn("# Test", md)
        self.assertIn("Desc", md)
        self.assertIn("简单", md)
        self.assertIn("tag1", md)

if __name__ == '__main__':
    unittest.main()
