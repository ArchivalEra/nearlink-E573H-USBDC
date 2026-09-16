---
type: harvest
title: "OHOS ssaps_service_param.c — the SSAP_Service_S memory topology confirmed in destructors: properties, references, methods, events, descriptors as owned vectors"
language: en
created: 2026-09-13
tags: [ohos, ssap, memory-topology, service-struct, methods, events, harvest]
sources:
  - "ArchivalEra/nearlink-E573H-USBDC workspace"
trust: A
stale_after: 2026-12-13
---

# OHOS ssaps_service_param.c — the SSAP_Service_S memory topology confirmed in destructors: properties, references, methods, events, descriptors as owned vectors

- Inspection date: 2026-09-13 (current clone); the param-validation/destructor layer of the service registry
- Mode: read-only local program inspection; no build, network, hardware, or PCB access

## Executive findings

1. `FreeService` exposes the **complete SSAP_Service_S memory topology**: a service owns five vectors — `properties`, **`references`**, **`methods`**, **`events`**, `descriptors` — the property/method/event object model (from `NEW-SSAPC-CLIENT-OBJECT-MODEL.md`) is visible at the C-struct level on the server side too, plus a `references` vector (included-service links, the GATT "secondary service reference" analogue). [`ssaps_service_param.c:8-19`]
2. `FreeProperty` shows properties own their descriptors and their initial value blob — the ownership tree is service → property → (descriptors, value), two levels deep. [`ssaps_service_param.c:22-31`]
3. The layer is the deep-free half of the registry: param structs in, validated vectors owned, destructors exhaustive — no leak paths left implicit.

## Boundaries and gaps

- Only the destructor half was read; the validation (param-check) half of the 291 lines was not line-audited.

## Reusable for our stack

- Our `assets/stack/ssap` service struct should grow `methods` and `events` vectors alongside properties — the upstream memory topology confirms they are first-class server-side members, not client-side fictions.
- The `references` vector (service-to-service links) is a feature our registry lacks.

## Comparison anchors (vs existing reports)

- `NEW-SSAP-REGISTRY-HANDLE-ALLOC.md`: the registry layer this frees into.
- `NEW-SSAPC-CLIENT-OBJECT-MODEL.md`: client-side member-type cache; this is the server-side storage twin.
