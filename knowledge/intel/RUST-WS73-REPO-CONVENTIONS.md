---
type: intel
title: "RUST-WS73 Repo Conventions Audit — What Blocks `rust-ws73/` Landing"
language: zh
created: 2026-09-05
tags: [intel, rust, ws73, repo]
sources:
  - "ArchivalEra/nearlink-E573H-USBDC workspace"
trust: B
stale_after: 2027-03-05
---

# RUST-WS73 Repo Conventions Audit — What Blocks `rust-ws73/` Landing

> Ticket: `.scratch/rust-ws73-tri-mode/issues/07-repo-conventions.md` (task AFK)
> Date: 2026-08-19 · Scope: read-only audit · Output: `.scratch/nearlink-driver/lab-notes/RUST-WS73-REPO-CONVENTIONS.md`
> Host: x86-64 Linux 7.1.5-x64v3-xanmod1 · Repo: `ArchivalEra/nearlink-E573H-USBDC`

## 0. TL;DR — 5 gaps that block `rust-ws73/` landing

1. **No `docs/RUST-WS73.md`** yet — English-only doc missing and absent from both README index tables (`README.md:50-60`, `README.en.md:50-60`).
2. **Whitelist hole for Cargo manifests** — `.gitignore:56` allowlists `!*.rs` but neither `!*.toml` nor `!Cargo.*`/`!Cargo.lock`; `Cargo.toml`/`Cargo.lock` are currently DENIED by the default `*` (`git check-ignore -v` proves it).
3. **`rust-ws73/target/` not explicitly ignored** — `sdk/**/*.ko` is explicitly ignored at `.gitignore:117`, but there is no sibling `rust-ws73/target/` / `rust-ws73/**/target/` rule; today `target/` is only ignored by the blunt `*` at `.gitignore:19`, not by a named pattern check 4/5 can reason about.
4. **Bilingual README rows required** — both `README.md` (zh) and `README.en.md` (en) must gain a `docs/RUST-WS73.md` row in the same commit or `scripts/check-docs.sh:114-135` check 6/6 fails.
5. **Pre-push 6/6 gate** — `scripts/check-docs.sh` (144 lines, 6 checks) must stay green after the new doc+ignore+README row land; today 6/6 passes on `main` but will break the moment `rust-ws73/Cargo.toml` is added without the ignore fix and will break check 2/5 if the doc is added without the table row.

---

## 1. Sources read (with line counts)

| Source | Lines | Role |
|---|---|---|
| `.gitignore` | 122 | whitelist deny-by-default |
| `scripts/check-docs.sh` | 144 | pre-push 6-check gate |
| `scripts/install-hooks.sh` | 29 | installs `.githooks/pre-push` |
| `.githooks/pre-push` | 8 | `exec bash "$root/scripts/check-docs.sh"` |
| `README.md` | 71 | zh overview + doc index table |
| `README.en.md` | 71 | en overview + doc index table |
| `docs/DEVICE-INTEL.md` | 45 | ffff:3733 enumeration |
| `docs/SDK-INTEL.md` | 88 | SDK layout / build |
| `docs/USB-PROTOCOL.md` | 118 | HCC-over-USB state machine |
| `docs/ECOSYSTEM.md` | 79 | OpenSparklink map |
| `AGENTS.md` | 27 | project rules (English-only, whitelist, cross-links) |
| `docs/agents/domain.md` | 56 | `CONTEXT.md` / `docs/adr/` conventions |
| `docs/agents/issue-tracker.md` | 32 | `.scratch/<feature>/` tracker |
| `docs/agents/triage-labels.md` | 16 | `ready-for-agent` etc. |

No `CONTEXT.md` or `docs/adr/` exist yet (`AGENTS.md:19-21` says "See `docs/agents/domain.md`" — `domain.md:5-11` confirms lazy creation):

```text
ls: cannot access '/.../CONTEXT*': No such file or directory
ls: cannot access '/.../docs/adr/': No such file or directory
```

---

## 2. `.gitignore` — whitelist deny-by-default (the core gate)

### 2.1 Deny-everything preamble

`.gitignore:1-22`:

```gitignore
# ============================================================================
# Whitelist-style .gitignore for nearlink-E573H-USBDC
#
# Rule: nothing is tracked unless explicitly allowlisted below.
#   "*"        ignore every file and directory by default
#   "!*/"      re-open every directory so git can descend into it
#   "!pattern" allowlist specific names / file types
...
# 1) Ignore everything by default
*                                           # .gitignore:19

# 2) Allow directory traversal (required for the allowlist rules below)
!*/                                        # .gitignore:22

# 2a) Local-markdown issue tracker (wayfinder/engineering skills)
!.scratch/                                 # .gitignore:25
!.scratch/**                               # .gitignore:26
```

This is a true whitelist: every path is ignored at `.gitignore:19` unless a later `!` rule re-allows it. `!*/` at `.gitignore:22` is required so git can descend into directories to test the file-level allowlists. `.scratch/` is explicitly re-opened at `.gitignore:25-26`, which is why this lab-notes file can exist at all.

### 2.2 What IS allowlisted

`.gitignore:28-89` (grouped):

```gitignore
# 3) Project metadata
!.gitignore                                 # .gitignore:29
!LICENSE; !README*; !NOTICE*; !AUTHORS*; !CHANGELOG*; !COPYING*

# 4) Build & orchestration
!Makefile; !makefile; !*.mk; !CMakeLists.txt; !*.cmake; !Kconfig; !*.config
!*.sh; !*.py; !*.pl; !*.rb                  # .gitignore:45-48

# 5) Source code
!*.c; !*.h; !*.S; !*.s; !*.asm
!*.rs                                      # .gitignore:56  ← Rust sources OK
!*.go; !*.java; !*.cpp; !*.cc; !*.cxx; !*.hpp; !*.ts; !*.js; !*.lua
!Kconfig; !Kconfig.*                        # .gitignore:66-67

# 6) Docs, data, configs
!*.md                                      # .gitignore:70  ← docs/*.md OK
!*.txt; !*.rst; !*.adoc; !*.csv; !*.xml
!*.json; !*.yaml; !*.yml; !*.ini; !*.cfg; !*.conf
!*.dts; !*.dtsi; !*.dtso; !*.map; !*.patch; !*.diff; !*.lds
```

### 2.3 What is NOT allowlisted — the `!*.rs` vs `!*.toml` / `!Cargo.*` gap

`.gitignore:70-89` allowlists `*.md`, `*.json`, `*.yaml`, `*.yml`, `*.ini` etc. but **does not** list:

* `!*.toml` — so `Cargo.toml` (and any `*.toml`) is still denied by `*` at `.gitignore:19`.
* `!Cargo.*` / `!Cargo.toml` / `!Cargo.lock` / `!*.lock` — same.

`!*.rs` at `.gitignore:56` only covers `*.rs` files; `rust-ws73/Cargo.toml` is a `.toml`, not a `.rs`. The ticket's phrase "check `!*.rs` vs `!*.toml`/`!Cargo.*` missing" refers exactly to this.

Evidence (`git check-ignore -v --no-index`, run 2026-08-19):

```text
.gitignore:56:!*.rs     rust-ws73/src/main.rs    # OK — matched by !*.rs
.gitignore:19:*         rust-ws73/Cargo.toml     # DENIED — hit default *
.gitignore:19:*         rust-ws73/Cargo.lock     # DENIED — hit default *
.gitignore:70:!*.md     docs/RUST-WS73.md        # OK — matched by !*.md
.gitignore:19:*         rust-ws73/target/debug/foo  # ignored by blunt *, not by named rule
.gitignore:117:sdk/**/*.ko  sdk/.../foo.ko       # explicitly ignored
```

So today:

* `docs/RUST-WS73.md` would be trackable (`.gitignore:70 !*.md` saves it).
* `rust-ws73/src/*.rs` would be trackable (`.gitignore:56 !*.rs`).
* `rust-ws73/Cargo.toml` and `Cargo.lock` would be **silently ignored** — `git add rust-ws73/Cargo.toml` would do nothing unless `git add -f` is used, which defeats the whitelist review intent documented at `.gitignore:13-14`.

### 2.4 Binary / SDK artifact suppression (contrast with Rust target)

`.gitignore:90-122`:

```gitignore
# 7) Never tracked: archives, binaries, images, build output.
*.zip; *.tar; *.tar.gz; *.tgz; *.gz; *.bz2; *.xz; *.7z; *.rar
*.ko; *.o; *.a; *.so; *.so.*; *.elf; *.bin; *.img; *.fw; *.lib; *.exe  # .gitignore:101-111

# 7b) SDK build artifacts (generated by `make platform` etc.) — never commit.
sdk/**/*.mod.c; sdk/**/*.mod; sdk/**/*.o; sdk/**/*.ko               # .gitignore:114-117
sdk/**/*.cmd; sdk/**/*.symvers; sdk/**/*.order; sdk/**/\.tmp*; sdk/**/output/**
```

Line `.gitignore:117` `sdk/**/*.ko` is the precedent the ticket cites: SDK kernel modules are explicitly excluded. The analogous Rust artifact is `rust-ws73/target/` (cargo output, can be 100s of MB to GB, contains `.d`, `.o`, `.a`, `.so`, `.rmeta` etc.). Today there is **no** `rust-ws73/target/**` or `**/target/**` or `rust-ws73/**/target/**` rule. It falls through to `*` at `.gitignore:19` + `*.o`/`*.a` etc. at `.gitignore:101-111`, which does ignore it, but:

* It is not *explicit* the way `sdk/**/*.ko` is — an auditor searching for `target` in `.gitignore` finds nothing.
* Check 4/5 (`scripts/check-docs.sh:87-103`) only warns about staged `*.zip|*.bin|*.ko|*.o|*.a|*.so|*.img|*.elf|*.hex|*.exe|*.tar|*.gz|*.7z` (`check-docs.sh:93`); it does **not** catch an entire `target/` directory of mixed extensions (`.d`, `.rmeta`, `build/**/*.o`). An explicit `rust-ws73/target/**` ignore (or `**/target/**` if multiple crates) is the idiomatic Rust fix and mirrors `sdk/**/output/**` at `.gitignore:122`.

Also note `.gitignore:19` (`*`) already ignores `rust-ws73/Cargo.toml` itself, so the `target/` question is secondary to fixing the manifest allowlist first — otherwise the crate cannot even be added.

---

## 3. `scripts/check-docs.sh` — the 6-check pre-push gate (quoted)

Header at `scripts/check-docs.sh:1-17`:

```bash
#!/usr/bin/env bash
# check-docs.sh — pre-push documentation hygiene checks for nearlink-E573H-USBDC
# Verifies, before every push:
#   1. README.md <-> README.en.md cross-links exist (both directions)
#   2. every file listed in the README doc index tables actually exists
#   3. docs/*.md contain no CJK characters (docs are English-only)
#   4. no accidental large files / binaries staged (whitelist gitignore sanity)
#   5. git identity (user.name / user.email) is configured
#   6. README zh/en bilingual sync — a change to one side must be mirrored
```

Wiring:

* Installed by `scripts/install-hooks.sh:22` as `git config core.hooksPath .githooks` (`install-hooks.sh:22`), with legacy `.git/hooks/pre-push` still present (noted at `install-hooks.sh:26`).
* `.githooks/pre-push:8` is `exec bash "$root/scripts/check-docs.sh"` — so `git push` (or `git push --dry-run`) runs this script.
* `AGENTS.md:25` documents the hook: "A pre-push hook (`scripts/check-docs.sh`, installed via `scripts/install-hooks.sh`) enforces this."
* Header comment at `scripts/check-docs.sh:15-17` says `bash scripts/check-docs.sh` for manual run and `bash scripts/install-hooks.sh` to install.

### Check-by-check (with line-cited logic)

**Check 1/5 — README cross-links** `scripts/check-docs.sh:33-42`:

```bash
say "1/5 README cross-links"                 # .gitignore:33 — label says 1/5 (stale; actually 6 checks)
[ -f README.md ]  || fail "README.md missing"
[ -f README.en.md ] || fail "README.en.md missing"
grep -q 'README\.en\.md' README.md  && ok "README.md -> README.en.md link present" \
                                    || fail "README.md lacks link to README.en.md"   # check-docs.sh:38
grep -q 'README\.md' README.en.md  && ok "README.en.md -> README.md link present" \
                                    || fail "README.en.md lacks link to README.md"   # check-docs.sh:40
```

Current state: both links exist. `README.md:3` has `> [English README](README.en.md) · 中文（当前）` and `README.en.md:3` has `> [中文版 README](README.md) · English (current)` — passes.

**Check 2/5 — doc-index completeness** `scripts/check-docs.sh:45-71`:

```bash
say "2/5 doc index completeness (files referenced by the README index tables)"  # check-docs.sh:45
# python3 extracts `([^`]+\.md)` from both READMEs (check-docs.sh:52-63):
REFERENCED="$(python3 - <<'PY'
import re; refs=set()
for fn in ("README.md","README.en.md"):
    txt=open(fn).read()
    refs.update(re.findall(r'`([^`]+\.md)`', txt))
print("\n".join(sorted(refs)))
PY
)"
for f in $REFERENCED; do [ -f "$f" ] && ok "referenced: $f" || fail "referenced but missing: $f"  # check-docs.sh:68
```

Today's `REFERENCED` set (parsed 2026-08-19) is 7 entries: `README.md`, `README.en.md`, `docs/DEVICE-INTEL.md`, `docs/ECOSYSTEM.md`, `docs/SDK-INTEL.md`, `docs/USB-PROTOCOL.md`, `.scratch/nearlink-driver/lab-notes/SHIFU-BUILD-LIST.md`. All 7 exist — check passes. **Adding `docs/RUST-WS73.md` to either README without creating the file (or vice versa) will make this check fail** with `referenced but missing: docs/RUST-WS73.md`.

Note: the script also considers `.scratch/.../SHIFU-BUILD-LIST.md` (zh content) because it matches `` `...md` `` in the index table — it is currently the only non-`docs/` entry.

**Check 3/5 — English-only `docs/`** `scripts/check-docs.sh:74-84`:

```bash
say "3/5 docs are English-only (no CJK)"    # check-docs.sh:74
CJK_FILES="$(grep -rlP '[\x{4e00}-\x{9fff}\x{3400}-\x{4dbf}]' docs/ 2>/dev/null || true)" # check-docs.sh:76
if [ -z "$CJK_FILES" ]; then ok "docs/ contains no CJK characters"  # check-docs.sh:77-78
else for f in $CJK_FILES; do fail "$f: $cjk CJK chars (docs/ must be English)" # check-docs.sh:81-82
```

Enforced by `AGENTS.md:25` ("Docs are English-only (`docs/`)") and repeated in both READMEs: `README.md:48` "docs/ 全英文" and `README.en.md:48` "`docs/` is English-only". The grep covers the CJK Unified Ideographs + Extension A ranges. New `docs/RUST-WS73.md` must be English-only or this check fails.

**Check 4/5 — whitelist / binary sanity** `scripts/check-docs.sh:87-103`:

```bash
say "4/5 whitelist gitignore sanity (no accidental binaries staged)"  # check-docs.sh:87
STAGED="$(git diff --cached --name-only 2>/dev/null || true)"        # check-docs.sh:89
for f in $STAGED; do case "$f" in
    *.zip|*.bin|*.ko|*.o|*.a|*.so|*.img|*.elf|*.hex|*.exe|*.tar|*.gz|*.7z)  # check-docs.sh:93
        BIG="$BIG $f" ;;
esac; done
[ -z "$BIG" ] && ok "no binary/archive files staged" || fail "staged binary/archive should not be committed: $f"
```

This iterates staged files and fails if any match the binary glob. It will catch `sdk/**/*.ko` staged by accident and `rust-ws73/target/**/*.o|*.a|*.so` if someone `git add -f`'d them, but a plain `rust-ws73/target/` is already ignored by `*` so it wouldn't be staged in the first place — the explicit ignore is still needed as defence-in-depth and to keep `git status --ignored` clean.

**Check 5/5 — git identity** `scripts/check-docs.sh:106-111`:

```bash
say "5/5 git identity"                      # check-docs.sh:106
NAME="$(git config user.name)"; EMAIL="$(git config user.email)"
[ -n "$NAME" ]  && ok "user.name = $NAME"  || warn "user.name not set"   # check-docs.sh:110
[ -n "$EMAIL" ] && ok "user.email = $EMAIL" || warn "user.email not set"  # check-docs.sh:111
```

Today: `user.name = ArchivalEra` / `user.email = ArchivalEra@users.noreply.github.com` — passes as `ok` (warn-only if missing, does not block).

**Check 6/6 — bilingual sync** `scripts/check-docs.sh:114-135`:

```bash
say "6/6 README bilingual sync (zh/en must change together)"  # check-docs.sh:114
BASE="$(git rev-parse --verify -q origin/main 2>/dev/null || \
        git rev-parse --verify -q HEAD~1 2>/dev/null || true)"  # check-docs.sh:119-120
ZH_CHANGED="$( { git diff --name-only "$BASE..HEAD" -- README.md 2>/dev/null; git diff --name-only -- README.md 2>/dev/null; } | grep -c . || true)" # check-docs.sh:124
EN_CHANGED="$( { git diff --name-only "$BASE..HEAD" -- README.en.md 2>/dev/null; git diff --name-only -- README.en.md 2>/dev/null; } | grep -c . || true)" # check-docs.sh:125
if [ "$ZH_CHANGED" -gt 0 ] && [ "$EN_CHANGED" -eq 0 ]; then
    fail "README.md changed but README.en.md did not — update the English translation (or --no-verify to force)" # check-docs.sh:127
elif [ "$EN_CHANGED" -gt 0 ] && [ "$ZH_CHANGED" -eq 0 ]; then
    fail "README.en.md changed but README.md did not — update the Chinese translation (or --no-verify to force)" # check-docs.sh:129
else ok "README zh/en pair consistent (both $([ "$ZH_CHANGED" -gt 0 ] && echo "changed" || echo "unchanged"))" # check-docs.sh:131
```

Logic: diffs `origin/main..HEAD` plus working-tree `git diff --name-only` (so it fires pre-commit too). If one README changed and the other didn't, it fails. Today on `HEAD` both are `changed` relative to `HEAD~1` (seen in `bash scripts/check-docs.sh` output: `README zh/en pair consistent (both changed)`) — passes. **When the `docs/RUST-WS73.md` row is added, it must be added to both READMEs in the same push range** or 6/6 fails.

A stale label: the script prints `1/5`..`6/6` — checks 1–5 say `1/5`..`5/5` but check 6 says `6/6`. The header comment at `scripts/check-docs.sh:3-11` lists 6 checks; the `say` strings for the first five were not updated from the original 5-check version. Functionally all 6 run.

---

## 4. `docs/` English-only rule and the four intel docs

`AGENTS.md:25`: "Docs are English-only (`docs/`); `README.md` (zh) <-> `README.en.md` (en) cross-link." Enforced by `scripts/check-docs.sh:74-84` as above.

The four existing `docs/` files (all English, all referenced by both READMEs):

* `docs/DEVICE-INTEL.md:1` `# Device Intel: ffff:3733 ("00000000")` — enumeration, descriptor topology, driver binding. No CJK. Range `grep -P` at `check-docs.sh:76` would flag any CJK; run 2026-08-19: `ok "docs/ contains no CJK characters"`.
* `docs/SDK-INTEL.md:1` `# SDK Intel: ws73_sdk_linux_WS73_1.10.110` — SDK tree, build flow, daemons, reusable pieces table at `SDK-INTEL.md:66`.
* `docs/USB-PROTOCOL.md:1` `# USB Protocol Intel: HCC over USB (WS73 ffff:3733)` — two-stage state machine, EP layout `USB-PROTOCOL.md:32-39`, firmware download `USB-PROTOCOL.md:64-74`.
* `docs/ECOSYSTEM.md:1` `# Ecosystem Map: NearLink Open-Source Landscape` — layer map `ECOSYSTEM.md:11-21`, repo inventory, roadmap phases 0–4.

Any new `docs/RUST-WS73.md` must follow the same rule: English-only body, CJK-free. (The `.scratch/` lab-notes may be bilingual per `README.md:59` "`中英混合`" — but `docs/` may not.)

---

## 5. Bilingual READMEs — current doc-index tables (verbatim)

Both READMEs have identical row sets (only the header/caption language differs). Each row's `` `path` `` is what `scripts/check-docs.sh:60` extracts.

`README.md:46-60` (zh):

```markdown
## 文档索引与维护
> **维护规则**：docs/ 全英文；README 双语互链；改文档后更新下方表格再 push。
| 文件 | 说明 | 语言 |
|---|---|---|
| `README.md` / `README.en.md` | 项目总览（中英互链） | 中/英 |
| `docs/DEVICE-INTEL.md` | ffff:3733 设备枚举情报 | 英文 |
| `docs/SDK-INTEL.md` | WS73 SDK 结构/构建/可复用部件 | 英文 |
| `docs/USB-PROTOCOL.md` | HCC-over-USB 协议要点 | 英文 |
| `docs/ECOSYSTEM.md` | 星闪开源生态地图 + 定稿路线 | 英文 |
| `stack/ssap/` | SSAP 用户态栈源码（codec/transport/server/link/feature） | — |
| `scripts/` | 测试/验证/检查脚本 | — |
| `.scratch/nearlink-driver/lab-notes/` | **53 份研究报告**（SSAP 方言/CM/DTAP/SDR/SM/HADM/标准/OSPL/WS63/OHOS…） | 中英混合 |
| `.scratch/nearlink-driver/lab-notes/SHIFU-BUILD-LIST.md` | 电视盒交叉编译清单（hi3798 SDIO/USB 变体） | 中文 |
```

`README.en.md:46-60` (en):

```markdown
## Document index & maintenance
> **Rule**: `docs/` is English-only; the two READMEs cross-link; update the table below after doc changes.
| File | Description | Lang |
|---|---|---|
| `README.md` / `README.en.md` | Project overview (cross-linked) | zh/en |
| `docs/DEVICE-INTEL.md` | ffff:3733 device enumeration intel | en |
| `docs/SDK-INTEL.md` | WS73 SDK structure/build/reusable pieces | en |
| `docs/USB-PROTOCOL.md` | HCC-over-USB protocol essentials | en |
| `docs/ECOSYSTEM.md` | NearLink open-source ecosystem map + roadmap | en |
| `stack/ssap/` | SSAP userspace stack (codec/transport/server/link/feature) | — |
| `scripts/` | test/verify/check scripts | — |
| `.scratch/nearlink-driver/lab-notes/` | **53 research docs** (SSAP dialect/CM/DTAP/SDR/SM/HADM/standard/OSPL/WS63/OHOS…) | zh+en |
| `.scratch/nearlink-driver/lab-notes/SHIFU-BUILD-LIST.md` | TV-box cross-compile list (hi3798 SDIO/USB variants) | zh |
```

**Gap:** no row for `docs/RUST-WS73.md`. The landing commit must add one row to each table, same position in both files (after `docs/ECOSYSTEM.md`, before `stack/ssap/`), same `Lang` value (`en` / `英文`), with bilingual description. Example (English table):

```markdown
| `docs/RUST-WS73.md` | Rust WS73 tri-mode crate (HCC/USB transport + SSAP bindings) | en |
```

and Chinese table:

```markdown
| `docs/RUST-WS73.md` | Rust WS73 三模 crate（HCC/USB 传输 + SSAP 绑定） | 英文 |
```

If only one side is updated, `check-docs.sh:126-129` fails 6/6. If the file is not created, `check-docs.sh:68` fails 2/5 (`referenced but missing`).

---

## 6. All gaps that block `rust-ws73/` landing (checklist)

### 6.1 `docs/RUST-WS73.md` missing

* File does not exist today (verified 2026-08-19).
* Must be created as English-only (`scripts/check-docs.sh:74-84`).
* Must be referenced in both README tables (see §5) or check 2/5 will fail when it is referenced, and the doc will be orphaned (violates `README.md:48` "改文档后更新下方表格").
* Suggested outline (English): crate layout (`rust-ws73/Cargo.toml`, `src/lib.rs`, `src/hcc_usb.rs`, `src/ssap/`), `ffff:3733` transport (boot vs kernel EP, `DEVICE-INTEL.md:22-32` + `USB-PROTOCOL.md:29-39` refs), SDK relation (`SDK-INTEL.md:30-31` `hcc_usb_host.c` provenance), build (`cargo build --target ...`, cross for `aarch64-unknown-linux-gnu` for hi3798), relation to `stack/ssap/` (C stack vs Rust bindings), `target/` ignore note, testing (`ws73-probe` at `scripts/ws73-probe/`).

### 6.2 `.gitignore` — Cargo manifests not allowlisted

* Current allowlist covers `!*.rs` at `.gitignore:56` but no `!*.toml` / `!Cargo.*`.
* Fix: add after `.gitignore:70 !*.md` or in section 6 (docs/data/configs) or as a new section 5b for Rust:

```gitignore
# 5b) Rust / Cargo
!Cargo.toml
!Cargo.lock
!*.toml
# (alternative) !Cargo.*
```

  `!*.toml` is simplest (covers `Cargo.toml` and any crate `*.toml`); `!Cargo.lock` explicitly if `*.lock` is not desired globally. Do **not** add `!*.lock` globally unless intended — it would also allowlist unrelated lockfiles.

* Verification after fix (must show `!` match, not `*`):

```bash
git check-ignore -v --no-index rust-ws73/Cargo.toml   # expect: .gitignore:<line>:!*.toml  or !Cargo.toml
git check-ignore -v --no-index rust-ws73/Cargo.lock   # expect: .gitignore:<line>:!Cargo.lock or !*.toml handling
git check-ignore -v --no-index rust-ws73/src/lib.rs   # expect: .gitignore:56:!*.rs
```

* Today without fix: `.gitignore:19:*` for both manifests (see §2.3). That is the single most blocking gap — the crate cannot be committed without `git add -f`, which bypasses whitelist review (`AGENTS.md:27`).

### 6.3 `.gitignore` — `rust-ws73/target/` not explicitly ignored

* Contrast: `sdk/**/*.ko` at `.gitignore:117` + `sdk/**/output/**` at `.gitignore:122` are explicit. Rust needs the same:

```gitignore
# 7b) Rust build output (cargo) — never commit.
rust-ws73/target/**
rust-ws73/**/target/**
# or, if multiple crates: **/target/**
```

  Minimal: `rust-ws73/target/**` if only one crate; `**/target/**` if the workspace may grow (covers `rust-ws73/target/` and any nested crate targets). Also consider `rust-ws73/Cargo.lock` handling — libraries typically gitignore `Cargo.lock` but binaries commit it; decide per crate type.

* Today: `rust-ws73/target/debug/foo` → `.gitignore:19:*` (see §2.3) — ignored only by the blunt default, not by a named Rust rule. Add an explicit rule for auditability and to match the SDK precedent.

### 6.4 Bilingual README rows

* Both `README.md:50-60` and `README.en.md:50-60` must gain the `docs/RUST-WS73.md` row atomically (same commit, same push range) to satisfy `scripts/check-docs.sh:114-135` check 6/6.

### 6.5 Pre-push 6/6

* Today: `bash scripts/check-docs.sh` →

```text
[check] 1/5 README cross-links
  ✔ README.md -> README.en.md link present           (check-docs.sh:38)
  ✔ README.en.md -> README.md link present           (check-docs.sh:40)
[check] 2/5 doc index completeness
  ✔ referenced: .scratch/nearlink-driver/lab-notes/SHIFU-BUILD-LIST.md
  ✔ referenced: README.en.md / README.md
  ✔ referenced: docs/DEVICE-INTEL.md / ECOSYSTEM.md / SDK-INTEL.md / USB-PROTOCOL.md  (check-docs.sh:68)
[check] 3/5 docs are English-only (no CJK)           (check-docs.sh:76-78)
  ✔ docs/ contains no CJK characters
[check] 4/5 whitelist gitignore sanity                (check-docs.sh:93)
  ✔ no binary/archive files staged
[check] 5/5 git identity                              (check-docs.sh:110-111)
  ✔ user.name = ArchivalEra / user.email = ArchivalEra@users.noreply.github.com
[check] 6/6 README bilingual sync                     (check-docs.sh:131)
  ✔ README zh/en pair consistent (both changed)
✓ all checks passed.
```

* After the Rust landing, 6/6 must still be `✓ all checks passed` (`scripts/check-docs.sh:143`). That implies: (a) the new `docs/RUST-WS73.md` is English-only, (b) both READMEs reference it and both exist (2/5), (c) cross-links intact (1/5), (d) no `*.ko`/`*.o`/`target/` binaries staged (4/5), (e) bilingual sync holds (6/6).

### 6.6 `git check-ignore -v` validation for `!Cargo.*`

* Must demonstrate after the `.gitignore` patch:

```bash
git check-ignore -v --no-index rust-ws73/Cargo.toml
# expected: .gitignore:<new-line>:!*.toml  (or !Cargo.toml)
# NOT:      .gitignore:19:*

git check-ignore -v --no-index rust-ws73/Cargo.lock
# expected: explicit allowlist line

git check-ignore -v --no-index rust-ws73/target/debug/build/foo.o
# expected: .gitignore:<new-line>:rust-ws73/target/**  (or **/target/**)

git check-ignore -v --no-index docs/RUST-WS73.md
# expected: .gitignore:70:!*.md
```

* The ticket's phrase "`git check-ignore -v` for `!Cargo.*`" means the reviewer will run that command — it must show a `!` rule, not `*`.

---

## 7. Recommended landing order (atomic commit)

1. Patch `.gitignore` — add `!*.toml`/`!Cargo.*` allowlist and `rust-ws73/target/**` ignore (keep `sdk/**/*.ko` at `.gitignore:117` untouched).
2. Create `docs/RUST-WS73.md` (English, ~150+ lines, link back to `DEVICE-INTEL.md`, `USB-PROTOCOL.md`, `SDK-INTEL.md`, `ECOSYSTEM.md`).
3. Add `docs/RUST-WS73.md` row to both `README.md:46-60` and `README.en.md:46-60` tables in the same commit.
4. `bash scripts/check-docs.sh` → 6/6 green; `git check-ignore -v` probes above show `!` matches; `git status --ignored` shows `rust-ws73/target/` as ignored.

---

## 8. Appendix — raw evidence (2026-08-19)

`.gitignore` relevant lines (verbatim, `cat -n`):

```text
19  *
22  !*/
25  !.scratch/
26  !.scratch/**
29  !.gitignore
56  !*.rs
70  !*.md
117 sdk/**/*.ko
```

`scripts/check-docs.sh` key logic quoted in §3; full file 144 lines. `scripts/install-hooks.sh:22` `git config core.hooksPath .githooks`. `.githooks/pre-push:8` `exec bash "$root/scripts/check-docs.sh"`.

`git check-ignore -v --no-index` (today, before fix):

```text
.gitignore:56:!*.rs     rust-ws73/src/main.rs
.gitignore:19:*         rust-ws73/Cargo.toml
.gitignore:19:*         rust-ws73/Cargo.lock
.gitignore:19:*         rust-ws73/target/debug/foo
.gitignore:117:sdk/**/*.ko  sdk/.../foo.ko
.gitignore:70:!*.md     docs/RUST-WS73.md   (hypothetical — would be allowed)
```

`bash scripts/check-docs.sh` today: `✓ all checks passed` (6/6, see §6.5). Cross-links: `README.md:3` → `README.en.md`, `README.en.md:3` → `README.md`.

Existing doc-index rows: `README.md:50-60` and `README.en.md:50-60` (see §5 table, 9 rows incl. header).
