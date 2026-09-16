---
type: harvest
title: "Sky05y/smart_cabinet — full-chain multi-node SLE reference (2 sensor nodes -> SLE center -> WiFi HTTP cloud -> web dashboard)"
language: en
created: 2026-09-13
tags: [ws63, h3863, sle, multi-node, http, lwip, cloud, sensor, full-chain, harvest]
sources:
  - "https://github.com/Sky05y/smart_cabinet"
trust: A
stale_after: 2026-12-13
---

# Sky05y/smart_cabinet — full-chain multi-node SLE reference (2 sensor nodes -> SLE center -> WiFi HTTP cloud -> web dashboard)

- Inspection date: 2026-09-13 (staleness check: pushed 2026-06-28, not archived — ALIVE but ~2.5 months quiet)
- Source root: `https://github.com/Sky05y/smart_cabinet`
- Mode: read-only local program inspection; no network, build, hardware, or PCB access
- Scope: 1-master+2-slave SLE topology, sensor payload set, cloud HTTP upload path

## Executive findings

1. The project positions itself as a complete-chain open reference (sensor acquisition -> SLE transport -> cloud upload -> web monitoring), which the code confirms: product roles live in `application/samples/ws63_center_dev` (SLE client master), `ws63_node_1` and `ws63_node_2` (SLE server slaves). [`README.md`, `application/samples/`]
2. Slave node sensor set is unusually rich for a sample: DHT11 temperature/humidity, BH1750 light, MQ-series gas via ADC, fingerprint reader, electric lock, and SSD1306 OLED display — each a separate module under `ws63_node_1/{include,src}` with its own driver. [`ws63_node_1/include/` listing]
3. The center device maintains a **multi-server tracking table** (`g_servers[i].connected`, `g_servers[i].device_no`) — device-number-keyed state per connected slave, the same scan-fill topology family as `NEW-SLE-1V8-VEHICLE.md` but with per-node application identity. [`ws63_center_dev/src/main.c:241`]
4. Cloud upload is **raw HTTP over lwIP**: the center joins WiFi as STA (`wifi_hotspot` STA state machine, `wifi_sta_connect`), then `http_post_data(lux, alcohol, t_int, t_dec, ...)` opens a socket and writes a hand-built `POST /api/upload/ HTTP/1.1` with Host header — no MQTT/SDK, deliberately minimal. [`ws63_center_dev/src/main.c:13-17, 194, 246-271`]
5. The wireless leg keeps the standard SLE UART dialect skeleton (ssapc_write_param, seek callbacks) — the project's originality is the payload taxonomy and the cloud chain, not the link layer. [`ws63_center_dev/sle_uart_client/`]

## Boundaries and gaps

- ~2.5 months without a push; student-competition-grade code (bilibili demo, single author, no CI/tests).
- HTTP is plaintext on a WiFi LAN; the cloud endpoint is a private server (`/api/upload/`) — the web dashboard backend is outside the repo's product tree.
- Fingerprint/lock drivers are integrations of vendor modules; security posture of the cabinet logic (auth for unlock commands) was not audited in this pass.
- The 1.2GB tree is the full vendor SDK; only the three product sample dirs were reviewed.

## Reusable for our stack

- The cleanest public template for a "SLE sensor network with a WiFi egress gateway" — the exact three-plane shape (SLE collection plane, multi-slave state table, HTTP egress) our dongle could expose as a reference design.
- Device-number-keyed multi-server table is a simpler alternative to UUID-based session maps when nodes are homogeneous.
- Hand-rolled HTTP POST over lwIP (no cloud SDK) keeps the egress auditable — a pattern worth keeping for our own cloud reporting.

## Comparison anchors (vs existing reports)

- `NEW-PET-COLLAR-GATEWAY.md`: pet-collar bridges SLE->UDP; smart_cabinet scales it to multi-node + HTTP + dashboard — the two ends of the gateway complexity spectrum.
- `NEW-SLE-1V8-VEHICLE.md`: same multi-slave scan-fill topology; smart_cabinet adds per-node identity and cloud egress.
- `NEW-BEARPI-H3863-DOCS.md`: rides the same BearPi H3863 sample skeleton the manual documents.
