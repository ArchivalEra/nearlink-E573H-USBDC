# SSAP Implementation Plan Audit

Title: SSAP-IMPLEMENTATION-PLAN.md cross-check against OHOS/WS63/OpenSparklink research
Date: 2026-08-17
Author: nearlink-driver research subagent
Status: audit / delta analysis

## Sources

**Original plan:**
- `SSAP-IMPLEMENTATION-PLAN.md` — 6 implementation steps + risks, authored 2026-08-16

**Authoritative reference (3 subagent deep-dives):**
- `OHOS-SSAP-ENGINE.md` — server-side function-by-function dissection (P0/P1/P2 gap list)
- `OHOS-SSAP-CLIENT.md` — client-side blue-print from ssapc_* authoritative code
- `OSPL-SSAP-COMPARE.md` — OpenSparklink dialect comparison (wire-level divergence from OHOS)
- `WS63-SSAP-API.md` — HiSilicon WS63 SDK cross-validation (public API enums + wire layouts)

**Current implementation:**
- `stack/ssap/` — codec, server, link, transport, feature_mgr (5 modules, 4 test suites)

---

## 1. Original Plan Task Checklist

The plan at `SSAP-IMPLEMENTATION-PLAN.md` defined 6 implementation steps:

| # | Plan Step | Status | Evidence |
|---|-----------|--------|----------|
| 1 | Port `ssap_pkt.h` (pure header, zero deps) | **DONE** | `stack/ssap/include/ssap_pkt.h` exists; byte-identical to OHOS inner header (WS63-SSAP-API.md:45-48) |
| 2 | Port `ssaps_server.c` + `ssap_link.c` + `ssap_manager.c` (strip SDF/CP abstraction, replace with POSIX) | **PARTIALLY DONE** | `ssap_server.c` is a slim reimplementation (not a port); `ssap_link.c` is a hardened FSM (new, not ported); `ssap_manager.c` was never created — see "Obsolete" section below |
| 3 | Write hwsle transport adapter: SSAP_Send -> write(/dev/hwsle, [0xA3][tcid=0x0A][payload]) | **DONE** | `hwsle_transport.c/h` implements ACB frame send/recv on tcid 0x0A |
| 4 | Test: self-loop (own PDU tx/rx) then dual-dongle or phone | **PARTIALLY DONE** | 4 test suites exist (codec, server, feature, link) covering encode/decode and server dispatch; no integration test with real transport |

**Unlisted but implemented** (not in original plan):
- `feature_mgr.c/h` — heuristic feature switching (capacity profiles, RAM pressure, peer gating)
- `ssap_link.c/h` — hardened connection FSM (timeouts, stale-event gating, supervision timeout, reject-revert) — a significantly more robust design than the plan's simple "connect -> wait -> done"

---

## 2. Completed Items — Module-Level Detail

### ssap_codec (DONE, 4 fixes applied)

Encoders/decoders exist for: EXCHANGE_INFO, FIND_STRUCTURE_REQ, READ_REQ, WRITE_CMD, WRITE_REQ, WRITE_RSP, VALUE_NTF/IND, VALUE_ACK, ERROR_RSP. Trans-type table complete for all 20 opcodes.

**Fixes confirmed (via WS63-SSAP-API.md):**
- VALUE_ACK: no handle on wire (3 bytes: code + ctrl + result) — `ssap_codec.c:208-217` is correct
- WRITE_RSP: error items use `[errorNum][handle u16][errCode]` framing — `ssap_codec.c:185-206` is correct
- READ_RSP single: value directly after ctrl, no handle/len prefix — `ssap_server.c:164-172` is correct
- FIND member layout: v1.3 `[start][end][uuid][memberValue]` and v1.0 `[start][end][memberValue]` — `ssap_server.c:119-127` is correct

**Missing codec functions** (no encoder/decoder for):
- READ_BY_UUID_REQ/RSP (0x0A/0x0B)
- FIND_BY_UUID_RSP (0x07) — REQ encoder exists via FIND_STRUCTURE_REQ with by-uuid opcode
- CALL_METHOD_CMD/REQ/RSP (0x12/0x13/0x14)
- Multi-item READ/WRITE encoders (v1.3 multiProcessing)

### ssap_server (DONE for basic ops, major gaps remain)

Handles: EXCHANGE_INFO_REQ, FIND_STRUCTURE_REQ (primary-service + property), READ_REQ (single), WRITE_CMD, WRITE_REQ, VALUE_NTF/IND send.

**Corrections already applied:**
- memberValue = bitmap (0x02 for property), not item-type enum — `ssap_server.c:111`
- v1.0 no-uuid layout gated on `version < 1.3` — `ssap_server.c:119-127`
- READ failure path returns READ_RSP with ctrl.error=1, not ERROR_RSP — `ssap_server.c:173-180`
- WRITE_RSP with proper error items (not ERROR_RSP) — `ssap_server.c:201-205`
- EXCHANGE_INFO does min(peer, local) version negotiation — `ssap_server.c:90`

**Remaining gaps** (see OHOS-SSAP-ENGINE.md P0/P1/P2):
- No READ_BY_UUID_REQ dispatch (opcode 0x0A returns -1)
- No FIND_BY_UUID dispatch (opcode 0x06 returns -1)
- No CALL_METHOD dispatch (opcodes 0x12/0x13/0x14 return -1)
- No FIND_METHOD (findType=4) response
- No FIND_EVENT (findType=5) response (should return ERROR_RSP ITEM_INEXIST)
- No FIND_SERVICE_STRUCTURE (findType=0) response
- No multi-handle READ items or ctrl.multi/error framing
- No multi-handle WRITE with sub-items + error list
- No CCCD (client-config) tracking for notify/indicate gating
- No descriptors in the property model (descriptorCount always 0)
- No permission gating (AUTH/ENCRYPT/AUTHZ -> UNAUTHENTICATED/UNENCRYPTED/UNAUTHORIZED)
- No VALUE_IND -> VALUE_ACK receive path
- No service-change notification (handle 0x000E)
- No value-length guard (> mtu-2 -> SERVER_FRAG)
- ExchangeInfo does not advertise fragment/multiProcessing capability bits
- No WRITE_RSP origin-echo (verify bit 0x20 handling)

### ssap_link (DONE — exceeds plan)

Hardened connection FSM with: connect/disconnect timeouts, stale-event gating, duplicate-peer rejection, reject-revert (0x1403 error -> back to CONNECTED), supervision timeout, SET_DATA_LEN retry. Fully unit-tested (test_link.c, 10 tests).

### hwsle_transport (DONE — exceeds plan)

ACB frame send/recv on /dev/hwsle, tcid 0x0A for SSAP, HCI cmd send for link layer. Non-blocking poll loop with frame parsing.

### feature_mgr (DONE — not in plan)

Heuristic feature switching: capacity profiles (TINY/SMALL/MEDIUM/FULL), RAM pressure evaluation, connection-state gating, peer-version gating. Unit-tested (test_feature.c, 7 tests). Integrates with ssap_server via `ssap_server_apply_config`.

---

## 3. Corrections Needed (OHOS/WS63/New Intelligence)

### C1. `ssap_item_type_t` enum values are wrong (HIGH)

**Plan silence.** Our `ssap_server.h:36-42` defines:
```
SSAP_ITEM_PRIMARY_SERVICE = 0x01, SSAP_ITEM_SECONDARY_SERVICE = 0x02,
SSAP_ITEM_PROPERTY = 0x03, SSAP_ITEM_METHOD = 0x04, SSAP_ITEM_EVENT = 0x05
```

The OHOS/WS63 wire values (`ssap_type.h:237-244` + `OSPL-SSAP-COMPARE.md:60-61`) are:
```
PRIMARY_SERVICE = 0x00, SECONDARY = 0x01, PROPERTY = 0x02,
METHOD = 0x03, EVENT = 0x04
```

**Impact:** These bytes are emitted raw in FIND_STRUCTURE_RSP as the `itemType` field. A WS63/OHOS client reads them to filter results. Our 0x01 for PRIMARY_SERVICE matches `REFERENCE_SERVICE` on the wire.

**Evidence:** WS63-SSAP-API.md:112-117, OSPL-SSAP-COMPARE.md:60-61, `ssap_type.h:237-244`.

### C2. FIND member memberValue byte semantics (FIXED but verify)

**Fixed:** `ssap_server.c:111` now emits the member-presence bitmap (0x02 = has properties). The `ssap_item_type_t` enum is only used in the FIND rsp ctrl itemType field (which is always STANDARD=0 for our uuid16-only stack) — the memberValue trailing byte is computed independently.

**Verification needed:** The `SSAP_ITEM_*` enum is also referenced in `ssap_server.c:108,132` in the dispatch for ctrl itemType; since we only emit STANDARD items, the ctrl bits happen to be correct (0). The enum values only matter if used as wire category bytes in a future SERVICE_STRUCTURE find.

### C3. FIND version-gated layout (FIXED)

**Fixed:** `ssap_server.c:103,119-127` gates v1.0 (no uuid) vs v1.3 (with uuid) based on `srv->version < SSAP_VERSION_1_3`. `ssap_server.c:89-90` does `min(peer, local)` version negotiation in EXCHANGE_INFO.

**Plan silence:** The original plan said "strip SDF/CP abstraction, replace with POSIX" but never mentioned version-gated wire layouts.

### C4. READ error path (FIXED)

**Fixed:** `ssap_server.c:173-180` returns READ_RSP (0x09) with ctrl.error=1 and 2-byte error item, not ERROR_RSP (0x01).

**Plan silence:** The plan said "port ssaps_server.c" but never specified error-path semantics.

### C5. WRITE_RSP error framing (FIXED)

**Fixed:** `ssap_server.c:201-205` uses `ssap_encode_write_rsp` which emits proper `[ctrl result][errorNum][handle u16][errCode]`.

**Plan silence:** The plan said "port ssaps_server.c" without mentioning WRITE_RSP specifics.

### C6. ExchangeInfo capability bits (NEEDED)

Our server sends ctrl=0x03 (mtu+version only) — `ssap_server.c:92`. OHOS advertises fragment (bit4) and multiProcessing (bit5) for v1.3 peers. Missing: set fragment=1 and multiProcessing=1 in ctrl when version>=1.3, then store `link->fragment`/`link->multiProcessing`.

**Evidence:** OHOS-SSAP-ENGINE.md:187 (gap #7).

### C7. FIND itemType filtering + MIX response (NEEDED)

OHOS dispatches by `ctrl.itemType` (STANDARD/CUSTOMIZE/MIX) and uses `SSAP_FindInfoIndicator` for MIX responses splitting std and cus runs. Our server ignores itemType entirely (only supports uuid16 standard items).

**Evidence:** OHOS-SSAP-ENGINE.md:188 (gap #8).

---

## 4. Omissions (Not Mentioned in Original Plan)

The original plan's 6 steps focused on transport-layer plumbing. The following P0 interop requirements from OHOS-SSAP-ENGINE.md were never discussed:

### O1. Multi-handle READ items + ctrl.multi/error framing (P0)

**Plan impact:** Plan step 2 said "port ssaps_server.c" but our reimplementation only handles single-item READ. The OHOS multi-handle READ (`{handle u16, type u8} x N` with per-item `{length:15, success:1, value}` framing) is essential for v1.3 interop.

**Evidence:** OHOS-SSAP-ENGINE.md:181 (gap #1), ssaps_server.c:424-578.

### O2. Multi-handle WRITE with sub-items + error list (P0)

**Plan impact:** Plan step 2 never mentioned multi-item write. OHOS WRITE_RSP carries `errorNum` + `{handle u16, errorCode u8}...` for partial results, and WRITE_REQ multi has `subItemCount` + `{type, len, value}` sub-items.

**Evidence:** OHOS-SSAP-ENGINE.md:182 (gap #2), ssaps_server_write.c:203-607.

### O3. READ_BY_UUID_REQ/RSP (0x0A/0x0B) (P0)

**Plan impact:** Plan step 2 said "port ssaps_server.c" which includes this handler. Our dispatch returns -1 for 0x0A. Essential for clients that discover-by-property-UUID.

**Evidence:** OHOS-SSAP-ENGINE.md:183 (gap #3), ssaps_server.c:714-1035.

### O4. FIND_STRUCTURE_BY_UUID_REQ (0x06) (P0)

**Plan impact:** Not mentioned. Our dispatch has no 0x06 handler. Essential for clients using `ssapc_find_by_uuid`.

**Evidence:** OHOS-SSAP-ENGINE.md:184 (gap #4), find.c:1453-1495.

### O5. CALL_METHOD_CMD/REQ/RSP (0x12/0x13/0x14) (P0)

**Plan impact:** Plan step 2 said "port ssaps_server.c" but our implementation has zero method support — no method table, no handle lookup, no pending-request queue, no response builder.

**Evidence:** OHOS-SSAP-ENGINE.md:185 (gap #5), ssaps_server.c:1087-1504.

### O6. CCCD gating for notify/indicate (P0)

**Plan impact:** Plan step 3 mentioned "SSAP_Recv" but never discussed the server-side notify/indicate gating mechanism. Without CCCD tracking, our server sends NTF/IND unconditionally — a WS63/OHOS client that hasn't written CCCD=1 will not expect notifications.

**Evidence:** OHOS-SSAP-ENGINE.md:186 (gap #6), ssaps_server.c:1235-1367.

### O7. VALUE_IND -> VALUE_ACK receive path (P0)

**Plan impact:** Plan never mentioned indication acknowledgement. When a peer sends VALUE_IND, the server must auto-respond with VALUE_ACK. Our server has no inbound IND handler.

**Evidence:** OHOS-SSAP-ENGINE.md:186 (gap #6), ssaps_server.c:1040-1060.

### O8. Descriptors (CCCD, user-desc, server-config, format) (P1)

**Plan impact:** Plan's "service table" concept had properties only. OHOS has a descriptor sub-model (`DESC_TYPE_CLIENT_CONFIG=0x02`, etc.) per property with their own handles. Without descriptors, `descriptorCount` in FIND_PROPERTY member is always 0, and clients cannot subscribe to notifications.

**Evidence:** OHOS-SSAP-ENGINE.md:193 (gap #11), ssaps_server_find.c:575-590.

### O9. Permission gating + authorization pending queue (P1)

**Plan impact:** Plan mentioned "permission" field in properties but never specified the gating logic. OHOS maps AUTH/ENCRYPT/AUTHZ bits to UNAUTHENTICATED/UNENCRYPTED/UNAUTHORIZED error codes and has a pending-request queue for authorization decisions.

**Evidence:** OHOS-SSAP-ENGINE.md:194 (gap #12), ssaps_server.c:131-169.

### O10. FIND_SERVICE_STRUCTURE (0x00) with per-item variable layout (P1)

**Plan impact:** Plan step 2 "port ssaps_server.c" would include this, but our implementation only handles PRIMARY_SERVICE and PROPERTY findTypes. SERVICE_STRUCTURE is the most complex find type with per-item variable layouts based on `itemType`.

**Evidence:** OHOS-SSAP-ENGINE.md:191 (gap #9), find.c:1118-1411.

### O11. Service-change notification on handle 0x000E (P1)

**Plan impact:** Constants defined (`SSAP_HANDLE_SERVICE_CHANGE 0x000E` in ssap_server.h:17) but nothing emits the notification. OHOS sends a VALUE_NTF on this handle when the service table changes.

**Evidence:** OHOS-SSAP-ENGINE.md:198 (gap #16), ssaps_service.c:469-531.

### O12. Client-side implementation (P0 for a client role)

**Plan impact:** Plan was entirely server-focused. OHOS-SSAP-CLIENT.md describes the full client state machine, service discovery cache, FIND response parsing, CCCD subscription, and VALUE_IND auto-ACK. None of this exists.

**Evidence:** OHOS-SSAP-CLIENT.md, ssapc_client.c (1682 lines).

---

## 5. Obsolete / Not Applicable

### X1. "Port ssap_manager.c" — not needed

The plan's step 2 included `ssap_manager.c` for connection lifecycle (link creation on CM event, timeout, teardown). On WS73, the kernel driver already handles CM connection events and tcid demux. Our `ssap_link.c` replaces this with a hardened FSM that speaks DLI directly. The OHOS ssap_manager.c's role (managing per-link SSAP state + forwarding to CM) is split between our link layer (hardware events) and server (protocol state).

### X2. "Strip SDF/CP abstraction" — no longer relevant

OHOS has SDF (Service Data Framework) and CP (Control Plane) abstraction layers between SSAP and the transport. WS73's kernel driver eliminates this — SSAP rides directly on ACB frames via /dev/hwsle. Our transport adapter (`hwsle_transport.c`) is the direct replacement; no SDF/CP stripping needed.

### X3. "Self-loop test" — superseded by unit tests

Plan step 4 said "first self-loop (own PDU tx/rx)". The 4 test suites (codec, server, feature, link) with 30+ assertions provide far more coverage than a self-loop. Integration testing against real hardware is the actual next step.

### X4. OSPL offset-based fragmentation — not applicable

OSPL uses offset-based long reads/writes (`ReadReq{handle, offset}`). OHOS (our dialect) uses fragment bits (begin/mid/end). Since we follow the OHOS dialect (confirmed as HiSilicon-compatible), OSPL's offset model is irrelevant to our implementation.

### X5. OSPL error code numbering — not applicable

OSPL's `SsapError` enum (InvalidHandle=0x01, etc.) differs entirely from OHOS's `SSAP_PduErrCode_E` (InvalidHandle=0x04, etc.). We follow OHOS numbering. OSPL is noted as "loopback-only" (`sle_ssap.rs:17-18`).

---

## 6. Updated SSAP Stack Roadmap

Based on all intelligence gathered. Priority rationale: P0 = blocks interop with OHOS/WS63 peers, P1 = functional completeness, P2 = full conformance/polish.

### P0 — Required for OHOS/WS63 Interop

| # | Task | Effort | Depends On | Status |
|---|------|--------|------------|--------|
| 1 | **Fix `ssap_item_type_t` wire values** (0x00-0x04, not 0x01-0x05) | XS | nothing | NOT STARTED |
| 2 | **READ_BY_UUID_REQ/RSP (0x0A/0x0B)** — dispatch + full server handler | M | #1 | NOT STARTED |
| 3 | **FIND_BY_UUID_REQ (0x06)** — dispatch + handler | M | #1 | NOT STARTED |
| 4 | **CALL_METHOD_CMD/REQ/RSP (0x12/0x13/0x14)** — dispatch + method table + pending queue + response builder | L | #1 | NOT STARTED |
| 5 | **Multi-handle READ (ctrl.multi)** — multi-item request parsing + per-item `{length:15,success:1,value}` response | M | nothing | NOT STARTED |
| 6 | **Multi-handle WRITE (ctrl.multi)** — sub-items + WRITE_RSP error list + partial result | M | nothing | NOT STARTED |
| 7 | **CCCD (ClientConfig) tracking** — descriptor type 0x02 per property, per-addr CCCD value store, NTF/IND gated on CCCD | L | #8 | NOT STARTED |
| 8 | **Descriptor model** — add descriptor types 0x01-0x04 to property, handle allocation, READ/WRITE by type | L | nothing | NOT STARTED |
| 9 | **VALUE_IND -> VALUE_ACK auto-response** — inbound IND handler, outbound ACK encoder | S | nothing | NOT STARTED |
| 10 | **ExchangeInfo capability bits** — advertise fragment + multiProcessing for v1.3, store negotiated flags | S | nothing | NOT STARTED |
| 11 | **Value-length guard** — > mtu-2 -> SERVER_FRAG on READ paths | S | nothing | NOT STARTED |

### P1 — Functional Completeness

| # | Task | Effort | Depends On | Status |
|---|------|--------|------------|--------|
| 12 | **FIND_SERVICE_STRUCTURE (findType=0x00)** — per-item variable layout (itemType/uuid/ref handles/op/descCount) | L | #1 | NOT STARTED |
| 13 | **FIND_METHOD (findType=0x04)** — method member: [handle][uuid][5 zero bytes] | S | #1 | NOT STARTED |
| 14 | **FIND_EVENT (findType=0x05)** — must return ERROR_RSP ITEM_INEXIST | XS | #1 | NOT STARTED |
| 15 | **Permission gating** — AUTH/ENCRYPT/AUTHZ -> UNAUTHENTICATED/UNENCRYPTED/UNAUTHORIZED + pending queue | L | #8 | NOT STARTED |
| 16 | **READ_RSP multi-item framing** — `{length:15,success:1,value}` per-item for multi-handle requests | M | #5 | NOT STARTED |
| 17 | **WRITE_RSP origin-echo (verify bit 0x20)** — echo `{handle,type,value}` when set | S | #6 | NOT STARTED |
| 18 | **WRITE_CMD multi-subitem path** — validation (oper==INSTANT, fragment==NO_FRAG) | M | #6 | NOT STARTED |
| 19 | **Service-change notification (handle 0x000E)** — VALUE_NTF on table mutation | M | #8 | NOT STARTED |
| 20 | **App registry + callbacks struct** — multi-app support, notify-result plumbing | L | #7 | NOT STARTED |
| 21 | **Server-side value-length guard on READ_BY_UUID paths** | S | #2 | NOT STARTED |

### P2 — Full Conformance / Polish

| # | Task | Effort | Depends On | Status |
|---|------|--------|------------|--------|
| 22 | **128-bit custom UUID** support in service table (currently uuid16 only) | L | #1 | NOT STARTED |
| 23 | **FIND itemType MIX indicator encoding** — std/cus split runs with SSAP_FindInfoIndicator | M | #22 | NOT STARTED |
| 24 | **Fragmentation** — real fragment reassembly if a peer requires it (currently both OHOS and ours reject non-NO_FRAG) | XL | nothing | NOT STARTED |
| 25 | **rspMode=MULTI_RSP** — OHOS rejects with UNSUPPORT_PDU; mirror that (already the behavior) | XS | nothing | DONE (implicit) |
| 26 | **Interaction timeout/timer** for method-call and indicate tasks (30s default) | M | #4 | NOT STARTED |
| 27 | **Client-side implementation** — discovery cache, FIND response parsing, CCCD subscription, VALUE_IND auto-ACK, connection FSM | XL | all P0 | NOT STARTED |

### Wire Fixes Already Applied (no further action)

| # | Fix | Evidence |
|---|-----|----------|
| F1 | VALUE_ACK has no handle | ssap_codec.c:208-217, test_codec.c:86-91 |
| F2 | READ_RSP single value = [hdr][value], no handle/len prefix | ssap_server.c:164-172, test_server.c:109-111 |
| F3 | WRITE_RSP error items = [errorNum][handle][errCode] | ssap_codec.c:185-206, test_codec.c:93-103 |
| F4 | FIND v1.3 member layout correct (start, end, uuid, memberValue bitmap) | ssap_server.c:113-127, test_server.c:80-83 |
| F5 | FIND v1.0 member layout (no uuid) gated on version | ssap_server.c:119-127, test_server.c:86-98 |
| F6 | READ failure path uses READ_RSP not ERROR_RSP | ssap_server.c:173-180, test_server.c:113-124 |
| F7 | WRITE_RSP (not ERROR_RSP) for write failures | ssap_server.c:201-205, test_server.c:144-153 |
| F8 | EXCHANGE_INFO does min(peer, local) version negotiation | ssap_server.c:89-90 |

---

## Open Questions (carried forward)

1. **Method `needAuth` quirk** — OHOS sets `needAuth` from the ENCRYPTION permission bit (ssaps_server.c:1141-1142), not AUTHORIZATION. Is this a bug or spec? (OHOS-SSAP-ENGINE.md:208)

2. **READ_BY_UUID single-vs-multi asymmetry** — single-instance item has no length field; multi has `{length:15,success:1}`. Client length inference in single mode relies on PDU end. (OHOS-SSAP-ENGINE.md:209)

3. **VALUE_ACK payload ambiguity** — server treats ack bytes as opaque `value` (no handle on wire). If a peer sends `{handle u16}{result u8}`, decode must handle both. (OHOS-SSAP-ENGINE.md:210)

4. **v1.0 peer detection** — OHOS uses `CM_GetLogicLinkDeviceType(lcid) == CM_DEVTYPE_OLD`; ours infers from `version < 1.3`. These can disagree. (OHOS-SSAP-ENGINE.md:211)

5. **WS73 firmware SSAP dialect fidelity** — does WS73 firmware implement the full OHOS server engine (multi-handle, descriptors, methods) or a subset? Cannot determine without firmware reverse engineering. (SSAP-IMPLEMENTATION-PLAN.md:70)
