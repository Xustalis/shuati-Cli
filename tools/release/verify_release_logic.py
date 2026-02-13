import os
import sys
import shutil
from pathlib import Path

# Add the tools directory to sys.path
sys.path.append(str(Path(__file__).resolve().parents[1]))

try:
    from release import plan_release, update_versions
except ImportError:
    # Handle running from root
    sys.path.append(str(Path("tools").resolve()))
    from release import plan_release, update_versions

def test_version_logic():
    print("Testing version increment logic...")
    assert plan_release._inc_version("1.2.3", "patch") == "1.2.4"
    assert plan_release._inc_version("1.2.3", "minor") == "1.3.0"
    assert plan_release._inc_version("1.2.3", "major") == "2.0.0"
    print("PASS: Version increment logic")

def test_changelog_generation():
    print("Testing changelog logic...")
    commits = [
        plan_release.ParsedCommit("sha1", "feat", "scope", False, "feat: new feature", "feat: new feature"),
        plan_release.ParsedCommit("sha2", "fix", "scope", False, "fix: bug fix", "fix: bug fix"),
        plan_release.ParsedCommit("sha3", "other", "", True, "feat!: breaking change", "feat!: breaking change"),
    ]
    
    bump = plan_release._bump_kind(commits)
    assert bump == "major" # Because of breaking change
    
    # Mock _contributors to avoid git dependency
    original_contrib = plan_release._contributors
    plan_release._contributors = lambda tag: ["Alice", "Bob"]
    try:
        notes = plan_release.build_notes("v1.0.0", "v2.0.0", commits)
    finally:
        plan_release._contributors = original_contrib

    assert "### 破坏性变更" in notes
    assert "breaking change" in notes
    assert "### 新功能" in notes
    assert "new feature" in notes
    assert "### 修复" in notes
    assert "bug fix" in notes
    print("PASS: Changelog generation")

if __name__ == "__main__":
    try:
        test_version_logic()
        test_changelog_generation()
        print("\nAll verification tests passed!")
    except AssertionError as e:
        print(f"\nFAIL: Assertion failed")
        raise
    except Exception as e:
        print(f"\nFAIL: {e}")
        raise
