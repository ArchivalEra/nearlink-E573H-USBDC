---
type: harvest
title: "Arctic2520/starbridge-ws63-firmware — SS928 edge computer drives WS63 SLE bridge: UART JSON-line master to remote IR sensor node"
language: en
created: 2026-09-13
tags: [ws63, sle, ss928, edge-computer, json-line, master-remote, ir, bridge, harvest]
sources:
  - "https://github.com/Arctic2520/starbridge-ws63-firmware"
trust: A
stale_after: 2026-12-13
---

# Arctic2520/starbridge-ws63-firmware — SS928 edge computer drives WS63 SLE bridge: UART JSON-line master to remote IR sensor node

- Inspection date: 2026-09-13 (staleness check: pushed 2026-07-06, not archived — ALIVE)
- Source root: `https://github.com/Arctic2520/starbridge-ws63-firmware`
- Mode: read-only local program inspection; no network, build, hardware, or PCB access
- Scope: master/remote SLE firmware pair, edge-computer integration, control-plane shape

## Executive findings

1. Three-tier architecture: an SS928 edge computer issues **UART JSON-line** commands to a WS63 master controller, which bridges them over SparkLink SLE to a WS63 remote node driving infrared LED and sensors via GPIO/I2C. [`README.md` architecture diagram]
2. The repository is a disciplined two-firmware tree (`master_controller/`, `remote_node/`) with per-board pin files (`board_bmh63_pins.c`), control and drivers subdirectories, Kconfig, and full licensing hygiene (LICENSES/, LICENSE_SCOPE.md, THIRD_PARTY_NOTICES.md). [repo tree]
3. JSON-line over UART keeps the edge-computer interface human-debuggable while the SLE leg carries the binary link — the same split-point choice as `NEW-DS10-SLE-DTU.md`'s AT plane, but with JSON instead of AT. [README architecture]

## Boundaries and gaps

- ~2 months quiet; competition-scale code, no CI.
- The remote node's sensor/IR payload schema lives in the master's control modules; not audited line-by-line in this pass.
- No security on the UART or SLE legs.

## Reusable for our stack

- The UART JSON-line split-point (edge intelligence speaks JSON, MCU speaks binary SLE) is a practical alternative to AT dialects for host-to-dongle control — directly applicable to our WS73 dongle control surface.
- Per-board pin files + explicit license scope tree is a tidy repo hygiene template.

## Comparison anchors (vs existing reports)

- `NEW-DS10-SLE-DTU.md` / `NEW-TXSTAR-DS10-REMOTE.md`: three serial-control split-points now archived (transactional AT / binary frames / JSON lines).
- `NEW-SMART-CABINET-FULLCHAIN.md`: both use WS63 master-to-node SLE topologies with upstream egress.
