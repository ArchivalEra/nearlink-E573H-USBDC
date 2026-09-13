---
type: harvest
title: nearlink-uwb-like-ranging parse_iq_raw.py — a 21-feature IQ signal-analysis schema (time/constellation/frequency/quality/statistics) over the COLLECT stream
language: en
created: 2026-09-13
tags: [iq, features, channel-sounding, zero-crossing, papr, snr, stream-parser, harvest]
sources:
  - url: https://github.com/zhuzhengyan50-spec/nearlink-uwb-like-ranging
    note: local clone (current, upstream 2026-09-12); read-only inspection of host/parse_iq_raw.py (690 lines)
trust: verified
stale_after: 2026-12-13
---

# nearlink-uwb-like-ranging parse_iq_raw.py — a 21-feature IQ signal-analysis schema (time/constellation/frequency/quality/statistics) over the COLLECT stream

- Inspection date: 2026-09-13 (current clone); deep-dive of the host-side IQ decoder
- Mode: read-only local program inspection; no build, network, hardware, or PCB access
- Scope: the feature-extraction schema that turns raw SLE channel-sounding IQ into analyzable vectors

## Executive findings

1. The decoder consumes the collector's `COLLECT_*` stream (`SAMPLE_META anchor/client/conn_id/sdk_dist_mm/sdk_rssi`, `LOCAL_IQ`/`REMOTE_IQ seq=N hex=...`) through a **`StreamParser` state machine** with explicit frame-flush boundaries — a paired-sample assembler, not a line-by-line parser. [`host/parse_iq_raw.py:29-34, 259-338`]
2. The feature schema is a **21-field `IQFeatures` vector in five groups**: time domain (mean/std amplitude, mean/std I, mean/std Q, peak amplitude, **PAPR**, `zero_crossing_i`, `zero_crossing_q`), constellation (spread, **phase jitter**, amplitude imbalance, **IQ correlation**), frequency (FFT peak frequency/magnitude, bandwidth, **spectral centroid**), signal quality (**SNR estimate**), and statistics (skewness, kurtosis). [`parse_iq_raw.py:69-95`]
3. The reference algorithms are minimal and correct: `calc_zero_crossing` counts sign transitions with a >=0/<0 boundary convention that counts exactly once per crossing; `calc_stats` computes mean/std/skewness/kurtosis with population variance and explicit zero-std guard. [`parse_iq_raw.py:141-166`]
4. `RangingResult` / `IQPacket` dataclasses separate the measurement record from the IQ payload and the derived features — a three-tier data model (measurement, raw, features). [`parse_iq_raw.py:37-68`]

## Boundaries and gaps

- The FFT windowing and SNR-estimate formula were not read line-by-line this pass.
- Features are computed on the fixed 80-point IQ capacity from the firmware (IQ_DATA_MAX=80); behavior on short `samp_cnt` payloads depends on the padding the decoder applies.

## Reusable for our stack

- The 21-feature schema is a ready-made quality-assessment feature set for our WS73 channel-sounding/ranging work — it defines what "a good vs bad IQ capture" means numerically before any ML or thresholding.
- The paired LOCAL/REMOTE IQ decode with meta-anchored stream reassembly is the reference ingestion pipeline for our dongle's ranging capture (the firmware protocol doc's five-step advice implemented in full).
- Three-tier data model (measurement / raw / features) keeps raw bytes re-analyzable — matches our own data-retention guidance.

## Comparison anchors (vs existing reports)

- `NEW-NEARLINK-UWB-LIKE-RANGING.md`: the system-level digest; this opens the feature pipeline behind it.
- `firmware PROTOCOL.md` (quoted there): the wire format this decoder implements.
- `SLE-MEASURE-QOS.md`: adds a concrete feature schema to our QoS measurement knowledge.
