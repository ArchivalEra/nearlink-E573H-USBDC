---
type: harvest
title: "Competition batch 6: dual-mode patrol car, smart hanger, smart water bottle — plus the 0xABCD/0x3344 ecosystem convention"
language: en
created: 2026-09-15
tags: [harvest, competition, ws63, sle, uuid-convention, ble-coexistence, telemetry]
sources:
  - "/mnt/hdd/nearlink-stuff/2026_embedded_competition/IOT/15413_WS63_DualMode_Env_Patrol_Car"
  - "/mnt/hdd/nearlink-stuff/2026_embedded_competition/IOT/15303_SmartHanger"
  - "/mnt/hdd/nearlink-stuff/2026_embedded_competition/IOT/15307_CoreAqua"
trust: B
stale_after: 2027-03-15
---

# Competition batch 6: patrol car + hanger + water bottle

## Executive findings

**1. Dual-mode patrol car (IOT/15413) — three-link fallback topology.** WS63E car node ↔ BearPi H3863 base station over SLE (telemetry up, joystick differential-drive commands down); base station uplinks via WiFi or a PC bridge program. The Android console (Capacitor, landscape) implements **three switchable connection modes: cloud server / base-station WiFi / car direct** — the app degrades the link itself instead of the firmware, defaulting to cloud and dropping to local links in weak-network situations. Cloud side is Node.js + SQLite storing telemetry, RSSI (link quality logged as a first-class telemetry field alongside temp/humidity/light), tasks, commands, audit logs, and Agent risk-analysis reports. Task loop: cloud creates patrol task → base station pulls → dispatches to car → results stored with audit trail.

**2. SmartHanger (IOT/15303) — canonical minimal dual-board split.** WS63 Client board owns all sensing (AHT20, BH1750, rain, PIR), the drying-rack servo, OLED, and WiFi/MQTT to IoTDA; Server board is a bare window-actuator receiving SLE commands. Shared `sle_window_protocol.h` defines the whole app protocol: 3 commands (0x01 OPEN / 0x02 CLOSE / 0x00 NONE) over service UUID 0xABCD / property 0x3344. Client main.c runs a three-task split (Motion/Sensor/UI) — the standard LiteOS task decomposition for these boards.

**3. CoreAqua water bottle (IOT/15307) — SLE + BLE coexistence on one WS63 and drink-event state machine.** Cup board: DS18B20 water temp, ADC water level/TDS, **AS7341 11-channel spectral sensor for liquid-type identification**, MPU6050 for drink-action detection; screen/base board: env sensors, MQ2/MQ135 air quality, WiFi+MQTT to IoTDA, **BLE server for the phone app while SLE carries cup↔screen traffic** — both radios active in one system with clean role division (BLE = user-facing, SLE = device-facing). `drink_detect.c` implements a tilt-hold state machine: `g_tilt_start_ms` timestamp on entering tilted state, drink event fires after hold duration, `g_new_drink_event_flag` for one-shot consumption. App is Kotlin/Compose MVVM with tea-brewing temperature curves (green/black/oolong).

**4. Ecosystem convention found: service UUID 0xABCD + property UUID 0x3344.** SparkSafe (batch 4), SmartHanger, and the underlying HiSilicon sle_uart sample all use this exact UUID pair for their first SLE property. It propagates unchanged from the vendor demo into competition code — meaning (a) scanning for 0xABCD identifies stock-sample-derived devices, (b) our own SSAP tools should treat it as the "default test service" signature, and (c) none of these projects customized identity, so coexistence of two such devices in RF range would need MAC-based disambiguation.

## Boundaries

- 15303's protocol has no length/CRC fields (unlike SparkSafe's 0xA5 framing) — commands are bare UUID-property writes; fine for 1-byte commands, not for telemetry.
- 15413's "Agent analysis" is LLM report generation over SQLite history, not on-device intelligence.
- 15307's spectral liquid-ID accuracy is not validated in-repo; treat as an AS7341 driver reference, not a proven classifier.
- No PCB dirs read (standing constraint); 15413 README has no per-file pin maps.

## Reusable

- App-side three-link fallback (cloud/LAN/direct) — cleaner than firmware-level multi-link fallback; ports to any NearLink gateway product.
- SLE device-facing + BLE user-facing role split on one WS63 — concrete proof of SLE/BLE coexistence deployment (complements HHD-01's mutual-exclusivity note: coexistence works when roles are split across services/boards, not toggled at runtime).
- RSSI as first-class telemetry field — cheap link-quality history for later analysis.
- 0xABCD/0x3344 as the de-facto stock-sample SLE signature — useful fingerprint for scanning tools.

## Comparison anchors

- vs. batch 4/5 topologies: 15413 adds a dedicated base-station tier (car→base→cloud) like 10102's sub-master tier — third occurrence of the relay pattern.
- vs. sle_uart sample (local): 15303 is the most faithful minimal derivative; SparkSafe the most extended — brackets the evolution range.
- vs. 15792/10347 medical gateways: 15307 is the lifestyle sibling — same coexistence + cloud + app shape, no safety requirements.
