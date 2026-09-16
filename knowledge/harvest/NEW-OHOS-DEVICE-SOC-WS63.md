---
type: harvest
title: "openharmony/device_soc_hisilicon — OHOS-vendored WS63 SDK (fresh, API-identical SLE headers, HAL glue; sparse-adopted)"
language: en
created: 2026-09-13
tags: [ohos, ws63, ws63v100, sdk, api-parity, hal, adapter, harvest]
sources:
  - "ArchivalEra/nearlink-E573H-USBDC workspace"
trust: A
stale_after: 2026-12-13
---

# openharmony/device_soc_hisilicon — OHOS-vendored WS63 SDK (fresh, API-identical SLE headers, HAL glue; sparse-adopted)

- Inspection date: 2026-09-13 (staleness check: pushed 2026-09-09, not archived — ALIVE)
- Method: metadata clone; ws63v100 subtree = 12648 files (tools 6386 / open_source 2677 / kernel 928 / protocol 662 / drivers 643 / middleware 516 / application 263); sparse checkout 3.9M
- Scope: fork verdict, API-generation parity check against our WS73 1.10.110 SDK, OHOS integration glue

## Executive findings

1. This is the OHOS-foundation-vendored WS63 (ws63v100) SDK, upstream-fresh (2026-09-09), with the same `middleware/services/bts` header family our stack work targets — the queued "not yet cloned" item from issue 11 is now resolved as a sparse adopt.
2. **Function-level API parity proven**: for the three SLE headers that matter to us — `sle_ssap_server.h` (825 vs 861 lines), `sle_ssap_client.h` (685 vs 677), `sle_hadm_manager.h` (352 vs 342) — the exported `NLSTK_*` function sets are IDENTICAL with zero one-side-only symbols against our `assets/sdk/ws73_sdk_linux_WS73_1.10.110/include/bsle/sle/` copies. The device (WS63/OHOS) and host (WS73 Linux) public SLE API generations are the same; line-count deltas are comments/layout, not API.
3. This empirically closes the portability argument recorded earlier in `NEW-NEARLINK-UWB-LIKE-RANGING.md` (hadm_manager isomorphism) by extending it to the full SSAP client/server surface: OHOS service-layer code written against these headers (see `NEW-OHOS-NEARLINK-SEPT-INCREMENT.md`) targets the same API our WS73 dongle firmware-side implements.
4. The OHOS integration glue lives in `ws63v100/adapter/`: `hals/` (KV store, UPG XTS adapt), `kal/cmsis/` (CMSIS-to-LiteOS adapters) — standard OHOS device-side integration shape, no NearLink-specific surprises.
5. The SDK sample tree under `application/samples/` (bt/ble, bt/sle, peripheral/*, radar, wifi) matches the stock HiSpark family already digested via community repos — no OHOS-unique samples surfaced in the sparse pass.
6. Unchecked but inventoried for later: `ws63v100/sdk/protocol/` (662 files) and `ws63v100/sdk/middleware/` (516) — candidates for a future deep pass if DLI/protocol-layer sources are needed.

## Boundaries and gaps

- Parity claim covers the three named headers' function sets only; struct/enum layouts and the non-SLE headers (DM, L2CAP-ish, DLI) were not exhaustively diffed.
- The 465MB tree was not cloned; `protocol/` and `middleware/` sources remain unchecked (metadata inventory only).
- OHOS build integration (ohos.build, BUILD.gn) was not validated by building — read-only inspection per standing rules.

## Reusable for our stack

- Parity evidence: we can now read OHOS NearLink service code (`communication_nearlink_service`, `tethering_nearlink`) as direct API documentation for our WS73 host-side headers — same `NLSTK_*` surface, zero translation table needed.
- The `adapter/kal/cmsis` pattern is the reference for any future effort to run the WS63 SDK under a different RTOS shim.
- Sparse-checkout playbook (from `NEW-YL63-FORK-VERDICT.md`) applied again: 465MB repo consumed at 3.9M.

## Comparison anchors (vs existing reports)

- `NEW-NEARLINK-UWB-LIKE-RANGING.md`: that report's "WS73 sle_hadm_manager.h API isomorphic" note is now proven across ssap_client/server + hadm with zero function deltas.
- `WS63-VS-WS73.md`: adds the strongest data point yet to that comparison — vendored trees share one public SLE API generation.
- `NEW-YL63-FORK-VERDICT.md`: second application of the big-fork playbook; this one with a clean parity outcome rather than a divergence verdict.
