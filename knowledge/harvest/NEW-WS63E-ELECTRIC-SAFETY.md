---
type: harvest
title: "2026 competition — WS63E electric-safety system: one-master-two-slave SLE star with WiFi/MQTT cloud egress"
language: en
created: 2026-09-13
tags: [ws63e, sle, star-topology, mqtt, safety, three-board, harvest]
sources:
  - "ArchivalEra/nearlink-E573H-USBDC workspace"
trust: verified
stale_after: 2026-12-13
---

# 2026 competition — WS63E electric-safety system: one-master-two-slave SLE star with WiFi/MQTT cloud egress

- Inspection date: 2026-09-13 (competition corpus)
- Source root: `IOT/16781_WS63E_Electric_Safety_Control_System/code`
- Mode: read-only structural inspection; no build, network, hardware, or PCB access

## Executive findings

1. Three-board WS63E star: **A board (master) — SLE — B board (gateway slave, OLED+WiFi) — WiFi/MQTT — cloud+APP**, and C board (executor slave) completing local actuation closed loop. The B board is the SLE↔WiFi bridge (the recurring gateway pattern, seventh instance in our corpus). [README.md architecture diagram]
2. Target scenario: dorm/lab smart electricity safety (environment sensing, alerting, remote control, cloud reporting, local execution).
3. First WS63E-specific competition digest: confirms WS63E projects ship as full SDK trees and use the same SLE star patterns as WS63 — the E-variant is API-compatible at the application layer.

## Boundaries and gaps

- Structural pass only; sensor/actuator specifics and SLE payload formats not extracted.

## Reusable for our stack

- Confirms the star topology (1 master + N slaves) as the default SLE deployment shape across the corpus; WS63E drop-in compatibility noted.

## Comparison anchors (vs existing reports)

- `NEW-SMART-CABINET-FULLCHAIN.md`: same master+gateway+cloud chain; WS63E vs WS63.
- `NEW-MESH-TRANSPORT-SUBSTRATE.md`: star (this) vs tree (DTU) vs route-table (AIGC) — topology spectrum complete.
