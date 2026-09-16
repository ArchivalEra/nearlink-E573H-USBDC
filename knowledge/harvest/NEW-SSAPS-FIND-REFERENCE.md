---
type: harvest
title: "OHOS ssaps_server_find.c — the authoritative FIND-family server reference (FIND_STRUCTURE / _BY_UUID, V10 framing split, MTU-chunked responses)"
language: en
created: 2026-09-13
tags: [ohos, ssap, find-by-uuid, find-structure, server, mtu, v10, reference-implementation, harvest]
sources:
  - "/mnt/hdd/nearlink-stuff/communication_nearlink_service"
trust: verified
stale_after: 2026-12-13
---

# OHOS ssaps_server_find.c — the authoritative FIND-family server reference (FIND_STRUCTURE / _BY_UUID, V10 framing split, MTU-chunked responses)

- Inspection date: 2026-09-13; deep-dive of the module our SSAP stack flagged as missing (FIND_BY_UUID)
- Source root: `/mnt/hdd/nearlink-stuff/communication_nearlink_service` (fresh, see staleness note in `NEW-OHOS-NEARLINK-SEPT-INCREMENT.md`)
- Mode: read-only local program inspection; no build, network, hardware, or PCB access

## Executive findings

1. The FIND family server side is a dedicated 1498-line module: `ssaps_server_find.c` handles both `SSAP_FIND_STRUCTURE_REQ` and `SSAP_FIND_STRUCTURE_BY_UUID_REQ`, answering with the matching `_RSP` message code — one handler family covers find-structure and find-by-uuid. [`services/stack/src/cp/bsl/sle/servm/ssap/src/ssaps_server_find.c:364-378`]
2. **Every find path has a V10 twin** (`FindPrimaryServiceListV10`, `BuildPrimaryServiceInfoV10`, `InfoEmplaceBackAndCheckFreeV10`, `BuildPrimaryServicePayloadV10`): the implementation carries explicit SSAP 1.0 vs newer framing variants side by side — protocol versioning is a first-class dimension in the reference implementation, not an afterthought. [`ssaps_server_find.c:109-207`]
3. UUID typing is tri-modal: `CheckFindItemType` distinguishes STANDARD / CUSTOMIZE / MIX find-item types by inspecting the UUID form, so a client can filter by standard-UUID services, custom-UUID services, or both. [`ssaps_server_find.c:65-79`]
4. Responses are **MTU-chunked by construction**: `BuildPrimaryServicePayload(mtu, findServices, serviceLen)` takes the negotiated MTU as its budget argument, and find results accumulate into `SDF_Vector_S` containers before serialization — the server never builds an oversized response. [`ssaps_server_find.c:180-207`]
5. Structural helpers worth mirroring: `GetUuidType` normalizes UUID forms; `CopyToServiceInfo` + `InfoEmplaceBackAndCheckFree` enforce allocation-checked vector growth; `SendFindRspPkt` routes the response code by request code. [`ssaps_server_find.c:39-104, 364`]

## Boundaries and gaps

- This pass analyzed the find module only; the neighboring `ssaps_server_read.c` / `ssaps_server_write.c` (read/write families) were catalogued earlier and remain a future deep-dive.
- The V10 split's exact wire deltas (field layout differences between 1.0 and current framing) were identified structurally, not byte-diffed against our codec tables.
- Module is OHOS-build specific; direct reuse requires the SDF/NLSTK shim layers.

## Reusable for our stack

- This is the implementation blueprint for FIND_BY_UUID in our `assets/stack/ssap` server — the three design axes to copy: request-code-routed response building, tri-modal UUID item typing, and MTU-budgeted response serialization with the V10 fork kept explicit.
- The MTU-as-budget-argument pattern (pass mtu into the payload builder) is cleaner than post-hoc truncation and should replace any size-assuming builder in our codec.
- Confirms our earlier FIND_BY_UUID gap note (from the ssap feature audit) with a concrete upstream shape to implement against.

## Comparison anchors (vs existing reports)

- `NEW-OHOS-NEARLINK-SEPT-INCREMENT.md`: same fresh tree; that report covered the increment's deltas, this one opens the find module the increment did not touch.
- `SSAP-DIALECT-COMPARISON.md`: the V10/current framing split gives that dialect report its first server-side implementation anchor.
- `NEW-OHOS-DEVICE-SOC-WS63.md`: API parity proven at header level; this module shows the implementation complexity the headers hide.
