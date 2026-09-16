---
type: harvest
title: "leion-kk/WS63E-NearLink-Mesh-AIGC-Frame — JPEG-over-SLE-Mesh Distributed E-Paper (AODV-style route table, on-chip decode + RLE direct drive)"
language: en
created: 2026-09-13
tags: [ws63e, sle, mesh, routing, aodv, jpeg, rle, epaper, streaming, wifi, harvest]
sources:
  - "/mnt/hdd/nearlink-stuff/WS63E-NearLink-Mesh-AIGC-Frame"
trust: verified
stale_after: 2026-12-13
---

# leion-kk/WS63E-NearLink-Mesh-AIGC-Frame — JPEG-over-SLE-Mesh Distributed E-Paper (AODV-style route table, on-chip decode + RLE direct drive)

- Inspection date: 2026-09-13 (staleness check: pushed 2026-07-08, not archived — ALIVE)
- Source root: `/mnt/hdd/nearlink-stuff/WS63E-NearLink-Mesh-AIGC-Frame`
- Mode: read-only local program inspection; no network, build, hardware, or PCB access
- Scope: mesh routing design, image streaming pipeline, gateway/cloud split

## Executive findings

1. System shape: a WS63E interactive gateway (OLED + knob) builds a NearLink Mesh network with AODV-style on-demand routing, then multi-hop-distributes AI-generated or cloud-gallery image streams to 7.3-inch and 4-inch e-paper display nodes over SLE Mesh only — "cloud hosting + terminal streaming direct-drive", zero local storage on display nodes. [`README.md:1-4`]
2. The mesh core implements a real route table, not flooding: `mesh_route_entry_t {dest_addr, next_hop, hop_count, lifetime_ms}` with lookup/update/remove/remove_by_next_hop/cleanup ops and a lifetime expiry — per-destination next-hop routing in the AODV family, with the gateway as the sweep origin that "maps terminal addresses, hop counts and screen specs". [`NearLink_Mesh_Firmware/sle_mesh_networking/mesh/network/mesh_route.h:24-48`]
3. The image transport pipeline is decode-on-device: chunks arrive with a declared `total_bytes` (OOM guarded against `IMG_RX_BUF_SIZE`), land in an on-chip-RAM streaming buffer (`mesh_img_buf`), are decoded with the vendored **TJpgDec JPEG decoder**, converted through **RLE** (`image_rle`, total_pixels-oriented) to e-paper-friendly data, and driven to the panel via SPI in a receive-while-refresh loop. [`mesh/image/image_receiver.h:65-135`, `mesh/image/` file set]
4. Transport endpoint is the stock SLE UART dialect (`sle_mesh_networking/sle_uart_client` + `sle_uart_server` + per-role `epaper_4in`/`epaper_7in3` builds) — the mesh layer sits above the same 1-to-1 sample lineage every other WS63 repo uses, adding routing and framing. [`sle_mesh_networking/` tree]
5. The gateway also runs a BLE side (`mesh/gateway/ble_gateway.c`) — the AI-painting path goes WiFi (Amap API for environment data) to a `Cloud_AIGC_Server` while display distribution goes Mesh; two radios, two purposes, same node. [`mesh/gateway/ble_gateway.c`, repo README]

## Boundaries and gaps

- ~2 months quiet, single author, zero stars; no CI, no tests directory — research-grade code.
- AODV control packets (RREQ/RREP flooding for route discovery) are not named as such in headers; only the route-table side is confirmed — discovery signaling may live in `mesh_transport` or be simplified to gateway-driven collection.
- TJpgDec and the cloud AIGC server internals were not audited; hardware schematics/3D-print material excluded per standing rule.
- No security layer anywhere on the mesh path (no encryption/authentication on image chunks).

## Reusable for our stack

- Second, independent NearLink-Mesh routing design (per-destination route table with lifetime) — directly comparable with the leader-rooted tier routing digested in `NEW-SLEMESH-RUST.md`; two public references now exist for mesh-over-SLE engineering.
- JPEG-in-transit + on-device decode + RLE-for-e-paper is a bandwidth-efficient pattern for any SLE display/streaming use; applicable to WS73 dongle-driven displays.
- "Two radios, two purposes" gateway (WiFi for cloud/AI, Mesh for distribution) is prior art for our tri-mode coexistence story at the application layer, alongside `NEW-PET-COLLAR-GATEWAY.md`.

## Comparison anchors (vs existing reports)

- `NEW-SLEMESH-RUST.md`: leader-rooted tier routing vs this per-destination lifetime route table — the two public SLE mesh routing styles now both archived.
- `SLE-MESH-EPAPER.md`: same e-paper-over-mesh application family from NearLink-ePaper; this repo adds JPEG/RLE streaming and an AIGC cloud loop.
- `NEW-PET-COLLAR-GATEWAY.md` / `NEW-WILDLINK-SENSOR-PAIR.md`: completes the multi-radio node pattern set on the WS63 family.
