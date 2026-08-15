#!/usr/bin/env bash
#
# check-docs.sh — pre-push documentation hygiene checks for nearlink-E573H-USBDC
#
# Verifies, before every push:
#   1. README.md <-> README.en.md cross-links exist (both directions)
#   2. every file listed in the README doc index tables actually exists
#   3. docs/*.md contain no CJK characters (docs are English-only)
#   4. no accidental large files / binaries staged (whitelist gitignore sanity)
#   5. git identity (user.name / user.email) is configured
#   6. README zh/en bilingual sync — a change to one side must be mirrored
#      in the other (translation trigger)
#
# Exit code 0 = all good; non-zero = fix and retry. Run manually with:
#     bash scripts/check-docs.sh
# Install as a pre-push hook with:
#     bash scripts/install-hooks.sh

set -u

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT"

FAIL=0
WARN=0

say()  { printf '\033[1;36m[check]\033[0m %s\n' "$*"; }
ok()   { printf '\033[1;32m  ✔\033[0m %s\n' "$*"; }
fail() { printf '\033[1;31m  ✘\033[0m %s\n' "$*"; FAIL=$((FAIL+1)); }
warn() { printf '\033[1;33m  ⚠\033[0m %s\n' "$*"; WARN=$((WARN+1)); }

# ---------------------------------------------------------------------------
say "1/5 README cross-links"
# ---------------------------------------------------------------------------
[ -f README.md ]  || fail "README.md missing"
[ -f README.en.md ] || fail "README.en.md missing"
if [ -f README.md ] && [ -f README.en.md ]; then
    grep -q 'README\.en\.md' README.md  && ok "README.md -> README.en.md link present" \
                                        || fail "README.md lacks link to README.en.md"
    grep -q 'README\.md' README.en.md  && ok "README.en.md -> README.md link present" \
                                        || fail "README.en.md lacks link to README.md"
fi

# ---------------------------------------------------------------------------
say "2/5 doc index completeness (files referenced by the README index tables)"
# ---------------------------------------------------------------------------
# Pull every markdown path referenced in the README index tables (python3 —
# robust against shell quoting surprises around backticks).
if ! command -v python3 >/dev/null 2>&1; then
    fail "python3 required for check 2/5"
else
    REFERENCED="$(python3 - <<'PY'
import re
refs = set()
for fn in ("README.md", "README.en.md"):
    try:
        txt = open(fn, encoding="utf-8").read()
    except OSError:
        continue
    refs.update(re.findall(r'`([^`]+\.md)`', txt))
print("\n".join(sorted(refs)))
PY
)"
    if [ -z "$REFERENCED" ]; then
        fail "could not parse any *.md references from README index tables"
    else
        for f in $REFERENCED; do
            [ -f "$f" ] && ok "referenced: $f" || fail "referenced but missing: $f"
        done
    fi
fi

# ---------------------------------------------------------------------------
say "3/5 docs are English-only (no CJK)"
# ---------------------------------------------------------------------------
CJK_FILES="$(grep -rlP '[\x{4e00}-\x{9fff}\x{3400}-\x{4dbf}]' docs/ 2>/dev/null || true)"
if [ -z "$CJK_FILES" ]; then
    ok "docs/ contains no CJK characters"
else
    for f in $CJK_FILES; do
        cjk=$(grep -oP '[\x{4e00}-\x{9fff}\x{3400}-\x{4dbf}]' "$f" | wc -l)
        fail "$f: $cjk CJK chars (docs/ must be English)"
    done
fi

# ---------------------------------------------------------------------------
say "4/5 whitelist gitignore sanity (no accidental binaries staged)"
# ---------------------------------------------------------------------------
STAGED="$(git diff --cached --name-only 2>/dev/null || true)"
BIG=""
for f in $STAGED; do
    case "$f" in
        *.zip|*.bin|*.ko|*.o|*.a|*.so|*.img|*.elf|*.hex|*.exe|*.tar|*.gz|*.7z)
            BIG="$BIG $f" ;;
    esac
done
if [ -z "$BIG" ]; then
    ok "no binary/archive files staged"
else
    for f in $BIG; do
        fail "staged binary/archive should not be committed: $f (use git reset HEAD <file>)"
    done
fi

# ---------------------------------------------------------------------------
say "5/5 git identity"
# ---------------------------------------------------------------------------
NAME="$(git config user.name 2>/dev/null || true)"
EMAIL="$(git config user.email 2>/dev/null || true)"
[ -n "$NAME" ]  && ok "user.name = $NAME"  || warn "user.name not set"
[ -n "$EMAIL" ] && ok "user.email = $EMAIL" || warn "user.email not set"

# ---------------------------------------------------------------------------
say "6/6 README bilingual sync (zh/en must change together)"
# ---------------------------------------------------------------------------
# Trigger: if one README changed in the outgoing commits but the other didn't,
# remind to translate the counterpart. Compares the push range
# (origin/main..HEAD when a remote exists, else the last commit).
BASE="$(git rev-parse --verify -q origin/main 2>/dev/null || \
        git rev-parse --verify -q HEAD~1 2>/dev/null || true)"
if [ -n "$BASE" ]; then
    ZH_CHANGED="$(git diff --name-only "$BASE..HEAD" -- README.md 2>/dev/null | wc -l)"
    EN_CHANGED="$(git diff --name-only "$BASE..HEAD" -- README.en.md 2>/dev/null | wc -l)"
    if [ "$ZH_CHANGED" -gt 0 ] && [ "$EN_CHANGED" -eq 0 ]; then
        fail "README.md changed but README.en.md did not — update the English translation (or --no-verify to force)"
    elif [ "$EN_CHANGED" -gt 0 ] && [ "$ZH_CHANGED" -eq 0 ]; then
        fail "README.en.md changed but README.md did not — update the Chinese translation (or --no-verify to force)"
    else
        ok "README zh/en pair consistent (both $([ "$ZH_CHANGED" -gt 0 ] && echo "changed" || echo "unchanged"))"
    fi
else
    warn "no base ref to diff for README sync check"
fi

# ---------------------------------------------------------------------------
printf '\n'
if [ "$FAIL" -gt 0 ]; then
    printf '\033[1;31m✗ %d problem(s) found — fix and retry.\033[0m\n' "$FAIL"
    exit 1
fi
printf '\033[1;32m✓ all checks passed%s.\033[0m\n' "$([ "$WARN" -gt 0 ] && echo " ($WARN warning(s))" || true)"
exit 0
