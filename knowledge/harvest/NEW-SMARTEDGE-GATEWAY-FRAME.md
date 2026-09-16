---
type: harvest
title: "SlumberMin/SmartEdge-WS63 — whole-home SLE gateway: unified 0xAA frame protocol, grouped address space, seven converging control entries"
language: en
created: 2026-09-13
tags: [ws63, sle, smart-home, gateway, frame-protocol, mqtt, flutter, multi-entry, harvest]
sources:
  - "https://github.com/SlumberMin/SmartEdge-WS63"
trust: A
stale_after: 2026-12-13
---

# SlumberMin/SmartEdge-WS63 — whole-home SLE gateway: unified 0xAA frame protocol, grouped address space, seven converging control entries

- Inspection date: 2026-09-13 (staleness check: pushed 2026-07-07, not archived — ALIVE)
- Source root: `https://github.com/SlumberMin/SmartEdge-WS63`
- Mode: read-only local program inspection; no network, build, hardware, or PCB access
- Scope: gateway frame protocol, address scheme, SLE UUID allocation, multi-entry control model

## Executive findings

1. Whole-home system: a WS63E center gateway joins lighting, environment, appliance and security nodes over SLE, and converges seven control entries into one control model — local touch screen, physical buttons, PAJ gesture sensor, BLE, HTTP, MQTT, and a Flutter app. [`README.md` feature table]
2. The **unified application frame protocol** (`gateway_frame.h`, replicated per node package): magic `0xAA`, min 6 bytes, max payload 247 (frame ≤253), with a grouped address byte — gateway `0x00`, broadcast `0xFF`, and node groups `0x10-0x1F` subdivided as lighting `0x10`, environment `0x11`, appliance `0x12`, security `0x13`. [`firmware/smartedge_ws63/envir/gateway_frame.h:12-25`]
3. SLE SSAP UUID allocation is three-slot: service `0xFF00`, control `0xFF01`, status `0xFF02` — separate control and status characteristics under one service, so command and telemetry paths never share a property. [`gateway_frame.h:26-28`]
4. Message classes span control, status, sync, maintenance, and alarm over the same frame envelope — one parser serves all node types, which is what makes multi-entry convergence tractable. [`README.md` feature table]
5. The Flutter app reaches the gateway three ways (BLE near-field, HTTP LAN, MQTT) and the repo ships firmware sample + app only, explicitly excluding the vendor SDK and build artifacts. [`README.md` scope note]

## Boundaries and gaps

- Node firmware packages (envir/, iiczm/, jdkzjd/) duplicate gateway_frame.c/h per package — copy-per-package rather than a shared component, the exact hygiene smell the ws63-sdk-dev-skill checklist flags.
- No pairing/security details on the SLE leg; MQTT credentials handling not audited.
- ~2 months quiet, demonstration-grade (live-demo closed loop is a stated goal).

## Reusable for our stack

- The grouped-address + dual-UUID (control/status) frame design is a clean SSAP application schema for multi-domain device control over our stack — more structured than the single-property UART dialect, simpler than full mesh.
- Seven-entry convergence into one control model is the reference shape for our dongle if it ever fronts multiple local control surfaces.
- The 247-byte payload budget sits neatly under the DS10-measured downlink ceiling, an empirical cross-check of frame sizing.

## Comparison anchors (vs existing reports)

- `NEW-TEKI128-MINIMAL-PAIR.md` / `NEW-OHOS-DK3863-SLE-CURRICULUM.md`: single-property dialect vs SmartEdge's three-UUID schema — the middle rung of the SSAP application ladder.
- `NEW-SMART-CABINET-FULLCHAIN.md`: both are gateway-centric multi-node smart systems; SmartEdge adds grouped addressing and a mobile app.
- `NEW-DS10-SLE-DTU.md`: frame sizing corroborates the measured transport envelope.
