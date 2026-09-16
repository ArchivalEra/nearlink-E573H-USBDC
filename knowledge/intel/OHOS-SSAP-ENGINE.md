---
type: intel
title: "OHOS SSAP Server Engine — Authoritative Semantics Anatomy"
language: zh
created: 2026-08-17
tags: [intel, ohos, ssap, server]
sources:
  - "/mnt/hdd/nearlink-stuff/communication_nearlink_service"
trust: B
stale_after: 2027-02-17
---

# OHOS SSAP Server Engine — Authoritative Semantics Anatomy

Title: OHOS SSAP server-side authoritative engine, function-by-function dissection
Date: 2026-08-17
Author: nearlink-driver research subagent
Status: reference/anatomy note
Scope: server-side authoritative implementation (client-side only cross-checked for API surface)

## Sources

OHOS primary (all read-only), base `/mnt/hdd/nearlink-stuff/communication_nearlink_service/services/stack/src/cp/bsl/sle/servm/ssap/`:
- `src/ssaps_server_find.c` (1498 L) — findType dispatch + all FIND rsp builders
- `src/ssaps_server_write.c` (940 L) — WRITE_CMD/WRITE_REQ single/multi + WRITE_RSP builders + CPCD update
- `src/ssaps_server.c` (1508 L) — EXCHANGE, READ (single/multi/by-uuid), VALUE_ACK, method call, notify/indicate, auth pending queue
- `src/ssaps_server_app.c` (614 L) — app registry + app callbacks (onRead/onWrite/onNotify/onCallMethod)
- `src/ssaps_service.c` + `src/ssaps_service_param.c` — service/property/descriptor cache, handle alloc, hash + SERVICE_CHANGE ntf
- `src/ssaps_server_api.c`, `src/ssap_common.c`, `src/ssap_handle.c`, `src/ssap_link.c`, `src/ssap_manager.c`, `src/ssap_utils.c`
- `include/inner/ssap_pkt.h`, `include/ssap_type.h`, `include/inner/ssap_utils.h`, `include/inner/ssap_manager.h`
- `include/nlstk_ssap_app_server.h` + `src/nlstk_ssap_app_server.c`, `include/nlstk_ssap_app_client.h`, `src/nlstk_ssap_app_link.c`

Our stack: `/home/archivalera/plum/zcode-projects/nearlink/stack/ssap/`
- `include/ssap_server.h` + `src/ssap_server.c`, `include/ssap_codec.h` + `src/ssap_codec.c`
- Prior notes: `../lab-notes/OSPL-SSAP-COMPARE.md` (opcode-level, OSPL dialect), `../lab-notes/WS63-SSAP-API.md` (WS63 app-layer enum values)

Notation: paths below are relative to the OHOS base above unless prefixed with `ours:`.

---

## 1. 能力面 — server capability surface (ssaps_server_find.c)

### findType coverage (ctrl.findType, 3 bits)
| value | name | server behavior |
|---|---|---|
| 0x00 | SERVICE_STRUCTURE | implemented — service + properties + methods of a service, one flat item list (`SendFindStructureRsp`, find.c:1375-1407) |
| 0x01 | PRIMARY_SERVICE | implemented, dual v1.0/v1.3 (`SendFindPrimaryServiceRsp`, find.c:378-465) |
| 0x02 | REFERENCE_SERVICE | NOT handled — falls to default → `ERROR_RSP UNSUPPORT_PDU` (find.c:1445-1449) |
| 0x03 | PROPERTY | implemented, dual v1.0/v1.3 (`SendFindPropertyRsp`, find.c:805-912) |
| 0x04 | METHOD | implemented (`SendFindMethodRsp`, find.c:1075-1116); methods are *not* version-gated (no V10 variant) |
| 0x05 | EVENT | deliberately unimplemented — `ERROR_RSP ITEM_INEXIST` (find.c:1441-1444) |

findType enum values: `ssap_type.h:237-244`. Request validation (find.c:1453-1495):
- FIND_STRUCTURE_REQ must be **exactly** `sizeof(SSAP_PduFindStructReq_S)` (==6 B); BY_UUID variant must be **larger** (uuid present) (find.c:1460-1471) → else `INVALID_PDU`
- `start<=end && start!=0` else `INVALID_HANDLE` (find.c:1472-1480)
- uuid length (BY_UUID) must be 2 or 16 else `INVALID_PDU` (find.c:1484-1490)
- itemType must be STANDARD/CUSTOMIZE/MIX; `rspMode==MULTI_RSP` rejected `UNSUPPORT_PDU` (find.c:1413-1419). **Multi-response mode is not implemented by OHOS server.**

### itemType / MIX handling
- itemType filter applied per-item via `CheckFindItemType` (find.c:65-78): MIX accepts all; STANDARD only std-UUID (checked with `SSAP_CheckUuidStd`, utils.c:101-110 — first 14 bytes equal the 0x37BE... base); CUSTOMIZE only non-std.
- MIX responses are split into two contiguous runs behind a 1-byte indicator each: `SSAP_FindInfoIndicator_S{count:7,type:1}` (`ssap_pkt.h:279-282`), STD run first (`type=0`), then CUS (`type=1`) — find.c:232-273 (services), 674-758 (property), 956-997 (method). `SSAP_FIND_INFO_INDICATION_LEN=1` (pkt.h:35).
- Per-item type class (std vs cus) is decided by `isStdUuid`, and 16-bit vs 128-bit uuid is serialized accordingly.

### member layouts (wire, little-endian)
**Primary service (findType=0x01):**
- v1.3: `[start u16][end u16][uuid 2|16][memberValue u8]` — BuildPrimaryServiceInfo, find.c:157-168; `SSAP_FIND_PRIMARY_SERVICE_STD_LEN=7`, CUS=21 (pkt.h:38-40)
- v1.0: `[start u16][end u16][memberValue u8]` — **no uuid**, BuildPrimaryServiceInfoV10, find.c:170-178; base len 5
- memberValue = bitmap `reference:1 property:1 method:1 event:1` (CopyToServiceInfo, find.c:80-98; struct at ssap_type.h:281-296). Our stack already emits this for properties (member=0x02).
- v1.0 selection: `CM_GetLogicLinkDeviceType(link->lcid) == CM_DEVTYPE_OLD` (find.c:1425-1429). The V10 path additionally forces MIX rsp if any custom service exists (`hasCus` → `FIND_ITEM_TYPE_MIX`, find.c:438-465, 448-454).

**Property (findType=0x03):**
- v1.3: `[handle u16][uuid 2|16][operation u32][descCount u8][descTypes…]` — BuildPropertyInfo, find.c:575-590; STD=9, CUS=23 base + descCount (pkt.h:45-46)
- v1.0: `[handle u16][operation u32][descCount u8][descTypes…]` — no uuid, BuildPropertyInfoV10, find.c:592-604; base 7
- descTypes are **raw descriptor type bytes** (DESC_TYPE_* 0x01..0x04) copied from property->descriptors (find.c:495-499).

**Method (findType=0x04):**
- `[handle u16][uuid 2|16][extra 5 B zeroed]` — BuildMethodInfo, find.c:914-923; `SSAP_FIND_METHOD_EXTRA_LEN = operation(4)+descCount(1)` all memset 0 (pkt.h:47-49). So method members carry **no operation/descriptor info on the wire** — 5 reserved zero bytes. STD=9, CUS=23.

**Service structure (findType=0x00)** — variable per item:
`[handle u16][itemType u8][uuid 2|16][start u16+end u16 iff itemType is a SERVICE_REFERENCE][operation u32 iff itemType is PROPERTY/METHOD/EVENT][descCount u8 + descTypes iff PROPERTY/METHOD/EVENT]` — BuildStructureInfo, find.c:1272-1311; field presence by `GetStartEndHandleLenByItemType` / `GetOperationLenByItemType` / `GetDescriptorCountLenByItemType` (utils.c:132-180).
- itemType bytes come from `NLSTK_SsapItemType_E` (ssap_type.h:73-89): STD_PRIMARY_SERVICE=0x00, STD_SECONDARY=0x01, STD_PROPERTY=0x02, STD_METHOD=0x03, STD_EVENT=0x04, STD_SERVICE_REFERENCE=0x05, STD_DESCRIPTOR=0x06, VENDOR_PRIMARY=0x08 … VENDOR_DESCRIPTOR=0x0E.
- Include rule: a service is included if its `[start..end]` overlaps the request range (`ShouldIncludeService`, find.c:1118-1132); then its service item + property items + method items each filtered by handle range (find.c:1256-1270).
- Service item `operation` is hard-coded `0x00000001` (READ) in the hash path (ssaps_service.c:431) but for FIND it's `service->serviceType` in itemType position and no operation (BuildStructureInfo only adds operation for property/method/event itemTypes).

### MTU-bounded packing rules
- Available payload = `mtu - SSAP_PDU_BASE_LEN(2)`. Fixed-size members: `count = leftSize / itemLen` (find.c:180-205). Variable members (property w/ descTypes, MIX, structure): greedy fill, stop at first item that doesn't fit (`break`), find.c:606-638, 760-781, 1329-1361.
- **One response per request**; items that don't fit are silently dropped (client issues next range request). `ctrl.fragment` always `SSAP_CTRL_NO_FRAG` (0b11) in rsp (find.c:364-376). No fragmentation of FIND responses.

## 2. 描述符与门控 — descriptors & client-config gating

### descriptor model
- Types: `DESC_TYPE_PROPERTY_RESERVE=0x00, PROPERTY_INSTRUCTION=0x01 (user desc), CLIENT_CONFIG=0x02 (CCCD), SERVER_CONFIG=0x03, PROPERTY_FORMAT=0x04` (ssap_type.h:127-135). **CCCD type is 0x02.**
- `SSAP_Descriptor_S{type, operation, permission, clientConfigs vector, val}` (ssap_type.h:176-182). `clientConfigs` holds per-remote-addr `SSAP_ClientPropertyConfigDescriptor_S{addr,type,val}` copies created when a client writes CCCD (ssap_type.h:167-171).
- Registration: `SSAPS_CacheDescriptor` appends to the **last cached property** (ssaps_service.c:154-191); full copy into a descriptor's own `clientConfigs` vector via `SsapAllocServiceCpyDescriptorParm` (ssaps_service_param.c:98-141).

### descriptor read
- `SSAPS_GetPropertyValue(link, property, type, &err)` (ssaps_server.c:286-316): type==SSAP_TYPE_DATA → property->val; else find descriptor by type; for CLIENT_CONFIG returns the **per-addr copy** `clientConfig->val` if present, else descriptor->val (default); error `DATA_TYPE` if value NULL or (len==0 && type is not DATA and not PROPERTY_INSTRUCTION); error `SERVER_FRAG` if `len > mtu-2`.

### descriptor write
- `SSAPS_UpdatePropertyValue(property, type, value, addr, isRmt)` (ssaps_server_write.c:150-174): type==DATA → replace property->val; else find descriptor by type → replace descriptor->val; if type==CLIENT_CONFIG && isRmt → **also upsert per-addr CCCD copy** (`SSAPS_UpdatePropertyValueCpcd`, write.c:116-148).
- Write gating: `SSAPS_WriteControlCheck` (write.c:388-422) maps descriptor type → op bit: PROPERTY_INSTRUCTION→`DESCRITOR_WRITE 0x100`, CLIENT_CONFIG→`CLIENT_CONFIGURATION_WRITE 0x200`, SERVER_CONFIG→`SERVER_CONFIGURATION_WRITE 0x400` (ssap_type.h:110-122); missing bit → `FORBID_WRITE`; AUTH permission without link auth → `UNAUTHENTICATED`; ENCRYPT without link encrypt → `UNENCRYPTED`. Perm/op lookup: `SSAPS_GetPermissionAndOperation` (ssaps_server.c:258-274).

### CCCD gate for notify/indicate (the key "notify gate")
- Mask constants: `SSAP_CPCD_MASK_NOTIFICATION=1, SSAP_CPCD_MASK_INDICATION=2`, len 2 B (ssaps_server.c:43-45).
- `SSAPS_GetClientConfigValue(property)` reads first 2 bytes of the CCCD descriptor->val (default), ssaps_server.c:1235-1249. `SSAPS_GetClientConfigByAddr` looks up the per-addr copy (ssaps_server.c:1251-1264).
- Delivery rule (ssaps_server.c:1266-1352): a local value update either applies to all remotes (addr==all-zero) or one remote (by addr). For each remote: `cpcdValue==INDICATION(2) && (property op & SSAP_OPERATE_INDICATION_INDICATE 0x10)` → queue `SSAPS_ValueInd` as a **request task with `SSAP_INTERACTION_DEFAULT_TIMEOUT`** (`SSAP_ProcessRequestTask`); `cpcdValue==NOTIFICATION(1) && (op & NOTIFY 0x08)` → `SSAPS_ValueNtf` as a normal task; anything else → dropped with log. So the property op-bit AND per-client CCCD both gate.
- Per-addr cpcd copy wins over the default descriptor value when the peer has written its own CCCD (ssaps_server.c:1339-1348).
- CCCD cleanup on link teardown: `SSAPS_CleanServiceCpcd` removes the per-addr entries (ssaps_service.c:533-568).

## 3. 通知/指示 — notify/indicate send + VALUE_ACK

- PDU format (both NTF/IND): `[msgCode 0x0F/0x10][ctrl fragment=0b11, type:1=property(0)/event(1)][handle u16][length u16][value]` — SSAPS_ValueNtf/SSAPS_ValueInd, ssaps_server.c:1172-1233; structs pkt.h:507-524. Length guard: `value.len <= mtu - 2 - 2 - 2` else dropped (ssaps_server.c:1194-1198, 1227-1231).
- Server-side entry: app calls `NLSTK_SsapServerUpdatePropertyValue` / `UpdateDescriptorValue` / `UpdateAndNotifyProperty` (nlstk_ssap_app_server.c:192-275) → `SSAPS_UpdateItemValueByHandle` (ssaps_server.c:1357-1367) → `SSAPS_UpdateItemValue` (ssaps_server.c:1326-1352) which does the local store, CCCD gate, and per-link task dispatch.
- `UpdateAndNotifyProperty` requires a non-zero addr (rejects all-zero, nlstk_ssap_app_server.c:255-257); `UpdatePropertyValue` uses addr=0 → broadcasts to all remotes.
- Result callbacks: `SsapServerUpdateValueResultCallback` (ssaps_server_app.c:387-404) — non-zero addr → `onNotifyProperty`; else dataType==0 → `onSetPropertyValue`; else → `onSetDescriptorValue`.
- **VALUE_ACK receive** (ssaps_server.c:1040-1060): min len 3 (`SSAP_VALUE_ACK_PDU_MIN_LEN`, pkt.h:100); PDU is `[msgCode 0x11][ctrl type:1][result…]` — **no handle** (pkt.h:534-545). Server copies all remaining bytes into `SSAP_ValuePkt_S.value` and calls `SsapServerAppIndicateCfmValueCallback` (ssaps_server_app.c:559-563) — an **app-layer stub that only logs** the handle from a `SSAP_ValuePkt_S`; OHOS currently does nothing with the ack result beyond logging. `SSAP_ACK_RECV_FAIL=0/SUCCESS=1` constants (pkt.h:60-61). Note: the ack's `result` byte(s) are opaque to the server; the client packs `valueInfo->value` (ssapc_client.c:1614-1624).
- Write/read app callbacks: `SsapServerAppWriteValueCallback` (ssaps_server_app.c:485-497) fires only on `errCode==SUCCESS`; dataType==0 → `onWriteProperty` else `onWriteDescriptor`; `needAuth` routes to `onWriteProperty/DescriptorAuthorizeRequest` (429-439, 469-483). `SsapServerAppReadValueCallback` (545-557) analogous with `onReadProperty/Descriptor`.

## 4. 方法调用 — CALL_METHOD server side (fully specified)

Dispatch: `SSAPS_MethodReqHandle` (0x13) / `SSAPS_MethodCmdHandle` (0x12) both → `SSAPS_MethodHandle` (ssaps_server.c:1154-1167, 1087-1149).

1. PDU: `[msgCode 0x12/0x13][ctrl fragment:2][handle u16][params…]` (pkt.h:557-568). `SSAP_METHOD_REQ_PDU_MIN_LEN=4` (ssaps_server.c:48). len<4 → `ERROR_RSP INVALID_PDU` (1093-1097).
2. `ctrl.fragment` must be `0x03` (NO_FRAG) → else `ERROR_RSP SERVER_FRAG` (1099-1104). Fragmentation of method calls not supported.
3. `SSAPS_GetMethodByHandle` (241-256); NULL → if handle not within any service range `INVALID_HANDLE`, else `METHOD_ACCESS` (1110-1115).
4. `SSAPS_MethodCallCheck(method->permission, link)` (644-654): AUTH/ENCRYPT permission gate → `UNAUTHENTICATED`/`UNENCRYPTED`.
5. `paramLen = size - 4`; operation composed (1062-1078): `needRsp = (msgCode == SSAP_CALL_METHOD_REQ)`, i.e. CMD is fire-and-forget.
6. Push to **pending vector** (`SSAPS_PushOperationPenddingVector`, 131-151) assigning a monotonically increasing `requestId`; then `SsapServerAppCallMethodCallback` (ssaps_server_app.c:565-582) → app `onCallMethod(appId, requestId, {handle,addr,uuid,param}, needRsp, needAuth)`.
   - Quirk: `needAuth` is set from the **ENCRYPTION** permission bit (1141-1142), not AUTHORIZATION.
   - Comment: a watchdog timer for method processing is acknowledged as **not implemented** (1144).
7. App replies via `NLSTK_SsapServerSendMethodCallRes` → `SSAPS_SendMethodCallRes` (1479-1504): pops pending by requestId, finds link by `operation->addr`; if `msgCode==SSAP_CALL_METHOD_REQ` sends `SSAP_CALL_METHOD_RSP` = `[msgCode 0x14][ctrl fragment=0b11][result…]` — **no handle** (SSAPS_SendMethodCallRsp, 1459-1475; struct pkt.h:578-588). CMD → no response.
8. Errors only sent for REQ variant (`SSAPS_MethodErrorProcess`, 1080-1085).

Method registration: methods declared in `NLSTK_ServiceParam_S.method[]`, copied into `service->methods` (ssaps_service_param.c:224-244; `SSAP_Method_S{handle,uuid,permission}`, ssap_type.h:199-203); handles allocated after service handle and after properties (`SsapServerGenerateHandleForMethod`, ssaps_service.c:205-215; ordering at 256-259).

## 5. 多值/长值 — multi-value & long-value

### multi-handle READ (0x08)
- Req items: `[handle u16][type u8]` × N (pkt.h:304-307). Precheck: len must satisfy `(len-2) % 3 == 0` and item type <= 4 (ssaps_server.c:656-672). `ctrl.fragment` must be NO_FRAG else `READ_RSP` error item `SERVER_FRAG` (690-693).
- If `itemCount>1` and any item has AUTHORIZATION_NEED permission → `ERROR_RSP ITEM_OVER_LIMIT` (700-704).
- Per item: `SSAPS_ReadMultiHandleItem` (516-548) resolves property, perm/op, control check, value; results array → `SSAPS_SendMultiReadReqRsp` (476-514): rsp ctrl `multi=1`, items each `{length:15, success:1, value}`; error items carry `length=errCode, success=0`; `ctrl.error=1` if any item failed. Whole-encode failure → `ERROR_RSP INVALID_PDU`.
- Single read (itemCount==1): `SSAPS_ReadSingleHandleReq` (599-642). Success rsp = `[msgCode 0x09][ctrl multi=0,error=0][raw value]` (no length/handle). Errors become a single error item `{length:15=errCode, success:1=0}` with `ctrl.error=1` (SSAPS_SendReadReqRsp, 353-393). Handle-not-found → `SSAPS_HandleReadReqError` (580-597): **DATA_TYPE** if the handle exists (i.e. it's a property with wrong type), **INVALID_HANDLE** if not. AUTHZ permission → push pending (632-634); app later approves/denies (see §6).

### multi-handle WRITE (0x0D)
- `ctrl.multi=0` single: `[handle u16][type u8][value…]`, dataLen = len-5 (`SSAP_WRITE_REQ_DATA_OFFSET`, pkt.h:92). `SSAPS_WriteSingleHandleReq` (663-690): type>4 → `ERROR_RSP UNSUPPORT_PDU`; property/perm/control gate → `WRITE_RSP` error items; update → `WRITE_RSP`.
- `ctrl.multi=1` multi: items `{handle u16}{subItemCount u8}{subItems {type u8}{len u16}{value}}` (pkt.h:450-460). `SSAPS_WriteMultiHandleReq` (562-607) iterates, per-handle result; subItemCount==0 → that handle gets `INVALID_HANDLE` error item (580-590); subitem len==0 → skipped (`LOOP_CONTINUE`, 452-454). AUTHZ subitem in multi write → `ERROR_RSP UNSUPPORT_PDU` (446-450).
- **WRITE_RSP** (pkt.h:470-497): `ctrl.result` = `WRITE_SUCCESS 0b00` | `WRITE_PART 0b01`; error payload `{errorNum u8}{errList {handle u16}{errorCode u8}…}` (SSAPS_BuildMultiWriteErrorRsp, 266-297). When ctrl bit `SSAP_WRITE_REQ_NEED_ORIGIN_RET 0x20` (verify bit, pkt.h:97, 437) is set, each successful handle additionally echoes original data `{handle u16}{count u8}{subItems {type u8}{len u16}{value}}` (236-254). `errorCount==255` → `ERROR_RSP INVALID_PDU` (216-218, 320).
- Write control gate (WRITE_REQ & CMD): `ctrl.oper` must be `SSAP_CTRL_WRITE_INSTANT 0b00` else `ERROR_RSP UNSUPPORT_PDU` (708-711); `ctrl.fragment != NO_FRAG` → `WRITE_RSP` error item `SERVER_FRAG` — **fragmented/continued writes are rejected, not reassembled** (712-717).
- WRITE_CMD (0x0C) is the no-response twin (915-937); its multi path (838-869) is fire-and-forget with the same validation minus response.

### READ_BY_UUID (0x0A) — complete implementation
- Req: `[ctrl uuidType:1][handleStart u16][handleEnd u16][dataType u8][uuid 2|16]`, exact lengths 9/23 (`SSAP_READ_BY_*_REQ_PKT_LEN`, pkt.h:70-73); strict len check + `INVALID_HANDLE` for start>end or start==0 (781-805, 961-983).
- `SSAPS_GetPropertyByUuid` collects every property matching uuid AND range (714-731). Zero matches → `ERROR_RSP`-style error item in READ_BY_UUID_RSP `ITEM_INEXIST` (1004-1009).
- Rsp packing (`SSAPS_ReadByUuidReqRspPkt`, 898-940): **single match** → `{handle u16}{value}` (`SSAP_PduReadByUuidRspSingleItem`, pkt.h:388-391) with `ctrl.multi=0`; **multiple matches** → items `{handle u16}{length:15}{success:1}{value}` (`SSAP_PduReadByUuidRspItem`, pkt.h:381-386) with `ctrl.multi=1`; per-property failures → error items with `length=errorCode, success=0`, `ctrl.error=1`. Note the asymmetry: single-instance has no length field, multi has 2-byte length:15|success:1.
- Whole rsp `> mtu-2` → error item `SERVER_FRAG` (759-762). Buffer math pre-computed via `SSAPS_ReadByUuidReqRspLen` (807-840).
- Value length guard everywhere: `SSAPS_GetPropertyValue` returns `SERVER_FRAG` when `len > mtu-2` (311-314).

### fragmentation / offset
- OHOS dialect has **no offsets** (unlike OSPL). Long values use fragment bits; the server currently only *validates* them (must be `0b11` NO_FRAG) and answers `SERVER_FRAG` otherwise. Real reassembly is **not implemented on the server** — the design point is "peer must ask within MTU".

## 6. 错误映射 — error code usage map

Error enum (pkt.h:188-211): `SUCCESS 0x0, INVALID_PDU 0x1, UNSUPPORT_PDU 0x2, UNKNOWN 0x3, INVALID_HANDLE 0x4, NO_RESOURCE 0x5, FORBID_READ 0x6, FORBID_WRITE 0x7, UNAUTHENTICATED 0x8, UNAUTHORIZED 0x9, UNENCRYPTED 0xA, ITEM_INEXIST 0xB, METHOD_ACCESS 0xC, DATA_TYPE 0xD, DATA_LENGTH 0xE, DATA_RANGE 0xF, SERVER_FRAG 0x10, ITEM_OVER_LIMIT 0x11, TIMEOUT 0xB0`.

`ERROR_RSP` (whole-PDU failures; `SSAP_PduErrorRsp`, ssap_manager.c:350-376; format pkt.h:180-186) is used for:
- FIND: bad len/INVALID_PDU (find.c:1463,1469,1488); start>end INVALID_HANDLE (1475); bad itemType/rspMode UNSUPPORT_PDU (1416); unknown findType UNSUPPORT_PDU (1447); EVENT ITEM_INEXIST (1443); empty result ITEM_INEXIST (400,441,834,903,1100,1398); malloc NO_RESOURCE (394,434,813,881,1081,1392); by-uuid wrong uuid type INVALID_PDU/ITEM_INEXIST (383-386, 818-829, 1381-1386).
- READ: malformed len INVALID_PDU (661,682); item type>4 UNSUPPORT_PDU (668); multi-with-auth ITEM_OVER_LIMIT (702); internal alloc NO_RESOURCE (556, 489).
- WRITE: len/type/oper protocol violations INVALID_PDU/UNSUPPORT_PDU (666,697,703,709,680); origin-echo overflow INVALID_PDU (320).
- Method: len INVALID_PDU, frag SERVER_FRAG, handle INVALID_HANDLE/METHOD_ACCESS, perm UNAUTHENTICATED/UNENCRYPTED, malloc NO_RESOURCE (1093-1134).

`READ_RSP` / `READ_BY_UUID_RSP` / `WRITE_RSP` **error items** (per-item, keep other items successful):
- READ single: no property → INVALID_HANDLE or DATA_TYPE (580-597); control check FORBID_READ/UNAUTHENTICATED/UNENCRYPTED (623-630); value fetch DATA_TYPE/SERVER_FRAG (636).
- READ multi: per-item codes in `{length:15, success:0}` (444-451).
- READ_BY_UUID: per-property ITEM_INEXIST/FORBID_READ/UNAUTHENTICATED/UNENCRYPTED/DATA_TYPE (876-896, 898-940); range-level SERVER_FRAG (759-762); no-match ITEM_INEXIST (1007).
- WRITE single/multi: per-handle INVALID_HANDLE/DATA_TYPE/FORBID_WRITE/UNAUTHENTICATED/UNENCRYPTED in error list (write.c:266-297, 633-651).

Authorization deny path (`SSAPS_SendUserResponse`, ssaps_server.c:1414-1457): READ deny → READ_RSP error item `UNAUTHORIZED`; WRITE_REQ deny → WRITE_RSP error item `UNAUTHORIZED`; WRITE_CMD deny → silent.

Our stack today (`ours:src/ssap_server.c`): only ERROR_RSP is produced (write path), READ single emits raw value or a 2-byte `length:15|success:1` error item, WRITE_RSP error list has one item (encode_write_rsp, ours:src/ssap_codec.c:185-206). No READ_RSP multi items, no WRITE_RSP error-list framing for partial multi, no UNAUTHENTICATED/UNENCRYPTED/UNAUTHORIZED/METHOD_ACCESS/DATA_TYPE, no SERVER_FRAG guards.

## 7. 应用层 API — app-layer surface vs ours

OHOS server app API (nlstk_ssap_app_server.h:359-498): `NLSTK_SsapServerRegApp[Asyn]`, `DeregisterApplication[Async]`, `SetMtu`, `AddService`, `RemoveService`, `ClearServices`, `AuthorizeResult(appId, requestId, allow)`, `UpdatePropertyValue`, `UpdateDescriptorValue`, `UpdateAndNotifyProperty`, `CheckServiceExistByUuid`, `SendMethodCallRes`, `CleanServerApp`, `FreeServiceParam`. Callback struct `NLSTK_SsapAppServerCb_S` (server.h:306-357): `onMtuChanged`, `onAddService`, `onSetPropertyValue`, `onSetDescriptorValue`, `onReadPropertyAuthorizeRequest`, `onReadDescriptorAuthorizeRequest`, `onWritePropertyAuthorizeRequest`, `onWriteDescriptorAuthorizeRequest`, `onReadProperty`, `onReadDescriptor`, `onWriteProperty`, `onWriteDescriptor`, `onCallMethod`, `onNotifyProperty`, `onConnectionStateChanged`, `onRegisterApp`.
- Registry: up to `NLSTK_SSAP_SERVER_APP_MAX_NUM=64` apps (ssaps_server_app.c:49); each holds a service vector + callbacks.
- AddService takes a rich `NLSTK_ServiceParam_S` (service statement + references + properties + methods + events) and deep-copies it (ssaps_service_param.c:247-291); handle block allocated from a first-fit allocator (ssap_handle.c:53-91) starting at `SSAP_HANDLE_MIN`, service handle first, then properties, then methods.
- Client surface for reference (nlstk_ssap_app_client.h:226-290): `ClientRegApp`, `DiscoverServices[ByUuid]`, `GetServices`, `ReadProperty/ReadProperties/ReadPropertyByUuid`, `ReadDescriptor`, `Get/SetPropertyNtf/Ind`, `WriteProperty/WriteDescriptor`, `CallMethod`, `CleanClientApp`, `GetMultiProcessing`.

Ours (`ours:include/ssap_server.h`): `add_service(uuid16, is_primary)`, `add_property(svc_handle, uuid16, operation, permission, read_cb, write_cb)`, `dispatch`, `notify`, `apply_config`. **No methods, no events, no descriptors, no 128-bit uuid, no appId registry, no callbacks struct, no auth.** Interface shape is a flat C callback table instead of an app registry.

## 8. 差距清单 — gap list vs our stack

### P0 — required for interconnection with OHOS/WS73/WS63-style peers
1. **Multi-handle READ items + ctrl.multi/error framing.** Ours only handles single-item READ; no `{length:15,success:1}` item packing, no per-item error. Reference: ssaps_server.c:424-514, 516-578. Ours: src/ssap_server.c:162-189.
2. **Multi-handle WRITE (ctrl.multi) + sub-items + WRITE_RSP error list/partial result.** Ours writes a single error item only; needs `errorNum{handle,errorCode}...` and `ctrl.result=PART`, plus `NEED_ORIGIN_RET(0x20)` echo of origin data. Reference: write.c:203-346, 562-607. Ours: ssap_codec.c:185-206, ssap_server.c:190-206.
3. **READ_BY_UUID_REQ/RSP (0x0A/0x0B)** — completely missing from dispatch. Reference: ssaps_server.c:714-1035. Ours: dispatch returns -1 for 0x0A.
4. **FIND_STRUCTURE_BY_UUID_REQ (0x06)** — missing. Reference: find.c:1453-1495 (uuid parse), 383-386/818-829/1381-1386 (uuid-type dispatch).
5. **CALL_METHOD_CMD/REQ/RSP (0x12/0x13/0x14)** — completely missing. Full semantics in §4. Reference: ssaps_server.c:1087-1149, 1459-1504; pkt.h:557-588.
6. **CCCD (client-config) gating of NTF/IND + per-addr CCCD tracking.** Ours sends NTF/IND un-gated. Reference: ssaps_server.c:1235-1367; ssaps_server_write.c:116-174. Also **VALUE_IND → VALUE_ACK** receive path (currently ours has no ack handler; add stub, ssaps_server.c:1040-1060).
7. **ExchangeInfo capability bits** — advertise/negotiate `fragment` and `multiProcessing` ctrl bits and store `link->fragment/multiProcessing` when version>=1.3 (ssaps_server.c:110-117); ours sends ctrl=0x03 only.
8. **FIND v1.0/v1.3 dual-format** — ours already gates primary/property on version<1.3 (ok), but **MIX responses** (std/cus indicator-split lists, needed when both uuid kinds coexist) and **itemType filtering** are missing. Reference: find.c:232-316, 674-802, 956-1019, 406-412.

### P1 — functional completeness
9. **findType SERVICE_STRUCTURE (0x00)** with per-item variable layout (itemType/uuid/ref handles/op/descCount). Reference: find.c:1118-1411; utils.c:132-180; itemType enum ssap_type.h:73-89.
10. **findType METHOD (0x04)** member (handle+uuid+5 zero bytes) and **findType EVENT (0x05)** must answer `ERROR_RSP ITEM_INEXIST`. Reference: find.c:914-923, 1075-1116, 1441-1444.
11. **Descriptors**: add descriptor types 0x01-0x04 to the property model; descriptor READ (SSAPS_GetPropertyValue type!=0) and WRITE (type!=0) paths; descriptor write gating on op bits 0x100/0x200/0x400 → FORBID_WRITE. Reference: ssaps_server.c:258-316; write.c:388-422.
12. **Permission gating** (AUTH/ENCRYPT/AUTHZ → UNAUTHENTICATED/UNENCRYPTED/UNAUTHORIZED) + **authorization pending queue** (requestId, pop on `AuthorizeResult`). Reference: ssaps_server.c:131-169, 1414-1457; read/write/method control checks.
13. **WRITE_CMD multi-subitem path + validation** (oper==INSTANT, fragment==NO_FRAG). Reference: write.c:725-937.
14. **Server-side value-length guard** `> mtu-2` → SERVER_FRAG on read paths. Reference: ssaps_server.c:311-314, 759-762.
15. **App registry + callbacks** (onAddService/onRead/onWrite/onMtuChanged/onConnectionStateChanged/onNotifyProperty/onCallMethod) — ours uses flat C callbacks; adding registry enables multi-app and notify-result plumbing. Reference: ssaps_server_app.c:49-494; nlstk_ssap_app_server.h:306-357.
16. **Service-change notification** on handle 0x000E (4-byte `{start u16}{end u16}` VALUE_NTF, gated to non-OLD peers) + SHA-256-truncated service hash on 0x000F. Reference: ssaps_service.c:469-531.

### P2 — full conformance / polish
17. **128-bit custom UUID** support in service table (ours is uuid16-only; affects FIND CUS members, READ_BY_UUID, uuid-type dispatch). Reference: find.c:39-63 (GetUuidType), utils.c:101-130.
18. **FIND itemType MIX indicator encoding** as full std/cus split runs (P2 if peers never mix uuid kinds). Reference: find.c:232-273.
19. **Fragmentation**: currently both OHOS and ours reject fragmented PDUs with SERVER_FRAG; implement true fragment reassembly only if a peer requires it. Reference: write.c:712-717, ssaps_server.c:690-693.
20. **rspMode=MULTI_RSP** (multiple responses per request) — OHOS rejects with UNSUPPORT_PDU; mirror that. Reference: find.c:1413-1419.
21. **Interaction timeout/timer** for method-call and indicate tasks (`SSAP_INTERACTION_DEFAULT_TIMEOUT=30000`). Reference: ssap_manager.c:89-120, ssap_link.c:344-364; OHOS admits method timeout is unimplemented (ssaps_server.c:1144).

## Open questions
1. **Method `needAuth` quirk**: OHOS sets method-call `needAuth` from the ENCRYPTION permission bit (ssaps_server.c:1141-1142) rather than AUTHORIZATION — bug or spec? Check WS63 sample method-call docs.
2. **READ_BY_UUID single-vs-multi asymmetry**: single-instance item is `{handle}{value}` (no length), multi is `{handle}{length:15|success:1}{value}`. Client length inference in single mode relies on PDU end. Confirm WS63 client parses it that way.
3. **VALUE_ACK payload**: server treats ack bytes as opaque `value` (no handle field on wire); client packs `valueInfo->value` (ssapc_client.c:1614-1624). If a peer sends `{handle u16}{result u8}`, our decode must handle both. WS63 client sample needed.
4. **v1.0 peer detection**: OHOS uses `CM_GetLogicLinkDeviceType(lcid) == CM_DEVTYPE_OLD`; ours infers from negotiated `version<1.3`. These can disagree when an old device negotiates v1.3; decide our rule once firmware behavior is captured.
5. **`SSAP_SERVICE_CHANGE_EVENT_HANDLE 0x000E` / `SSAP_HASH_HANDLE 0x000F`**: our `ssap_server.h` defines the constants but nothing emits/reads them; hash algorithm (SHA-256 low 128 bits over structure-info bytes, ssaps_service.c:494-531) is a candidate for adv-type-0x09 if we implement advertising later.
6. **Method call with AUTHZ**: OHOS only checks AUTH/ENCRYPT for methods (no AUTHORIZATION_NEED check in `SSAPS_MethodCallCheck`), yet the app callback receives `needAuth`. The authorize-response path (`SSAPS_SendUserResponse`) has no METHOD case — a pending method call can only be resolved via `SendMethodCallRes`. Confirm intended UX.
