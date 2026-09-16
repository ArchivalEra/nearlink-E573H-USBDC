---
type: harvest
title: "SmartEdge envir node — five environment sensors over the 0xAA frame protocol; symmetric frame build/parse with magic+min-length guard"
language: en
created: 2026-09-13
tags: [ws63, sle, node, sensors, bh1750, bmp180, pm25, dht20, frame-parse, harvest]
sources:
  - "ArchivalEra/nearlink-E573H-USBDC workspace"
trust: verified
stale_after: 2026-12-13
---

# SmartEdge envir node — five environment sensors over the 0xAA frame protocol; symmetric frame build/parse with magic+min-length guard

- Inspection date: 2026-09-13 (same current clone as `NEW-SMARTEDGE-MULTI-ENTRY.md`)
- Mode: read-only local program inspection; no build, network, hardware, or PCB access
- Scope: the environment node package — sensor set, frame codec on the node side, SLE server wiring

## Executive findings

1. The environment node integrates **five sensor drivers as sibling directories**: BH1750 (light), BMP180 (pressure), DC01 (PM2.5), DHT20 (temperature/humidity), GUVA-S12SD (UV) — each self-contained with its own driver sources, plus an OLED display and a dedicated advertiser file. [`firmware/smartedge_ws63/envir/` tree]
2. The node-side frame codec is symmetric and guarded: build writes `out_buf[0] = GATEWAY_FRAME_HEAD` with `frame->cmd` at byte 3; parse validates `buf_len >= GATEWAY_FRAME_MIN_LEN` and `buf[0] == GATEWAY_FRAME_HEAD` before extracting `cmd` — reject-early framing rather than tolerate-and-skip. [`firmware/smartedge_ws63/envir/gateway_frame.c:37-74`]
3. The SLE leg is the stock two-file shape (`sle_envir_node.c` 398 lines + `sle_envir_node_adv.c` 140 lines) — node packages reuse the same server/advertiser skeleton the gateway does, differing only in the frame payloads they serve. [`sle_envir_node.c` line counts]

## Boundaries and gaps

- The five sensor drivers are integrations of vendor modules; driver correctness was not audited.
- Frame CRC/ack behavior on the node side was not verified this pass (the gateway frame definition showed no CRC field — control-plane reliability relies on the SLE link).
- Node package duplication (gateway_frame.c/h copied per node) persists as noted in `NEW-SMARTEDGE-GATEWAY-FRAME.md`.

## Reusable for our stack

- The five-sensor taxonomy (light/pressure/PM2.5/humidity-temperature/UV) with per-driver directories is a clean sensor-node template for our dongle-attached sensor projects.
- Reject-early frame parsing (magic + min-length before field extraction) matches the DS10 measured finding that link-layer corruption is rare but real — guard cheaply, fail fast.
- Confirms the frame header layout byte-exactly: HEAD at offset 0, CMD at offset 3 — filling in the wire detail `NEW-SMARTEDGE-GATEWAY-FRAME.md` left at the constant level.

## Comparison anchors (vs existing reports)

- `NEW-SMARTEDGE-GATEWAY-FRAME.md` / `NEW-SMARTEDGE-MULTI-ENTRY.md`: the node-side counterpart of the gateway frame and entry convergence — the full SmartEdge story now spans gateway, entries, and node.
- `NEW-WS63-SDK-DEV-SKILL.md`: the sensor-per-directory layout follows the hygiene that skill's checklist recommends.
- `NEW-PET-COLLAR-GATEWAY.md`: both are multi-sensor SLE nodes; SmartEdge's per-driver directories vs pet-collar's task-per-sensor shows two organizational styles at the same scale.
