---
type: harvest
title: "2026 competition — XingYu sign-language glove: dual IMU + flex sensors over SLE to an OrangePi edge-AI translation server (~1s to speech)"
language: en
created: 2026-09-13
tags: [ws63, sle, wearable, sign-language, edge-ai, flex-sensor, jy901p, orangepi, harvest]
sources:
  - "ArchivalEra/nearlink-E573H-USBDC workspace"
trust: A
stale_after: 2026-12-13
---

# 2026 competition — XingYu sign-language glove: dual IMU + flex sensors over SLE to an OrangePi edge-AI translation server (~1s to speech)

- Inspection date: 2026-09-13 (competition corpus deep-dive)
- Source root: `IOT/18384_WS63_XingYu_SLE_SignLanguage_Glove` (firmware_ws63, edge_orangepi, training_pc, miniprogram, docs)
- Mode: read-only local program inspection; no build, network, hardware, or PCB access

## Executive findings

1. **System shape**: wearable gloves (flex sensors via ADC + JY901P IMU modules over UART) → WS63 SLE transport (master client + slave server pair per the sle_uart lineage) → OrangePi **edge-AI server** translating gestures to speech/text in ~1 second. The edge placement deliberately avoids cloud round-trips for the latency budget. [README.md, firmware_ws63/ tree]
2. **Sensor design**: per-hand flex voltages from ADC channels (`g_flex_voltages[FLEX_CHANNEL_COUNT]`, ADC callback with `next` buffer chaining) + per-hand IMU pitch/roll/yaw ×100 (`g_left_imu`/`g_right_imu`, hand_id 0x01/0x02 discriminator in the packet) — a two-hand 3-DoF-imu + N-flex gesture encoding. [`firmware_ws63/sle_uart.c:107-128, 261-273`]
3. **The firmware evolved from MPU6050/BNO085 to JY901P**: `jy901p_bsp.h` notes "the old I2C register map and MPU6050/BNO085 references have been removed" — an in-project sensor migration to a UART IMU. [firmware_ws63/jy901p_bsp.h:12]
4. **The ML lifecycle is repo-complete**: `training_pc/scripts/` covers collect_data, add_gestures, balance_data, train, evaluate, benchmark; `edge_orangepi/` ships install.sh, model/, server/, systemd/ — a full data-collection → training → deployment pipeline, unusual rigor for the corpus. [training_pc/, edge_orangepi/]
5. Sides: a miniprogram (WeChat) for control, hardware/ and docs/ with images — the most complete repo structure in the competition sample we have inspected.

## Boundaries and gaps

- The model type (onnx/tflite/sklearn) and gesture vocabulary size were not read this pass.
- The SLE payload cadence (sampling rate → packet rate) is not documented in the README.
- Recognition accuracy claims live in docs images, not machine-readable form.

## Reusable for our stack

- The **flex+IMU gesture encoding over the SLE UART dialect** is a clean wearable-to-edge pattern: sensor nodes stay dumb, the edge carries the model — matching our host-side compute placement instincts.
- The repo-complete ML lifecycle (collect/balance/train/evaluate/benchmark + systemd deployment) is the rigor template for any smart-edge project on our stack.

## Comparison anchors (vs existing reports)

- `NEW-SLE-AUDIO-SPEAKER.md`: same SLE-transport competition genre; glove sends sensor frames where the speaker sends PCM.
- `NEW-IQ-FEATURES-SCHEMA.md`: another feature-extraction pipeline, at gesture rather than PHY level.
- `NEW-MULTI-NODE-MOTION-DETECTION.md`: sibling wearable with WiFi transport — the corpus again splits SLE vs WiFi by role.
