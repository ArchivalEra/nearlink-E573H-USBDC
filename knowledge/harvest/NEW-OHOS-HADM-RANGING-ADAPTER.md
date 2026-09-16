---
type: harvest
title: "OHOS HADM ranging algorithm layer decoded: 6 algorithm modes (150m/75m), dual-side IQ pipeline, DisResult confidence output, SLEM error codes"
language: en
created: 2026-09-15
tags: [harvest, hadm, sle, ranging, channel-sounding, algorithm, ohos]
sources:
  - ""/mnt/hdd/nearlink-stuff/communication_nearlink_service (frameworks/ranging_alogorithm_adapter/)""
trust: A
stale_after: 2027-03-15
---

# OHOS HADM ranging algorithm layer

## Executive findings

`communication_nearlink_service` yields one more layer — `frameworks/ranging_alogorithm_adapter/` (~400 lines) is the **official algorithm tier between raw Channel-Sounding IQ and the HADM ranging API**. Copyright Huawei 2026, Apache-2.0.

**1. Six distance algorithm modes with explicit range budgets** (`ranging_alogorithm_adapter_def.h` `DisAlgType`): METHOD_1M (1MHz channel frequency hopping, **150m limit**), METHOD_2M (2MHz FH), METHOD_1M_2M (low-complexity, **75m limit**), METHOD_ADJ_R_END + V2 + V3 (1MHz FH dynamic-r, 150m; V2 noted "for northpole"). So the official SLE CS distance envelope is 75-150m depending on algorithm class — the first hard range numbers attached to the CS stack we harvested from the ranging mirrors.

**2. Dual-side IQ pipeline.** `MeasureAlgPara` carries IQ vectors for **both DUT and RTD sides** (`iqDut`/`iqRtd` as `uint16` I/Q pairs), per-side `tofDut/tofRtd`, per-side SLEM channel maps, per-side IQ bit lengths, plus multi-tone flag, keyId, RSSI per side, and local/remote NV + ToF offsets. `HadmRangingAdapter::TransferSoundingToAlgPara` (ranging_alogorithm_adapter.cpp:88-95) maps `NearlinkHamdSoundingResult` (vector-valued DUT/RTD I and Q) into these parameters. Two-sided IQ with dynamic-r weighting explains the accuracy claims; the adapter is also the layer where the "smoothing" happens.

**3. Output is confidence-scored, not a bare distance.** `DisResult`: `disSmoothed`, `disOri` (raw), `disSlightSmoothed` (three smoothing tiers exposed at once), **`prob` (distance confidence)**, rssi, `height` (confidence intermediate), `smoothNum` (consecutive valid ranging count). Consumers can gate on confidence + streak length instead of trusting every frame — the same philosophy as 18600's residual/GDOP gates but implemented centrally.

**4. SLEM error namespace 0x8000A400-0x8000A800.** Distinct ranging failure modes: `ERRCODE_SLEM_RSSI_ABNORMAL` (0x8000A450), `ERRCODE_SLEM_MARIX_INV_FAIL`, **`ERRCODE_SLEM_TOF_IQ_NOTMATCH` (ToF distance far greater than IQ distance — the divergence detector)**, `ERRCODE_SLEM_IQ_LOW_ENERGY`, `ERRCODE_SLEM_TOA_ABNORMAL` (ToA below lower bound). The ToF-vs-IQ cross-check is a built-in integrity assertion worth copying into any CS implementation.

## Boundaries

- The actual floating-point algorithm bodies are in the closed `nearlink_hadm_sounding_result`/`nearlink_sle_ranging` SDK binaries — the adapter exposes parameters and outputs, not the DSP math.
- `ParaPair {rssiLimit, rStart, thresholdCond2}` semantics inferable but undocumented; treat as tuning triple.
- Adapter only; multi-anchor aggregation lives in HADM layers above (not read).

## Reusable

- Algorithm menu (1M/2M/low-complexity/dynamic-r V1-V3) with 75m/150m budgets — parameter-selection table for any CS product.
- Three-tier smoothing output + confidence prob + streak counter — the output contract to imitate in our ranging tools.
- ToF-vs-IQ divergence assertion + IQ low-energy rejection — two cheap integrity gates for sensor-side CS.
- SLEM error-code block layout (0x8000A400-0x8000A800, shared/ranging sub-ranges) — namespace convention for our SSAP tool error reporting.

## Comparison anchors

- vs. 18600 tag trilateration (batch 9): OH centralizes smoothing/confidence per distance; 18600 does it per position with geometric gates — complementary, combinable.
- vs. uwb-like-ranging mirror (sync 47): that project captured the IQ the OH pipeline consumes; this adapter is what the official stack does with it — the CS story now has capture, algorithm, and solver endpoints all harvested.
- vs. USB-PROTOCOL.md: if our dongle ever supports HADM sounding, `TransferSoundingToAlgPara`'s field list is the expected IQ payload shape.
