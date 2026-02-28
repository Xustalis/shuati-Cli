#!/usr/bin/env python3
"""Generate deterministic crawler fixture dataset for local/CI tests."""
import json
from pathlib import Path

out = Path("tools/testing/datasets")
out.mkdir(parents=True, exist_ok=True)

sample = {
    "source": "matiji",
    "version": 1,
    "pages": [
        {
            "url": "https://www.matiji.net/exam/ojquestion/123",
            "html": "<title>A+B 问题 - 马蹄集</title><div>输入样例</div><pre>1 2</pre><div>输出样例</div><pre>3</pre>",
            "expected": {
                "id": "123",
                "title": "A+B 问题",
                "test_cases": [{"input": "1 2", "output": "3", "is_sample": True}],
            },
        }
    ],
}

(out / "crawler_fixture_matiji.json").write_text(json.dumps(sample, ensure_ascii=False, indent=2), encoding="utf-8")
print(f"generated: {out / 'crawler_fixture_matiji.json'}")
