---
type: harvest
title: "Competition batch 8: WS73 SDK 1.10.111 delta (13-level MCS enum), sports-coach visual-IMU fusion, multi-boat conn_id rotation"
language: en
created: 2026-09-15
tags: [harvest, competition, ws73, sdk-delta, mcs, sle, fusion]
sources:
  - ""/mnt/hdd/nearlink-stuff/2026_embedded_competition/AIOT/11065_Sports_coach""
  - ""/mnt/hdd/nearlink-stuff/2026_embedded_competition/IOT/16010_Hisilicon_WS63_Plateau_Water_Monitor""
  - ""/mnt/hdd/nearlink-stuff/2026_embedded_competition/IOT/15998_LAB_Assitant""
trust: B
stale_after: 2027-03-15
---

# Competition batch 8: SDK version delta + fusion coach + fleet control

## Executive findings

**1. Sports coach (AIOT/11065) — ships a second WS73 Linux SDK copy: 1.10.111.** The project bundles the `ws73_sdk_linux_WS73_1.10.111/` source dir as the receiving side of its wearable IMU nodes — one version newer than our `assets/sdk/ws73_sdk_linux_WS73_1.10.110/`. Tree-diff verdict: 1.10.111 here is a **pruned build tree** (803 headers absent — no sle_android/, no dft/, only application/driver/include), not a full release. One genuinely new header: `include/bsle/common/logger_manager.h` (H_LOG_LEVEL_NONE→ERROR/WARN/INFO/DEBUG enum, generic logger manager for SLE host logging). The one real API delta found: `sle_device_discovery.h` gains the **complete 13-level SLE MCS modulation enum** (`SLE_MCS_00` BPSK1/4 → `SLE_MCS_12` 8PSK 1, `SLE_MCS_MAX`) absent from 1.10.110 — the wire-level modulation ladder behind SLE's headline rates. `sle_errcode.h` differs only by copyright year. The same header also documents announce params precisely: interval unit 125µs (range 0x20~0xffffff), 3-channel map {76,77,78}, TX power [-127,20]dBm (0x7F = don't set), conn interval [0x001E,0x3E80]. Project itself: SS928 vision + two WS63E IMU wearables over SLE with a **visual-IMU three-state fusion FSM** (pure vision → vision+IMU fusion → IMU-only fallback on occlusion, smoothed transitions to avoid boundary flicker), non-standard reps get corrective feedback instead of being counted, SQLite + offline upload queue to Supabase. Both wearable nodes run identical firmware, identity chosen by compile config.

**2. Plateau water monitor boat (IOT/16010) — multi-boat SLE conn_id rotation.** Control board (joystick + OLED + WS63 SLE client + WiFi MQTT forwarder) manages multiple boat nodes. `qhmu_water_ctrl/src/dataExc.c:226-259` implements fleet addressing: `g_boat_ctrl_active_conn_id` tracks the selected boat; auto-select cycles ready boats by index ("[CTRL] auto select boat conn_id:%d index:%d"), manual select validates the target is ready before use, failures fall through with explicit "[CTRL] no ready boat to select" logging. Boats run SLE server + L610 cellular MQTT, auto-return-to-charge at low battery. Simple, but a working pattern for one-client-many-servers command routing on SLE.

**3. Lab assistant (IOT/15998) — voice-first multi-node desk control.** Atlas 200 DK as edge display/service host, WS63 as multi-node radio hub (SU-03T voice module, OpenMV vision, temp/hum/smoke/PIR sensors, relays, fan, Mi desk lamp), Huawei OBS for experiment-video archival. Architecture is "one WS63 hub + heterogeneous commodity modules" — nothing new at SLE level, but confirms SU-03T (offline voice) + WS63 as a recurring pairing in competition builds.

## Boundaries

- The 1.10.111 tree is competition-pruned: cannot diff-replace our full 1.10.110; only its discovery-header MCS enum and logger_manager.h are worth merging into our copies.
- 11065's Supabase offline queue is Android-side Kotlin, not device-side — device code ships only binaries in the package dir.
- 16010's conn_id table has no discovery/attach timeout logic visible; assume it relies on sle_uart-style ready flags.
- 15998 read at README level only (architecture validated, no SLE code pulled).

## Reusable

- `SLE_MCS_00..12` enum from 1.10.111 `sle_device_discovery.h:146-200` — patch into our SDK copy + SSAP rate documentation (completes the "SLE 12Mbps" param story with the actual modulation names).
- `logger_manager.h` log-level enum — candidate for our stack's log facade.
- Announce-param unit documentation (125µs interval, ch 76/77/78, 0x7F tx-power sentinel) — already verified consistent with our USB-PROTOCOL findings.
- conn_id fleet rotation (`dataExc.c` auto/manual select with ready-check) — for multi-device SSAP sessions.
- Three-state vision/IMU fusion FSM — pattern for any SLE IMU wearable product.

## Comparison anchors

- vs. our assets/sdk 1.10.110: only MCS enum + logger_manager differ; confirms 1.10.110 is sufficient for all competition use — no urgent upgrade path needed.
- vs. batch 7 broadcast fleet (10714): 16010 is the connected counterpart (conn_id-based control), 10714 connectionless — two fleet-control models.
- vs. 10102 posture rig: same IMU-over-SLE idea, 11065 adds vision fusion and drop-tolerant link fallback.
