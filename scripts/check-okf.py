#!/usr/bin/env python3
"""check-okf.py — OKF v0.2 conformance + repo-specific quality gate.

Implements OKF v0.2 §11's three conformance rules (MUST), plus
repo-specific quality checks layered on top (spec-tolerated violations
stay tolerated unless they break THIS repo's own contracts):

  M1  every non-reserved .md under knowledge/ parses with YAML frontmatter
  M2  every frontmatter carries a non-empty `type`
  M3  reserved files follow §8/§9 (index.md: no frontmatter except a
      bundle-root okf_version; log.md: ISO 8601 date headings)
  R1  (repo) files declaring `language: en` contain no CJK characters
  R2  (repo) bundle has a root index.md declaring okf_version
  R3  (repo) README index references into knowledge/ resolve

Deliberately NOT enforced (§11 tolerated): broken cross-links, unknown
type values, unknown frontmatter keys, missing optional fields, missing
sub-index files.

Usage: python3 scripts/check-okf.py            # bundle at knowledge/
       python3 scripts/check-okf.py <bundle>   # custom bundle path
Exit: 0 = pass; 1 = problems; 2 = usage/IO error.
"""
from __future__ import annotations

import re
import sys
from datetime import date
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
BUNDLE_DEFAULT = REPO / "knowledge"

RESERVED = {"index.md", "log.md"}
CJK = re.compile(r"[\u4e00-\u9fff\u3400-\u4dbf]")
ISO_DATE = re.compile(r"^## (\d{4}-\d{2}-\d{2})\s*$")

errors: list[str] = []


def fail(msg: str) -> None:
    errors.append(msg)


def parse_frontmatter(text: str) -> dict | None:
    """Minimal YAML-frontmatter parser for the flat key/value shape OKF uses."""
    if not text.startswith("---\n"):
        return None
    m = re.match(r"^---\n(.*?)\n(---\n|$)", text, re.S)
    if not m:
        return None
    fields: dict[str, str] = {}
    for line in m.group(1).splitlines():
        kv = re.match(r"^([A-Za-z_][A-Za-z0-9_-]*):\s*(.*)$", line)
        if kv:
            fields[kv.group(1)] = kv.group(2).strip()
    return fields


def main() -> int:
    bundle = Path(sys.argv[1]) if len(sys.argv) > 1 else BUNDLE_DEFAULT
    if not bundle.is_dir():
        print(f"✗ bundle not a directory: {bundle}", file=sys.stderr)
        return 2

    md_files = sorted(bundle.rglob("*.md"))
    if not md_files:
        fail("bundle contains no .md files")

    # M1+M2: frontmatter and non-empty type on every non-reserved file
    for p in md_files:
        text = p.read_text(encoding="utf-8", errors="replace")
        rel = p.relative_to(REPO).as_posix() if p.is_relative_to(REPO) else str(p)
        if p.name in RESERVED:
            continue
        fm = parse_frontmatter(text)
        if fm is None:
            fail(f"{rel}: no parseable YAML frontmatter (OKF §11 rule 1)")
            continue
        if not fm.get("type", "").strip():
            fail(f"{rel}: frontmatter lacks non-empty `type` (OKF §11 rule 2)")
        # R1: language: en files must be CJK-free
        if fm.get("language", "").strip().lower() == "en":
            hits = CJK.findall(text.split("---\n", 2)[-1])
            if hits:
                fail(f"{rel}: declares language: en but has {len(hits)} CJK characters")

    # M3a: index.md files — no frontmatter except bundle-root okf_version
    for p in md_files:
        if p.name != "index.md":
            continue
        text = p.read_text(encoding="utf-8", errors="replace")
        rel = p.relative_to(REPO).as_posix()
        if text.startswith("---\n"):
            fm = parse_frontmatter(text)
            is_root = p == bundle / "index.md"
            if not is_root:
                fail(f"{rel}: index.md must not carry frontmatter (OKF §8)")
            elif set(fm or {}) - {"okf_version"}:
                fail(f"{rel}: bundle-root index.md frontmatter may only carry okf_version (OKF §12)")
            elif not (fm or {}).get("okf_version", "").strip('"'):
                fail(f"{rel}: okf_version empty")

    # M3b: log.md — ISO 8601 date headings
    for p in bundle.rglob("log.md"):
        text = p.read_text(encoding="utf-8", errors="replace")
        rel = p.relative_to(REPO).as_posix()
        headings = [ln for ln in text.splitlines() if ln.startswith("## ")]
        for ln in headings:
            if not ISO_DATE.match(ln.strip()):
                fail(f"{rel}: log heading not ISO 8601 (OKF §9): {ln.strip()!r}")

    # R2: root index.md must declare okf_version
    root_index = bundle / "index.md"
    if root_index.is_file():
        fm = parse_frontmatter(root_index.read_text(encoding="utf-8", errors="replace"))
        if not (fm and fm.get("okf_version", "").strip('"')):
            fail("knowledge/index.md: bundle-root index must declare okf_version (OKF §12)")
    else:
        fail("knowledge/index.md missing (bundle root index required by this repo)")

    # R3: every `knowledge/....md` referenced in the READMEs resolves
    for fn in ("README.md", "README.en.md"):
        txt = (REPO / fn).read_text(encoding="utf-8") if (REPO / fn).is_file() else ""
        for ref in set(re.findall(r"`(knowledge/[^`]+\.md)`", txt)):
            if not (REPO / ref).is_file():
                fail(f"{fn}: referenced but missing: {ref}")

    if errors:
        for e in errors:
            print(f"\033[1;31m  ✘\033[0m {e}")
        print(f"\033[1;31m✗ {len(errors)} OKF problem(s) found — fix and retry.\033[0m")
        return 1
    n = sum(1 for p in md_files if p.name not in RESERVED)
    print(f"\033[1;32m✓ OKF check passed: {n} concept(s), conformance M1-M3 + repo quality rules.\033[0m")
    return 0


if __name__ == "__main__":
    sys.exit(main())
