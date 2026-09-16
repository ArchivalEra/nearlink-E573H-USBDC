---
type: harvest
title: "SmartEdge gateway_ui — seven control entries as one-header-per-entry modules converging in a feature-gated main; latency meter built in"
language: en
created: 2026-09-13
tags: [ws63, gateway, multi-entry, mqtt, ble, gesture, radar, latency, feature-gate, harvest]
sources:
  - "ArchivalEra/nearlink-E573H-USBDC workspace"
trust: verified
stale_after: 2026-12-13
---

# SmartEdge gateway_ui — seven control entries as one-header-per-entry modules converging in a feature-gated main; latency meter built in

- Inspection date: 2026-09-13 (same current clone as `NEW-SMARTEDGE-GATEWAY-FRAME.md`)
- Mode: read-only local program inspection; no build, network, hardware, or PCB access
- Scope: the multi-entry convergence implementation behind the gateway's control model

## Executive findings

1. The convergence is architectural, not ad-hoc: `gateway_ui/inc/` declares **one header per control entry** — `ble_gateway_server.h` (BLE entry), `gateway_webserver.h` (HTTP), `gateway_mqtt.h` (MQTT), `key_driver.h` (physical keys), `gesture_paj7620.h` (PAJ7620 gesture), plus sensing inputs `gateway_radar_presence.h` and `heart_rate_monitor.h`, with `gateway_frame.h` as the shared wire format they all emit. [`gateway_ui/inc/` listing]
2. Entries are **compile-time feature-gated**: MQTT is wrapped in `CONFIG_GATEWAY_MQTT_ENABLE` with `APP_MQTT_UNUSED_ATTR` stubbing so the same main builds with or without cloud control — the Kconfig pattern from the DK curriculum applied at entry granularity. [`gateway_ui_main.c:50-103`]
3. The MQTT path carries **explicit timing contracts**: boot wait 1000 ms, child-ACK wait 18000 ms polled at 20 ms, state-report floor 3000 ms — cloud-control latency budgets are named constants in the entry module, and a `g_mqtt_command_detail[64]` state string exposes the current command for diagnostics. [`gateway_ui_main.c:118-143`]
4. A **latency meter is a first-class module** (`gateway_latency.h`) alongside the entries — the gateway measures its own control-plane latency rather than leaving it to external tooling; audio (audio_pwm/audio_voice), buzzer, LCD (LVGL via `lv_port_disp`), and radar presence round out the sensory/actuator surface. [`gateway_ui/inc/`]
5. The shared `csrc/` directory is the vendored **u8g2 graphics library** (with a `mui` mini-UI layer on top) — display middleware kept separate from the control model, reinforcing that all entries converge into the frame protocol rather than into any single UI. [`csrc/` listing]

## Boundaries and gaps

- The final convergence point (the control model that deduplicates/conflicts multi-entry commands) was not isolated to a single function this pass — it lives across `gateway_ui_main.c`'s glue; conflict-resolution policy (last-writer-wins? priority?) is not documented.
- Node packages (envir/, iiczm/, jdkzjd/) each re-vendor gateway_frame.c/h — the copy-per-package hygiene smell noted in the prior report persists.
- Security entry headers exist (security/ dir at firmware root) but were not audited.

## Reusable for our stack

- One-header-per-entry + feature-gated main is the cleanest multi-entry convergence shape we have archived — directly applicable to our dongle daemon if it fronts USB, network, and serial control surfaces.
- Named timing constants per entry (ACK waits, report floors) should become named constants in our control plane, not magic delays.
- A built-in latency meter module on the gateway is a pattern worth copying for our SLE transport diagnostics.

## Comparison anchors (vs existing reports)

- `NEW-SMARTEDGE-GATEWAY-FRAME.md`: the wire format these entries emit; read together they complete the SmartEdge story.
- `NEW-WS63-SDK-DEV-SKILL.md`: the feature-gating style matches the Kconfig-per-task discipline that skill recommends.
- `NEW-SMART-CABINET-FULLCHAIN.md`: both are gateway-centric; SmartEdge's entry-per-module header layout vs cabinet's single main-loop shows the two convergence extremes.
