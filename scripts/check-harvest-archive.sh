#!/usr/bin/env bash
#
# check-harvest-archive.sh — require README updates when new harvest notes are pushed.
#
# This script is intended to run as a pre-push hook. Git supplies one line per
# ref on stdin:
#
#   <local-ref> SP <local-sha> SP <remote-ref> SP <remote-sha>
#
# The check compares each pushed tip with its remote counterpart. It therefore
# covers ordinary branch updates, new branches, tags, force pushes, and
# multi-ref pushes without treating the current checkout as the push target.
#
# Exit code 0 = all good; non-zero = fix and retry.

set -u

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT"

FAIL=0
ZERO_SHA="0000000000000000000000000000000000000000"
EMPTY_TREE="4b825dc642cb6eb9a060e54bf8d69288fbee4904"
REPORT_PATHSPEC="knowledge/harvest/*.md"

declare -A NEW_REPORTS=()
declare -A README_CHANGED=()

say()  { printf '\033[1;36m[harvest]\033[0m %s\n' "$*"; }
ok()   { printf '\033[1;32m  ✔\033[0m %s\n' "$*"; }
fail() { printf '\033[1;31m  ✘\033[0m %s\n' "$*"; FAIL=$((FAIL+1)); }
warn() { printf '\033[1;33m  ⚠\033[0m %s\n' "$*"; }

validate_report() {
    local commit="$1"
    local file="$2"
    local content=""
    local nonblank=0

    NEW_REPORTS["$file"]=1

    if ! content="$(git show "$commit:$file" 2>/dev/null)"; then
        fail "$file: new harvest note is not present at pushed tip $commit"
        return 0
    fi
    if [ -z "${content//[[:space:]]/}" ]; then
        fail "$file: new harvest note is empty at $commit"
    fi
    if printf '%s\n' "$content" | grep -Eqi '^[[:space:]]*placeholder[[:space:]]*$'; then
        fail "$file: new harvest note is only a placeholder at $commit"
    fi
    if ! printf '%s\n' "$content" | grep -Eq '^#[[:space:]]'; then
        fail "$file: new harvest note has no Markdown heading at $commit"
    fi
    nonblank="$(printf '%s\n' "$content" | grep -vc '^[[:space:]]*$' || true)"
    if [ "$nonblank" -lt 3 ]; then
        fail "$file: new harvest note has only $nonblank non-blank line(s) at $commit"
    fi
}

check_ref() {
    local local_ref="$1"
    local local_sha="$2"
    local remote_ref="$3"
    local remote_sha="$4"
    local local_commit=""
    local remote_commit=""
    local range=""
    local report_diff=""
    local readme_diff=""
    local file=""

    if [ "$local_sha" = "$ZERO_SHA" ]; then
        warn "skip deleted ref $local_ref"
        return 0
    fi
    if ! local_commit="$(git rev-parse "$local_sha^{commit}" 2>/dev/null)"; then
        fail "cannot resolve pushed commit for $local_ref ($local_sha)"
        return 0
    fi

    if [ "$remote_sha" = "$ZERO_SHA" ]; then
        # A new ref has no remote tip. Prefer the closest existing mainline
        # ancestor so a branch copied from main is not treated as an entirely
        # new archive batch. Fall back to the empty tree for orphan history.
        if [ -n "$(git rev-parse --verify -q origin/main 2>/dev/null || true)" ]; then
            remote_commit="$(git merge-base "$local_commit" origin/main 2>/dev/null || true)"
        fi
        if [ -z "$remote_commit" ] && [ -n "$(git rev-parse --verify -q main 2>/dev/null || true)" ]; then
            remote_commit="$(git merge-base "$local_commit" main 2>/dev/null || true)"
        fi
        if [ -n "$remote_commit" ]; then
            range="$remote_commit..$local_commit"
        else
            range="$EMPTY_TREE..$local_commit"
        fi
    else
        if ! remote_commit="$(git rev-parse "$remote_sha^{commit}" 2>/dev/null)"; then
            fail "cannot resolve remote commit for $remote_ref ($remote_sha)"
            return 0
        fi
        range="$remote_commit..$local_commit"
    fi

    if ! report_diff="$(git diff --name-only --diff-filter=ACR "$range" -- "$REPORT_PATHSPEC" 2>/dev/null)"; then
        fail "cannot inspect harvest report changes for $local_ref ($range)"
        return 0
    fi
    while IFS= read -r file; do
        [ -n "$file" ] || continue
        validate_report "$local_commit" "$file"
    done <<< "$report_diff"

    if ! readme_diff="$(git diff --name-only "$range" -- README.md README.en.md 2>/dev/null)"; then
        fail "cannot inspect README changes for $local_ref ($range)"
        return 0
    fi
    while IFS= read -r file; do
        [ -n "$file" ] || continue
        case "$file" in
            README.md|README.en.md)
                if git cat-file -e "$local_commit:$file" 2>/dev/null; then
                    README_CHANGED["$file"]=1
                else
                    fail "$file: README is removed at pushed tip $local_commit"
                fi
                ;;
        esac
    done <<< "$readme_diff"
}

say "checking outgoing harvest archives and README updates"

check_head_manually() {
    # Manual invocation: inspect HEAD against origin/main when available,
    # otherwise against the parent commit. Pre-push stdin takes precedence.
    if git rev-parse --verify -q origin/main >/dev/null 2>&1; then
        check_ref "HEAD" "$(git rev-parse HEAD)" "origin/main" "$(git rev-parse origin/main)"
    elif git rev-parse --verify -q HEAD~1 >/dev/null 2>&1; then
        check_ref "HEAD" "$(git rev-parse HEAD)" "HEAD~1" "$(git rev-parse HEAD~1)"
    else
        warn "no pre-push refs and no base commit; nothing to inspect"
    fi
}

if [ -t 0 ]; then
    check_head_manually
else
    mapfile -t ref_lines
    if [ "${#ref_lines[@]}" -eq 0 ]; then
        check_head_manually
    else
        for line in "${ref_lines[@]}"; do
            IFS=' ' read -r local_ref local_sha remote_ref remote_sha extra <<< "$line"
            if [ -z "${local_ref:-}" ] || [ -z "${local_sha:-}" ] || [ -z "${remote_ref:-}" ] || [ -z "${remote_sha:-}" ] || [ -n "${extra:-}" ]; then
                fail "malformed pre-push ref line: $line"
                continue
            fi
            check_ref "$local_ref" "$local_sha" "$remote_ref" "$remote_sha"
        done
    fi
fi

report_count="${#NEW_REPORTS[@]}"
if [ "$report_count" -eq 0 ]; then
    ok "no new $REPORT_PATHSPEC files in outgoing commits"
else
    printf '\033[1;36m[harvest]\033[0m new archive(s):\n'
    mapfile -t report_list < <(printf '%s\n' "${!NEW_REPORTS[@]}" | sort)
    for file in "${report_list[@]}"; do
        printf '  - %s\n' "$file"
    done

    if [ "${README_CHANGED[README.md]:-0}" -eq 0 ] || [ "${README_CHANGED[README.en.md]:-0}" -eq 0 ]; then
        missing=()
        [ "${README_CHANGED[README.md]:-0}" -eq 0 ] && missing+=("README.md")
        [ "${README_CHANGED[README.en.md]:-0}" -eq 0 ] && missing+=("README.en.md")
        fail "new harvest knowledge requires both README.md and README.en.md to change in the same push (missing: ${missing[*]})"
    else
        ok "README.md and README.en.md both change with new harvest knowledge"
    fi
fi

printf '\n'
if [ "$FAIL" -gt 0 ]; then
    printf '\033[1;31m✗ %d harvest archive problem(s) found — fix and retry.\033[0m\n' "$FAIL"
    exit 1
fi
printf '\033[1;32m✓ harvest archive check passed.\033[0m\n'
exit 0
