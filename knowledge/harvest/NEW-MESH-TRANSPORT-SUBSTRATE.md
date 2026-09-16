---
type: harvest
title: "AIGC mesh transport substrate — dual-role connection pool with chip-limited 1-server+4-client capacity and a ring-buffer dedup cache for forwarding"
language: en
created: 2026-09-13
tags: [ws63e, mesh, connection-pool, capacity, dedup, ring-buffer, forwarding, harvest]
sources:
  - "ArchivalEra/nearlink-E573H-USBDC workspace"
trust: verified
stale_after: 2026-12-13
---

# AIGC mesh transport substrate — dual-role connection pool with chip-limited 1-server+4-client capacity and a ring-buffer dedup cache for forwarding

- Inspection date: 2026-09-13 (same current clone as `NEW-WS63E-MESH-AIGC-FRAME.md`)
- Mode: read-only local program inspection; no build, network, hardware, or PCB access
- Scope: the transport substrate under the route table — connection pool, capacity constants, forwarding dedup

## Executive findings

1. The mesh transport layer maintains a **dual-role connection pool** (`mesh_conn_pool_t`): a fixed-size `entries[MESH_MAX_CONNECTIONS]` array with per-role counters — `server_count` (inbound, capped `MESH_MAX_SERVER_CONN = 1`, commented as a chip hard limit) and `client_count` (outbound, capped `MESH_MAX_CLIENT_CONN = 4`, also chip-limited). Each mesh node therefore anchors one inbound SLE link and reaches up to four peers outbound — the physical constraint that shapes the mesh into a tree. [`mesh_transport.h:24-33`], [`mesh_config.h:74-75`]
2. Pool lookups are **dual-keyed**: `mesh_transport_find_by_conn_id` and `find_by_mesh_addr` — the stack translates between SLE connection handles and 16-bit mesh addresses at this single layer, and role-specific connect/disconnect callbacks (`on_server_connected/_disconnected`, `on_client_connected/_disconnected`) keep the pool current. [`mesh_transport.h:36-44`]
3. The forwarding engine suppresses duplicates with a **ring-buffer dedup cache**: `g_dedup_cache[MESH_DEDUP_CACHE_SIZE]` of `(src_addr, seq_num) → msg_id` entries, newest-first reverse scan (`g_dedup_head` cursor, modular arithmetic) — flood/forwarded frames are checked-and-added in one pass, with the hot window kept cache-resident. [`mesh_forward.c:26-83`]
4. Layer separation is clean: `mesh_transport` (who is connected) → `mesh_route` (where to send) → `mesh_forward` (repeating others' traffic with dedup) → `mesh_main` (wiring) — the substrate is independent of the image payload above it. [mesh/network/ tree]

## Boundaries and gaps

- The 1-server limit comment says "chip hard limit" — whether that is the WS63E's SLE server-connection ceiling (vs a firmware choice) was not verified against the vendor docs.
- MESH_DEDUP_CACHE_SIZE's value and the msg_id construction were not dumped; the eviction behavior under churn is therefore unquantified.
- Forwarding policy (hop limit, TTL) beyond dedup was not examined this pass.

## Reusable for our stack

- The dual-role pool with per-role caps documents the real WS63-family multi-connection budget (1 inbound + 4 outbound) — a hard planning number for any mesh or star topology we build on SLE, complementing the 8-connection client limit from the 1v8 vehicle digest.
- A ring-buffer dedup keyed by (source, sequence) is the minimal correct flood-suppression primitive — far cheaper than per-flow tracking, and reusable in any flooding we do.

## Comparison anchors (vs existing reports)

- `NEW-WS63E-MESH-AIGC-FRAME.md`: the route table and image pipeline sit on this substrate.
- `NEW-SLE-TEAM-MESH-V456.md`: that team mesh's flood/direct route types would need exactly this dedup layer; the 1+4 capacity explains its topology choices.
- `NEW-SLE-1V8-VEHICLE.md`: 8 client connections (vehicle, pure-client role) vs this 1-server+4-client mixed budget — the role-mix changes the per-role ceilings.
