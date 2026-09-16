#!/usr/bin/env bash
#
# install-hooks.sh — install the pre-push docs + OKF + table-format + harvest hooks.
#
# Usage:
#     bash scripts/install-hooks.sh
#
# Sets core.hooksPath to .githooks/ (repo-local, tracked in git, shared
# with everyone). The hook runs documentation hygiene, the OKF bundle
# conformance check, the table/frontmatter format gate (no machine-local
# paths, canonical frontmatter, well-formed markdown tables), and the
# archive/README coupling check. Run once per clone; re-run to upgrade.

set -eu

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT"

HOOK=".githooks/pre-push"
TARGET=".git/hooks/pre-push"

[ -f "$HOOK" ] || { echo "error: $HOOK not found" >&2; exit 1; }

# Use core.hooksPath so the hook stays versioned in the repo.
git config core.hooksPath .githooks
echo "hooksPath set to .githooks (repo-local)"

[ -x "$HOOK" ] || chmod +x "$HOOK"
[ -f "$TARGET" ] && echo "note: legacy hook $TARGET exists but is superseded by core.hooksPath"

echo "pre-push hook installed. Verify with: git push --dry-run"
