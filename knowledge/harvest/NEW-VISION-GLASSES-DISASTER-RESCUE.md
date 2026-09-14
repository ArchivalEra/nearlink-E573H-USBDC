---
type: harvest
title: 2026 competition pair — vision-guiding glasses (WS63E + ESP32 + Python/FastAPI edge) and adaptive disaster rescue (three-node SLE sensor fusion)
language: en
created: 2026-09-13
tags: [ws63e, sle, vision, glasses, disaster, sensor-fusion, three-node, fastapi, harvest]
sources:
  - url: https://gitcode.com/HiSpark/2026_embedded_competition
    note: IOT/10447-Vision-Guiding Glasses_WS63E_src_sle + IOT/12148_WS63_Disaster_Rescue; cloned 2026-09-13, pushed 2026-09-07
trust: verified
stale_after: 2026-12-13
---

# 2026 competition pair — vision-guiding glasses (WS63E + ESP32 + Python/FastAPI edge) and adaptive disaster rescue (three-node SLE sensor fusion)

- Inspection date: 2026-09-13 (competition corpus deep-dive round 2)
- Mode: read-only README + tree-level inspection; no build, network, hardware, or PCB access

## Executive findings

1. **10447 vision-guiding glasses** ("" for visually impaired): a four-tier system — glasses (camera/mic/speaker + serial) → WS63 sending (GPS+ICM42688 IMU + **SLE transmit + packet assembly**) → WS63 receiving (**SLE receive → serial passthrough to ESP32**) → Python/FastAPI server (blind-path detection, crosswalk, traffic-light, object-find, dialogue via `/ws/camera`, `/ws_audio`, `/stream.wav` WebSocket endpoints). The **WS63→ESP32 serial passthrough** is the interesting bridge — ESP32 carries the vision/WiFi load that the WS63 can't. [10447 README.md]
2. **12148 disaster rescue**: a three-node **SLE sensor-fusion topology** — A gateway (fusion judgment + serial display + App), B main node (smoke/particulate/visual flame + risk judgment), C auxiliary node (MQ-2/BME680 ambient supplementary) — with a **NODE_AUX typed protocol** to distinguish local vs regional risk at the fusion point. [12148 README.md]
3. Both confirm the **three-board + gateway** deployment pattern and the **SLE sensor-aggregation role** (WS63 as wireless sensor bridge) that dominates the competition corpus.

## Boundaries and gaps

- README+tree pass; per-module code not line-read (both have 30+ files).
- 10447's ESP32 bridge protocol (serial frame format) not extracted this pass.

## Reusable for our stack

- The **WS63→ESP32 serial passthrough** is a compute-partitioning pattern: NearLink SoC handles SLE+GPS+IMU, offloads vision/WiFi to a stronger SoC. Relevant to our WS73 dongle's host-offload decisions.
- The **NODE_AUX typed protocol** for multi-node sensor fusion (distinguishing main vs auxiliary nodes at the fusion point) is a governance primitive.

## Comparison anchors (vs existing reports)

- `NEW-SIGNLANGUAGE-GLOVE.md` / `NEW-MULTI-NODE-MOTION-DETECTION.md`: same competition, same SLE-sensor-bridge role for WS63.
- `NEW-SSAPS-FIND-REFERENCE.md` / `NEW-SSAP-LINK-PLANE.md`: the SSAP server infrastructure these projects implicitly use.
