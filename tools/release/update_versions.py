import datetime as _dt
import os
import re
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]


def _read(p: Path) -> str:
    return p.read_text(encoding="utf-8", errors="strict").replace("\r\n", "\n")


def _write(p: Path, s: str) -> None:
    p.write_text(s, encoding="utf-8", newline="\n")


def _replace_one(text: str, pattern: str, repl: str) -> str:
    new, n = re.subn(pattern, repl, text, count=1, flags=re.M)
    if n != 1:
        raise RuntimeError(f"expected exactly 1 replacement for pattern: {pattern}, got {n}")
    return new


def _replace_optional(text: str, pattern: str, repl: str) -> str:
    """Like _replace_one but silently returns unchanged text when pattern is not found."""
    new, n = re.subn(pattern, repl, text, count=1, flags=re.M)
    return new


def bump_cmake(version: str) -> None:
    p = ROOT / "CMakeLists.txt"
    txt = _read(p)
    txt = _replace_one(
        txt,
        r"^project\(shuati-cli\s+VERSION\s+[0-9]+\.[0-9]+\.[0-9]+\s+LANGUAGES\s+CXX\)\s*$",
        f"project(shuati-cli VERSION {version} LANGUAGES CXX)",
    )
    _write(p, txt)


def bump_vcpkg(version: str) -> None:
    p = ROOT / "vcpkg.json"
    txt = _read(p)
    txt = _replace_one(
        txt,
        r'^\s*"version-string"\s*:\s*"[0-9]+\.[0-9]+\.[0-9]+"\s*,\s*$',
        f'  "version-string": "{version}",',
    )
    _write(p, txt)


def bump_readme_badge(version: str) -> None:
    p = ROOT / "README.md"
    if not p.exists():
        return
    txt = _read(p)
    txt = _replace_optional(
        txt,
        r"^\[!\[Version\]\(https://img\.shields\.io/badge/version-[0-9]+\.[0-9]+\.[0-9]+-green\.svg\)\]\(https://github\.com/Xustalis/shuati-Cli/releases\)\s*$",
        f"[![Version](https://img.shields.io/badge/version-{version}-green.svg)](https://github.com/Xustalis/shuati-Cli/releases)",
    )
    _write(p, txt)


def bump_installer(version: str) -> None:
    p = ROOT / "install.ps1"
    if not p.exists():
        return
    txt = _read(p)
    txt = _replace_optional(txt, r"^# Shuati CLI Installer v[0-9]+\.[0-9]+\.[0-9]+\s*$", f"# Shuati CLI Installer v{version}")
    _write(p, txt)


def prepend_changelog(version: str, notes_markdown: str) -> None:
    p = ROOT / "CHANGELOG.md"
    if not p.exists():
        return
    txt = _read(p)
    today = _dt.date.today().isoformat()
    header = f"## [{version}] - {today}\n\n"
    body = notes_markdown.strip() + "\n\n" if notes_markdown.strip() else "### Changed\n- (No notable changes)\n\n"
    insert = header + body

    m = re.search(r"(?m)^## \[", txt)
    if not m:
        new_txt = txt.rstrip() + "\n\n" + insert
    else:
        new_txt = txt[: m.start()] + insert + txt[m.start() :]
    _write(p, new_txt)


def main() -> int:
    if len(sys.argv) < 2:
        raise SystemExit("usage: update_versions.py <new_version> [notes_file]")
    version = sys.argv[1].strip()
    notes = ""
    if len(sys.argv) >= 3:
        notes_path = Path(sys.argv[2])
        if notes_path.exists():
            notes = notes_path.read_text(encoding="utf-8", errors="ignore")
    bump_cmake(version)
    bump_vcpkg(version)
    bump_readme_badge(version)
    bump_installer(version)
    prepend_changelog(version, notes)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
