---
type: harvest
title: teki128/nearlink — Minimal 564-line SLE Client/Server Pair (WS63 SDK, fixed-address dialect)
language: en
created: 2026-09-13
tags: [sle, ws63, ssap, minimal-sample, fixed-address, client-server, harvest]
sources:
  - url: https://github.com/teki128/nearlink
    note: cloned 2026-09-13, pushed 2026-09-11 (fresh, not archived), C, 4 files / 564 lines
trust: verified
stale_after: 2026-12-13
---

# teki128/nearlink — Minimal 564-line SLE Client/Server Pair (WS63 SDK, fixed-address dialect)

- Inspection date: 2026-09-13 (staleness check: pushed 2026-09-11, not archived — ALIVE)
- Source root: `/mnt/hdd/nearlink-stuff/teki128-nearlink`
- Mode: read-only local program inspection; no network, build, hardware, or PCB access
- Scope: complete minimal SLE client/server exchange over the WS63/Hi3863 SDK SSAP API

## Executive findings

1. The whole protocol story fits in 564 lines across 3 files (`nearlink_client.c` 276, `nearlink_server.c` 266, `nearlink_common.h` 22) — the smallest complete SSAP client+server pair we have archived, useful as a dialect baseline. [`wc -l`, repo root]
2. Fixed-address connect dialect: server address `12:34:56:78:9A:BC`, client address `11:22:33:44:55:66` (the latter equals the standard SDK sample address also used by our HHD-01 violin board), both public type; the client connects by static address rather than by filtering advertisement payload. [`nearlink_common.h:10-11`] [`nearlink_client.c:26-28`]
3. Server bring-up is the canonical 5-step SSAP sequence: `ssaps_register_server` (app_uuid `0x00A0`) → `ssaps_add_service_sync` (svc `0x01A0`, is_primary=true) → `ssaps_add_property_sync` (prop `0x02A0`, value max 32) → `ssaps_add_descriptor_sync` (permissions READ|WRITE, i.e. CCCD) → `ssaps_start_service`; on connect it pushes MTU via `ssaps_set_info`. [`nearlink_server.c:24-52, 100`]
4. Client bring-up mirrors it: `ssapc_register_client` (same app_uuid `0x00A0`), `sle_set_local_addr`, then default connect params — `gt_negotiate = SLE_ANNOUNCE_ROLE_G_CAN_NEGO`, scan 200/20, fixed `min=max=SLE_CONN_INTERVAL(100)`, supervision timeout 500; scan itself is ACTIVE on 1M PHY with interval=window=100 and duplicate filtering on. [`nearlink_client.c:38-60`]
5. Data exchange is one property, write-driven: client task periodically issues `ssapc_write_req` with 4-byte payloads and confirms via `write_cfm_cb`; server answers reads with `ssaps_send_response` from its read callback; MTU declared 512, prop cap 32. [`nearlink_client.c:216-258`] [`nearlink_server.c:110-120`] [`nearlink_common.h:13-14`]
6. Threading: one osal task each side (priority 25, 4096 stack) drives the exchange loop after SLE enable; link state tracked with a single `g_conn_id` + `link_ready` flag — strictly 1-to-1. [`nearlink_common.h:6-7`] [`nearlink_client.c:16-20`]

## Boundaries and gaps

- No security: no pairing/bonding, no permissions beyond the descriptor's READ|WRITE; not a production pattern.
- Fixed addresses mean zero discovery logic — the seek callback is unused for filtering; nothing scales beyond one peer.
- No user README; CMakeLists targets a HiSpark Studio tree that is not vendored here, so the pair does not build standalone.
- Author-side "demo" tag: treat as a reference skeleton, not maintained product code (pushed 2026-09-11 but content is SDK-sample-derived).

## Reusable for our stack

- Best minimal checklist for bring-up smoke tests of our WS73 host SSAP stack: the exact server call order and the client params above give a wire-level acceptance target (app_uuid 0x00A0 family, MTU exchange, one property + CCCD).
- The `gt_negotiate = G_CAN_NEGO` + fixed-interval connect param block is a compact reference for our connection-param plumbing.
- Confirms the SDK sample default client address (11:22:33:44:55:66) reused across community repos — our HHD-01 identity report already tracks this; rename guidance applies.

## Comparison anchors (vs existing reports)

- `SSAP-DIALECT-COMPARISON.md`: this pair is the smallest expression of the device-firmware dialect (5-step server bring-up, client write loop) — consistent with that report's API mapping.
- `NEW-SLE-1V8-VEHICLE.md`: same SDK generation; the delta here is strict 1-to-1 with static addressing vs the vehicle's scan-fill loop for 8.
- `HHD01-BOARD.md`: the shared default addresses tie this sample family to our violin board identity.
