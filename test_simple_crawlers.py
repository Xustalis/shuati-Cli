#!/usr/bin/env python3
"""
测试简化后的爬虫 - 只获取HTML页面文本
"""
import subprocess
import sys
import os

def test_simple_crawlers():
    """测试简化的爬虫 - 只获取HTML"""
    print("=" * 70)
    print("  测试简化后的爬虫 (只获取HTML)")
    print("=" * 70)
    print()

    # 测试用例
    test_cases = [
        {
            "url": "https://codeforces.com/problemset/problem/1500/A",
            "platform": "Codeforces",
            "expected": "Codeforces"
        },
        {
            "url": "https://leetcode.com/problems/two-sum/",
            "platform": "LeetCode",
            "expected": "Two Sum"
        },
        {
            "url": "https://www.luogu.com.cn/problem/P1001",
            "platform": "洛谷",
            "expected": "A+B"
        },
    ]

    print("测试说明:")
    print("  - 爬虫现在只获取HTML页面，不进行复杂的API调用")
    print("  - 提取页面标题和基本信息")
    print("  - 不需要Cookie或登录")
    print()

    results = []

    for i, test in enumerate(test_cases, 1):
        print(f"\n[{i}/{len(test_cases)}] 测试 {test['platform']}")
        print(f"  URL: {test['url']}")
        print("-" * 70)

        # 调用 shuati pull 命令
        shuati_path = os.path.join(os.getcwd(), "build", "shuati.exe" if os.name == 'nt' else "shuati")

        if not os.path.exists(shuati_path):
            print(f"  ⚠️  未找到可执行文件: {shuati_path}")
            print(f"  请先编译: mkdir build && cd build && cmake .. && cmake --build .")
            results.append((test['platform'], False, "未编译"))
            continue

        cmd = [shuati_path, "pull", test['url']]
        print(f"  运行: {' '.join(cmd)}")

        try:
            result = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                timeout=30
            )

            output = result.stdout + result.stderr
            print(output)

            if result.returncode != 0:
                print(f"  ❌ 失败 (返回码: {result.returncode})")
                results.append((test['platform'], False, "命令失败"))
            else:
                print(f"  ✅ 成功")
                results.append((test['platform'], True, "成功"))

        except subprocess.TimeoutExpired:
            print("  ❌ 超时 (30秒)")
            results.append((test['platform'], False, "超时"))
        except Exception as e:
            print(f"  ❌ 异常: {e}")
            results.append((test['platform'], False, str(e)))

    # 总结
    print("\n" + "=" * 70)
    print("  测试结果总结")
    print("=" * 70)

    for platform, success, reason in results:
        status = "✅" if success else "❌"
        print(f"{status} {platform:12s} - {reason}")

    success_count = sum(1 for _, success, _ in results if success)
    total = len(results)
    print(f"\n总计: {success_count}/{total} 成功")

    print("\n" + "=" * 70)
    print("  下一步")
    print("=" * 70)
    print("1. 检查 .shuati/problems/ 目录中的md文件")
    print("2. 用文本编辑器打开md文件，查看抓取的HTML内容")
    print("3. 如果某个平台失败，可能是网络问题或需要设置User-Agent")

if __name__ == "__main__":
    test_simple_crawlers()
