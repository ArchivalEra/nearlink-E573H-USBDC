---
type: harvest
title: MeshMaxDesktop — third consumer of the mesh image protocol: Electron + noble BLE sender with parallel in-flight chunk upload
language: en
created: 2026-09-13
tags: [electron, noble, ble, image-transfer, in-flight, chunking, mesh, harvest]
sources:
  - url: https://github.com/leion-kk/WS63E-NearLink-Mesh-AIGC-Frame
    note: local clone (pushed 2026-07-08, current); read-only inspection of NearLink_Mesh_Firmware/pc/MeshMaxDesktop/
trust: verified
stale_after: 2026-12-13
---

# MeshMaxDesktop — third consumer of the mesh image protocol: Electron + noble BLE sender with parallel in-flight chunk upload

- Inspection date: 2026-09-13 (same current clone as `NEW-WS63E-MESH-AIGC-FRAME.md`)
- Mode: read-only local program inspection; no build, network, hardware, or PCB access
- Scope: the PC desktop sender — Electron packaging, noble BLE transport, chunk pipelining

## Executive findings

1. `MeshMaxDesktop` v2.2.0 ("Mesh MAX Gateway PC client", MIT, AIoTComm) is an **Electron + Nuxt/React app whose BLE transport runs on `@abandonware/noble`** — the PC speaks BLE directly to the mesh gateway, a third independent implementation of the image protocol after the phone app (`NEW-MESHGATEWAY-APP-PROTOCOL.md`) and the board Web API. [`package.json:1-20`]
2. Its transfer policy mixes reliability modes per packet class: **START/END are acknowledged and serial; IMG_DATA is unacknowledged with multiple packets in flight** (`BLE_MAX_IN_FLIGHT`, `BLE_UPLOAD_PARALLEL = 6`, inter-packet pace 0 ms) — bulk data pipelined, control frames serialized. [`electron/bleImageSend.ts:3-19`]
3. Chunk framing is explicit: `buildDataBuffer(dstAddr, seq, chunk)` lays out a 7-byte header (dst + seq + length) before each `IMG_PKT_PAYLOAD`-sized chunk, and the sender pre-splits the image into a chunk array to know `total` up front — matching the protocol's TOTAL/seq fields from the phone-side spec. [`bleImageSend.ts:56-86`]

## Boundaries and gaps

- Windows build scripts only (electron-builder nsis/portable); no macOS/Linux packaging.
- The noble dependency is unmaintained (`@abandonware/noble`) — a supply-chain note for anyone reusing this client.
- In-flight concurrency (6) vs gateway queue limits was not cross-checked against the firmware side.

## Reusable for our stack

- The mixed-mode transfer policy (serial-acked control frames + parallel unacked data with a small in-flight cap) is a transport pattern our host tooling should mirror: it matches our credit-gated bulk send with explicit control sequencing.
- Three independent implementations of one wire protocol (Kotlin, Electron/TS, firmware C) with bit-exact RLE agreement is the strongest interop evidence set we hold — our protocol docs should cite multi-implementation round-trips as the completeness bar.

## Comparison anchors (vs existing reports)

- `NEW-MESHGATEWAY-APP-PROTOCOL.md` / `NEW-WS63E-MESH-AIGC-FRAME.md`: completes the three-consumer picture of the mesh image protocol.
- `NEW-DS10-SLE-DTU.md`: the parallel-in-flight bulk policy vs DS10's serialized-ACK latency blowup — two answers to the same reliability/throughput tension.
- `NEW-NLCHAT-WEB.md`: another Electron-class desktop client in the ecosystem, but BLE-native instead of serial.
