---
type: harvest
title: "Competition batch 4: SparkSafe dual-board car monitoring, WS63 health monitor gateway, Smart Helmet cascade mesh"
language: en
created: 2026-09-15
tags: [harvest, competition, ws63, sle, mesh, gateway, pairing, dag]
sources:
  - ""competition-2026/IOT/15239_Sparksafe""
  - ""competition-2026/IOT/15792_WS63_Health_Monitor""
  - ""competition-2026/IOT/17966_Smart_Helmet""
trust: B
stale_after: 2027-03-15
---

# Competition batch 4: SparkSafe + Health Monitor + Smart Helmet

## Executive findings

**1. SparkSafe (IOT/15239) — WS63E dual-board child-left-in-car monitor.** Two WS63E boards split by role: SparkGuard (sensor hub, top of cabin) is the SLE Client; SparkAlert (actuator unit, dash) is the SLE Server. The pair shares one hand-written protocol header that both sides include — `common/protocol.h` defines `nearlink_frame_t {sync=0xA5, cmd, len, payload[32], crc8}` with CRC-8 covering `cmd+len+payload`, and a 7-command set (0x01 ALARM_ON … 0x07 STATUS_REPORT, heartbeat bidirectional). This is a compact, reusable dual-board command-plane template: server registers service 0xABCD / property 0x3344, client scans for name "SparkAlert", MTU 300, conn interval 125ms, TX 10dBm.

Key reconnect trick at `SparkGuard/drivers/src/nearlink.c:231`: on disconnect, call `sle_remove_all_pairs()` **before** re-pairing — comment (translated: "clear old pairing keys, otherwise reconnect will be rejected"). Pairing is driven from `conn_state_cbk`: on connected with `pair_state == SLE_PAIR_NONE` it calls `sle_pair_remote_device()` itself (lines 202-244). Reconnect window is 2s. That is the clearest working example of SLE pairing lifecycle management found in the whole competition corpus.

Risk assessment uses an asymmetric-hysteresis state machine: escalation is immediate, de-ranking requires the reading to leave the hysteresis band; four levels SAFE→CRITICAL with distinct action sets (MQTT-only → MQTT+buzzer → MQTT+buzzer+window motor). One Guard unit carries WiFi STA + WiFi SoftAP + SLE concurrently (radar connected over the SoftAP, telemetry over STA MQTT, actuation over SLE) — proof the WS63 supports three concurrent radios without channel conflict.

**2. WS63 Health Monitor (IOT/15792) — gateway-heavy telecare stack.** Four subsystems: two identical WS63 sensor nodes (SHT4X + SGP30 + SSD1306 + R60AFD1 mmWave fall radar on UART2), a WS63 smart pill-box gateway (`my_medbox`), a Kotlin/Ktor + MySQL backend, and a Jetpack Compose app. The gateway is the interesting node: it is simultaneously **SLE Client to two sensor boards AND BLE Client to a health wristband, plus WiFi HTTP uploader** — tri-radio gateway aggregation in one WS63. Fixed 16-byte SLE frame from sensor nodes: presence/motion/fall bytes + two floats + TVOC/eCO2 uint16 pair + location char 'A'/'B'. Task table: vitals 10s, env 30s, posture 30s, med-plan pull 30s, time sync 60s, heartbeat 5s. Offline resilience: medication plan persisted to NV, falls back to default plan when server unreachable for long periods. Voice module ASRPRO talks `*...#` ASCII frames over UART1. Backend is a clean reference for the "embedded → REST" bridge shape: JWT auth, per-room tables, unified `{code,msg,data}` envelope, device-binding lifecycle endpoints.

**3. Smart Helmet (IOT/17966) — SLE cascade mesh with 16-bit DAG topology sync.** Five SLE stations form a tree (212 demo topologies), root station uplinks via WiFi/MQTT. Routing is not per-hop tables but a **whole-topology bitmap**: `service/dag_sync/dag_sync.c` syncs a 16-bit DAG value + alive mask + terminal position via NOTIFY/MODIFY frames; each station's upstream edge is one bit set/cleared by `dag_learn_notify_client_apply_upstream()` (parent 0xFF = none; `dag_edge_bit(my_id, parent_id)` is the bit index). Parent selection with anti-ping-pong hysteresis at `service/sle_service/sle_client_common.h:32`: `CLIENT_RSSI_HYSTERESIS_DB = 6` — switch parent only if new RSSI beats current by >6dB (`sle_client_callbacks.c:66`); after disconnect there is a 10s window preferring the original parent. Helmet itself is two hi3863 boards UART-coupled (Client message plane + Server sensor plane), handheld terminal does Kalman-smoothed RSSI distance estimation. This is the most complete SLE multi-hop reference in the local library — the DAG-bitmap design is dramatically simpler than per-node routing tables for ≤6 nodes.

## Boundaries

- All three are competition-quality prototypes: no security on the SLE application frames beyond CRC-8 (no encryption/auth at app layer), fixed UUIDs/MACs hard-coded, no OTA.
- Sparksafe README is Chinese; code comments Chinese Doxygen — fine for reading, not for direct doc reuse.
- Smart Helmet PCBs/ and 3D_Models/ are hardware design artifacts — excluded from harvest per standing constraint; only Code/ was read.
- Health Monitor repo ships "core code without full project config" per its own README — build configs incomplete.
- Cascade mesh DAG value is 16-bit → max 16 edges, 5 stations in demo; scaling beyond that needs a different encoding.

## Reusable

- `sle_remove_all_pairs()` before re-pair on reconnect (Sparksafe `nearlink.c:231`) — directly applicable to our SSAP reconnect path.
- Shared single-header dual-board protocol (`protocol.h` pattern: sync+cmd+len+crc8, ≤32B payload) — good default for SLE command planes.
- DAG-bitmap topology sync (NOTIFY/MODIFY, 16-bit edge set + alive mask) — candidate model for small SLE mesh topologies.
- 6dB RSSI parent-switch hysteresis + 10s reattach-preference window — copy as-is for any mobile SLE node.
- Gateway tri-radio pattern (SLE client ×N + BLE client + WiFi uplink in one WS63) — validates our tri-mode goal architecture on the WS63 side.

## Comparison anchors

- vs. sle_mesh_new (local): DAG bitmap is a different mesh philosophy — whole-topology gossip vs. per-hop forwarding; complements the mesh_forward.c hop-based study.
- vs. 12148_Disaster_Rescue and 12279 gateway (batch earlier): same gateway-aggregation shape but 15792 adds BLE coexistence and NV offline persistence.
- vs. Nld dongle protocol: competition frames are application-layer overlays on the SSAP transport, same layering we assumed in the SSAP dialect reports.
