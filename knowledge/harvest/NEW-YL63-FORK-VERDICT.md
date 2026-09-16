---
type: harvest
title: "yanlinkos/fbb_ws63 fork verdict — YL63 vendor SDK restructure with official AT Command and Boot API docs (sparse-adopted)"
language: en
created: 2026-09-13
tags: [ws63, yl63, at-commands, vendor-sdk, fork-diff, ble, sle, harvest]
sources:
  - "ArchivalEra/nearlink-E573H-USBDC workspace"
trust: A
stale_after: 2026-12-13
---

# yanlinkos/fbb_ws63 fork verdict — YL63 vendor SDK restructure with official AT Command and Boot API docs (sparse-adopted)

- Inspection date: 2026-09-13 (staleness check: pushed 2026-08-28, not archived — ALIVE)
- Method: metadata-only clone (17427 file list) diffed against local `fbb_ws63` (13173 files); no full 472MB clone
- Scope: fork-diff verdict, doc-adoption decision, AT command family mapping to our HHD-01 reverse engineering

## Executive findings

1. Fork-diff verdict: NOT a byte-identical rehost. `src/` file lists diverge massively (10055 files only-in-yanlinkos, 8316 only-in-local) — this is a restructured, different-generation YL63-series vendor tree, not the same layout as our local fbb_ws63. [`comm -23/-13 of src/ file lists`]
2. Unique high-value content: a full English **AT Command User Guide** (7791 lines, 100 documented `AT+` commands), a **Boot API Development Reference**, a `ci/` gate harness (`ci_gate.py` + tests + a `ci/CLAUDE.md` agent contract), `.clang-format`, and BLE `ble_hello` client/server samples absent from our local tree. [sparse checkout: `docs/` 636K, `ci/` 48K]
3. The AT guide is the OFFICIAL documentation of the exact command family we reverse-engineered on HHD-01: `AT+BLESETNAME=<len,name>` (matches our hard-won two-parameter format), plus BLEPAIR/BLEUNPAIR/BLEGETPAIRED*, BLESETADVDATA/BLESETADVPAR/BLESTARTADV, BLESETSCANPAR/BLESETPHY/BLESETFEATURE. [`AT Command User Guide.md`, BLESETNAME section]
4. The SLE AT family is equally documented: SLEENABLE/SLEDISABLE-family, SLECONN/SLEDISCONN/SLECONNPARUPD, SLEPAIR/SLEGETPAIRED*, SLESTARTADV/SLESTARTSCAN/SLESETADVDATA/SLESETSCANPAR, SLESETMCS (modulation coding scheme), SLESETDEFAULTCONNP, SLEREGCONNCBK/SLEATCOMMONREGCBK — a complete control surface we previously only had as partial OH-dialect captures. [`AT Command User Guide.md`, SLE sections]
5. Companion `fbb_bs2x` (217MB, same org, pushed 2026-08-28) deferred: same vendor pattern, lower marginal value after the WS63 verdict; revisit only if BS2X AT docs are needed.

## Boundaries and gaps

- Verdict is file-list + sparse-doc based; blob-level diffing of `src/` was deliberately skipped (would require the full 472MB).
- The YL63 vendor branch may differ from our 1.10.110 SDK generation in kernel/API details; the AT guide documents the firmware generation shipped in THIS tree, which may not byte-match HHD-01's 1.10.102 AT firmware — cross-check per command before relying on it.
- Docs are HTML-table-laden markdown exports (HiSilicon doc-generator style); quoting them verbatim is noisy — reference by section heading instead.
- `fbb_bs2x` remains uncloned (metadata decision only).

## Reusable for our stack

- Adopt the sparse-checked docs as the authoritative AT reference for HHD-01/WS63 work: BLESETNAME format, pairing family, SLE connection/adv/scan/MCS control — all now officially documented instead of reverse-engineered.
- `ci/ci_gate.py` + `test_ci_gate.py` is a compact self-test gate pattern worth a look when we next touch our own scripts.
- The fork-diff method (metadata-only `--filter=blob:none --no-checkout` + file-list `comm`, then sparse-checkout of high-value paths) is the reusable playbook for all future big-vendor-fork decisions — adopted here at ~684K cost for a 472MB repo.

## Comparison anchors (vs existing reports)

- `HHD01-BOARD.md`: our reverse-engineered BLESETNAME=<len> format and fwpkg partition map now have official doc backing; the guide's AT list also catalogs commands we never probed (pairing family).
- `WS63-AT-FRAMEWORK.md`: extends that framework report with the official 100-command surface, including SLESETMCS which was absent from our dialect captures.
- `NEW-OHOS-NEARLINK-SEPT-INCREMENT.md`: complementary — that covers the OHOS service layer; this covers the vendor firmware control plane.
