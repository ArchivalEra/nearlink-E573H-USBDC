---
type: harvest
title: OHOS NearLink IPC opcode taxonomy — CALL_METHOD closes the object-model loop; coex parameter and VCP volume ride the same surface
language: en
created: 2026-09-13
tags: [ohos, ipc, opcode, call-method, object-model, coex, vcp, harvest]
sources:
  - url: https://github.com/xingkaiyueying/tethering_nearlink
    note: local clone (pushed 2026-09-11, current); read-only inspection of nearlink_service_ipc_interface_code.h (387 lines)
trust: verified
stale_after: 2026-12-13
---

# OHOS NearLink IPC opcode taxonomy — CALL_METHOD closes the object-model loop; coex parameter and VCP volume ride the same surface

- Inspection date: 2026-09-13 (same current clone as `NEW-OHOS-SA1190-IPC-SURFACE.md`)
- Mode: read-only local program inspection; no build, network, hardware, or PCB access
- Scope: the interface-code constants that complete the IPC surface's opcode taxonomy

## Executive findings

1. **`NL_SSAP_CLIENT_CALL_METHOD` exists as a dedicated IPC opcode** — the METHOD member class from `NEW-SSAPC-CLIENT-OBJECT-MODEL.md` has an explicit call command crossing the IPC boundary, mapping app → service → SSAP `CALL_METHOD_REQ` → stack. With property read/write and event notify/indicate commands already mapped, all three member classes now have distinct IPC verbs: the object model is end-to-end. [`ipc_parcel/interface/nearlink_service_ipc_interface_code.h:146`]
2. The opcode table (387 lines) is organized per-interface with per-callback event codes — every one of the 28 interfaces from the surface map carries its own command and callback enum blocks (SSAP server/client, advertiser, central manager, datatransfer, HADM client, TWS, ASC, CDSM, host observer). [`nearlink_service_ipc_interface_code.h`]
3. Two commands of special interest to our project ride this surface: **`NL_SET_SLE_COEX_PARAM`** — SLE coexistence configuration is an IPC-level command, meaning coex tuning is a runtime service concern, and **`NL_VCP_CLIENT_SET_DEVICE_ABSOLUTE_VOLUME`** — VCP absolute volume control over NearLink IPC. [`opcode table`]

## Boundaries and gaps

- Numeric opcode values are assigned per enum block starting at 0 (dispatch is per-interface descriptor, so values repeat across interfaces); the cross-referencing of value→handler was not traced to the stub implementations.
- Full opcode list not dumped (387 lines); representative commands only.

## Reusable for our stack

- Design rule for our daemon's control surface: model member classes (property/method/event) as separate verbs end to end, exactly as OHOS does — it keeps the object semantics honest across every boundary.
- `NL_SET_SLE_COEX_PARAM` confirms coex tuning is expected to be runtime-adjustable in the OHOS world — a capability worth exposing in our own dongle control plane for tri-mode work.

## Comparison anchors (vs existing reports)

- `NEW-OHOS-SA1190-IPC-SURFACE.md`: the interface map; this adds the opcode dimension.
- `NEW-SSAPC-CLIENT-OBJECT-MODEL.md`: completes the object-model loop from stack cache to IPC verb.
- `NEW-OHOS-NEARLINK-SEPT-INCREMENT.md`: the audio-adjacent commits (karaoke ear-return) now have their IPC context (ASC/TWS/VCP opcodes).
