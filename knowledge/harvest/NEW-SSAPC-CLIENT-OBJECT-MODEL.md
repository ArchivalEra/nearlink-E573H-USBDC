---
type: harvest
title: "OHOS ssapc_client.c client view — SSAP services are a property/method/event object model; find responses decode into a per-address member cache"
language: en
created: 2026-09-13
tags: [ohos, ssap, client, object-model, property-method-event, cache, v10, harvest]
sources:
  - "ArchivalEra/nearlink-E573H-USBDC workspace"
trust: A
stale_after: 2026-12-13
---

# OHOS ssapc_client.c client view — SSAP services are a property/method/event object model; find responses decode into a per-address member cache

- Inspection date: 2026-09-13; client-side deep-dive completing the SSAP servm picture
- Mode: read-only local program inspection; no build, network, hardware, or PCB access
- Scope: discovery decode paths, the member-type semantic layer, and cache population from the client view

## Executive findings

1. **SSAP services carry an object-model semantics, not just GATT-like characteristics**: every property item is classified `MEMBER_TYPE_PROPERTY` (data), `MEMBER_TYPE_METHOD` (callable), or `MEMBER_TYPE_EVENT` (notification), each in STANDARD and VENDOR variants (`ITEM_TYPE_STD_PROPERTY` / `ITEM_TYPE_VENDOR_METHOD` / …). The OHOS dialect models a remote object interface over SSAP — methods and events are first-class member classes. [`ssapc_client.c:547-565`]
2. The client **populates its per-address cache during discovery decode**: `SsapcCacheCharacter(addr, prty, memberType)` routes each decoded property into `SsapcCachePrty` / `SsapcCacheMethod` / `SsapcCacheEvent` buckets, and found services land via `SsapcCacheServ` at two decode sites — the cache is filled as a side effect of discovery, not as a separate pass. [`ssapc_client.c:332-343, 215-235`]
3. Find responses decode through **V10-variant paths keyed on response item type**: `SSAP_DecodePrimaryServiceByUuidV10` switches on `rsp->ctrl.itemType` (STANDARD/CUSTOMIZE → single-type decode; MIX → mixed decode) — the client mirrors the server's tri-modal item typing from `NEW-SSAPS-FIND-REFERENCE.md`. [`ssapc_client.c:320-330`]
4. Property decoding is length-audited field-by-field: `SSAP_DecodeSingleProperty` computes `needSize` (handle + find-operation + uuidLen + descriptor-count + descriptors) and rejects short payloads before parsing, then walks handle (little-endian u16), UUID, operation value, and descriptors with an explicit cursor. [`ssapc_client.c:354-380`]
5. Value indications are rebuilt per link: `SSAPC_ValueIndBuildSingleItemPkt` / `SSAPC_ValueIndParseItems` handle the notify indication PDU at the client, with `CountReqHandleNum` pre-counting request handles for multi-operation sizing. [`ssapc_client.c:920, 1216-1235`]

## Boundaries and gaps

- The METHOD invocation path (how MEMBER_TYPE_METHOD items are called) was located in the cache layer but not traced to its write-exchange format this pass.
- Vendor item-type semantics (VENDOR_PROPERTY etc.) beyond the classification mapping are SDK-application defined, not documented in-tree.

## Reusable for our stack

- The property/method/event member classification is the strongest argument yet that our SSAP stack should expose an interface-description layer (like a slim IDL) on top of properties: our feature_mgr and server API currently model data-only properties, while the upstream dialect treats methods and events as distinct member classes with their own cache buckets and (presumably) call/notify semantics.
- Decode-with-computed-needSize cursor walking is the exact hardening pattern for our codec's client-side parsers.
- Discovery-aside cache population (cache fills during find decode) eliminates a rescan pass — worth adopting in our client cache design.

## Comparison anchors (vs existing reports)

- `NEW-SSAP-SERVM-MODULE-MAP.md`: positions this client module inside the full map; the cache it fills is the `ssapc_cache.c` described there.
- `NEW-SSAPS-FIND-REFERENCE.md`: server-side tri-modal typing; this is the client-side mirror.
- `SSAP-DIALECT-COMPARISON.md`: the object-model layer (METHOD/EVENT members) is a dialect dimension that report did not yet carry — OHOS-side semantics go beyond AT-style command strings.
