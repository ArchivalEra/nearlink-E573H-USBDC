---
type: harvest
title: "tethering port profile internals — per-state dispatch FSM and per-address client cache (port_stm.c 566 lines, port_client.c 236 lines)"
language: en
created: 2026-09-13
tags: [sle, port-profile, state-machine, dispatch-table, cache, ssap, tethering, harvest]
sources:
  - "ArchivalEra/nearlink-E573H-USBDC workspace"
trust: A
stale_after: 2026-12-13
---

# tethering port profile internals — per-state dispatch FSM and per-address client cache (port_stm.c 566 lines, port_client.c 236 lines)

- Inspection date: 2026-09-13 (same current clone as `NEW-PORT-PROFILE-FSM.md`)
- Mode: read-only local program inspection; no build, network, hardware, or PCB access
- Scope: the Port Profile FSM implementation mechanics — dispatch structure and the client-side cache

## Executive findings

1. The eight-state port FSM is implemented as a **per-state dispatch table**, not a transition matrix: `portStateMachine[PORT_STATE_BUTT]` maps each state (`PORT_STATE_IDLE` … `PORT_STATE_CONNECTED`) to its own dispatcher function (`PortIdleStateDispatch`, `PortRegisterAppStateDispatch`, …), so each state owns its event handling and transitions live beside the state code. [`services/stack/src/cp/bal/profile/port/src/port_stm.c:42-50`]
2. The client side is a **per-address cache with three lookup keys**: `PortInfoCacheCreate(addr, connParam)` builds a `PortInfoCache_S` per connected peer, and comparator predicates `PortAddrCompFunc`, `PortAppIdCompFunc`, `PortUuidCompFunc` let the cache be searched by device address, registered app id, or service UUID — three natural keys over the same session table. [`services/stack/src/cp/bal/profile/port/src/port_client.c:87-207`]
3. Lifecycle pairs are explicit and symmetric: `PortClientEnable`/`PortClientDisable`, `PortClientRegCbkInner`/`PortClientDeregCbkInner`, plus a `FreePortCache` destructor hook — enable/disable and register/deregister bracket every resource acquisition. [`port_client.c:34-80`]

## Boundaries and gaps

- The dispatcher internals (event-to-action edge labels) live inside the per-state functions; only the dispatch structure and cache layer were read this pass.
- `PortInfoCache_S` full field layout was not dumped; the connection-parameter subset is visible from the create signature.

## Reusable for our stack

- The per-state dispatch pattern keeps FSM edge logic local to each state — a cleaner structure than a global transition matrix for our SSAP link/session machines as state count grows.
- The three-key session cache (addr/appId/uuid) is the natural data structure for our SSAP server's per-peer session bookkeeping (the clientConfigs vector need from `NEW-SSAPS-MULTI-OPERATIONS-CORRECTION.md` maps onto `PortAppIdCompFunc`-style lookup).
- Symmetric enable/disable + register/deregister bracketing is a leak-resistant API discipline for our transport layers.

## Comparison anchors (vs existing reports)

- `NEW-PORT-PROFILE-FSM.md`: that report captured the state/event enums; this one captures the implementation structure.
- `NEW-SSAPS-MULTI-OPERATIONS-CORRECTION.md`: the per-address cache here parallels the server-side clientConfigs vectors.
- `NEW-HIDITING-SLE2-EVIDENCE.md`: the 2.0 `bs_sle_port_create_local_port/remote_port` API is the successor interface to this 1.x implementation.
