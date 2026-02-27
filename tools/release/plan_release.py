import json
import os
import re
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, List, Optional, Tuple


def _run(args: List[str]) -> str:
    p = subprocess.run(args, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, check=False)
    if p.returncode != 0:
        raise RuntimeError(f"command failed: {' '.join(args)}\n{p.stderr.strip()}")
    return p.stdout


def _get_latest_tag() -> Optional[str]:
    out = _run(["git", "tag", "--list", "v*", "--sort=-v:refname"]).strip().splitlines()
    return out[0].strip() if out else None


@dataclass
class ParsedCommit:
    sha: str
    type: str
    scope: str
    breaking: bool
    subject: str
    raw_subject: str


_CC_RE = re.compile(r"^(?P<type>[a-zA-Z]+)(?:\((?P<scope>[^)]+)\))?(?P<bang>!)?: (?P<subject>.+)$")
_BREAKING_RE = re.compile(r"(?m)^BREAKING CHANGE:\s+.+$")


def _parse_commit(subject: str, body: str, sha: str) -> ParsedCommit:
    m = _CC_RE.match(subject.strip())
    if not m:
        return ParsedCommit(sha=sha, type="other", scope="", breaking=bool(_BREAKING_RE.search(body)), subject=subject.strip(), raw_subject=subject.strip())
    ctype = m.group("type").lower()
    scope = (m.group("scope") or "").strip()
    breaking = bool(m.group("bang")) or bool(_BREAKING_RE.search(body))
    return ParsedCommit(
        sha=sha,
        type=ctype,
        scope=scope,
        breaking=breaking,
        subject=m.group("subject").strip(),
        raw_subject=subject.strip(),
    )


def _collect_commits(prev_tag: Optional[str]) -> List[ParsedCommit]:
    rev_range = f"{prev_tag}..HEAD" if prev_tag else "HEAD"
    raw = _run(["git", "log", rev_range, "--pretty=format:%H%x1f%s%x1f%b%x1e"])
    commits: List[ParsedCommit] = []
    for rec in raw.split("\x1e"):
        rec = rec.strip("\n")
        if not rec:
            continue
        parts = rec.split("\x1f")
        if len(parts) < 3:
            continue
        sha, subject, body = parts[0].strip(), parts[1], parts[2]
        commits.append(_parse_commit(subject, body, sha))
    return commits


def _bump_kind(commits: List[ParsedCommit]) -> str:
    if any(c.breaking for c in commits):
        return "major"
    if any(c.type == "feat" for c in commits):
        return "minor"
    if any(c.type in {"fix", "perf"} for c in commits):
        return "patch"
    return "none"


def _inc_version(version: str, bump: str) -> str:
    m = re.match(r"^(\d+)\.(\d+)\.(\d+)$", version.strip())
    if not m:
        raise RuntimeError(f"invalid version: {version}")
    major, minor, patch = int(m.group(1)), int(m.group(2)), int(m.group(3))
    if bump == "major":
        return f"{major + 1}.0.0"
    if bump == "minor":
        return f"{major}.{minor + 1}.0"
    if bump == "patch":
        return f"{major}.{minor}.{patch + 1}"
    return version


def _group(commits: List[ParsedCommit]) -> Dict[str, List[ParsedCommit]]:
    groups: Dict[str, List[ParsedCommit]] = {"breaking": [], "feat": [], "fix": [], "perf": [], "refactor": [], "docs": [], "ci": [], "build": [], "test": [], "chore": [], "other": []}
    for c in commits:
        if c.breaking:
            groups["breaking"].append(c)
        if c.type in groups:
            groups[c.type].append(c)
        else:
            groups["other"].append(c)
    return groups


def _contributors(prev_tag: Optional[str]) -> List[str]:
    rev_range = f"{prev_tag}..HEAD" if prev_tag else "HEAD"
    out = _run(["git", "shortlog", "-sne", rev_range]).strip().splitlines()
    names: List[str] = []
    for line in out:
        line = line.strip()
        if not line:
            continue
        m = re.match(r"^\s*\d+\s+(.+?)(?:\s+<.+>)?$", line)
        if m:
            names.append(m.group(1).strip())
    seen = set()
    uniq = []
    for n in names:
        if n in seen:
            continue
        seen.add(n)
        uniq.append(n)
    return uniq


def _md_line(c: ParsedCommit) -> str:
    short = c.sha[:7]
    return f"- {c.subject} ({short})"


def build_notes(prev_tag: Optional[str], new_tag: str, commits: List[ParsedCommit]) -> str:
    template_path = os.environ.get("RELEASE_NOTES_TEMPLATE", "").strip()
    groups = _group(commits)
    lines: List[str] = []
    if groups["breaking"]:
        lines.append("### 破坏性变更")
        lines.extend([_md_line(c) for c in groups["breaking"]])
        lines.append("")
    if groups["feat"]:
        lines.append("### 新功能")
        lines.extend([_md_line(c) for c in groups["feat"]])
        lines.append("")
    if groups["fix"]:
        lines.append("### 修复")
        lines.extend([_md_line(c) for c in groups["fix"]])
        lines.append("")
    optim = groups["perf"] + groups["refactor"]
    if optim:
        lines.append("### 优化")
        lines.extend([_md_line(c) for c in optim])
        lines.append("")
    misc = groups["docs"] + groups["ci"] + groups["build"] + groups["test"] + groups["chore"] + groups["other"]
    if misc:
        lines.append("### 其他")
        lines.extend([_md_line(c) for c in misc])
        lines.append("")

    prev = prev_tag or ""
    repo = os.environ.get("GITHUB_REPOSITORY", "")
    contrib = _contributors(prev_tag)
    compare_url = f"https://github.com/{repo}/compare/{prev}...{new_tag}" if repo and prev_tag else ""
    sections_md = "\n".join(lines).strip() + "\n"
    if template_path:
        t = Path(template_path).read_text(encoding="utf-8", errors="ignore")
        version = new_tag[1:] if new_tag.startswith("v") else new_tag
        rendered = t
        rendered = rendered.replace("{{tag}}", new_tag)
        rendered = rendered.replace("{{version}}", version)
        rendered = rendered.replace("{{previous_tag}}", prev_tag or "")
        rendered = rendered.replace("{{compare_url}}", compare_url)
        rendered = rendered.replace("{{notes}}", sections_md.strip())
        rendered = rendered.replace("{{contributors}}", ", ".join(contrib))
        return rendered.strip() + "\n"

    out = sections_md.strip()
    if compare_url:
        out += f"\n\n**对比**: {compare_url}"
    if contrib:
        out += "\n\n**贡献者**: " + ", ".join(contrib)
    return out.strip() + "\n"


def main() -> int:
    prev_tag = _get_latest_tag()
    prev_ver = prev_tag[1:] if prev_tag else "0.0.0"
    commits = _collect_commits(prev_tag)
    bump = _bump_kind(commits)
    new_ver = _inc_version(prev_ver, bump) if bump != "none" else prev_ver
    new_tag = f"v{new_ver}"
    notes = build_notes(prev_tag, new_tag, commits) if bump != "none" else ""

    out_path = os.environ.get("OUT_JSON", "").strip()
    payload = {
        "previous_tag": prev_tag or "",
        "previous_version": prev_ver,
        "bump": bump,
        "new_version": new_ver,
        "new_tag": new_tag,
        "notes_markdown": notes,
        "commit_count": len(commits),
    }
    if out_path:
        os.makedirs(os.path.dirname(out_path), exist_ok=True)
        with open(out_path, "w", encoding="utf-8") as f:
            json.dump(payload, f, ensure_ascii=False, indent=2)

    if os.environ.get("GITHUB_OUTPUT"):
        with open(os.environ["GITHUB_OUTPUT"], "a", encoding="utf-8") as f:
            f.write(f"previous_tag={payload['previous_tag']}\n")
            f.write(f"bump={payload['bump']}\n")
            f.write(f"new_version={payload['new_version']}\n")
            f.write(f"new_tag={payload['new_tag']}\n")
            f.write(f"commit_count={payload['commit_count']}\n")

    sys.stdout.write(json.dumps(payload, ensure_ascii=False) + "\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
