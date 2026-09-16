---
type: harvest
title: "openharmony-sig/communication_nearlink is the tethering upstream; @kit.NearLinkKit is the official ArkTS surface (scan/ssap/dataTransfer)"
language: en
created: 2026-09-13
tags: [ohos, sig, nearlink-kit, arkts, ssap, upstream, attribution, harvest]
sources:
  - "/mnt/hdd/nearlink-stuff/communication_nearlink"
trust: verified
stale_after: 2026-12-13
---

# openharmony-sig/communication_nearlink is the tethering upstream; @kit.NearLinkKit is the official ArkTS surface (scan/ssap/dataTransfer)

- Inspection date: 2026-09-13 (staleness check: SIG pushed 2026-07-07, kit 2025-10-23 — both ALIVE)
- Source roots: `/mnt/hdd/nearlink-stuff/communication_nearlink`, `/mnt/hdd/nearlink-stuff/nearlink-kit_-sample-code`
- Mode: read-only local program inspection; no build, network, hardware, or PCB access

## Executive findings

1. **Attribution resolved**: `openharmony-sig/communication_nearlink` (23M) has the identical top-level structure to the `tethering_nearlink` repo we digested (`bundle.json`, frameworks/services/socket/sa_profile/ipc_parcel/test/tools); `sa_profile/1190.json` is byte-identical while `socket/` differs in 6 files and `services/` in ~500 — the SIG repo is the upstream family and `xingkaiyueying/tethering_nearlink` is a developed fork (its datatransfer/cache/antenna-manager work is the divergence). [`diff -rq` socket/services]
2. The official HarmonyOS app-facing API is **`@kit.NearLinkKit`**, exporting four modules: `scan`, `ssap`, `dataTransfer`, `constant`. The official sample app (ArkTS/hvigor) exercises them through `ScanConfigPage` and `SsapClientPage` — i.e., the sanctioned app surface covers discovery configuration and SSAP client operations (plus data transfer), matching the IPC surface map (advertiser/central/datatransfer/SSAP client) rather than exposing the server side to apps. [`SsapClientPage.ets:4-7`]
3. The kit sample is a complete DevEco project (AppScope/entry/hvigor + screenshots) — the reference for how NearLink apps are structured in ArkTS.

## Boundaries and gaps

- The `@kit.NearLinkKit` type definitions (dts) live in the OHOS SDK, not this repo; the sample shows usage, not the full API surface.
- Which of the 28 IPC interfaces map to the three kit modules (and which are internal-only) was not traced.

## Reusable for our stack

- Our knowledge plane's tethering coverage should cite the SIG repo as upstream and note the fork's deltas (datatransfer service evolution, frame-4 antenna manager) as fork contributions.
- `@kit.NearLinkKit` (scan/ssap/dataTransfer) is the app-developer contract our daemon-compatible surface should mirror for ArkTS consumers.

## Comparison anchors (vs existing reports)

- `NEW-OHOS-TETHERING-SERVICE.md`: upstream found; fork deltas now attributable.
- `NEW-OHOS-SA1190-IPC-SURFACE.md`: the IPC surface vs the three-module kit surface — internal vs app-facing split now clear.
