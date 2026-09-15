---
type: harvest
title: "GitCode hinearlink org scan: firmware_repo firmware index, StarFish nl CLI, sle_throughput samples; GitHub/Gitee windows empty"
language: en
created: 2026-09-15
tags: [harvest, gitcode, firmware, cli-tooling, scan, verdict]
sources:
  - "gitcode.com/hinearlink (firmware_repo, tools, skills, nearlink-contrib, starclaw)"
  - "api.github.com search nearlink/sparklink pushed:>2026-08-20"
  - "api.gitee.com search v5"
trust: B
stale_after: 2026-12-15
---

# GitCode hinearlink org scan

## Executive findings

**1. `hinearlink/firmware_repo` is the official firmware index — 10 curated WS63 packages.** `ws63/` contains: `hh-k01_1v3`, `ws63_AT_v1.10.102` (the exact AT firmware our HHD-01 board runs — now has an official download home), `sle_uart`, `sle_uart_multi`, `sle_throughput` + `_v2` + `_v3_display` (three generations of SLE throughput test firmware), `littlefs_shell`, `littlefs_test`; plus `bs21e/` and an `atomgit/` mirror dir. This completes the fwpkg sourcing picture: any board can be restored to a known sample firmware without building from source. (Read-only listing; binaries excluded per standing constraint.)

**2. StarFish `nl` CLI (hinearlink/tools, updated 2026-01-26) — official build ergonomics.** A bashrc-injected wrapper: `nl path <chip>` binds an SDK dir, `nl build <chip> [-c]` incremental/full build (auto-locates build.py), `nl set <chip>` = menuconfig, short-name mapping (`bs2x` → `standard-bs21e-1100e`-style targets), non-standard path support, idempotent install, and passthrough to a line-number printer for non-build args. Community-grade but the official answer to the same pain our `ssap` scripts address. Related: `hinearlink/skills` (AI-agent skill library + MCP toolset, updated 2026-05-07) ships just CLAUDE.md/LICENSE/README at top level — consistent with the hs-fbb-cli agent-contract direction harvested earlier.

**3. Scan verdicts — no new NearLink repos on any platform this window.** GitHub (pushed >2026-08-20): only our repo, the OHOS mirrors, and re-pushes of already-harvested repos (uwb-like-ranging, keyboard-cli). Gitee search API v5: zero hits for `nearlink`/`sparklink` (API search there indexes poorly/only restricted scopes); GitCode nearlink search surfaces only the known set (nearlink-contrib, communication_nearlink_service, web-flasher, nearlink_oh_ws63, sdr_sim, NearLink kit samples). **GitCode platform staleness reality-check**: the org's freshest repo (ws63flash-win 2026-08-19) is a month old — GitCode updates arrive in waves after events, so the next meaningful GitCode harvest trigger should be a new competition season or release announcement, not daily polling.

## Boundaries

- firmware_repo/tool listings are API-tree level (no binary content read, per standing constraint).
- Gitee search returning zero is an API-capability verdict, not an ecosystem verdict — Gitee may host NearLink content not indexed by its search endpoint; org enumeration would be the follow-up if ever needed.
- StarFish README read; starfish-setup.sh not decompiled to verify idempotence claims.

## Reusable

- firmware_repo as the canonical restore point for WS63/BS21E boards — cite it in bring-up docs instead of ad-hoc firmware copies.
- `sle_throughput_v3_display` trio names the standard progression for SLE performance testing (raw → v2 → with display) — mirrors the ss928_board_capability_eval pattern from batch 7.
- `nl` CLI command surface as the UX reference if our scripts grow a public wrapper.
- Platform monitoring priority: GitHub daily > GitCode weekly/after-events > Gitee org-walk only.

## Comparison anchors

- vs. our HHD-01 AT firmware backup (.scratch assets, commit d68d592): `ws63_AT_v1.10.102` is the same version — our backup is now proven to be the official build.
- vs. ssap build scripts: StarFish `nl` targets the same fbb_ws63/bs2x workflow; ours is goal-specific (SSAP), theirs general (per-chip).
- vs. GitCode competition corpus harvest: this scan closes the GitCode window; competition material (batches 4-11) remains the deepest fresh source this period.
