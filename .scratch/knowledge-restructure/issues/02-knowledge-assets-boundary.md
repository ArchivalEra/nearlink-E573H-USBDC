# 02 — knowledge vs assets boundary

Type: grilling
Status: open
Blocked by:

## Question

What is the decision rule that separates the knowledge plane from the artifact plane, and where does every existing directory/file land?

The user's framing: knowledge plane (all intelligence/research/decision documents) decoupled from the artifact plane (actual products), as top-level `knowledge` vs `assets`. Settle by ruling on each case:

1. **The rule itself** — candidates to weigh: "read to decide" (knowledge) vs "runs or is linked into a build" (assets); origin (produced by research vs produced by development); consumer (agents/humans reading vs toolchains).
2. **Existing tree, case by case**: `stack/ssap/` (source + its README/tests), `scripts/`, `sdk/ws73_sdk_linux_WS73_1.10.110/` (reference-only vendor sources + gitignored binaries — is reference source knowledge, or an asset?), `.githooks/`, `.gitignore`, `docs/` (current English intel — knowledge?), `.scratch/nearlink-driver/` and `.scratch/rust-ws73-tri-mode/` (wayfinder trackers — process assets or knowledge?), README.md/README.en.md themselves (index of both planes?).
3. **Boundary stress cases**: a lab note documenting the SSAP wire protocol (knowledge) vs `stack/ssap/include/ssap_pkt.h` encoding the same wire truth in code (asset) — intended duplication, or does one side reference the other? Firmware blobs and calibration data (gitignored — not in either plane?); the HHD-01 AT-command text asset (data as knowledge or asset?).
4. **What the top-level domains are named** (`knowledge/`, `assets/`) and what stays at repo root (README pair, LICENSE, .gitignore, .githooks — root infra or moved?).

Record the decision as a table: path → plane → rationale; the rule in one sentence; and explicit veto-able assumptions. The user reviews after the fact (no live exchange).
