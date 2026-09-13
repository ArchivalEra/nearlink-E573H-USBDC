---
type: harvest
title: OHOS SSAP service registry + handle range allocator — cache-based registration, remove-by-handle-range, and a splitting block allocator for 16-bit handles
language: en
created: 2026-09-13
tags: [ohos, ssap, service-registry, handle-allocation, range-block, gatt-handles, harvest]
sources:
  - url: https://github.com/openharmony/communication_nearlink_service
    note: local clone at b6c6c48 (current); read-only inspection of ssaps_service.{c,h}, ssaps_service_param.c, ssap_handle.c
trust: verified
stale_after: 2026-12-13
---

# OHOS SSAP service registry + handle range allocator — cache-based registration, remove-by-handle-range, and a splitting block allocator for 16-bit handles

- Inspection date: 2026-09-13 (current clone); completing the SSAP server infrastructure series
- Mode: read-only local program inspection; no build, network, hardware, or PCB access
- Scope: how services are registered, cached, removed — and how 16-bit handles are allocated

## Executive findings

1. Service registration is **cache-based with typed param structs**: `SSAPS_CacheService` / `CacheProperty` / `CacheDescriptor` take `SSAP_ParamAdd*` structures — services carry (uuid, `SSAP_ServiceRegisterCallback`, `serviceType`), properties (uuid, permission, operation, initial value), descriptors (permission, type, operation, value). Registration carries a **callback**, implying service-add completes asynchronously. [`ssaps_service.h:22-47`]
2. **Removal is by handle range**: `SSAP_ParamRemoveService_S {startHandle, endHandle}` — a service occupies a 16-bit handle interval and is removed as a range, GATT-style. [`ssaps_service.h:44-46`]
3. The 16-bit handle space is managed by a **splitting range-block allocator**: `SSAP_HandleAllocator_S` holds `blocks[]` of `SSAP_HandleRange_S {start, end}`, initialized as a single block `[SSAP_HANDLE_MIN, SSAP_HANDLE_MAX]` with a capacity for block fragmentation; init is single-shot (double-init rejected), destruction frees the block array. This is the standard "split on allocate, coalesce on free" pattern sized for SSAP handles. [`ssap_handle.c:9-35`]
4. Registry API surface: `SSAPS_StartService`, `SSAPS_RemoveService`, `SSAPS_GetServices` (vector accessor), `SSAPS_ServiceInit/DeInit` — the full CRUD cycle for the service table. [`ssaps_service.h:49-58`]

## Boundaries and gaps

- The split/coalesce algorithms inside `SSAP_AllocHandle`/free were not read line-by-line; only the allocator structure was verified.
- Property permission/operation semantics (which operations each permission gate covers) inherit from `NLSTK_Ssap*` types outside this pass.

## Reusable for our stack

- Our SSAP server's service table should adopt: cache-based registration with an async-add callback, remove-by-handle-range, and a range-block handle allocator — the three together handle dynamic service lifecycle cleanly (the hot-update scenario our SERVICE_CHANGE work targets).
- The allocator's single-shot init and capacity-bounded fragmentation are simple invariants worth copying.

## Comparison anchors (vs existing reports)

- `NEW-SSAP-SERVM-MODULE-MAP.md`: positions these modules in the server half's registry layer.
- `NEW-SSAPS-FIND-REFERENCE.md`: the find family queries exactly this registry cache.
- `assets/stack/ssap` ssap_server.c: our current add_service path can be re-baselined against this registry design for the dynamic-service feature.
