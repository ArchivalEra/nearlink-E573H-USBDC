---
type: harvest
title: "OHOS ssaps_server write/read families — multi-READ and multi-WRITE CONFIRMED (correction of an earlier miss); multi-processing negotiated at MTU exchange"
language: en
created: 2026-09-13
tags: [ohos, ssap, multi-read, multi-write, correction, mtu-exchange, validation-loop, harvest]
sources:
  - "ArchivalEra/nearlink-E573H-USBDC workspace"
trust: A
stale_after: 2026-12-13
---

# OHOS ssaps_server write/read families — multi-READ and multi-WRITE CONFIRMED (correction of an earlier miss); multi-processing negotiated at MTU exchange

- Inspection date: 2026-09-13
- Scope: the SSAP server read/write multi-processing implementation, and a correction to this session's earlier feature audit
- Mode: read-only local program inspection; no build, network, hardware, or PCB access

## Executive findings

1. **Correction**: an earlier pass this session (recorded in `NEW-OHOS-DEVICE-SOC-WS63.md`'s parity context and the memory-plane notes it fed) grepped for `multi.*READ`/`MULTI_WRITE` patterns and concluded multi-READ/WRITE were "not confirmed in source". The features DO exist under different naming: `SSAPS_ExecWriteMultiSubItem`, `SSAPS_CheckMultiReadAuthNeed`, `SSAPS_CalcMultiReadTotalSize`, `SSAPS_SerializeMultiReadItem`, `SSAPS_SendMultiReadReqRsp`. The original project memory's claim ("multi-READ/WRITE done") was accurate; the audit method was the failure. Lesson recorded: SSAP multi-operation features are named `*MultiRead*`/`*WriteMulti*`, not `MULTI_READ`/`multi-WRITE`. [`services/stack/src/cp/bsl/sle/servm/ssap/src/ssaps_server_write.c:476`], [`ssaps_server.c:395-476`]
2. **Multi-processing is negotiated at the MTU exchange**: the server sets `exchangePkt->ctrl.multiProcessing = 1` during SSAP exchange and stores it per-link (`link->multiProcessing`) — multi-read/write availability is a link capability bit, not a global compile-time feature. [`ssaps_server.c:76, 115`]
3. The write-multi path is a validated sub-item loop: `SSAPS_ValidateSubItem` and `SSAPS_ValidateCmdSubItem` (a write-cmd variant) return a `SSAP_LoopControlType_E` (LOOP_RET_FALSE/TRUE/BREAK/CONTINUE/NORMAL) driving `SSAPS_ExecWriteMultiSubItem` over `SSAP_PduWriteMultiSubItem_S` items with a `SSAP_BufferedOperation_S` buffer — queued-write semantics with per-item admission control. [`ssaps_server_write.c:424-538`]
4. The read-multi path is authorization- and size-aware before serialization: `SSAPS_CheckMultiReadAuthNeed` checks permission per item first, `SSAPS_CalcMultiReadTotalSize` computes the response budget, then `SSAPS_SerializeMultiReadItem` fills the buffer under an explicit `leftSize` cursor — auth, budget, serialize, in that order. [`ssaps_server.c:395-476`]
5. Per-peer CCCD state lives in per-address vectors: `descriptor->clientConfigs` searched with `CompClientConfigAddr` — the server keeps a per-connected-peer client-config list rather than a single global CCCD flag. [`ssaps_server_write.c:119-163`]

## Boundaries and gaps

- The read family was sampled via ssaps_server.c's multi-read helpers; a dedicated read module file was not enumerated this pass.
- Wire-level deltas of the multi-operation PDU layouts were not byte-diffed against our codec tables.
- OHOS-build specific; the loop-control framework (SDF_Vector, SSAP_LoopControlType_E) requires shims for direct reuse.

## Reusable for our stack

- Our SSAP server should add the multi-processing capability bit to its exchange response and gate multi-read/write on it per link — this is the upstream's actual capability model.
- The read-multi sequence (auth-check all items → compute total size → serialize with cursor) is the safe ordering; serializing before auth would leak on error paths.
- The per-peer clientConfigs vector replaces any single global CCCD flag in our server's notify gating.

## Comparison anchors (vs existing reports)

- `NEW-SSAPS-FIND-REFERENCE.md`: companion module of the same server suite; find and multi-operation modules share the SDF vector/loop idioms.
- `NEW-OHOS-DEVICE-SOC-WS63.md`: supersedes this report's "multi-READ/WRITE not confirmed" side-note — the API surface parity claim itself is unaffected.
- `SSAP-DIALECT-COMPARISON.md`: multi-processing negotiation adds a link-capability dimension to that dialect comparison.
