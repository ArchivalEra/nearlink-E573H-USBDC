---
type: harvest
title: "Competition batch 9: on-device radar CNN fall detection pair + BS21E Channel-Sounding trilateration with full outlier pipeline"
language: en
created: 2026-09-15
tags: [harvest, competition, radar, tinml, channel-sounding, trilateration, bs21e]
sources:
  - ""/mnt/hdd/nearlink-stuff/2026_embedded_competition/IOT/17513_Fall-Detection""
  - ""/mnt/hdd/nearlink-stuff/2026_embedded_competition/IOT/18007_基于WS63的无摄像头的跌倒检测系统""
  - ""/mnt/hdd/nearlink-stuff/2026_embedded_competition/IOT/18600_SLE_Indoor_Locate""
trust: B
stale_after: 2027-03-15
---

# Competition batch 9: radar fall detection + Channel Sounding locate

## Executive findings

**1. Fall detection with on-board radar CNN (IOT/17513).** Three subsystems on HH-D02 (WS63E): radar node runs a **lightweight CNN (18KB ROM / 7KB RAM) doing fall vs. daily-activity classification on raw 2.4GHz radar IF signal, on-device** — trained in PyTorch on PC from 500ms windows, deployed to the WS63E; wristband node (MAX30102 HR/SpO2 + MPU6050) streams over SLE to a gateway that forwards both streams to Huawei IoTDA (radar results over WiFi/UDP). A SoftAP mode saves raw IF frames to a phone for model iteration — a complete "collect → train → deploy → re-collect" edge-AI loop. Second radar fall detector for contrast: **IOT/18007** uses Rd-03_V2 mmWave + SG90 servo local alarm, WiFi HTTP-POST JSON through a Node.js local proxy into WeChat CloudBase (14 cloud functions, 5-page mini program) — no on-device AI, decision in the radar module, seconds-level end-to-end.

**2. SLE Indoor Locate (IOT/18600) — second full Channel Sounding stack, this one with the solver on the tag.** BS21E (Hi2821) anchors + tag; tag connects multiple anchors simultaneously via **GTTT (group time-division transmission)**; each anchor aggregates local+remote IQ, matches timestamps, computes CS distance with the SDK call and returns it to the tag; the tag runs a **linear least-squares trilateration solver** (`code/tag/sle_locate_trilateration.c`, header comment lines 23-28): 3D non-coplanar by default, 2D mode fixes z and solves (x,y); **A^T A determinant checked against `LOCATE_GDOP_DET_MIN`** (below = geometric degeneracy rejected); per-anchor range jumps rejected by a tiny median history *before* EMA smoothing (`st->dist = (1-α)·dist + α·d`, :160); with >3 fresh anchors, full-set solve → evict single high-residual anchor → retry. Up to 8 anchors, results leave via BLE GATT bridge as JSON position frames to a phone app. Anchor side is SSAP Server with message-queue-driven connection management (`anchor/sle_locate_anchor.c`, `sle_locate_anchor_alg.c`).

## Boundaries

- 17513's CNN architecture details (layers, quantization) are in the pc_training code not yet read; only the footprint constants verified from README.
- 18600's GTTT scheduling is SDK-internal (`sle_locate_sdk` glue) — the multi-anchor simultaneity claim rests on SDK behavior, not visible scheduling code.
- 18007 is infrastructure-as-usual (proxy + cloud functions); only its radar/servo split is notable.
- Neither fall-detection repo publishes confusion matrices; accuracy claims unverified.

## Reusable

- 18KB/7KB CNN footprint numbers as the realistic WS63E TinyML budget for radar classification.
- Median-jump reject → EMA → LSQ → GDOP determinant check → residual eviction pipeline — a complete, ordered robustness stack for any range-based positioning (copy verbatim into our ranging work; matches and extends the uwb-like-ranging mirror's pipeline).
- `LOCATE_GDOP_DET_MIN` determinant test as the practical GDOP filter (cheaper than true GDOP computation).
- BLE GATT JSON bridge as the positioning-result egress pattern.
- SoftAP raw-data collection mode for edge-model iteration — reusable for any WS63 sensor-AI product.

## Comparison anchors

- vs. nearlink-uwb-like-ranging (local mirror, our first CS stack): 18600 is the second independent CS implementation — now two anchors of evidence that multi-anchor SLE CS + tag-side LSQ is the emerging standard architecture; uwb-like-ranging collects bidirectional IQ, 18600 computes distance at the anchor.
- vs. 14710 (batch 5, IMU threshold fall detection): 17513/18007 show the radar alternative — IMU detects the person's motion, radar detects the body shape/dynamics without wearables.
- vs. 15792 health gateway: same wristband SLE pattern; 17513 adds the edge-AI radar dimension.
