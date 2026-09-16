---
type: harvest
title: "Competition batch 7 (AIOT track): BS21 racket connectionless broadcast + WS73 Linux seek/announce tools + WS63E CSV-over-SLE bridge"
language: en
created: 2026-09-15
tags: [harvest, competition, ws73, sle, connectionless-broadcast, ss928, linux]
sources:
  - ""/mnt/hdd/nearlink-stuff/2026_embedded_competition/AIOT/10714_XingyuHuiju_SLE_Badminton""
  - ""/mnt/hdd/nearlink-stuff/2026_embedded_competition/AIOT/20520_基于WS63E的无接触演示控制系统与星闪低时延交互优化""
  - ""/mnt/hdd/nearlink-stuff/2026_embedded_competition/AIOT/15252_SafeSmartBag_Pro""
  - ""/mnt/hdd/nearlink-stuff/2026_embedded_competition/AIOT/15913_SeaEcho_Sign_language_recognition""
trust: B
stale_after: 2027-03-15
---

# Competition batch 7: connectionless broadcast fleet + WS73 Linux tools

## Executive findings

**1. SLE Badminton (AIOT/10714) — the single most relevant project for our WS73 dongle goal in the whole corpus.** Racket grips embed BS20 + MPU-9250 and report 9-axis IMU via **SLE connectionless broadcast (announce, no connection, no pairing)** at 10ms interval; the SS928 master uses a **WS73 module** to scan/receive, discriminating 40+ rackets purely by MAC. Claimed benefit over BLE: no pairing ceremony, unlimited receivers, power-on-to-air. The master-side WS73 tooling is userspace C against the **same SDK headers our SSAP stack uses** — `sle_common.h`, `sle_device_discovery.h`, `sle_errcode.h`, with `sle_announce_seek_callbacks_t` callbacks (`code/ss928/version3.0/ws73/sle_seek_print_all/sle_seek_print_client.c:11-14`). Shipped tools: `sle_seek_print_all` (scan all broadcasters, human-readable AD-type decode), `sle_announce_cmd` (panel measurement announce), `sle_connect_imu`, plus shell bridges writing scan results to `/tmp/sle_seek_lines` for a Qt UI.

Hard-won operational facts from their scripts: (a) **before starting a scan, the WS73 must be fully reset — "mcu nl + rmmod/insmod" — otherwise you get "sle adapter init open fail"** (`sle_seek_bridge.sh:26-28`); (b) a boot-prep script warms the radio + preloads the .ko 30-120s ahead of UI need (`ws73_boot_prep.sh`); (c) announce, seek, and connected-IMU modes are mutually exclusive at runtime — each bridge `killall`s the others first. Their dedup solution for repeated broadcasts: 10ms adv interval vs ~100ms scan frame means the same frame arrives multiple times — two-layer filter, C-side `imu_dedup_allow()` slot table + Qt-side `m_lastSampleTByMac` per-MAC timestamp, yielding a stable 10Hz business stream. There is also an SLE throughput benchmark (`ss928_board_capability_eval/sle_throughput_test`) and a Python IMU reader (`imu_speed_demo.py`, 325 lines, parses the bridge's text output).

**2. WS63E touchless presentation control (AIOT/20520) — ASCII CSV over SLE with end-to-end timestamps.** Sensor board streams `"MPU6050,seq,ts_ms,ax_mg,ay_mg,az_mg,gx_mdps,gy_mdps,gz_mdps,temp_centi"` (CSV) over SLE; gateway parses in `sle_handler.c:38-76` and re-publishes as JSON to MQTT topic `IoT/Sensors/MPU6050`, preserving the sensor-side `ts_ms` so latency is measurable downstream. Publishing is throttled to 500ms (`performance.c:5`), MQTT reconnect backoff 100ms. Human-readable framing trades bandwidth for debuggability — the opposite pole from SparkSafe's binary 0xA5 framing.

**3. AIOT track context (15252, 15913):** SS928-vision projects use WS63E only as an accessory radio (servo gimbal/radar/indicator). 15252 SmartBag is notable for RedCap 5G + hardware H.264 evidence recording but carries **no NearLink at all** — skipped for SLE content. 15913 SeaEcho pairs SS928 sign-language NPU with a WS63E servo cluster and STM32 vitals card; its PC bridge (Vosk STT + DeepSeek over UDP 9000) is a reusable conversation-bridge shape, but the SLE surface is thin.

## Boundaries

- 10714's WS73 tools are compiled for their SS928 board (`Makefile.board`, cross toolchain) and reference `/opt/widget_ui/ws73` paths — relocatable but not drop-in for our Ubuntu x86 host; still, the API call sequence ports directly since it is the same userspace SDK.
- Their WS73 kernel-module load/reset choreography (`ws73_prep_radio`, `ws73_insmod_sle`) hints at a different (likely UART/SDIO-attached WS73 module) integration than our USB dongle; the reset-before-scan rule may or may not transfer.
- 20520 has no top-level README (dirs only); latency-optimization claims rest on timestamp preservation, no measured numbers in-repo.
- 15913/15252 read only at README level; their SS928 NPU pipelines (NNNE quantization, ACL inference) are out of NearLink harvest scope.

## Reusable

- Connectionless broadcast as the multi-sensor fleet pattern: no connection limit, MAC-keyed dedup, 10ms adv + per-MAC timestamp filter → stable rate. Directly applicable to a WS73 dongle receiving many SLE sensors.
- Scan-requires-full-reset rule + radio pre-warm script — first candidate explanation if our WS73 USB dongle ever reports "sle adapter init open fail".
- Three-mode exclusivity (announce/seek/connection) enforced by killall-bridges — operational model for a dongle control CLI.
- AD-structure human-readable decoder (`sle_ad_type_name`, TLV length/type/value walking) — reusable in our nlwatch/ssap tools.
- CSV-with-ts_ms over SLE → JSON MQTT bridge with end-to-end latency accounting — simplest honest telemetry chain.

## Comparison anchors

- vs. our SSAP stack: 10714 confirms the same userspace SLE API (discovery/announce callbacks) runs on Linux hosts against WS73 — independent corroboration of our assets/stack/ssap/ header set.
- vs. Nld dongle eRPC: 10714 loads a proprietary .ko + userspace SDK instead of Nld's D-Bus daemon — two coexisting WS73-on-Linux stacks; ours should document which it follows.
- vs. 10102 two-level topology: both answer "many sensors, one master" — 10102 connects 3 sub-masters, 10714 abandons connections entirely; broadcast scales further at the cost of no downlink.
