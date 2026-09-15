---
type: harvest
title: "Competition batch 5: two-level SLE posture capture, voice-MEDBOX, threshold fall detection, multi-role shield"
language: en
created: 2026-09-15
tags: [harvest, competition, ws63, sle, topology, mcp, fall-detection, kconfig]
sources:
  - "/mnt/hdd/nearlink-stuff/2026_embedded_competition/IOT/10102_WS63_Multimodal_Posture_Sensing_System"
  - "/mnt/hdd/nearlink-stuff/2026_embedded_competition/IOT/10347_Smart_MEDBOX"
  - "/mnt/hdd/nearlink-stuff/2026_embedded_competition/IOT/14710_fall_detection"
  - "/mnt/hdd/nearlink-stuff/2026_embedded_competition/IOT/15625_NearLink_Shield"
trust: B
stale_after: 2027-03-15
---

# Competition batch 5: posture capture + MEDBOX + fall detection + shield

## Executive findings

**1. Multimodal posture system (IOT/10102) — the highest node-count SLE capture rig in the corpus, and the answer to WS63 connection limits.** 10 wearable MPU6050 nodes + 1 ADS1292R ECG node stream full-body quaternions at 20Hz. The trick is a **two-level SLE topology**: the master connects to only 3 peers — `sle_master.h:17` `#define SLE_MASTER_MAX_NODES 3`; `sle_master.c:106` states the split (translated: "two-level network: master connects Node1 (upper-limb sub-master), Node6 (lower-limb sub-master), Node11 (ECG module)"). Each sub-master aggregates its limb's IMU nodes and forwards. Quaternion wire format: `int16 qw/qx/qy/qz` scaled ×10000 inside a per-node struct with `conn_id` (`sle_master.h:36,63`). Master→TFT display uses two distinct frame types: skeleton frame 0xBB (67 bytes = 16 joints × 2 × int16 + head/seq/crc) vs. text frame 0xDD (5 bytes) — `skeleton_frame.h:21-29`. Cloud bus is a single Huawei IoTDA topic `/realtime_data` shared by all four ends (device, Python scoring server, WeChat mini-program, Blender skeleton driver) — four consumers, no direct device-to-device coupling. Bonus: uses the WS63's onboard WiFi radio for **RF-sensing presence detection** (no extra sensor).

**2. Smart MEDBOX (IOT/10347) — voice-agent on-device via MCP.** BearPi-Pico H3863 dual-board: medbox master (LVGL ST7789 touch UI, 4+1 medication silos, PID cooling 2-13°C, hall-sensor take-dose detection) + doubao voice board (ES8311 mic/speaker) that WebSocket-connects to **xiaozhi-server and exposes medication queries/controls as MCP tools** — LLM-driven device control over a NearLink-adjacent device, first concrete xiaozhi/MCP integration seen in the corpus. Board-to-board link is a 2-wire MBLK channel on GPIO8/13 (not SLE). Device uplink is **CoAP** (unusual choice vs. the corpus-dominant MQTT) plus SLE for the HarmonyOS patient app to connect directly when LAN/cloud is down. Full stack: HarmonyOS app + hospital web panel + server sharing one backend.

**3. Fall detection (IOT/14710) — cleanest minimal WS63 sample skeleton.** Single MPU6050, three-threshold logic (acceleration + tilt angle + stillness duration on ground), 300ms end-to-end alert, UDP WiFi push to guardian phone, five-state FSM (standby/monitoring/fall-alert/sedentary/low-power-sleep), dual-rate sampling for power (high rate active, intermittent asleep, posture wake). Files are a clean three-member parallel split: human-sensor.c (drivers+local alert), behavior_detect.c (detection+power), net_comm.c (WiFi/UDP), sys_master.c (scheduler). Not novel science, but the best-structured "hello world" for a WS63 wearable; its Kconfig + CMakeLists integration recipe (adding a sample under `application/samples/peripheral/`) is the standard pattern all these projects follow.

**4. NearLink Shield (IOT/15625) — Kconfig one-codebase-many-firmware-roles.** WS63 gateway aggregates environment (SHT31), NFC door (servo+OLED), health (MAX30102 HR + MPU6050 fall) nodes over SLE, uplinks MQTT to Huawei IoTDA, drives a 2.66" e-paper dashboard; Node.js attendance backend + Capacitor Android app. The reusable bit is structure: one shared `12_sle_uart/` client/server comm base, and **one codebase producing N different node firmwares purely via `MYDEMO_SAMPLE_SUPPORT_*` Kconfig switches** — role selection at compile time, not runtime.

## Boundaries

- 10102's SLE_MASTER_MAX_NODES=3 confirms WS63 SLE master practical multi-connection ceiling in competition code; scaling beyond uses sub-masters (this project) or DAG relay (batch 4's helmet) — two different answers to the same limit.
- 10347's MBLK board-to-board link is vendor-internal (2-wire GPIO protocol), not a public standard; only the MCP-over-WebSocket idea ports.
- 15625's `12_sle_uart` is the stock sle_uart sample, lightly parameterized — no custom pairing/reconnect logic to study.
- 14710 UDP alert has no auth/encryption; fine for demo, not for production guardianship.
- No PCB/hardware dirs read anywhere (per standing constraint).

## Reusable

- Two-level SLE topology with SLE_MASTER_MAX_NODES=3 sub-master fan-out — the working pattern for >3-node WS63 star capture.
- ×10000 int16 quaternion packing + 0xBB/0xDD dual-frame display protocol — copy as-is for any motion-to-skeleton pipeline.
- Single MQTT topic as multi-consumer bus (device/server/mini-program/3D viewer all on /realtime_data) — simplest possible broadcast fan-in/fan-out.
- Kconfig-selectable firmware roles from one tree — best practice for maintaining gateway/node variants.
- xiaozhi-server MCP tool exposure for LLM voice control of embedded devices — candidate for an SSAP/MCP bridge experiment.

## Comparison anchors

- vs. batch 4 helmet DAG mesh: 10102 solves >3 nodes with hierarchy, helmet with relay+bitmap — both confirm 3-4 SLE connections as the practical per-node limit on WS63-class silicon.
- vs. 15792 health gateway (batch 4): 10347 adds LLM voice + CoAP; 15792 stays REST — both prove gateway offline persistence is now table stakes (NV/flash plan cache).
- vs. local MPU6050 driver report (sync 70): 14710's triple-threshold + dual-rate FSM is the application layer on top of the same I2C driver contract.
