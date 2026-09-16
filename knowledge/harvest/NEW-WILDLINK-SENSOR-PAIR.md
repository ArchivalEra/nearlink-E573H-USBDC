---
type: harvest
title: "twyora/WildLink — H3863 Multi-Property SSAP Health-Sensor Pair with Tri-Link Client Node (SLE + BLE + LoRa)"
language: en
created: 2026-09-13
tags: [h3863, ws63, sle, ssap, multi-property, ble, lora, dual-stack, sensor, harvest]
sources:
  - "https://github.com/twyora/WildLinkServer"
trust: A
stale_after: 2026-12-13
---

# twyora/WildLink — H3863 Multi-Property SSAP Health-Sensor Pair with Tri-Link Client Node (SLE + BLE + LoRa)

- Inspection date: 2026-09-13 (staleness check: both pushed 2026-07-19, not archived — ALIVE)
- Source roots: `https://github.com/twyora/WildLinkServer`, `https://github.com/twyora/WildLinkClient`
- Mode: read-only local program inspection; no network, build, hardware, or PCB access
- Scope: SSAP multi-property server design, dual/tri-stack node architecture, sensor task layout

## Executive findings

1. The pair implements a health-sensor link over SLE with a **multi-property SSAP server design**: instead of the community-standard single-property UART dialect, the server registers one property per sensor — dedicated handles `g_sle_server_max30102_handle` (heart-rate/SpO2) and `g_sle_server_max30205_handle` (skin temperature) under a custom 128-bit UUID base `37BEA880-FC70-11EA-B720-000000000000`. [`WildLinkServer/src/task/sle_server/sle_server_task.c:35-45`]
2. The server address is configurable at build time via `CONFIG_SLE_SERVER_ADDR_LOW_BYTE` over a fixed suffix — a Kconfig-parameterized addressing scheme that lets multiple WildLink server nodes coexist without code changes. [`sle_server_task.c:38-40`]
3. The client node is a **tri-link device in one firmware**: `app_entry()` starts `sle_client_task_entry()`, `ble_server_task_entry()`, AND `atk_lora_task_entry()` concurrently — SLE client to the sensor server, BLE GAP server (own adv id 0x01) for phones, plus an ATK-LoRa module link, alongside sensor tasks (AHT20, BMP280), an SH1106 OLED task, and an emergency-alarm task. [`WildLinkClient/src/app_entry.c:22-124`]
4. Both trees use per-task Kconfig gating (`CONFIG_*_TASK_ENABLED`, `Kconfig.tasks`, `Kconfig.sle`) — every sensor/link is a build-time feature flag, a clean pattern for keeping one firmware tree across hardware variants. [repo Kconfig layout]
5. "wlid" is the authors' log-prefix namespace (wlid_link_*_log), not a wire protocol — the transport is stock SSAP read/write/notify on top of the standard WS63 SDK stack. [`WildLinkClient/src/common/wlid_link_client_log/`]

## Boundaries and gaps

- Minimal READMEs ("running on the BearPi-Pico H3863"); no protocol documentation — the property UUID map must be read from source.
- No pairing/security on the SLE link; the BLE server side is a plain GAP advertiser.
- ~2 months quiet, zero-star hobby project; SDK scaffolding assumed external (driver/Kconfig shells reference a HiSpark tree not vendored here).
- Concurrent SLE+BLE operation is unverified for radio coexistence limits — the code starts both stacks but no coexistence measurement exists in-tree.

## Reusable for our stack

- Multi-property-per-sensor SSAP layout (sensor = property handle, not a byte protocol inside one property) is a cleaner alternative to the single-property UART dialect for structured telemetry over our WS73 SSAP stack — it moves framing into the ATT-like layer.
- Kconfig-per-task feature gating keeps client/server/variant builds in one tree — adoptable layout for our sample programs.
- Tri-link node (SLE + BLE + LoRa) is real prior art for our tri-mode ambitions: a single small device running three link layers concurrently with task-per-link isolation.

## Comparison anchors (vs existing reports)

- `NEW-TEKI128-MINIMAL-PAIR.md`: that pair is the single-property floor; WildLink shows the same SDK's ceiling with per-sensor properties.
- `NEW-SLE-1V8-VEHICLE.md`: vehicle is many-servers-one-client over one property; WildLink is one-server-many-properties — the two orthogonal multi-axis designs of the WS63 family.
- `HHD01-BOARD.md`: same BearPi/HiSpark board family and SDK sample lineage; WildLink's Kconfig addressing echoes our violin board addressing notes.
