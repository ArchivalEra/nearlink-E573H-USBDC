---
type: harvest
title: "OHOS ssapc_app.c — the client application layer: per-appId link state, async registration callbacks, and an interaction timeout knob"
language: en
created: 2026-09-13
tags: [ohos, ssap, client-app, appid, link-state, timeout, async, harvest]
sources:
  - "ArchivalEra/nearlink-E573H-USBDC workspace"
trust: verified
stale_after: 2026-12-13
---

# OHOS ssapc_app.c — the client application layer: per-appId link state, async registration callbacks, and an interaction timeout knob

- Inspection date: 2026-09-13 (current clone); completing the SSAP client-side map beyond ssapc_client.c
- Mode: read-only local program inspection; no build, network, hardware, or PCB access
- Scope: the client app-registration and lifecycle layer between the API surface and the protocol core

## Executive findings

1. Client state is **per-appId**: `SsapcAppSetLinkState(appId, newState)` / `SsapcAppGetLinkState(appId)` — each registered client application owns its own link-state slot (the client analogue of the server's per-peer clientConfigs vectors). [ssapc_app.c:86-94]
2. **Registration is asynchronous with synthesized success callbacks**: `SsapcAppRegister` → `SsapcAppRegisterAsync` invokes `wanted->cb->onRegisterApp(appId, &addr, NLSTK_ERRCODE_SUCCESS)` — even local registration flows through the same callback shape a remote result would, keeping app code uniform. [ssapc_app.c:174-193]
3. **An interaction timeout is a first-class knob**: `SsapcAppSetInteractionTimeout(param)` — the client can bound how long an interaction may take, separate from link supervision timeouts. [ssapc_app.c:212]
4. Cleanup is centralized: `SsapcAppNotifyClientCleanUp` — one entry notifies all registered clients during teardown. [ssapc_app.c:148]

## Boundaries and gaps

- The discovery-completion callback (`SsapcAppDiscServCompCb`) and its cache interplay were identified but not traced into ssapc_cache.c this pass.

## Reusable for our stack

- Per-appId state slots + async-uniform registration callbacks + an interaction-timeout knob are the three client-lifecycle features our SSAP client implementation should carry — they complete the server-side features already harvested.

## Comparison anchors (vs existing reports)

- `NEW-SSAP-SERVM-MODULE-MAP.md`: this layer sits between `ssapc_client_api.c` and `ssapc_client.c` in the map.
- `NEW-SSAPC-CLIENT-OBJECT-MODEL.md`: the appId-scoped link state is where member-type caches hang per app.
- `NEW-SSAPS-MULTI-OPERATIONS-CORRECTION.md`: server-side per-peer state mirrors client-side per-app state — symmetric bookkeeping.
