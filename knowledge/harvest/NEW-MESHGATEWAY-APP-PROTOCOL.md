---
type: harvest
title: "NearLink-ePaper/MeshGatewayAPP — phone-side spec of the SLE-mesh image protocol (checkpoint + 240-packet bitmaps, FAST/ACK flow control, bit-exact RLE)"
language: en
created: 2026-09-13
tags: [android, ble, gateway, mesh, image-transfer, rle, flow-control, bitmap-ack, harvest]
sources:
  - "https://github.com/NearLink-ePaper/MeshGatewayAPP"
trust: A
stale_after: 2026-12-13
---

# NearLink-ePaper/MeshGatewayAPP — phone-side spec of the SLE-mesh image protocol (checkpoint + 240-packet bitmaps, FAST/ACK flow control, bit-exact RLE)

- Inspection date: 2026-09-13 (staleness check: pushed 2026-03-16, not archived — ~6 months quiet, reference grade)
- Source root: `https://github.com/NearLink-ePaper/MeshGatewayAPP`
- Mode: read-only local program inspection; no network, build, hardware, or PCB access
- Scope: the phone-side half of the SLE-mesh image distribution protocol whose firmware half lives in the WS63E AIGC frame repo

## Executive findings

1. `MeshProtocol.kt` is a complete, versioned wire spec for driving an SLE mesh from a phone via a BLE gateway: magic `0xAA`, downlink commands (unicast 0x01, broadcast 0x02, topology query 0x03, IMG_START/DATA/END/CANCEL 0x04-0x07, checkpoint 0x08, multicast 0x0A) and uplink responses (data 0x81, topo 0x83, IMG_ACK 0x85, result 0x86, missing 0x87, checkpoint-ACK 0x88, progress 0x89). [`MeshGatewayApp/app/src/main/java/com/meshgateway/MeshProtocol.kt:6-31`]
2. The v2 reliability design uses **checkpoints + missing-packet bitmaps**: the gateway probes a target (`AA 08 DST SEG_ID`), the target answers with `SRC TOTAL BITMAP[30]` — a 30-byte bitmap acknowledging 240 packets per checkpoint — and internal checkpoint-ACK/progress messages are consumed by the gateway and not forwarded to the app. [`MeshProtocol.kt:8-12`]
3. v2.1 adds a per-transfer flow-control mode in IMG_START: `XFER 0=FAST` (gateway-managed flow control) vs `XFER 1=ACK` (per-packet confirm with immediate injection), inside a fixed 14-byte image header `AA 04 DST(2) TOTAL(2) PKT(2) W(2) H(2) MODE(1) XFER(1)`. [`MeshProtocol.kt:13-15`]
4. `ImageRleEncoder.kt` implements the Kotlin-side RLE that must round-trip byte-identically with the firmware's `image_rle.c`: literal nibble `0xxxxxxx` = 7 raw pixels, run `1vNccccc` for runs ≥8 with varint length, pixels MSB-first within bytes. [`ImageRleEncoder.kt:3-10`]
5. Cross-repo closure: this app + the `WS63E-NearLink-Mesh-AIGC-Frame` firmware (see `NEW-WS63E-MESH-AIGC-FRAME.md`) together document BOTH endpoints of the same image-over-mesh protocol — rare for this ecosystem, where most repos expose only one side.

## Boundaries and gaps

- ~6 months without a push; Kotlin app for a specific BLE-gateway dongle, no CI/tests.
- The BLE-to-mesh gateway firmware itself is not in this repo (the phone speaks BLE to a gateway box that bridges to SLE mesh).
- Security: no authentication on the mesh control plane; any phone with the gateway's BLE address can drive displays.

## Reusable for our stack

- The checkpoint + missing-bitmap selective-retransmit design (240 packets per 30-byte bitmap) is an efficient bulk-transfer recovery scheme for any multi-hop SLE mesh we build — far cheaper than per-packet ACK at scale, and the FAST/ACK dual-mode shows when each pays off.
- Bit-exact cross-language codec pairing (Kotlin encoder ↔ C decoder) with the byte-order contract written down is the discipline our host/device framing pairs should follow.
- The v2→v2.1 changelog-in-comments style is a tidy way to evolve a wire spec in-tree.

## Comparison anchors (vs existing reports)

- `NEW-WS63E-MESH-AIGC-FRAME.md`: firmware half of the same protocol; read together they form the complete spec.
- `NEW-DSOFTBUS-SLE-STUB.md`: both document reliability scaffolding around SLE transports, from vendor and community sides.
- `SLE-MESH-EPAPER.md`: the original ePaper-mesh repo survey — this app is that system's control surface.
