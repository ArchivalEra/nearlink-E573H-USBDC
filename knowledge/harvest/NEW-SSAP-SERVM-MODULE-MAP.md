---
type: harvest
title: "OHOS SSAP servm module map — 9,601 lines server+client with a peer service cache and client link SM; the reference architecture for our stack build-out"
language: en
created: 2026-09-13
tags: [ohos, ssap, module-map, server, client, cache, link-sm, architecture, harvest]
sources:
  - "ArchivalEra/nearlink-E573H-USBDC workspace"
trust: A
stale_after: 2026-12-13
---

# OHOS SSAP servm module map — 9,601 lines server+client with a peer service cache and client link SM; the reference architecture for our stack build-out

- Inspection date: 2026-09-13; full module enumeration of the SSAP servm suite
- Mode: read-only local program inspection; no build, network, hardware, or PCB access
- Scope: module inventory with sizes and responsibilities, plus the two structures our reports had not yet covered — the client service cache and the client link state machine

## Executive findings

1. The complete SSAP servm suite is **9,601 lines across two symmetrical halves**: server side 4,652 lines (`ssaps_server.c` 1509 core+read, `ssaps_server_find.c` 1498, `ssaps_server_write.c` 940, `ssaps_server_app.c` 630 registration, `ssaps_server_api.c` 75 shims) and client side 4,949 lines (`ssapc_client.c` 1694, `ssapc_app.c` 1305, `ssapc_cache.c` 1009, `ssapc_app_link_sm.c` 623, `ssapc_client_api.c` 227, `ssapc_app_util.c` 91). [`wc -l` over `services/stack/src/cp/bsl/sle/servm/ssap/src/`]
2. The client keeps a **peer service cache** (`SsapcCache_S` with `SsapCacheServ_S`/`SsapCacheServInfo_S` entries, 1009 lines): discovered remote services/properties are cached per link so subsequent reads/writes skip re-discovery — the client-side counterpart of the server's per-peer clientConfigs vectors. [`ssapc_cache.h:29-57`]
3. The client carries its own **link state machine** (`ssapc_app_link_sm.c`, 623 lines) separate from the connection manager's link state — application-level link lifecycle (register, exchange, discover, ready) is modeled independently of the ACL, mirroring the server-side REGISTER_APP-before-CREATE_LINK ordering seen in the port FSM. [`ssapc_app_link_sm.c`]
4. Shared infrastructure is factored out cleanly: `ssap_link.c`/`ssap_link_state.c` (link plane), `ssap_handle.c` (handle allocation), `ssap_manager.c`, `ssap_common.c`, `ssap_utils.c` — the server and client halves share the link and handle planes rather than duplicating them. [module listing]
5. Reading map for our build-out: our `assets/stack/ssap` currently models codec + link + server + feature_mgr (~2,400 lines); the OHOS reference is roughly 4× that with the delta concentrated in the find module, the multi-operation loops, the client cache, and the app link SM — each already covered by dedicated harvest reports this session.

## Boundaries and gaps

- Line counts are structural evidence, not quality judgments; OHOS code carries SDF/NLSTK framework dependencies throughout.
- The controller-facing plane (`ssap_handle.c` handle allocation semantics) was not read line-by-line this pass.
- Client cache eviction/invalidation policy (what happens on link loss) is the one behavior this enumeration did not verify.

## Reusable for our stack

- This module map is the build-out plan for our SSAP stack: the missing pieces (find module, multi-op loops, client cache, app link SM) are each 600–1500 lines — matching the effort scale our LTO/deep-module planning already assumed.
- The shared-link-plane + per-role halves structure (server/client over one ssap_link.c) is the layout our codec/link split should converge to.
- The client service cache justifies a `session cache` module in our host stack: remote GATT-like structure discovered once per link, cached for the link lifetime.

## Comparison anchors (vs existing reports)

- `NEW-SSAPS-FIND-REFERENCE.md` / `NEW-SSAPS-MULTI-OPERATIONS-CORRECTION.md`: those deep-dives sit inside this map at their enumerated positions.
- `NEW-PORT-PROFILE-INTERNALS.md`: the port profile's cache+FSM pattern is the same architecture applied at the profile layer.
- `SSAP-IMPLEMENTATION-PLAN.md` / `SSAP-PLAN-AUDIT.md`: our original implementation plan can now be re-baselined against the full reference at 9.6K lines.
