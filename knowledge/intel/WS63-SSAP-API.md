---
type: intel
title: "WS63 SDK SSAP API Cross-Validation (fbb_ws63)"
language: en
created: 2026-08-17
tags: [intel, ws63, ssap, cross]
sources:
  - "https://github.com/x-eks-fusion/fbb_ws63"
  - "https://github.com/openharmony/communication_nearlink_service"
trust: B
stale_after: 2027-02-17
---

# WS63 SDK SSAP API Cross-Validation (fbb_ws63)

Title: HiSilicon WS63 SSAP headers as primary reference for our SSAP stack semantics
Date: 2026-08-17
Author: nearlink-driver research subagent
Status: reference/validation note

## Sources

Primary (fbb_ws63 SDK, all read-only):
- `https://github.com/x-eks-fusion/fbb_ws63/blob/master/src/include/middleware/services/bts/sle/sle_ssap_stru.h` (173 lines)
- `https://github.com/x-eks-fusion/fbb_ws63/blob/master/src/include/middleware/services/bts/sle/sle_ssap_server.h` (815 lines)
- `https://github.com/x-eks-fusion/fbb_ws63/blob/master/src/include/middleware/services/bts/sle/sle_ssap_client.h` (675 lines)
- `https://github.com/x-eks-fusion/fbb_ws63/blob/master/src/application/samples/bt/sle/sle_uuid_server/src/sle_uuid_server.c`
- `https://github.com/x-eks-fusion/fbb_ws63/blob/master/src/application/samples/bt/sle/sle_uuid_client/src/sle_uuid_client.c`
- `https://github.com/x-eks-fusion/fbb_ws63/blob/master/src/application/ws63_porting_xf_0p2/port_xf/port_xf_sle/port_xf_sle_ssap_server.c` + `_client.c` (2nd official wrapper)
- `https://github.com/x-eks-fusion/fbb_ws63/blob/master/src/include/middleware/services/bts/sle/sle_errcode.h` (SSAP error base 0x80006100)
- `https://github.com/x-eks-fusion/fbb_ws63/blob/master/src/application/samples/bt/sle/sle_uuid_server/inc/sle_uuid_server.h` (UUID/perm constants)

Corroborating (OHOS reference engine, same dialect, locally cloned):
- `https://github.com/openharmony/communication_nearlink_service/blob/master/services/stack/src/cp/bsl/sle/servm/ssap/include/inner/ssap_pkt.h`
- `https://github.com/openharmony/communication_nearlink_service/blob/master/services/stack/src/cp/bsl/sle/servm/ssap/src/ssaps_server.c`
- `https://github.com/openharmony/communication_nearlink_service/blob/master/services/stack/src/cp/bsl/sle/servm/ssap/src/ssaps_server_find.c`
- `https://github.com/openharmony/communication_nearlink_service/blob/master/services/stack/src/cp/bsl/sle/servm/ssap/src/ssaps_server_write.c`
- `https://github.com/openharmony/communication_nearlink_service/blob/master/services/stack/src/cp/bsl/sle/servm/ssap/src/ssapc_client.c`
- `https://github.com/openharmony/communication_nearlink_service/blob/master/services/stack/src/cp/bsl/sle/servm/ssap/include/ssap_type.h`

Our stack (comparison target):
- `stack/ssap/include/ssap_pkt.h`
- `stack/ssap/include/ssap_codec.h` / `src/ssap_codec.c`
- `stack/ssap/include/ssap_server.h` / `src/ssap_server.c`
- `stack/ssap/test/test_codec.c`, `test_server.c`
- Prior note: `.scratch/nearlink-driver/lab-notes/SSAP-DIALECT-COMPARISON.md`

## Key structural finding

`sle_ssap_stru.h` does **NOT** contain PDU wire structs. It contains only app-layer
enums + one struct:
- `sle_uuid_t { uint8_t len; uint8_t uuid[16]; }` — sle_ssap_stru.h:146-151
- `ssap_exchange_info_t { uint32_t mtu_size; uint16_t version; }` — sle_ssap_stru.h:160-165
- enums `ssap_write_type_t` (38-43), `ssap_permission_t` (52-63), `ssap_find_type_t`
  (72-85), `ssap_property_type_t` (94-109), `ssap_operate_indication_t` (118-137)

The wire PDU definitions live in the internal stack headers. Our `ssap_pkt.h` is
**byte-identical** (`diff` exit 0) to the OHOS inner
`.../servm/ssap/include/inner/ssap_pkt.h`, which is the reference engine for the
exact dialect WS63 exposes. So our header provenance is confirmed; field-by-field
comparison below is therefore against that internal header (the WS63 public API
never contradicts it).

### App-layer enum values (WS63 primary, all match our codec)
- find types: SERVICE_STRUCTURE 0x00, PRIMARY_SERVICE 0x01, REFERENCE 0x02,
  PROPERTY 0x03, METHOD 0x04, EVENT 0x05 — sle_ssap_stru.h:72-85 == ssap_codec.h:70-77 ✅
- operation-indication bits: READ 0x01, WRITE_NO_RSP 0x02, WRITE 0x04, NOTIFY 0x08,
  INDICATE 0x10, BROADCAST 0x20, DESCRIPTOR_WRITE 0x100 — sle_ssap_stru.h:118-137 == ssap_codec.h:80-85 ✅
  (our codec defines only up to 0x20; 0x100 descriptor-write is unused — see gaps)
- property types: VALUE 0x00, USER_DESC 0x01, CLIENT_CFG 0x02, SERVER_CFG 0x03,
  PRESENTATION 0x04, RFU 0x05, CUSTOM 0xFF — sle_ssap_stru.h:94-109
- permissions (APP level, translated to wire auth later): READ 0x01 WRITE 0x02
  ENCRYPTION 0x04 AUTHENTICATION 0x08 AUTHORIZATION 0x10 — sle_ssap_stru.h:52-63
- write types: NO_RSP 0x01, WITH_RSP 0x02 — sle_ssap_stru.h:38-43
- SSAP error base 0x80006100; protocol errors are base+0x01..0x0F
  (INVALID_PDU=+1 ... VALUE_OUT_OF_RANGE=+0x0F) — sle_errcode.h:38,124-183.
  Order matches our SSAP_ERRCODE_* (ssap_pkt.h:188-211) ✅

## SSAP PDU struct comparison table

Wire layouts verified against OHOS inner `ssap_pkt.h` (identical to our file) and
the emission/parse code in the reference engine. Our `ssap_pkt.h` = reference.

| PDU | Wire layout (all u16/u32 LE) | Our stack | Verdict |
|---|---|---|---|
| ERROR_RSP (0x01) | `[code][ctrl][reqCode u8][errHandle u16][errorCode u8]` (ssap_pkt.h:180-186) | encode_error_rsp emits exactly this, 6 B (ssap_codec.c:171-183) | ✅ |
| EXCHANGE_INFO (0x02/0x03) | `[code][ctrl:6 flags][mtu u16?][version u16?]` (ssap_pkt.h:223-236) | encode/decode honor ctrl flags (ssap_codec.c:55-94) | ✅ |
| FIND_STRUCTURE_REQ (0x04/0x06) | `[code][ctrl: findType3\|itemType2\|rspMode1\|resv2][start u16][end u16][uuid 2/16?]` (ssap_pkt.h:248-259) | encoder sets bits exactly (ssap_codec.c:96-116) | ✅ |
| FIND_STRUCTURE_RSP (0x05/0x07) | `[code][ctrl: frag2\|itemType2\|resv4][members...]` (ssap_pkt.h:269-277) | ctrl=0x03 (frag=NO_FRAG, itemType=std) ok (ssap_server.c:110,132) | ✅ ctrl; member byte value ⚠️ (below) |
| READ_REQ (0x08) | `[code][ctrl: frag2\|resv6][{handle u16,type u8}...]` (ssap_pkt.h:292-302) | encoder matches (ssap_codec.c:118-133) | ✅ |
| READ_RSP (0x09) | `[code][ctrl: frag2\|multi1\|error1\|resv4][single: value... \| multi: {len15\|success1,value}...]` (ssap_pkt.h:317-335) | single-value: `[code][ctrl][value]` (ssap_server.c:164-172) | ✅ single; error-item path missing ⚠️ |
| READ_BY_UUID_REQ (0x0A) | `[code][ctrl: uuidType1][start u16][end u16][dataType u8][uuid 2/16]` (ssap_pkt.h:347-357) | not implemented | ❓ gap |
| READ_BY_UUID_RSP (0x0B) | single: `{handle u16,value}`; multi: `{handle u16,len15\|success1,value}` (ssap_pkt.h:367-391) | not implemented | ❓ gap |
| WRITE_CMD (0x0C) | `[code][ctrl: frag2\|multi1\|oper2\|recv3][{handle u16,type u8,value}...]` (ssap_pkt.h:401-419) | encoder matches (ssap_codec.c:135-151) | ✅ |
| WRITE_REQ (0x0D) | same item layout + `verify` bit 0x20 in ctrl (ssap_pkt.h:429-448, 97) | encoder matches, verify bit unused | ✅ layout; verify ⚠️ |
| WRITE_RSP (0x0E) | `[code][ctrl: frag2\|result2\|resv4][on error: errorNum u8,{handle u16,errorCode u8}...]` (ssap_pkt.h:470-491) | success=2 B, error=6 B (ssap_codec.c:185-206) | ✅ (fix confirmed) |
| VALUE_NTF/IND (0x0F/0x10) | `[code][ctrl: frag2\|type1\|resv5][{handle u16,len u16,value}...]` (ssap_pkt.h:507-524) | encoder matches (ssap_codec.c:153-169) | ✅ |
| VALUE_ACK (0x11) | `[code][ctrl: frag2\|type1\|resv5][result u8]` — **no handle** (ssap_pkt.h:534-545) | encoder emits 3 B, no handle (ssap_codec.c:208-217) | ✅ (fix confirmed) |
| CALL_METHOD_* (0x12-0x14) | `[code][ctrl][handle u16][param/result...]` (ssap_pkt.h:557-588) | not implemented | ❓ gap |

## FIND / READ / WRITE / VALUE byte confirmations

### FIND_STRUCTURE_RSP member layouts (version-gated — CRITICAL)
Reference engine has **two layouts** selected by negotiated SSAP version:

- v1.3 primary-service member: `[start u16][end u16][uuid 2/16][memberValue u8]`
  - build: ssaps_server_find.c:157-168 (`BuildPrimaryServiceInfo`)
  - parse: ssapc_client.c:188-206 (`SSAP_DecodeSinglePrimaryService`) — member byte read
    after uuid as `service.memberValue`.
- v1.0 primary-service member: `[start u16][end u16][memberValue u8]` — **NO uuid**
  - build: ssaps_server_find.c:170-178 (`BuildPrimaryServiceInfoV10`)
  - parse: ssapc_client.c:208-226 (`SSAP_DecodeSinglePrimaryServiceV10`)

**memberValue is a member-presence BITMAP, not a category enum:**
`MEMBER_TYPE_REFERENCE=0x01, MEMBER_TYPE_PROPERTY=0x02, MEMBER_TYPE_METHOD=0x04,
MEMBER_TYPE_EVENT=0x08` — ssap_type.h:273-296; bits are set only when the service
actually has that member kind (ssaps_server_find.c:80-98). Client uses the bitmap
to decide whether to walk properties/methods/events:
`servCacheInner->structure.memberValue & MEMBER_TYPE_PROPERTY` — ssapc_cache.c:213-219.

Property member v1.3: `[handle u16][uuid 2/16][operation u32 LE][descriptorCount u8][descriptors...]`
(ssaps_server_find.c:575-590); v1.0 drops the uuid (592-604).
Method member v1.3: `[handle u16][uuid 2/16][5 zero bytes]` (ssaps_server_find.c:914-923).

**Our FIND member value 0x01-0x05 is WRONG for the primary-service record.** Our
`SSAP_ITEM_PRIMARY_SERVICE=0x01` etc. (ssap_server.h:36-42) is emitted at
ssap_server.c:115. A WS63/OHOS client reads that byte as a bitmap: 0x01 =
`MEMBER_TYPE_REFERENCE` → it will **skip property discovery** (bit 0x02 clear) and
break interop. Layout (7 B, uuid included) is correct for v1.3 peers; the byte
value must be the member bitmap (e.g. 0x02 when the service has >=1 property).

**Version gating is a second issue:** a peer that negotiates v1.0 must get the
no-uuid member layout. The WS63 client sample explicitly sends `info.version = 1`
(sle_uuid_client.c:89), i.e. requests v1.0. Negotiation rule:
`link->version = min(peer, local)` (ssaps_server.c:112). Our server always emits the
v1.3-with-uuid layout and answers EXCHANGE_INFO with its own version regardless of
the peer's request (ssap_server.c:84-93) — no negotiation, no layout switch.

### READ_RSP single-value format
Confirmed: single-value READ_RSP is `[msgCode][ctrl][value...]` with **no handle and
no length prefix**. `SSAPS_SendReadReqRsp` (ssaps_server.c:353-393): success path
copies `value->value` straight into `readRsp->items`; only `fragment=NO_FRAG` and
`error=0` bits are set, `multi` stays 0. Error path (376-391) returns the 2-byte
item `{length:15=errCode, success:1=0}` with `ctrl.error=1`.

**Our single-value READ_RSP fix is confirmed** (ssap_server.c:164-172, test_server.c:94-96).
**Difference found:** for read failures the reference engine sends **READ_RSP (0x09)
with ctrl.error=1 + 2-byte error item**, not ERROR_RSP (0x01) — see
ssaps_server.c:580-597 (invalid handle / wrong data type → `SSAPS_SendReadReqRsp(link,
SSAP_READ_RSP, errCode, NULL)`). ERROR_RSP is reserved for PDU-level failures
(invalid PDU, unsupported, server-frag...). Our server replies to bad reads with
ERROR_RSP (ssap_server.c:159-163).

### WRITE_RSP error items
Confirmed: error WRITE_RSP = `[code][ctrl.result=0b01 WRITE_PART][errorNum u8][{handle u16, errorCode u8}...]`.
- build: `SSAPS_BuildWriteRspErrorPayload` — ssaps_server_write.c:92-114 (errorNum=1,
  errList[0]={handle,errorCode}).
- success: `[code][ctrl.result=0b00]` only, unless the `0x20` need-origin-return bit
  was set in WRITE_REQ ctrl — then a `{handle u16, type u8, value}` origin item is
  appended (ssaps_server_write.c:63-90).

**Our WRITE_RSP error fix is confirmed** (ssap_codec.c:185-206: 6 B,
`[0x0E][result][errorNum][handle][code]`, test_codec.c:93-103, test_server.c:117-126).
Gap: we never honor the 0x20 verify bit (bare 2-byte success always).

### VALUE_ACK
Confirmed: VALUE_ACK = `[code][ctrl.type:1][result u8]`, **no handle**.
- struct: ssap_pkt.h:534-545 (`result[0]` after ctrl; min len 3 =
  `SSAP_VALUE_ACK_PDU_MIN_LEN` ssap_pkt.h:100).
- parse: ssaps_server.c:1040-1060 (`SSAPS_ValueAckHandle` — reads `valueAck->result`
  at PDU base+2; valueLen = len - SSAP_PDU_BASE_LEN).

**Our VALUE_ACK no-handle fix is confirmed** (ssap_codec.c:208-217, test_codec.c:86-91).

## Server API sequence (sle_ssap_server.h) vs our userland steps

Official registration flow (sle_uuid_server.c:269-278 + sle_uuid_server_add 163-193,
property_add 112-161):
1. `enable_sle()` (sle_uuid_server.c:271)
2. `sle_connection_register_callbacks(&conn_cbks)` — connect_state_changed_cb gives
   `conn_id` (242-250) → our `ssap_link_on_event` DLI_CONNECTION_COMPLETE_EVT
   (ssap_link.c:144-181)
3. `ssaps_register_callbacks(&ssaps_cbk)` (89-97) — sets start_service/mtu_changed/
   read_request/write_request
4. `ssaps_register_server(&app_uuid, &server_id)` (173) — app_uuid = 16-byte base,
   len=2, value {0x00,0x00}
5. `ssaps_add_service_sync(server_id, &service_uuid, is_primary=1, &service_handle)`
   (104) — sync variant returns the handle directly → our `ssap_server_add_service`
   (ssap_server.c:21-34)
6. `ssaps_add_property_sync(server_id, service_handle, &property, &prop_handle)` (132)
   — property: uuid, permissions (READ|WRITE), value (initial), value_len →
   our `ssap_server_add_property` (ssap_server.c:36-60)
7. `ssaps_add_descriptor_sync(server_id, service_handle, prop_handle, &descriptor)` (151)
   — CCF descriptor with value {0x01,0x00} (LE = 0x0001 = "notification allowed",
   sle_ssap_server.h:704-708) → we have no descriptor table (gap)
8. `ssaps_start_service(server_id, service_handle)` (186)
9. ADV init + peer connect

Server→peer data paths:
- by handle: `ssaps_notify_indicate(server_id, conn_id, {handle, type=SSAP_PROPERTY_TYPE_VALUE,
  value_len, value})` (220-240) → our `ssap_server_notify` (ssap_server.c:196-206)
- by uuid: `ssaps_notify_indicate_by_uuid(server_id, conn_id, {uuid, start_handle=service_handle,
  end_handle=prop_handle, type, value_len, value})` (196-217)
- conn_id=0xFFFF = broadcast (sle_ssap_server.h:710)
- read/write arrive via callbacks: `ssaps_req_read_cb_t{request_id, handle, type, need_rsp,
  need_authorize}` (75-86), `ssaps_req_write_cb_t{..., length, value}` (95-110); app
  replies via `ssaps_send_response(server_id, conn_id, {request_id, status, value_len,
  value})` (699) → the firmware serializes into READ_RSP/WRITE_RSP itself. This is an
  async request/response model with a per-request `request_id` — our server instead
  synchronously calls the app cb and serializes inline (ssap_server.c:150-190). Same
  wire effect; no request_id bookkeeping on our side.
- notify gating: firmware enforces CCF descriptor value 0x0000/0x0001/0x0002 before
  sending (sle_ssap_server.h:704-708); we have no per-client CCF tracking.
- `ssaps_set_info(server_id, {mtu_size, version})` (787) — pre-connect local MTU/version;
  we have `ssap_server_apply_config` (ssap_server.c:208-220).
- callbacks struct (414-431) includes `add_service_cb/add_property_cb/add_descriptor_cb/
  delete_all_service_cb` — all async-ack callbacks we don't need (we use *_sync).

## Client API sequence (sle_ssap_client.h)

Official flow (sle_uuid_client.c): pair complete (82-92) →
1. `ssapc_exchange_info_req(client_id, conn_id, {mtu_size=300, version=1})` (87-91)
2. `exchange_info_cb` (100-112) → `ssapc_find_structure(client_id, conn_id,
   {type=SSAP_FIND_TYPE_PRIMARY_SERVICE, start_hdl=1, end_hdl=0xFFFF})`
3. `find_structure_cb` (114-132) → collect `ssapc_find_service_result_t{start_hdl,
   end_hdl, uuid}`
4. `find_structure_cmp_cb` (134-156) → **writes to the service start handle**:
   `ssapc_write_req(client_id, conn_id, {handle=service.start_hdl, type=
   SSAP_PROPERTY_TYPE_VALUE, data, data_len})`
5. `write_cfm_cb` (179-184) → `ssapc_read_req(client_id, conn_id, write_result->handle,
   write_result->type)`
6. `read_cfm_cb` (186-196) → `ssapc_handle_value_t{handle, type, data_len, data}`

Other API (sle_ssap_client.h): `ssapc_read_req_by_uuid` (549), `ssapc_write_cmd` (622),
`ssapc_register_client` (479), `ssapc_register_callbacks` (666); callbacks struct
(436-456) also has `ssapc_find_property_cbk`, `read_by_uuid_cmp_cb`, `notification_cb`,
`indication_cb`. `ssapc_find_property_result_t{handle, operate_indication, uuid,
descriptors_count, descriptors_type[]}` (49-61).

Notes:
- The sample never runs property discovery (find type stays PRIMARY_SERVICE; property
  cbk is registered but never fired). The `port_xf` wrapper has `xf_sle_ssapc_discovery_property`
  returning `XF_ERR_NOT_SUPPORTED` (port_xf_sle_ssap_client.c:229-238) — i.e. even the
  vendor wrapper doesn't implement a standalone property find.
- Client IDs 0 and 1 are hardcoded in the sample (exchange uses 1, find/write/read use 0).
- Sample's find param leaves `uuid` zeroed and `reserve=0` — "discover all structures".

## Difference list: what the WS63/OHOS dialect has that we lack

1. **FIND member byte semantic bug (high).** Primary-service member byte must be the
   member-presence bitmap (0x02 property / 0x04 method / 0x08 event / 0x01 reference),
   not a 0x01-0x05 item-type enum. Our 0x01 makes clients skip property discovery.
2. **Version-gated find layouts (high).** v1.0 peers get `[start][end][memberValue]`
   (no uuid); v1.3 gets uuid. We always emit v1.3 layout and never negotiate down
   (we reply EXCHANGE_INFO with our fixed version, ssap_server.c:84-93). WS63 sample
   client requests version=1 → a v1.0 peer would misparse our find rsp.
3. **Read-error framing (medium).** Read failures should return READ_RSP 0x09 with
   ctrl.error=1 + 2-byte `{len15=errcode, success:1=0}` item (ssaps_server.c:353-393,
   580-597). We send ERROR_RSP 0x01 instead.
4. **Fragmentation (long data) (medium).** Long reads/writes/notifies use
   fragment begin/mid/end (ctrl.fragment != 0b11). No offset-read exists in either
   dialect — fragmentation is the only long-value mechanism. We emit only NO_FRAG.
5. **READ_BY_UUID (0x0A/0x0B) (medium).** WS63 client API `ssapc_read_req_by_uuid`
   (sle_ssap_client.h:549). Prior note flags WS73 firmware may lack the server-side
   handler (SSAP-DIALECT-COMPARISON.md:16).
6. **Notify subscription gating (medium).** Firmware enforces the CCF descriptor
   (0x0000/0x0001/0x0002) before notifying (sle_ssap_server.h:704-708). We track no
   per-client CCF state; `ssap_server_notify` always sends.
7. **WRITE_REQ verify/origin-return bit (0x20) (low).** When set, success WRITE_RSP
   appends `{handle u16, type u8, value}` (ssaps_server_write.c:63-90). We ignore it.
8. **Multi-value read/write (low/medium).** v1.3 multiProcessing bit
   (exchange ctrl bit5, ssap_pkt.h:231) enables `multi=1` item lists. We only emit
   single-item reads/writes.
9. **Method/event members and find types 4/5 (low).** We return nothing for
   METHOD/EVENT find; the dialect defines `[handle][uuid][5 zero bytes]` method records.
10. **CALL_METHOD (0x12-0x14) (low).** Not implemented (params follow a u16 handle).
11. **Descriptor table.** Official server model has descriptors (incl. CCF) per
    property with their own handles/types; we have a flat property list, no descriptor
    handles → our FIND_PROPERTY member always carries descriptorCount=0 (ssap_server.c:142).
12. **request_id-based async send_response model.** Official callbacks carry a
    `request_id` and the app answers asynchronously via `ssaps_send_response`
    (sle_ssap_server.h:75-130, 699). We serialize synchronously inside dispatch.

## Confirmation of our 4 recent wire fixes

1. **VALUE_ACK has no handle** — ✅ CONFIRMED by ssap_pkt.h:534-545 and
   ssaps_server.c:1040-1060 (result byte directly after ctrl; min PDU 3 B). Our
   encode_value_ack (3 B) is correct.
2. **READ_RSP single value = [hdr][value], no handle/len prefix** — ✅ CONFIRMED by
   ssaps_server.c:353-393 (value copied into items, multi=0). Our emission
   (ssap_server.c:164-172) is correct.
3. **WRITE_RSP error items = [errorNum u8][{handle u16, errorCode u8}]** — ✅ CONFIRMED
   by ssaps_server_write.c:92-114 (ctrl.result=0b01, errorNum, errList). Our 6-byte
   error response is correct.
4. **FIND member layout** — ⚠️ LAYOUT CONFIRMED (v1.3: `[start][end][uuid][memberByte]`,
   7 B standard), **BYTE VALUE WRONG**: the trailing byte is a member-presence bitmap
   (property=0x02/method=0x04/event=0x08/reference=0x01, ssap_type.h:273-296), not the
   0x01-0x05 category enum we emit (ssap_server.h:36-42). Also must switch to the
   no-uuid v1.0 layout for v1.0 peers. Fix: emit the bitmap (e.g. 0x02 when the
   service has properties) and gate the layout on negotiated version.

## Open questions

- Does WS73 firmware implement the OHOS v1.0 no-uuid find layout, or does it always
  behave as v1.3? We cannot see WS73/WS63 firmware (precompiled libbgtp.a); the
  version-gated layout logic comes from the OHOS reference engine. Until proven
  otherwise, treat a peer that negotiates <1.3 as expecting the no-uuid member layout.
- What does WS63 firmware do when a client sends EXCHANGE_INFO with version=1
  (as its own sample does)? Same uncertainty as above; the sample's own server never
  calls `ssaps_set_info`, so the firmware default version is unknown.
- WS63 sample never discovers properties — is property discovery normally driven by
  a find with `type=SSAP_FIND_TYPE_PROPERTY` over the service handle range? The
  official flow in the port wrapper suggests property find is effectively
  unsupported there (port_xf_sle_ssap_client.c:229-238).
- Is a bare 2-byte WRITE_RSP success acceptable to WS73 when the peer sets the 0x20
  verify bit? (Spec says origin value should be echoed.)
- `ssap_exchange_info_t` at API level is `{u32 mtu, u16 version}` (sle_ssap_stru.h:160-165)
  but the wire EXCHANGE_INFO MTU field is u16 (ssap_pkt.h:234). WS63 sample sends
  mtu=300 — fits in u16, but an API-side u32 suggests firmware may cap internally.

## Verdict

WS63 SDK (HiSilicon official) confirms our wire dialect: opcodes 0x01-0x14, LE
multi-byte fields, ctrl bit layouts, exchange/read/write/notify/ack framing all
match our ssap_pkt.h (which is byte-identical to the OHOS reference header). Three of
our four recent wire fixes are byte-confirmed. The FIND member fix is **layout-correct
but value-incorrect** (member-presence bitmap, not item-type enum) and additionally
needs version-gated layout selection (v1.0 = no uuid in member).
