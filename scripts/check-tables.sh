#!/usr/bin/env bash
# check-tables.sh — knowledge/docs format gate.
# Bans pushes with: machine-local paths, non-canonical frontmatter ("header table"),
# malformed markdown tables. Pure format checks; content quality stays with authors.
set -u

fail=0
note() { printf '  %b\n' "$1"; }
bad()  { note "✘ $1"; fail=1; }

# --- collect tracked markdown (frontmatter rules apply to knowledge/ only) ---
# reserved files (auto-generated indexes) carry no frontmatter by design
mapfile -t FILES < <(git ls-files 'knowledge/*.md' 'knowledge/**/*.md' 'docs/**/*.md' 2>/dev/null \
  | grep -vE '(^|/)(index|log|README)\.md$' | sort -u)

# ---------- 1. no machine-local absolute paths ----------
for f in "${FILES[@]}"; do
  [ -f "$f" ] || continue
  if grep -nE '/(mnt|home|root|Users)/' "$f" | grep -q .; then
    bad "$f: machine-local path(s) found (use repo-relative paths or upstream URLs):"
    grep -nE '/(mnt|home|root|Users)/' "$f" | head -3 | sed 's/^/      /'
  fi
done

# ---------- 2. canonical frontmatter ("header table") ----------
ORDER=(type title language created tags sources trust stale_after)
is_date() { [[ "$1" =~ ^[0-9]{4}-[0-9]{2}-[0-9]{2}$ ]]; }

for f in "${FILES[@]}"; do
  [ -f "$f" ] || continue
  case "$f" in knowledge/*) ;; *) continue ;; esac   # docs/ is not an OKF bundle: no frontmatter rule
  head -1 "$f" | grep -q '^---$' || { bad "$f: missing frontmatter fence"; continue; }
  fm=$(awk 'NR==1{next} /^---$/{exit} {print}' "$f")

  # field order: first occurrences must appear in canonical order
  prev=-1
  for i in "${!ORDER[@]}"; do
    ln=$(grep -n "^${ORDER[$i]}:" <<<"$fm" | head -1 | cut -d: -f1)
    if [ -z "$ln" ]; then bad "$f: frontmatter field '${ORDER[$i]}' missing"; continue; fi
    if [ "$ln" -le "$prev" ]; then bad "$f: frontmatter field '${ORDER[$i]}' out of order"; fi
    prev=$ln
  done

  # title must be double-quoted
  title=$(grep -m1 '^title:' <<<"$fm")
  [[ "$title" =~ ^title:\ \" ]] || bad "$f: title not double-quoted"

  # tags non-empty
  tags=$(grep -m1 '^tags:' <<<"$fm" | sed 's/^tags:[[:space:]]*//')
  [[ -n "$tags" && "$tags" != "[]" && "$tags" != '""' ]] || bad "$f: tags empty"

  # sources non-empty
  grep -qE '^sources:' <<<"$fm" && grep -qE '^[[:space:]]+-[[:space:]]+' <<<"$fm" \
    || bad "$f: sources missing/empty"

  # trust whitelist
  trust=$(grep -m1 '^trust:' <<<"$fm" | awk '{print $2}')
  case "$trust" in A|B|C) ;; *) bad "$f: trust '$trust' not in {A,B,C}";; esac

  # date fields
  created=$(grep -m1 '^created:' <<<"$fm" | awk '{print $2}')
  is_date "$created" || bad "$f: created '$created' not YYYY-MM-DD"
  stale=$(grep -m1 '^stale_after:' <<<"$fm" | awk '{print $2}')
  is_date "$stale" || bad "$f: stale_after '$stale' not YYYY-MM-DD"

  # language whitelist
  lang=$(grep -m1 '^language:' <<<"$fm" | awk '{print $2}')
  case "$lang" in en|zh) ;; *) bad "$f: language '$lang' not en|zh";; esac

  # CJK ban for language: en (mirror of check-okf R1, cheap early signal)
  if [ "$lang" = "en" ] && grep -qP '[\x{4e00}-\x{9fff}]' "$f"; then
    bad "$f: declares language: en but contains CJK characters"
  fi
done

# ---------- 3. markdown table well-formedness (body tables) ----------
# A table block = consecutive lines starting with '|'. Expect:
#   header row |...|, separator row |---|..., all rows same column count.
check_table_block() {
  local f="$1"
  shift
  local -a rows=("$@")
  local ncols_header ncols_sep i n
  # count column separators only: drop escaped pipes (\|) and pipes inside `code` spans
  count_pipes() { sed 's/\\|//g; s/`[^`]*`//g' <<<"$1" | grep -o '|' | wc -l; }
  ncols_header=$(count_pipes "${rows[0]}")
  # find separator row
  sep=-1
  for i in "${!rows[@]}"; do
    if [[ "${rows[$i]}" =~ ^\|[[:space:]]*:?- ]]; then sep=$i; break; fi
  done
  if [ "$sep" -ne 1 ]; then
    bad "$f:$block: table lacks separator row on line 2 of the block"
    return
  fi
  ncols_sep=$(count_pipes "${rows[1]}")
  [ "$ncols_sep" -eq "$ncols_header" ] || { bad "$f:$block: separator columns ($ncols_sep) != header columns ($ncols_header)"; return; }
  for i in "${!rows[@]}"; do
    n=$(count_pipes "${rows[$i]}")
    [ "$n" -eq "$ncols_header" ] || { bad "$f:$block: row $((i+1)) has $n pipes, header has $ncols_header"; return; }
  done
}

for f in "${FILES[@]}"; do
  [ -f "$f" ] || continue
  # strip fenced code blocks so example tables inside ``` ``` don't trip the checker
  tmp=$(mktemp)
  awk '/^```/{inblk=!inblk; next} !inblk{print}' "$f" > "$tmp"
  lineno=0; block_start=0
  declare -a rows=()
  while IFS= read -r line; do
    lineno=$((lineno+1))
    if [[ "$line" =~ ^\| ]]; then
      if [ "$block_start" -eq 0 ]; then block_start=$lineno; rows=(); fi
      rows+=("$line")
    else
      if [ "$block_start" -ne 0 ]; then
        check_table_block "$f:$block_start" "${rows[@]}"
        block_start=0
      fi
    fi
  done < "$tmp"
  [ "$block_start" -ne 0 ] && check_table_block "$f:$block_start" "${rows[@]}"
  rm -f "$tmp"
done

# ---------- verdict ----------
if [ "$fail" -eq 0 ]; then
  note "\e[32m✓ table/format check passed (${#FILES[@]} files).\e[0m"
else
  note "\e[31m✗ table/format check FAILED — fix and retry.\e[0m"
fi
exit $fail
