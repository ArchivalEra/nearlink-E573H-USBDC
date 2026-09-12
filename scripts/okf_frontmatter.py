#!/usr/bin/env python3
"""okf_frontmatter.py — one-shot OKF bundle bootstrap/maintenance helper.

Used by the knowledge-restructure migration (wayfinder effort, ticket 05):
  1. inject conformant YAML frontmatter into concept documents that lack it
     (type, title from H1, language auto-detect, created from first commit,
     tags list);
  2. regenerate index.md files (bundle root carries okf_version) and log.md
     per OKF v0.2 §8/§9.

Idempotent: documents that already have frontmatter are left untouched.
Manual run:  python3 scripts/okf_frontmatter.py [--dry-run]
"""
from __future__ import annotations

import argparse
import re
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
BUNDLE = REPO / "knowledge"
BUNDLE_NAME = "NearLink Knowledge Bundle"

SECTIONS = [  # (subdir, type label, heading, description)
    ("harvest", "harvest", "Harvest Reports",
     "Digestions of external NearLink/SparkLink ecosystem repositories."),
    ("intel", "intel", "Intel Deep-Dives",
     "Protocol, chip, SDK, and driver research documents."),
    ("decisions", "decision", "Decisions",
     "Recorded rulings and architecture decisions."),
]

CJK = re.compile(r"[\u4e00-\u9fff\u3400-\u4dbf]")
H1 = re.compile(r"^#\s+(.+?)\s*$", re.MULTILINE)


def has_frontmatter(text: str) -> bool:
    return text.startswith("---\n")


def first_commit_date(path: Path) -> str:
    out = subprocess.run(
        ["git", "log", "--diff-filter=A", "--format=%ad", "--date=short", "--", str(path)],
        capture_output=True, text=True, cwd=REPO,
    ).stdout.strip().splitlines()
    return out[-1].strip() if out else "2026-09-12"


def inject_frontmatter(path: Path, ctype: str, dry: bool) -> bool:
    text = path.read_text(encoding="utf-8")
    if has_frontmatter(text):
        return False
    m = H1.search(text)
    title = m.group(1).strip() if m else path.stem
    language = "zh" if CJK.search(text) else "en"
    created = first_commit_date(path)
    block = (
        f"---\ntype: {ctype}\ntitle: {title}\nlanguage: {language}\n"
        f"created: {created}\ntags: []\n---\n\n"
    )
    if not dry:
        path.write_text(block + text, encoding="utf-8")
    return True


def concept_entry(path: Path) -> tuple[str, str]:
    text = path.read_text(encoding="utf-8")
    m = H1.search(text)
    title = m.group(1).strip() if m else path.stem
    rel = path.relative_to(path.parent).as_posix()
    return title, rel


def write_index(directory: Path, ctype: str, heading: str, dry: bool) -> None:
    entries = []
    for p in sorted(directory.glob("*.md")):
        if p.name == "index.md":
            continue
        title, rel = concept_entry(p)
        entries.append(f"* [{title}]({rel}) - {ctype} concept ({p.stat().st_size // 1024} KB)")
    body = f"# {heading}\n\n" + ("\n".join(entries) if entries else "* (empty)\n") + "\n"
    if not dry:
        (directory / "index.md").write_text(body, encoding="utf-8")


def write_root_index_real(dry: bool) -> None:
    lines = [
        "---",
        'okf_version: "0.2"',
        "---",
        "",
        f"# {BUNDLE_NAME}",
        "",
        "OKF v0.2 knowledge bundle for the WS73 tri-mode USB dongle effort.",
        "Language tags live in each concept's frontmatter (`language: zh|en`);",
        "new documents are English-only. Wire ground truth lives in code under",
        "`assets/stack/ssap` — knowledge documents cite it, never restate it.",
        "",
    ]
    for subdir, _ctype, heading, desc in SECTIONS:
        count = len([p for p in (BUNDLE / subdir).glob("*.md") if p.name != "index.md"])
        lines.append(f"# {heading}")
        lines.append("")
        lines.append(f"* [{heading}]({subdir}/) - {desc} ({count} concepts)")
        lines.append("")
    if not dry:
        (BUNDLE / "index.md").write_text("\n".join(lines), encoding="utf-8")


def write_log(dry: bool) -> None:
    n_harvest = len([p for p in (BUNDLE / "harvest").glob("*.md") if p.name != "index.md"])
    n_intel = len([p for p in (BUNDLE / "intel").glob("*.md") if p.name != "index.md"])
    body = (
        "# Knowledge Bundle Update Log\n\n"
        "## 2026-09-12\n"
        f"* **Creation**: Initialized the OKF v0.2 bundle: migrated {n_harvest} harvest "
        f"reports and {n_intel} intel documents from the legacy layout, plus the "
        "knowledge/assets split decision.\n"
        f"* **Migration**: Lossless relocation from `.scratch/nearlink-driver/lab-notes/`, "
        "`.scratch/nearlink-driver/assets/`, and `docs/`; file names preserved; "
        "frontmatter injected by `scripts/okf_frontmatter.py`.\n"
    )
    if not dry:
        (BUNDLE / "log.md").write_text(body, encoding="utf-8")


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--dry-run", action="store_true")
    args = ap.parse_args()

    injected = 0
    for subdir, ctype, heading, _desc in SECTIONS:
        directory = BUNDLE / subdir
        if not directory.is_dir():
            print(f"missing directory: {directory}", file=sys.stderr)
            return 2
        for p in sorted(directory.glob("*.md")):
            if p.name == "index.md":
                continue
            if inject_frontmatter(p, ctype, args.dry_run):
                injected += 1
        write_index(directory, ctype, heading, args.dry_run)

    write_root_index_real(args.dry_run)
    write_log(args.dry_run)
    print(f"frontmatter injected: {injected}; indexes/log regenerated "
          f"{'(dry-run)' if args.dry_run else ''}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
