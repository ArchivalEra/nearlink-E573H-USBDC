# 02 — knowledge vs assets boundary

Type: grilling
Status: resolved
Blocked by:

## Answer

User confirmed all recommendations (2026-09-12, "全都按你推荐"). Decision rule, one sentence: **knowledge = read-to-decide (documents carrying intelligence, research, decisions); assets = run-or-referenced-by-builds (code, toolchains, hooks, vendor SDK); root infra stays at root.**

| Path | Plane | Rationale |
|---|---|---|
| `.scratch/nearlink-driver/lab-notes/*.md` (79 files) | knowledge | research/intelligence documents |
| `.scratch/nearlink-driver/assets/*.md` (6 spec/intel files) | knowledge | firmware/DLI dialect reference documents |
| `docs/*.md` (4 English intel files) | knowledge | English intel documents |
| `docs/agents/*.md` (3 files) | **stays at `docs/agents/`** | agent operating conventions, referenced by AGENTS.md and user-level skills; moving them widens blast radius for zero knowledge-plane value |
| `.scratch/nearlink-driver/{issues,map.md}`, `.scratch/rust-ws73-tri-mode/` | neither — stays | living wayfinder trackers / process artifacts (user-confirmed) |
| `stack/` (`stack/ssap` self-written stack) | **assets** | the repo's core code artifact |
| `sdk/ws73_sdk_linux_WS73_1.10.110/` | **assets** | vendor reference source + gitignored binaries; build/reference context, not decision material |
| `scripts/` | stays at root | tooling adjacent to hooks/gates (user-confirmed root infra) |
| `.githooks/`, `.gitignore`, `README.md`, `README.en.md`, `LICENSE` | stays at root | root infrastructure |

Boundary stress case (settled): the SSAP wire protocol has two truths — prose in lab notes (knowledge) and `stack/ssap/include/ssap_pkt.h` in code (assets). Intended duplication resolved by reference: code is wire ground truth; knowledge documents **cite** the header and must not restate it as an independent authority. Firmware blobs/calibration data are gitignored → in neither plane.

Naming: top-level `knowledge/` + `assets/`, literal per user's words (Q2=A). Veto-able assumption recorded: none outstanding.

What is the decision rule that separates the knowledge plane from the artifact plane, and where does every existing directory/file land?

The user's framing: knowledge plane (all intelligence/research/decision documents) decoupled from the artifact plane (actual products), as top-level `knowledge` vs `assets`. Settle by ruling on each case:

1. **The rule itself** — candidates to weigh: "read to decide" (knowledge) vs "runs or is linked into a build" (assets); origin (produced by research vs produced by development); consumer (agents/humans reading vs toolchains).
2. **Existing tree, case by case**: `stack/ssap/` (source + its README/tests), `scripts/`, `sdk/ws73_sdk_linux_WS73_1.10.110/` (reference-only vendor sources + gitignored binaries — is reference source knowledge, or an asset?), `.githooks/`, `.gitignore`, `docs/` (current English intel — knowledge?), `.scratch/nearlink-driver/` and `.scratch/rust-ws73-tri-mode/` (wayfinder trackers — process assets or knowledge?), README.md/README.en.md themselves (index of both planes?).
3. **Boundary stress cases**: a lab note documenting the SSAP wire protocol (knowledge) vs `stack/ssap/include/ssap_pkt.h` encoding the same wire truth in code (asset) — intended duplication, or does one side reference the other? Firmware blobs and calibration data (gitignored — not in either plane?); the HHD-01 AT-command text asset (data as knowledge or asset?).
4. **What the top-level domains are named** (`knowledge/`, `assets/`) and what stays at repo root (README pair, LICENSE, .gitignore, .githooks — root infra or moved?).

Record the decision as a table: path → plane → rationale; the rule in one sentence; and explicit veto-able assumptions. The user reviews after the fact (no live exchange).
