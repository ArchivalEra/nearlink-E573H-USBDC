---
type: harvest
title: "HiSpark/fbb_ws53 — the WS53V100 vendor SDK (Wi-Fi/BLE/SLE Combo SoC): FBB unified framework with sle_conn_param_tuning and sle_chba samples"
language: en
created: 2026-09-13
tags: [ws53, vendor-sdk, fbb, sle, chba, conn-param-tuning, combo-soc, harvest]
sources:
  - "/mnt/hdd/nearlink-stuff/fbb_ws53"
trust: verified
stale_after: 2027-03-13
---

# HiSpark/fbb_ws53 — the WS53V100 vendor SDK (Wi-Fi/BLE/SLE Combo SoC): FBB unified framework with sle_conn_param_tuning and sle_chba samples

- Inspection date: 2026-09-13 (staleness check: HEAD 2025-03-13 — vendor SDK seeded same date as our fbb_ws63 mirror; low churn)
- Source root: `/mnt/hdd/nearlink-stuff/fbb_ws53` (536M)
- Mode: read-only structural inspection; no build, network, hardware, or PCB access

## Executive findings

1. **WS53V100 = Wi-Fi/BLE/SLE Combo SoC** for small appliances/lighting/IoT — a third NearLink chip family in our library alongside WS63 (and WS63E/BS21E variants). The SDK comes from the **FBB (Family Big Box) unified framework** — "tongyi kaifa kuangjia, tongyi API (unified framework, unified API)" with explicit cross-portability to other NearLink solutions as a design goal. [README.md:1-18]
2. The sample tree carries two SLE-specific samples absent (under these names) from the ws63 tree: **`bt/sle/sle_conn_param_tuning`** (connection-parameter tuning sample — the GT/TT interval/latency knob surface as a teachable sample) and **`bt/sle_chba`** — the CHBA sample whose name matches the HiDiTing lwip adapter's `g_sle_chba_localdev` naming, now confirmed as a vendor-sample-level entity across two chip families. [`src/application/samples/bt/sle/`, `bt/sle_chba/`]
3. Tree shape mirrors fbb_ws63 (bt/peripheral/wifi samples, bootloader/kernel/drivers/interim_binary) — the FBB framework normalizes repo layout across chips, which is why community examples port between them with minimal friction. [src/ tree]

## Boundaries and gaps

- Structural pass only; the sle_conn_param_tuning sample's parameter surface and the CHBA sample's function were not read.
- 536M tree not fully materialized in analysis; SDK version pinned at 2025-03-13 HEAD.

## Reusable for our stack

- **sle_conn_param_tuning** is a vendor-blessed starting point for our own connection-parameter tuning work (GT/TT intervals per the DS10 envelope).
- CHBA confirmed as a cross-chip HiSilicon SLE entity (sample in ws53, adapter naming in HiDiTing) — queue a targeted read of the sample to define CHBA.
- FBB framework: cross-chip portability is a vendor design goal — our knowledge-plane chip-family cross-references (WS53/WS63/WS63E/BS21E) should tag shared APIs as FBB-level.

## Comparison anchors (vs existing reports)

- `fbb_ws63` (local mirror) / `NEW-YL63-FORK-VERDICT.md`: same FBB framework, different chip; the official gitee/gitcode upstreams now both tracked.
- `NEW-HIDITING-LWIP-SLE-NETIF.md`: CHBA naming link closed (adapter name ↔ vendor sample).
- `NEW-SSAP-LINK-PLANE.md`: conn_param_tuning connects to link-plane parameter surfaces.
