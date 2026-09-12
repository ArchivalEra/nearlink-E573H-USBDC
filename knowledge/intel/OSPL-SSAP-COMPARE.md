---
type: intel
title: OpenSparklink SSAP vs Our OHOS-based SSAP Stack — Opcode & Semantics Comparison
language: en
created: 2026-08-17
tags: []
---

# OpenSparklink SSAP vs Our OHOS-based SSAP Stack — Opcode & Semantics Comparison

Date: 2026-08-17

## Verdict up front

OpenSparklink's `sle_ssap.rs` implements the **same opcode table (0x01–0x14) as OHOS/HiSilicon**, but its **PDU field layouts diverge substantially from the OHOS dialect** (no `msgCtrl` byte, offset-based long reads/writes, no length field in ValueNtf/Ind, no handle in ValueAck). Its own header comment says the current implementation is "loopback-only" (`sle_ssap.rs:17-18`). Treat OSPL as a **semantics reference** (feature inventory, negotiation model, fragmentation approach), **not** as a byte-level wire reference. For wire compatibility with HiSilicon WS73/BS2x/WS63 and phones, the OHOS layout in our `ssap_pkt.h` is authoritative (confirmed by `SSAP-DIALECT-COMPARISON.md`).

## Sources

Primary:
- `/mnt/hdd/nearlink-stuff/OpenSparklink-linux/net/sparklink/sle_ssap.rs` (2481 lines) — opcode enum, EntryCategory, OpIndicator, PropertyEntry/MethodEntry/EventEntry, SsapError, SsapPdu codec, SsapSession (client+server), RemoteServiceDb.
- `/mnt/hdd/nearlink-stuff/sparklink/crates/slk-protocol/src/types.rs` — UAPI ioctl structs (SsapSummary, SsapReadWrite, SsapAddService, SsapRemoteReadWrite, SsapUuidOp, etc.).
- `/mnt/hdd/nearlink-stuff/sparklink/crates/slk-protocol/src/service_hash.rs` — service structure hash (SHA-256 truncated to 128 bits) + EntryCategory values 0x00–0x0D.
- `/mnt/hdd/nearlink-stuff/sparklink/crates/slk-protocol/src/advdata.rs` — adv TLV incl. ServiceStructureHash type 0x09.
- `/mnt/hdd/nearlink-stuff/sparklink/crates/libsparklink/src/adapter.rs` + `ffi.rs` + `ioctl.rs` — client-side SSAP surface via ioctls 0x50–0x5F, 0x6F, 0x72.

Our side:
- `/home/archivalera/plum/zcode-projects/nearlink/stack/ssap/include/ssap_pkt.h` (OHOS-derived PDU defs).
- `/home/archivalera/plum/zcode-projects/nearlink/stack/ssap/include/ssap_codec.h` + `src/ssap_codec.c`.
- `/home/archivalera/plum/zcode-projects/nearlink/stack/ssap/include/ssap_server.h` + `src/ssap_server.c`.
- `/home/archivalera/plum/zcode-projects/nearlink/.scratch/nearlink-driver/lab-notes/SSAP-DIALECT-COMPARISON.md` and `SSAP-IMPLEMENTATION-PLAN.md`.

---

## 1. Opcode inventory

OSPL enum (`sle_ssap.rs:31-52`) is **hex-identical** to our `ssap_pkt.h:121-143` / `ssap_codec.h:19-40` enum:

| Hex | OSPL name (sle_ssap.rs) | Our name (ssap_pkt.h/ssap_codec.h) | Match |
|---|---|---|---|
| 0x01 | ErrorRsp | SSAP_ERROR_RSP | ✅ |
| 0x02 | ExchangeInfoReq | SSAP_EXCHANGE_INFO_REQ | ✅ |
| 0x03 | ExchangeInfoRsp | SSAP_EXCHANGE_INFO_RSP | ✅ |
| 0x04 | FindStructureReq | SSAP_FIND_STRUCTURE_REQ | ✅ |
| 0x05 | FindStructureRsp | SSAP_FIND_STRUCTURE_RSP | ✅ |
| 0x06 | FindByUuidReq | SSAP_FIND_STRUCTURE_BY_UUID_REQ | ✅ (naming differs) |
| 0x07 | FindByUuidRsp | SSAP_FIND_STRUCTURE_BY_UUID_RSP | ✅ (naming differs) |
| 0x08 | ReadReq | SSAP_READ_REQ | ✅ |
| 0x09 | ReadRsp | SSAP_READ_RSP | ✅ |
| 0x0A | ReadByUuidReq | SSAP_READ_BY_UUID_REQ | ✅ |
| 0x0B | ReadByUuidRsp | SSAP_READ_BY_UUID_RSP | ✅ |
| 0x0C | WriteCmd | SSAP_WRITE_CMD | ✅ |
| 0x0D | WriteReq | SSAP_WRITE_REQ | ✅ |
| 0x0E | WriteRsp | SSAP_WRITE_RSP | ✅ |
| 0x0F | ValueNtf | SSAP_VALUE_NTF | ✅ |
| 0x10 | ValueInd | SSAP_VALUE_IND | ✅ |
| 0x11 | ValueAck | SSAP_VALUE_ACK | ✅ |
| 0x12 | CallMethodCmd | SSAP_CALL_METHOD_CMD | ✅ |
| 0x13 | CallMethodReq | SSAP_CALL_METHOD_REQ | ✅ |
| 0x14 | CallMethodRsp | SSAP_CALL_METHOD_RSP | ✅ |

No side defines opcodes above 0x14. No PDU length field and no transaction-id field in **either** dialect (OSPL negotiates an *optional* max TID count, `sle_ssap.rs:321, 1741`, but never actually uses TIDs on the wire).

---

## 2. Service / attribute / method / event model

OSPL (`sle_ssap.rs:58-74, 231-289, 438-512`):
- **EntryCategory** byte values per T/XS 20001-2025 §7.4.3: PrimaryService 0x00, SecondaryService 0x01, Property 0x02, Method 0x03, Event 0x04, ServiceRef 0x05, CustomPrimary 0x08, CustomSecondary 0x09, CustomProperty 0x0A, CustomMethod 0x0B, CustomEvent 0x0C, CustomServiceRef 0x0D. Identical values in `service_hash.rs:20-33`.
- Addressing: flat **16-bit handles** allocated sequentially from 0x0010 (`sle_ssap.rs:412`); each service holds `start_handle`/`end_handle`; properties/methods/events are per-service lists each with their own handle. 16-bit standard UUID **or** 128-bit custom UUID (`SsapUuid`, `sle_ssap.rs:82-87`).
- **OpIndicator** 4-byte bitmask (`sle_ssap.rs:112-131`): READ 1<<0, WRITE_NO_RSP 1<<1, WRITE_WITH_RSP 1<<2, NOTIFY 1<<3, INDICATE 1<<4, BROADCAST 1<<5, DESC_WRITABLE 1<<8, CLIENT_CFG_WR 1<<9, SERVER_CFG_WR 1<<10. (Matches our `SSAP_OP_*` low 6 bits, `ssap_codec.h:80-85`, and OHOS.)
- **Descriptors** (`sle_ssap.rs:174-216`): PropertyDesc 0x01, ClientConfig 0x02, ServerConfig 0x03, PropertyFormat 0x04 (7 bytes: type, exponent, unit UUID, desc UUID); ClientConfigFlag None/Notification 0x0001/Indication 0x0002; DataType codes 0x01–0x0F.
- Per-property `max_len` u16 and `PROPERTY_VALUE_MAX = 512` (`sle_ssap.rs:229`).
- Notification gating: notify only if `ops.NOTIFY && (client_cfg & 0x01)`; indicate only if `ops.INDICATE && (client_cfg & 0x02)` (`sle_ssap.rs:714-716`); client-config write is gated by `CLIENT_CFG_WR` (`sle_ssap.rs:786-799`).
- No access/permission (AUTH/ENCRYPT/AUTHZ) field in OSPL — read/write gating is purely OpIndicator-based.

Our server (`ssap_server.h:23-55, ssap_server.c`):
- Service table: max 8 services × 32 properties, handle allocation from 0x0001 (`SSAP_HANDLE_START`), `uuid16` only (no 128-bit), a `permission` byte (AUTH/ENCRYPT/AUTHZ, `ssap_server.h:43`), and read/write callbacks. No methods, no events, no descriptors in the table.
- **Category byte mismatch (❓ likely bug)**: our `ssap_item_type_t` uses PrimaryService 0x01, SecondaryService 0x02, Property 0x03, Method 0x04, Event 0x05 (`ssap_server.h:23-29`), but the spec/OSPL wire category bytes are 0x00/0x01/0x02/0x03/0x04. These values are emitted raw into FIND_STRUCTURE_RSP (`ssap_server.c:108, 130`), so our on-wire category bytes are wrong vs OSPL and presumably vs the HiSilicon dialect. `ssap_pkt.h` does not define the category enum, so the exact OHOS wire value is unverified — see Open questions.

---

## 3. PDU field layout

Fundamental framing divergence:

| Aspect | OSPL (sle_ssap.rs) | OHOS / ours (ssap_pkt.h) |
|---|---|---|
| Header | opcode(1) + payload. **No msgCtrl byte.** (`encode`, `sle_ssap.rs:1028+`) | msgCode(1) + msgCtrl(1) + payload (`SSAP_PDU_BASE_LEN 2`, `ssap_pkt.h:30`) |
| Trans type | n/a | msgCode high bit = reply mask, CMD/REQ 0x82/RSP/NOTI/IND 0x85/ACK (`ssap_pkt.h:107-116`) |
| PDU length | none (MTU-bounded) | none (MTU-bounded) |
| TID | optional, negotiated, unused | none |

Per-opcode layout comparison:

- **ErrorRsp** — OSPL: `[opc][req_opcode][handle u16][error]` = 5 B (`sle_ssap.rs:1030-1043`). Ours/OHOS: `[msgCode][msgCtrl resv][msgCodeReq][errHandle u16][errorCode]` = 6 B (`ssap_pkt.h:180-186`, `ssap_codec.c:171-183`). ⚠️ OSPL drops the ctrl byte.
- **ExchangeInfo** — OSPL: `mcc` byte then conditionals (bit0→mtu u16, bit1→version major,minor 2 B, bit2→ext_mcc 4 B) (`sle_ssap.rs:1299-1339`); reliable = mcc bit3; backwards-compat "old format" 2-byte MTU-only payload (`sle_ssap.rs:1348-1355`). Ours/OHOS: ctrl bitfield (mtu bit0, version bit1, extMsgCtrl bit2, reliable bit3, fragment bit4, multiProcessing bit5) + msgMtu u16 + msgVersion u16 (`ssap_pkt.h:223-236`, `ssap_codec.c:55-74`). ⚠️ Mostly wire-compatible for MTU+version 1.0–1.3 (byte-identical for 1.0 and 1.3); OSPL's ext_mcc sub-bits (frag_seq, TID) are its own invention.
- **FindStructureReq** — OSPL: `[start u16][end u16]` = 5 B, no ctrl/uuid (`sle_ssap.rs:1082-1093`). Ours/OHOS: ctrl(findType:3, itemType:2, rspMode:1) + start + end + optional uuid (`ssap_pkt.h:248-259`, `ssap_codec.c:96-116`). ⚠️ OSPL loses findType/itemType/rspMode + UUID filter.
- **FindStructureRsp** — OSPL: entries `[handle u16][category 1][uuid_len 1][uuid 2|16]` = 6/20 B each, no ops/descriptor count (`sle_ssap.rs:1094-1129, 1437-1487`). Ours/OHOS: per-type members; primary-service member = start(2)+end(2)+type(1)+uuid(2/16), property member = handle(2)+uuid(2/16)+operation(4)+descriptorCount(1) (`ssap_pkt.h:37-55, 269-282`). ⚠️ Different member structure.
- **FindByUuidReq (0x06)** — OSPL: `[uuid_len 1][uuid]` only, no handle range (`sle_ssap.rs:1130-1151`). Ours/OHOS (SSAP_FIND_STRUCTURE_BY_UUID_REQ): ctrl(findType/itemType/rspMode) + start + end + uuid (`ssap_pkt.h:248-259`). ⚠️ OSPL simplified.
- **ReadReq** — OSPL: `[handle u16][offset u16]` = 5 B (`sle_ssap.rs:1162-1170`); offset drives long-read continuation. Ours/OHOS: ctrl(fragment:2) + items `[handle u16][type 1]` (`ssap_pkt.h:292-307`, `ssap_codec.c:118-133`); long reads via fragment bits, **no offset**. ⚠️ Different long-read mechanism.
- **ReadRsp** — OSPL: bare `[data...]`, continuation implied by full-MTU payload (`sle_ssap.rs:1171-1178, 2191-2222`). Ours/OHOS: ctrl(fragment, multi, error) + either raw value (single) or items `[length:15][success:1][value]` (multi) (`ssap_pkt.h:317-335`, `SSAP_READ_RSP_DATA_OFFSET 2`). ⚠️ Our server currently writes handle(2)+vlen(2)+value into READ_RSP (`ssap_server.c:155-158`) — matches neither dialect (see takeaways).
- **ReadByUuidReq** — OSPL: `[uuid_len 1][uuid]` (`sle_ssap.rs:1179-1200`). Ours/OHOS: ctrl(uuidType:1) + handleStart + handleEnd + dataType + uuid (`ssap_pkt.h:347-357`). ⚠️
- **ReadByUuidRsp** — OSPL: `[handle u16][data]` (`sle_ssap.rs:1201-1210`). Ours/OHOS: ctrl(fragment/multi/error) + single `[handle][value]` or items (`ssap_pkt.h:367-391`). ⚠️
- **WriteCmd / WriteReq** — OSPL: `[handle u16][offset u16][data]` = 5+len (`sle_ssap.rs:1211-1230`). Ours/OHOS: ctrl(fragment:2, multi:1, oper:2, [verify:1 for WRITE_REQ]) + items `[handle u16][type 1][value]` (`ssap_pkt.h:401-448`). ⚠️ OSPL has no `type`, no oper/multi bits; uses offset-based fragmentation instead.
- **WriteRsp** — OSPL: `[handle u16]` = 3 B (`sle_ssap.rs:1231-1238`). Ours/OHOS: ctrl(result:2 = 00 success/01 partial/10 cancel) + items `[handle u16][errorCode 1]` for errors (`ssap_pkt.h:470-491`). ⚠️ Our server never emits WRITE_RSP at all — it answers WRITE_REQ with ERROR_RSP (`ssap_server.c:181-183`).
- **ValueNtf / ValueInd** — OSPL: `[handle u16][data]`, **no length field** (`sle_ssap.rs:1239-1256`). Ours/OHOS: ctrl(fragment:2, type:1=property/event) + items `[handle u16][length u16][value]` (`ssap_pkt.h:507-524`, `ssap_codec.c:153-169`). ⚠️ OSPL omits per-item length.
- **ValueAck** — OSPL: `[handle u16]` = 3 B, no result byte (`sle_ssap.rs:1257-1264`). Ours/OHOS: `[msgCode][ctrl(type:1)][result 1]` = 3 B, **no handle** (`ssap_pkt.h:534-545`, `SSAP_VALUE_ACK_PDU_MIN_LEN 3`). ⚠️ Ours currently encodes handle(2)+result(1) = 5 B (`ssap_codec.c:185-195`) — mismatch with OHOS (drop handle, keep result).
- **CallMethodCmd / Req** — OSPL: `[handle u16][data]` (`sle_ssap.rs:1265-1284`). Ours/OHOS: ctrl(fragment:2) + handle u16 + params (`ssap_pkt.h:557-568`). ⚠️ ctrl byte again.
- **CallMethodRsp** — OSPL: `[handle u16][data]` (`sle_ssap.rs:1285-1294`). Ours/OHOS: ctrl(fragment:2) + result only, **no handle** (`ssap_pkt.h:578-588`). ⚠️ OSPL includes handle, OHOS doesn't (correlation by request order).

Request/response correlation: OSPL uses a **half-duplex ordered pending-request model** (`PendingRequest`, `sle_ssap.rs:1681-1701`; "the client must not send another request until the previous response has been received", `sle_ssap.rs:1679-1680`). OHOS also relies on ordered request/response (trans-type bits in msgCode). Neither uses TID by default.

### Error codes

OSPL `SsapError` (`sle_ssap.rs:343-368`): Success 0x00, InvalidHandle 0x01, ReadNotPermitted 0x02, WriteNotPermitted 0x03, InvalidMsgCode 0x04, AuthRequired 0x05, RequestNotSupported 0x06, InvalidOffset 0x07, InsufficientResources 0x08, ServiceNotFound 0x09, PropertyNotFound 0x0A, MethodAccessError 0x0C.

Ours/OHOS `SSAP_PduErrCode_E` (`ssap_pkt.h:188-211`): Success 0x00, InvalidPdu 0x01, UnsupportedPdu 0x02, Unknown 0x03, InvalidHandle 0x04, NoResource 0x05, ForbidRead 0x06, ForbidWrite 0x07, Unauthenticated 0x08, Unauthorized 0x09, Unencrypted 0x0A, ItemInexist 0x0B, MethodAccess 0x0C, DataType 0x0D, DataLength 0x0E, DataRange 0x0F, ServerFrag 0x10, ItemOverLimit 0x11, Timeout 0xB0.

⚠️ **Different numbering**: OSPL InvalidHandle=0x01 vs OHOS 0x04; OSPL has no InvalidPdu/Unknown/Unencrypted/DataType/DataLength/DataRange/ServerFrag/ItemOverLimit/Timeout. OSPL adds InvalidOffset 0x07 (offset model) and ServiceNotFound 0x09. Our stack (OHOS numbering) is the right one for HiSilicon interop. Note our server currently returns error 0x0F (OHOS `DATA_RANGE`) for write failures (`ssap_server.c:182`) — semantically should be `FORBID_WRITE` (0x07) or an error list in WRITE_RSP.

---

## 4. Semantics we might be missing

1. **Offset-based long reads / writes** — OSPL supports `ReadReq{handle, offset}` and `WriteReq{handle, offset, data}` with client-side continuation state (`TxFragState`/`RxFragState`, `sle_ssap.rs:1706-1720`, `2191-2222`) and server-side reassembly (`sle_ssap.rs:2076-2161`). Our/OHOS model uses ctrl fragment bits (SSAP_CTRL_FRAG_BEGIN/MID/END/NO_FRAG) and write `oper` bits (immediate/continue/cancel) instead of offsets (`ssap_pkt.h:155-165, 404-410, 432-438`). **Our server ignores fragment/oper bits entirely** — long reads/writes are broken on our side.
2. **Notifications vs indications vs confirmations** — both dialects have ValueNtf/ValueInd/ValueAck. OSPL gates delivery on `client_cfg` from the ClientConfig descriptor and auto-ACKs indications (`sle_ssap.rs:714-716, 2259-2264`). Our server can send NTF/IND (`ssap_server_notify`) but has **no client-config tracking and no inbound IND→ACK handling**.
3. **Write-with-response vs write-without** — both support WriteCmd (no rsp) and WriteReq (rsp). OSPL additionally checks `WRITE_NO_RSP` vs `WRITE_WITH_RSP` ops (`sle_ssap.rs:703-706`). Our server answers WriteReq with ERROR_RSP instead of a proper WRITE_RSP, and doesn't gate on the operation bit.
4. **Read-by-UUID (0x0A/0x0B)** — OSPL implements full request/response (`sle_ssap.rs:2337-2362`). Our server returns -1 (unhandled) for 0x0A. Caveat from `SSAP-DIALECT-COMPARISON.md:16-17`: WS73 firmware server API lacks a read-by-UUID handler; if WS73 is the peer, 0x0A may be unsupported there.
5. **Find-by-UUID (0x06/0x07)** — OSPL implements (`sle_ssap.rs:2308-2336`); ours unhandled.
6. **CallMethod (0x12/0x13/0x14)** — OSPL has methods with input/output buffers, echo default (`sle_ssap.rs:850-867, 2275-2307`); ours unhandled (returns -1). OSPL includes handle in CallMethodRsp; OHOS does not.
7. **Multicast / manycast / group discovery** — **neither** OSPL nor our stack implements SSAP group/manycast. OSPL has BROADCAST op bit (1<<5) only as a property flag. Adv-data types for service lists (CompleteStdServiceList 0x05, ServiceStructureHash 0x09, etc., `advdata.rs:22-37`) exist for advertising discovery, not in-band group discovery. No gap to close beyond note.
8. **Service structure hash & SERVICE_CHANGE** — OSPL implements the 128-bit service-hash computation (`service_hash.rs:66-107`, SHA-256 truncated low 16 B) for adv TLV type 0x09. **Neither** OSPL nor our server emits a VALUE_NTF on special handle 0x000E (SERVICE_CHANGE) when the service table changes, and neither reads handle 0x000F (HASH). Our codec/server define the constants (`ssap_codec.h:88-89`, `ssap_server.h:17-18`) but never use them.
9. **Attribute value length limits** — OSPL: per-property `max_len` u16, global `PROPERTY_VALUE_MAX=512`, `SSAP_PDU_MAX=247` (`sle_ssap.rs:229, 964`). Ours: `SSAP_MAX_VALUE_LEN 1024`, MTU default 251 / max 1024 (`ssap_pkt.h:27-28`, `ssap_server.h:21`). Our codec's `ssap_encode_value` uses a u16 length, OHOS ValueItem length is u16 too — fine. Our server's write path bounds to `len-5` without checking a per-property max (`ssap_server.c:178`).
10. **ExchangeInfo capability flags** — OSPL negotiates reliable mode (mcc bit3, `sle_ssap.rs:1951`), MTU = min, version = min tuple, optional frag_seq/TID via ext_mcc (`sle_ssap.rs:1936-1982`). Our server sends ctrl=0x03 (mtu+version only) and never advertises reliable/fragment/multiProcessing bits (`ssap_server.c:89-91`); our codec encoder also ignores bits 3–5 and ext fields.
11. **Multi-value (multiProcessing) read/write** — OHOS v1.3 supports multi-item READ/READ_BY_UUID/WRITE with `multi` ctrl bit and sub-item `{type,len,value}` tuples (`ssap_pkt.h:444-460, 323-335`). Neither OSPL nor our server implements it.
12. **rspMode (multiple responses to one request)** in FIND_STRUCTURE ctrl (`ssap_pkt.h:253`) — not implemented on our side; OSPL always returns a single response.

---

## 5. Opcode-by-opcode comparison table

Legend: ✅ match (hex), ⚠️ same opcode but field-layout diff, ❓ unverified, 🆕 theirs-only feature/field.

| Opcode | OSPL | OHOS (ours) | Hex | Layout | Notes |
|---|---|---|---|---|---|
| 0x01 | ErrorRsp | ERROR_RSP | ✅ | ⚠️ | OSPL omits ctrl byte (5 B vs 6 B); error code numbering differs entirely |
| 0x02 | ExchangeInfoReq | EXCHANGE_INFO_REQ | ✅ | ⚠️ | MTU+version wire-compatible for 1.0–1.3; OSPL adds ext_mcc/back-compat |
| 0x03 | ExchangeInfoRsp | EXCHANGE_INFO_RSP | ✅ | ⚠️ | same |
| 0x04 | FindStructureReq | FIND_STRUCTURE_REQ | ✅ | ⚠️ | OSPL: start/end only; OHOS adds ctrl(findType,itemType,rspMode)+uuid |
| 0x05 | FindStructureRsp | FIND_STRUCTURE_RSP | ✅ | ⚠️ | OSPL entry = handle+category+uuid; OHOS per-type members with ops+descCount |
| 0x06 | FindByUuidReq | FIND_STRUCTURE_BY_UUID_REQ | ✅ | ⚠️ | OSPL: uuid only; OHOS: ctrl+start/end+uuid |
| 0x07 | FindByUuidRsp | FIND_STRUCTURE_BY_UUID_RSP | ✅ | ⚠️ | OSPL: handle+data; OHOS rsp member layout |
| 0x08 | ReadReq | READ_REQ | ✅ | ⚠️ | OSPL: handle+offset; OHOS: ctrl+items(handle,type) — different long-read model |
| 0x09 | ReadRsp | READ_RSP | ✅ | ⚠️ | OSPL: bare data; OHOS: ctrl+value or ctrl+items(len/success/value) |
| 0x0A | ReadByUuidReq | READ_BY_UUID_REQ | ✅ | ⚠️ | OSPL: uuid; OHOS: ctrl+start/end handle+dataType+uuid |
| 0x0B | ReadByUuidRsp | READ_BY_UUID_RSP | ✅ | ⚠️ | OSPL: handle+data; OHOS: ctrl+items |
| 0x0C | WriteCmd | WRITE_CMD | ✅ | ⚠️ | OSPL: handle+offset+data; OHOS: ctrl(frag/multi/oper)+items(handle,type,value) |
| 0x0D | WriteReq | WRITE_REQ | ✅ | ⚠️ | OSPL: handle+offset+data; OHOS adds verify bit in ctrl |
| 0x0E | WriteRsp | WRITE_RSP | ✅ | ⚠️ | OSPL: handle only; OHOS: ctrl result + error items; ours never sends it |
| 0x0F | ValueNtf | VALUE_NTF | ✅ | ⚠️ | OSPL: handle+data (no len); OHOS: ctrl+items(handle,len,value) |
| 0x10 | ValueInd | VALUE_IND | ✅ | ⚠️ | same |
| 0x11 | ValueAck | VALUE_ACK | ✅ | ⚠️ | OSPL: handle only; OHOS: ctrl+result (no handle); our encoder is wrong (has both) |
| 0x12 | CallMethodCmd | CALL_METHOD_CMD | ✅ | ⚠️ | OSPL: handle+data; OHOS: ctrl+handle+params |
| 0x13 | CallMethodReq | CALL_METHOD_REQ | ✅ | ⚠️ | same |
| 0x14 | CallMethodRsp | CALL_METHOD_RSP | ✅ | ⚠️ | OSPL includes handle; OHOS: ctrl+result only |

Feature/field-level comparison:

| Feature | OSPL | Ours | Status |
|---|---|---|---|
| EntryCategory bytes 0x00–0x0D | ✅ | ❓ our item_type uses 0x01–0x05 | ⚠️ verify against OHOS wire |
| 128-bit custom UUID | ✅ `SsapUuid` | ❌ uuid16 only | 🆕 theirs |
| Methods in service table | ✅ | ❌ | 🆕 theirs |
| Events in service table | ✅ | ❌ (notify only, no event handle space) | 🆕 theirs |
| Descriptors (ClientConfig/PropertyFormat…) | ✅ | ❌ | 🆕 theirs |
| client_cfg gating of NTF/IND | ✅ | ❌ | 🆕 theirs |
| Offset long read | ✅ | ❌ (frag bits instead) | 🆕 theirs |
| Offset long write + reassembly | ✅ | ❌ (oper bits instead) | 🆕 theirs |
| Multi-value (v1.3) | ❌ | ❌ | — |
| TID negotiation | ✅ (optional, unused) | ❌ | 🆕 theirs |
| Reliable-mode transport (TCID connect) | ✅ | ❌ | 🆕 theirs |
| Service structure hash (adv 0x09) | ✅ | ❌ | 🆕 theirs |
| SERVICE_CHANGE 0x000E notify | ❌ | ❌ | — |
| CallMethod | ✅ | ❌ | 🆕 theirs |
| ReadByUuid / FindByUuid server handlers | ✅ | ❌ | 🆕 theirs |
| Half-duplex ordered client session | ✅ | ❌ (server-only) | 🆕 theirs |
| Remote service DB cache (client) | ✅ | ❌ | 🆕 theirs |

---

## 6. Actionable takeaways for ssap_codec.c / ssap_server.c

Codec (`stack/ssap/src/ssap_codec.c` / `include/ssap_codec.h`):

1. **Fix `ssap_encode_value_ack`** — OHOS VALUE_ACK is `[msgCode][ctrl(type bit)][result 0/1]`, **no handle** (`ssap_pkt.h:534-545`; `SSAP_VALUE_ACK_PDU_MIN_LEN 3`). Current encoder emits msgCode+ctrl+handle(2)+result (5 B, `ssap_codec.c:185-195`). Change signature to drop the handle (keep a `type` bit and result).
2. **Add a WRITE_RSP encoder** (`SSAP_MSG_WRITE_RSP`, 0x0E): `[msgCode][ctrl result:2][optional error items (handle,errorCode)]` per `ssap_pkt.h:470-491`. There is none today.
3. **Add READ_BY_UUID_REQ/RSP (0x0A/0x0B) and FIND_STRUCTURE_BY_UUID_REQ/RSP (0x06/0x07) encoders/decoders** following `ssap_pkt.h:337-391, 248-282`.
4. **Add CALL_METHOD_CMD/REQ/RSP (0x12/0x13/0x14) encoders/decoders** following `ssap_pkt.h:557-588`. Note OHOS CallMethodRsp has **no handle**; OSPL includes one — follow OHOS.
5. **Extend `ssap_encode_exchange_info`** to emit reliable/fragment/multiProcessing ctrl bits (3–5) and, per version, the ext-message-control field, mirroring `ssap_pkt.h:226-233`. The decoder already only reads mtu/version (`ssap_codec.c:76-94`) — extend to parse version min and flags.
6. **Add multi-item support** (v1.3): READ/READ_BY_UUID multi items `{length:15, success:1, value}` (`ssap_pkt.h:331-335`), WRITE multi sub-items `{type,len,value}` (`ssap_pkt.h:444-460`), gated by multiProcessing negotiation.

Server (`stack/ssap/src/ssap_server.c` / `include/ssap_server.h`):

7. **Fix FIND_STRUCTURE_RSP member encoding.** For findType=primary-service emit `start(2)+end(2)+type(1)+uuid(2/16)`; for property emit `handle(2)+uuid(2/16)+operation(4)+descriptorCount(1)` per `ssap_pkt.h:37-49`. Current code emits a hand-rolled 7-byte member missing end_handle for services (`ssap_server.c:103-115`) and the wrong field order/width for properties (`ssap_server.c:126-136`). Also **verify the category/type byte values** (see Open questions) — current 0x01–0x05 likely wrong on the wire.
8. **Fix WRITE_REQ response.** On success respond `WRITE_RSP` (0x0E) with ctrl.result=0b00; on failure send `WRITE_RSP` with error items `{handle, errorCode}` (or ERROR_RSP with a correct code — e.g. `FORBID_WRITE` 0x07, not 0x0F DATA_RANGE as at `ssap_server.c:182`).
9. **Fix READ_RSP format.** For a single read the OHOS response is `[msgCode][ctrl]` + value (or multi items) — drop the spurious handle(2)+vlen(2) prefix currently emitted (`ssap_server.c:154-162`); set the ctrl `multi` bit and item framing when responding to multi-instance requests, and honor fragment bits for long values.
10. **Add offset/fragment/oper handling** in READ_REQ and WRITE_CMD/WRITE_REQ dispatch (parse ctrl fragment + oper bits, reassemble continue-fragments and cancel-write per `ssap_pkt.h:155-165, 404-410, 432-438`). Currently ctrl is ignored (`ssap_server.c:79, 176`).
11. **Add READ_BY_UUID_REQ (0x0A) dispatch** (route to read_cb by UUID; WS73-firmware-as-peer caveat from `SSAP-DIALECT-COMPARISON.md:16-17`), **FIND_BY_UUID (0x06)** and **CALL_METHOD (0x12/0x13/0x14)** dispatch.
12. **Add VALUE_IND → VALUE_ACK auto-response** on the server receive path and **client-config descriptor tracking** so notify/indicate are gated on the per-client ClientConfig like OSPL (`sle_ssap.rs:714-716, 786-799`).
13. **Add SERVICE_CHANGE notification:** when `ssap_server_add_service`/`add_property` mutate the table, send a VALUE_NTF on handle 0x000E to subscribed clients (OHOS `SSAP_HANDLE_SERVICE_CHANGE`, defined at `ssap_server.h:17` but unused).
14. **Wire the permission byte** (AUTH/ENCRYPT/AUTHZ, `ssap_server.h:43`) into ERROR_RSP codes `UNAUTHENTICATED`/`UNAUTHORIZED`/`UNENCRYPTED` (0x08/0x09/0x0A) when a callback is denied — OSPL only gates on OpIndicator; OHOS adds permission gating.
15. If a client role is ever added, model it on OSPL's `SsapSession` (`sle_ssap.rs:1756-1982`): half-duplex `PendingRequest`, MTU-min/version-min negotiation, offset-based read continuation, tx/rx write-fragmentation state, and `RemoteServiceDb` cache.

---

## 7. Open questions

1. **Category byte values on the HiSilicon wire.** OSPL and `service_hash.rs` use 0x00/0x01/0x02/0x03/0x04 for primary/secondary/property/method/event. Our `ssap_item_type_t` uses 0x01–0x05 (`ssap_server.h:23-29`). `ssap_pkt.h` never defines the enum, so the exact OHOS/firmware wire value is unverified — needs a packet capture against a real BS2x/WS63 or an OHOS stack trace to confirm before fixing the FIND_STRUCTURE_RSP type byte.
2. **OHOS READ_RSP single-instance wire format.** `SSAP_READ_RSP_DATA_OFFSET = 2` (`ssap_pkt.h:79`) implies bare value after msgCode+ctrl for single reads, but our server currently emits handle+len. Confirm against OHOS `ssaps_server.c` send-read-rsp before changing.
3. **OHOS WRITE_REQ failure response.** Does OHOS reply with WRITE_RSP+error-items or ERROR_RSP? Our server uses ERROR_RSP 0x0F; OSPL uses ErrorRsp{WriteNotPermitted}. Needs an OHOS source check (not in our local set).
4. **OSPL wire compatibility claim.** OSPL cites T/XS 20001-2025 §7.4, but its layouts (no ctrl byte, offsets, no value length) contradict OHOS. If OSPL ever talks to HiSilicon hardware, which interpretation is correct? The DIALECT doc's "1:1" verdict is about OHOS vs HiSilicon firmware, not OSPL — OSPL's SSAP should be considered an independent (and currently loopback-only) implementation.
5. **ValueNtf/ValueInd length field.** OHOS ValueItem has an explicit u16 length (`ssap_pkt.h:520-524`); OSPL has none. Does HiSilicon firmware require the length field? (It should, per OHOS `SSAP_VALUE_NTF_PDU_MIN_LEN 7`.)
