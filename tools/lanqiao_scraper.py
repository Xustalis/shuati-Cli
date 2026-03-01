import os
import time
import random
import logging
import requests
import re
from typing import Optional, Dict, Any, List
from requests.adapters import HTTPAdapter
from urllib3.util.retry import Retry
from dataclasses import dataclass
import json

# Setup logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s [%(levelname)s] %(message)s',
    datefmt='%Y-%m-%d %H:%M:%S'
)
logger = logging.getLogger("LanqiaoScraper")

@dataclass
class ProblemData:
    id: str
    title: str
    description: str
    difficulty: str
    tags: List[str]

class LanqiaoScraper:
    BASE_URL = "https://www.lanqiao.cn"
    API_URL = "https://www.lanqiao.cn/api/v2/problems/{id}/"
    
    # Common user agents to rotate
    USER_AGENTS = [
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/91.0.4472.124 Safari/537.36",
        "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/14.0.3 Safari/605.1.15",
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64; rv:89.0) Gecko/20100101 Firefox/89.0",
        "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/92.0.4515.107 Safari/537.36"
    ]

    def __init__(self, cookie: Optional[str] = None):
        self.session = requests.Session()
        self.cookie = cookie or os.getenv("LANQIAO_COOKIE")
        
        # Configure retries
        retries = Retry(total=3, backoff_factor=1, status_forcelist=[500, 502, 503, 504])
        self.session.mount('https://', HTTPAdapter(max_retries=retries))
        
        # Initial headers
        self.update_headers()

    def update_headers(self):
        """Rotate User-Agent and set Cookie"""
        ua = random.choice(self.USER_AGENTS)
        logger.debug(f"Rotating User-Agent to: {ua}")
        self.session.headers.update({
            "User-Agent": ua,
            "Accept": "application/json, text/plain, */*",
            "Referer": "https://www.lanqiao.cn/"
        })
        if self.cookie:
            self.session.headers["Cookie"] = self.cookie

    def _sleep_random(self):
        """Sleep for a random interval between 1-3 seconds"""
        sleep_time = random.uniform(1.0, 3.0)
        logger.debug(f"Sleeping for {sleep_time:.2f}s")
        time.sleep(sleep_time)

    def extract_id(self, url: str) -> Optional[str]:
        """Extract problem ID from URL"""
        match = re.search(r"problems/(\d+)", url)
        if match:
            return match.group(1)
        return None

    def fetch_problem(self, url: str) -> Optional[ProblemData]:
        problem_id = self.extract_id(url)
        if not problem_id:
            logger.error(f"Could not extract ID from URL: {url}")
            return None
            
        api_url = self.API_URL.format(id=problem_id)
        logger.info(f"Fetching problem {problem_id} from {api_url}")
        
        try:
            self._sleep_random()
            self.update_headers()
            
            response = self.session.get(api_url, timeout=10)
            logger.info(f"Response Status: {response.status_code}, Length: {len(response.content)}")
            
            if response.status_code == 401 or response.status_code == 403:
                logger.error("Authentication required. Please set LANQIAO_COOKIE environment variable.")
                return None
                
            if response.status_code != 200:
                logger.error(f"Failed to fetch data. Status: {response.status_code}")
                return None

            data = response.json()
            logger.debug(f"Parsed JSON keys: {list(data.keys())}")
            
            # Extract fields
            title = data.get("title", "Untitled")
            description = data.get("content", "")
            
            # Difficulty mapping
            difficulty_raw = data.get("difficulty", 0)
            if difficulty_raw <= 20:
                difficulty = "简单"
            elif difficulty_raw <= 50:
                difficulty = "中等"
            else:
                difficulty = "困难"
                
            # Tags
            tags = []
            if "tags" in data:
                for tag in data["tags"]:
                    if isinstance(tag, str):
                        tags.append(tag)
                    elif isinstance(tag, dict) and "name" in tag:
                        tags.append(tag["name"])
            
            logger.info(f"Successfully parsed problem: {title} ({difficulty})")
            
            return ProblemData(
                id=problem_id,
                title=title,
                description=description,
                difficulty=difficulty,
                tags=tags
            )

        except Exception as e:
            logger.exception(f"Error fetching problem: {e}")
            return None

    def generate_markdown(self, problem: ProblemData) -> str:
        """Generate Markdown content from problem data"""
        template = """# {title}

## 题目描述
{description}

## 题目信息
- **难度**: {difficulty}
- **标签**: {tags}
- **来源**: 蓝桥云课 (ID: {id})
"""
        return template.format(
            title=problem.title,
            description=problem.description,
            difficulty=problem.difficulty,
            tags=", ".join(problem.tags),
            id=problem.id
        )

    def save_markdown(self, problem: ProblemData, output_dir: str = "."):
        """Save problem to markdown file"""
        filename = f"lanqiao_{problem.id}.md"
        path = os.path.join(output_dir, filename)
        content = self.generate_markdown(problem)
        
        with open(path, "w", encoding="utf-8") as f:
            f.write(content)
        logger.info(f"Saved problem to {path}")

def main():
    import argparse
    parser = argparse.ArgumentParser(description="Lanqiao Problem Scraper")
    parser.add_argument("url", help="Problem URL (e.g. https://www.lanqiao.cn/problems/348/learning/)")
    parser.add_argument("--output", "-o", default=".", help="Output directory")
    parser.add_argument("--cookie", "-c", help="Lanqiao Cookie (optional, or set LANQIAO_COOKIE env)")
    
    args = parser.parse_args()
    
    scraper = LanqiaoScraper(cookie=args.cookie)
    problem = scraper.fetch_problem(args.url)
    
    if problem:
        scraper.save_markdown(problem, args.output)
    else:
        logger.error("Failed to scrape problem.")
        exit(1)

if __name__ == "__main__":
    main()
