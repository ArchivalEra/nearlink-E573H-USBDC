---
type: harvest
title: yeyeye0212/starflash-pet-collar — WS63 Three-Node Pet Collar (SLE-WiFi Gateway Bridge, MPU6050 State Machine)
language: en
created: 2026-09-13
tags: [ws63, hi3863, sle, wifi, gateway, lwip, udp, mpu6050, dual-radio, harvest]
sources:
  - url: https://github.com/yeyeye0212/starflash-pet-collar
    note: cloned 2026-09-13, pushed 2026-07-13 (~2 months old, not archived), C, full HiSpark WS63 SDK tree
trust: verified
stale_after: 2026-12-13
---

# yeyeye0212/starflash-pet-collar — WS63 Three-Node Pet Collar (SLE-WiFi Gateway Bridge, MPU6050 State Machine)

- Inspection date: 2026-09-13 (staleness check: pushed 2026-07-13, not archived — ALIVE but ~2 months quiet)
- Source root: `/mnt/hdd/nearlink-stuff/starflash-pet-collar`
- Mode: read-only local program inspection; no network, build, hardware, or PCB access
- Scope: dual-radio gateway bridge, SLE data exchange, three-node architecture; PCB/schematics explicitly excluded

## Executive findings

1. Three-node system per README: pet collar (WS63 BearPi + MPU6050 I2C + DF-PLAYER UART speaker), owner handheld (WS63 + keys + serial UI screen + climate sensor), and a phone/PC web page; SLE carries collar-to-handheld traffic, WiFi carries handheld/device-to-phone-PC traffic. [`README.md:1-30`]
2. The reusable core is the `sle_gateway` product sample (1435 lines total): a WS63 dual-radio bridge that pumps bytes between an SLE UART-dialect link and a WiFi UDP datagram endpoint. [`application/samples/products/sle_gateway/`]
3. SLE side keeps the standard dialect: server advertises and sends via `sle_uart_server_send_report_by_handle(buffer, length)`; client side writes via `ssapc_write_req(0, g_sle_uart_conn_id, sle_uart_send_param)` with a single conn_id — the same 1-to-1 lineage as `NEW-TEKI128-MINIMAL-PAIR.md`. [`sle_gateway/sle_gateway.c:119, 223-225`]
4. WiFi side is lwIP: `wifi_connect(SSID, PWD)` then a blocking `socket(AF_INET, SOCK_DGRAM)` bound to a fixed `CONFIG_SERVER_IP:CONFIG_SERVER_PORT`, with a 512-byte `recvBuf` receive loop — datagram relay, no TCP, no framing beyond the SLE UART payload. [`sle_gateway/sle_gateway.c:32-33, 231-261`]
5. The repo ships the full stock sample family alongside the product code (`ble_uart`, `sle_uart`, `sle_uart_1_vs_8`, `sle_gateway`) — confirming the 1-vs-8 topology sample (`NEW-SLE-1V8-VEHICLE.md`) circulates as a stock HiSpark sample, not just that competition repo. [`application/samples/products/`]
6. WiFi credentials are hardcoded demo values in the sample config macros; treat them as non-secrets sample placeholders — do not propagate real credentials into this report (none were read or recorded).

## Boundaries and gaps

- ~2 months without a push; sample-family code is SDK-derived, so "staleness" is low-risk but the project is single-author, zero-star, no CI.
- The gesture/state recognition (MPU6050 threshold logic) and DF-PLAYER command mapping live in the collar-side sources; this pass verified the transport bridge in depth and the sensor/music mapping only via README — the bridge is the transferable part.
- Hardware schematics/3D-print files exist in-repo but are PCB/hardware-design material — excluded per standing rule.
- No encryption on either radio path; UDP endpoint is fixed-IP, so the "web page" side depends on a LAN host.

## Reusable for our stack

- The SLE-report-out / SSAP-write-in + lwIP-UDP-out bridge shape is a 150-line proof that WS63 can run both radios concurrently with a trivial pump loop — relevant to our tri-mode WS73 goals as prior art for SLE+WiFi coexistence at the application layer.
- `send_report_by_handle` (server-initiated notify by handle) vs `ssapc_write_req` (client-initiated) shows both directions of the same UART dialect — mirrors our SSAP server notify gating work.
- Confirms stock-sample diffusion: `sle_uart_1_vs_8` is now in at least two community repos, so future harvest can treat that sample as common knowledge rather than a per-repo discovery.

## Comparison anchors (vs existing reports)

- `NEW-TEKI128-MINIMAL-PAIR.md` / `NEW-SLE-1V8-VEHICLE.md`: same SDK sample lineage; this repo's delta is the dual-radio gateway bridging SLE to WiFi.
- `NEW-NLCHAT-WEB.md`: both expose a phone/PC web surface; NLChat is browser-serial only, this adds a real UDP radio hop.
- `WS63-SLE-EXAMPLES.md`: extends that survey with the sle_gateway dual-radio pattern absent from the stock SDK examples digest.
