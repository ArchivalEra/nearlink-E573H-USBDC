---
type: harvest
title: "Ranging host IQ pairing: temporal matching, quality scoring, and session persistence"
language: en
created: 2026-09-17
tags: [harvest, ranging, iq, persistence, quality]
sources:
  - "https://github.com/zhuzhengyan50-spec/nearlink-uwb-like-ranging/blob/5d6382ad6bcf112db1aae44f2801c4255c26dd5e/host/gui/services/iq_mode3_service.py"
  - "https://github.com/zhuzhengyan50-spec/nearlink-uwb-like-ranging/blob/5d6382ad6bcf112db1aae44f2801c4255c26dd5e/host/gui/services/iq_pair_store.py"
  - "https://github.com/zhuzhengyan50-spec/nearlink-uwb-like-ranging/blob/5d6382ad6bcf112db1aae44f2801c4255c26dd5e/host/gui/services/iq_quality_rules.py"
  - "https://github.com/zhuzhengyan50-spec/nearlink-uwb-like-ranging/blob/5d6382ad6bcf112db1aae44f2801c4255c26dd5e/host/tests/test_core.py"
trust: A
stale_after: 2027-03-17
---

# Ranging host IQ pairing: temporal matching, quality scoring, and session persistence

## Executive findings

The inspected upstream revision is `5d6382ad6bcf112db1aae44f2801c4255c26dd5e`; a direct `gh api` HEAD query matched it on 2026-09-17. Paths below are relative to that upstream repository. This report follows one measurement through matching, pairing, scoring and persistence rather than repeating the previously archived IQ feature inventory.

1. **Measurement association is nearest-time, with conditional client filtering.** `host/gui/services/iq_mode3_service.py:34-42,87-109` keeps 4,000 measurements and accepts matches within 0.20 seconds. The reverse scan skips a different client only when both requested and stored client keys are present. It stops early for a match within one millisecond; therefore it is not guaranteed to select the mathematical nearest record among all remaining entries.
2. **IQ pairing depends on packet order, not equality of sample identifiers.** `iq_mode3_service.py:202-218,332-336` remembers a client packet, pairs it with the next recognized anchor packet, then clears the pending client. Another client packet replaces the pending one. An unknown anchor is skipped without consuming it. This method does not compare local and remote connection IDs or timestamps before pairing.
3. **Matched measurements can replace frame metadata.** `iq_mode3_service.py:169-193` replaces nonempty distance/RSSI maps with those from the selected measurement and records the time difference. Positive measurement cost overrides frame cost. Missing identity fields may be filled from the measurement. The association is useful provenance, but is not proof of acquisition simultaneity.
4. **Session persistence preserves both sides under explicit names.** `host/gui/services/iq_pair_store.py:45-117` emits `iq_pair_research_v3`: client samples become `local_iq`; anchor samples become `remote_iq`. Connection IDs, timestamps, sample counts, completeness flags, matching delay, feature dictionaries and selected scores survive, while duplicate hex strings are omitted.
5. **Quality labels have a normalization caveat.** `host/gui/services/iq_quality_rules.py:65-95` sums scores from both endpoints but divides by the maximum for one endpoint. The aggregate can reach 2 rather than 1. Missing features default to zero, which receives favorable classifications for four lower-is-better metrics. The resulting overall label should not serve as a validated capture-quality gate.

## Pairing and storage contracts

| Stage | Contract | Source anchor |
|---|---|---|
| Anchor reconfiguration | Count is clamped to at least one; a changed count resets parser, history, feature extractor and confidence scorer. | `iq_mode3_service.py:55-61` |
| Retention | Default history is 200 records per anchor; multiple clients share that anchor's budget. | `iq_mode3_service.py:28-42,332-335` |
| Link lookup | Exact anchor/client lookup scans that anchor's retained records backwards. | `iq_mode3_service.py:161-167` |
| Snapshot caching | Cache is keyed by anchor and requested limit; appending any record clears it. | `iq_mode3_service.py:138-153,332-335` |
| Pair construction | Packet completeness is carried into the output, not checked as a rejection condition by this method. | `iq_mode3_service.py:217-251,294-325` |
| Session rotation | Flushes and closes an existing handle, assigns a timestamped directory, but does not create it yet. | `iq_pair_store.py:20-31` |
| Append | First nonempty append creates the directory and opens JSONL; a lock serializes access. | `iq_pair_store.py:33-42` |
| Readback | Flushes an open writer, then parses all nonblank session lines into a list. | `iq_pair_store.py:135-143` |

The service creates a separate CFR/confidence result from paired samples (`iq_mode3_service.py:222-279`). It emits a `valid: True` field after estimation and derives labels from blockage and dynamic thresholds of 0.5. This is separate from `evaluate_pair_quality`; consumers should not assume the two fields express the same validation policy.

The compact writer retains selected temporal and multipath features rather than the full in-memory CFR object. It does not persist every estimator parameter or every derived array. Raw IQ and metadata enable later analysis, but identical reconstruction additionally requires the estimator version and configuration.

## Boundaries

- Source-only inspection; no upstream tests, scientific validation, builds or hardware operations were executed. PCB and firmware binaries were excluded.
- `host/tests/test_core.py:66-97` contains a lazy-creation/readback test asserting the schema and local/remote sample mapping. It also checks that an export file is produced. This is test-source evidence, not a passing execution result, and does not cover quality normalization or temporal ambiguity.
- The per-anchor history is bounded, but `read_records` loads the full session. No streaming iterator or corrupt-line recovery appears in the inspected store.
- Append does not flush on each record. Explicit flush, readback, close or rotation flushes buffered data; there is no `fsync` durability guarantee in this implementation.
- Stored completeness and matching delay are provenance fields, not acceptance gates. Stronger validity requirements must be enforced by a consumer or another verified stage.
- No source code was copied into the driver. Repository-wide licensing was not established by this focused pass; licensing review remains necessary before importing implementation code.

## Reusable

- Preserve both raw IQ endpoints alongside connection identity, sample timestamps and matching delay in a WS73 capture format; avoid storing only plotted distance values.
- Keep acquisition association distinct from display quality and scientific confidence. An explicit unknown/missing state is preferable to silently treating absent measurements as favorable zeros.
- Retain lazy file creation and explicit session rotation as logging patterns, but document flush policy and memory cost for long unattended captures.
- Validate endpoint identity and sample correspondence before interpreting packet-order pairing as a synchronized channel-sounding measurement.

## Comparison anchors

- [NEW-IQ-FEATURES-SCHEMA](NEW-IQ-FEATURES-SCHEMA.md) inventories individual IQ features. This report adds the pair lifecycle and explains why feature presence alone cannot establish quality.
- [NEW-NEARLINK-UWB-LIKE-RANGING](NEW-NEARLINK-UWB-LIKE-RANGING.md) covers system architecture. This note supplies the host-side temporal association and persistence contracts without claiming new radio capability.
