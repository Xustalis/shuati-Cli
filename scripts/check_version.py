#!/usr/bin/env python3
import os
import re
import sys
import json
from pathlib import Path

def get_cmake_version(txt):
    m = re.search(r'project\([^)]+VERSION\s+([0-9]+\.[0-9]+\.[0-9]+)', txt)
    return m.group(1) if m else None

def get_vcpkg_version(txt):
    try:
        obj = json.loads(txt)
        return obj.get('version-string')
    except:
        return None

def get_readme_version(txt):
    m = re.search(r'version-([0-9]+\.[0-9]+\.[0-9]+)-green', txt)
    return m.group(1) if m else None

def main():
    repo_root = Path(__file__).parent.parent.resolve()
    versions = {}
    errors = []

    # 1. CMakeLists.txt
    cmake_path = repo_root / 'CMakeLists.txt'
    if cmake_path.exists():
        v = get_cmake_version(cmake_path.read_text(encoding='utf-8', errors='ignore'))
        if v:
            versions['CMakeLists.txt'] = v
        else:
            errors.append(f"Could not find version in {cmake_path.name}")

    # 2. vcpkg.json
    vcpkg_path = repo_root / 'vcpkg.json'
    if vcpkg_path.exists():
        v = get_vcpkg_version(vcpkg_path.read_text(encoding='utf-8', errors='ignore'))
        if v:
            versions['vcpkg.json'] = v
        else:
            errors.append(f"Could not find version in {vcpkg_path.name}")

    # 3. README.md
    readme_path = repo_root / 'README.md'
    if readme_path.exists():
        v = get_readme_version(readme_path.read_text(encoding='utf-8', errors='ignore'))
        if v:
            versions['README.md'] = v
        else:
            print("Warning: Could not find version badge in README.md")

    if not versions:
        print("Error: No versions found in any files.")
        sys.exit(1)

    unique_versions = set(versions.values())
    print("Found versions:")
    for file, ver in versions.items():
        print(f"  {file}: {ver}")

    if len(unique_versions) > 1:
        print("\nError: Version mismatch detected!")
        for err in errors:
            print(f"  {err}")
        sys.exit(1)

    print("\nSuccess: All versions match!")
    sys.exit(0)

if __name__ == '__main__':
    main()
