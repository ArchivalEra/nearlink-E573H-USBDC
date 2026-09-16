---
type: harvest
title: "2026 competition positioning pair — BS21E multi-anchor SLE Channel Sounding indoor location (GTTT scheduling, LSQ trilateration, GDOP/rank-deficiency handling)"
language: en
created: 2026-09-13
tags: [sle, channel-sounding, indoor-positioning, trilateration, gttt, gdop, bs21e, competition, harvest]
sources:
  - "ArchivalEra/nearlink-E573H-USBDC workspace"
trust: verified
stale_after: 2026-12-13
---

# 2026 competition positioning pair — BS21E multi-anchor SLE Channel Sounding indoor location (GTTT scheduling, LSQ trilateration, GDOP/rank-deficiency handling)

- Inspection date: 2026-09-13 (corpus registered in `NEW-2026-COMPETITION-SURVEY.md`; this digests the top deep-dive queue item)
- Source roots: `IOT/18600_SLE_Indoor_Locate/code` (33 source files), `IOT/11706_BS21E_sle_location`
- Mode: read-only local program inspection; no build, network, hardware, or PCB access
- Scope: multi-anchor SLE CS indoor positioning — architecture, ranging mode, solver, degenerate-geometry handling

## Executive findings

1. **18600 is a full multi-anchor SLE Channel Sounding positioning system on BS21E (Hi2821)**: multiple fixed Anchors + a mobile Tag; the Tag connects to up to **8 Anchors via GTTT (zu shifen chuanshu - group time-division transmission)**; each Anchor computes CS ranging from local + remote IQ and returns it to the Tag; the Tag runs a self-developed **linear-least-squares trilateration solver** producing 2D/3D coordinates in real time. Positions ship to a phone over a BLE GATT bridge as JSON frames. [18600 README.md, `code/tag/sle_locate_tag.c`]
2. **GTTT scheduling detail**: "In GTTT mode the SDK schedules CS events for all participating T nodes" — no application-level rotation; the Tag stays in **G-T (Group-Tag) ranging mode** rather than T-T. The SDK natively orchestrates multi-node CS time slots — a capability confirmation for our own multi-anchor ranging plans. [`sle_locate_tag.c:206, 818-907`]
3. The solver is **self-contained and production-shaped** (1,310-line tag module): `locate_trilat_solve(&g_trilat_ctx, now_ms, &g_last_pos)` with success/fail counters, residual reported in millimeters in the output frame, and — the standout — **automatic 2D/3D mode switching with rank-deficiency detection**: "the 3D system is rank-deficient. Force 2D mode and fix z to the anchor [height]" when anchors are coplanar. EMA distance smoothing, GDOP-based geometric filtering, and residual threshold validation are stated in the README. [`sle_locate_tag.c:592-621, 1151`]
4. Ranging calibration is empirical: the anchor algorithm file documents "empirically derived scale/offset compensate the SDK's residual range bias" — the student workflow of calibrating raw CS distances against ground truth. [`code/anchor/sle_locate_anchor_alg.c:128`]
5. **11706 takes a two-chip split** (BS21E + WS63 directories plus a serial display): the BS21E handles the SLE location leg while the WS63 drives the display/uplink — a hardware partitioning alternative to 18600's single-chip-per-role design. [11706 tree]
6. Meta-fact: 18600 is based on the official BS2X SDK sample path `src/application/samples/products/sle_locate/` — meaning the vendor SDK ships a `sle_locate` product sample; the official gitee `fbb_ws63` upstream is the place to confirm the latest version of it.

## Boundaries and gaps

- 18600's GDOP and EMA implementations live behind the solver context; only their presence (README + call sites) was verified, not the math line-by-line.
- The BLE GATT bridge JSON frame format is documented in the README but the phone app side is not in the repo.
- 11706's BS21E/WS63 division of labor was read structurally; its solver approach was not compared line-by-line with 18600's.

## Reusable for our stack

- The **GTTT multi-anchor CS scheduling** (SDK-managed slots, G-T mode, up to 8 anchors) is the concrete multi-node ranging capability our WS73 ranging ambitions need — with the official SDK sample path to build from.
- The solver stack (LSQ trilateration + EMA + GDOP filter + residual gate + 2D/3D rank-deficiency fallback) is a complete, proven algorithm bundle for multi-anchor positioning — directly portable to our host-side GnUls work (`nearlink-uwb-like-ranging`'s algorithm.py solves the same problem; comparing the two implementations is a queued exercise).
- Rank-deficiency-forces-2D is a degenerate-geometry guard our positioning code should have.

## Comparison anchors (vs existing reports)

- `NEW-NEARLINK-UWB-LIKE-RANGING.md` / `NEW-IQ-FEATURES-SCHEMA.md`: the UWB-like suite collects bidirectional IQ with host-side GnUls positioning; 18600 computes CS ranging on-anchor with tag-side LSQ — two points on the compute-placement spectrum for the same CS primitive.
- `NEW-2026-COMPETITION-SURVEY.md`: this digests the survey's top queue item; the remaining five queue items stand.
- `NEW-BS21-WTSL.md`: BS21E confirmation widens the chip family evidence beyond WS63.
