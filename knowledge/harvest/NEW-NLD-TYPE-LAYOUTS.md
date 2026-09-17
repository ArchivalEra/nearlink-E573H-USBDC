---
type: harvest
title: "Nld eRPC shared argument layouts: one type block across nine service headers"
language: en
created: 2026-09-17
tags: [harvest, nld, erpc, layouts, ssap]
sources:
  - "https://gitcode.com/goodspeed1/Nld/tree/68b0e078e96ac30b1b5ae161df62a876e4ff89c7/erpc_gen/bs2x"
  - "https://gitcode.com/goodspeed1/Nld/blob/68b0e078e96ac30b1b5ae161df62a876e4ff89c7/erpc_gen/bs2x/sle_sle_ssap_client_common.hpp"
  - "https://gitcode.com/goodspeed1/Nld/blob/68b0e078e96ac30b1b5ae161df62a876e4ff89c7/erpc_gen/bs2x/sle_sle_connection_common.hpp"
  - "https://gitcode.com/goodspeed1/Nld/blob/68b0e078e96ac30b1b5ae161df62a876e4ff89c7/erpc_gen/bs2x/sle_sle_seek_common.hpp"
trust: A
stale_after: 2027-03-17
---

# Nld eRPC shared argument layouts: one type block across nine service headers

## Executive findings

The remaining queue item was the argument-struct layouts behind the previously inventoried service methods. At revision `68b0e078e96ac30b1b5ae161df62a876e4ff89c7`, all nine generated `_common.hpp` headers contain the same shared type block. A direct comparison of lines 22-501 across connection, seek, SSAP client and SSAP server headers produced no differences, and the nine file hashes are all distinct only because each carries its own include guard; the type sections match. Field layouts below therefore describe one shared contract.

- **Address, UUID and constants.** `sle_addr_t` is one type byte plus six address bytes (`sle_sle_ssap_client_common.hpp:220-224`). `sle_uuid_t` is a one-byte length with a 16-byte array (`:226-230`). Constants declared at `:503-511` are defined in the generated sources: address 6, UUID 16, name 31, link key 16, channel map 10 (`sle_sle_low_latency_client.cpp:30-54`).
- **Connection knobs are concrete.** PHY selection uses separate tx/rx format, PHY, pilot density and G/T feedback bytes (`:325-335`). Default connection parameters are filter policy, initiate PHYs, G/T negotiation, 16-bit scan interval/window and connection min/max/timeout (`:337-347`). Connection updates carry the connection ID in the applied variant and omit it in the request variant (`:308-323`).
- **SSAP registration uses pointer-plus-length pairs, not inline buffers.** Property and descriptor registration carry a UUID, 16-bit permission mask, 32-bit operate-indication and length-plus-pointer value fields (`:379-396`). Notification and response payloads follow the same length-plus-pointer pattern (`:398-422`).
- **Find and read-by-UUID requests share a shape with one extra reserved byte.** `ssapc_find_structure_param_t` has find type, handle range, UUID and one reserve byte; `ssapc_read_req_by_uuid_param_t` omits the reserve byte (`:354-369`). Consumers must not copy one into the other without accounting for that byte.
- **Host-side read/write events mirror request framing.** Read and write requests carry request ID, handle, property type, response and authorization flags; the write request appends length-plus-pointer value (`:440-469`). Find results expose handle ranges, operate indications, per-property descriptor type arrays and structure-level UUID plus property type (`:471-499`).
- **Seek parameters are arrays indexed by PHY.** `sle_seek_param_t` holds three seek types, three 16-bit intervals and three windows, matching the declared maximum of three seek PHYs (`:269-278`; `SLE_SEEK_PHY_NUM_MAX = 3` in `sle_sle_low_latency_client.cpp:64-66`).
- **Security and bonding are compact.** Security parameters are four bytes: IO capability, pair/binding, MITM defense, privacy (`:291-297`). Authentication events carry a 16-byte link key, crypto and key-derivation algorithm bytes, an integrity-check indicator and a bond flag (`:299-306,424-431`).

## Contract notes

| Type group | Members | Serialization implication |
|---|---|---|
| Addressing | `sle_addr_t`, `sle_uuid_t` | Fixed arrays; UUID length is explicit, allowing 2/4/16-byte forms. |
| Announcement | `sle_announce_param_t`, `sle_announce_data_t`, `sle_adv_ext_param_t` | Interval min/max are 32-bit; connection defaults ride in the same struct. |
| Connection | `sle_connection_param_update_req_t`/`_t`, `sle_set_phy_t`, `sle_default_connect_param_t`, `sle_set_host_channel_classification_t` | 10-byte channel map matches the declared constant. |
| Seek | `sle_seek_param_t`, `sle_seek_result_info_t` | Duplicate filtering and three-PHY scheduling. |
| Security | `sle_sec_params_t`, `sle_auth_info_evt_t` | MITM and privacy bits present at contract level. |
| SSAP client | `ssapc_find_structure_param_t`, `ssapc_read_req_by_uuid_param_t`, `ssapc_handle_value_t` | One reserved byte difference between find and read requests. |
| SSAP server | `ssaps_property_info_t`, `ssaps_desc_info_t`, `ssaps_send_rsp_t`, `ssaps_ntf_ind_t`, `ssaps_ntf_ind_by_uuid_t` | Permission and operate-indication bitmasks verified earlier at enum level. |
| Host events | read/write/find structures | Request IDs pair responses with asynchronous requests. |

These are eRPC codec fields, not raw wire bytes. The generated codec writes each member through typed accessors; pointer fields become length-plus-data sequences. Byte offsets, alignment and endianness on the serial transport remain the domain of the separately documented eRPC framing layer.

## Boundaries

- Only the BS2X tree was compared. WS63 headers were not diffed in this pass; the low-latency report already established that service surfaces differ between parts.
- No RPC was exercised and no build or hardware operation occurred. GitCode revision freshness was not re-queried this pass; the pinned revision is explicit.
- Struct presence does not prove that every daemon path populates every field. Call-site behavior for security, privacy and MITM fields was not traced.
- `bool` fields in host request structs map to eRPC boolean encoding; their wire width is not derived from C++ size here.
- GPL-3.0-or-later applies to the adapter sources; generated headers are autogranted outputs of the vendor IDL. Facts are recorded without importing implementation code.

## Reusable

1. Use this as the field-level reference when mapping WS73 host calls onto the Nld contract: one shared layout block, nine service headers.
2. Preserve the one-byte difference between find-structure and read-by-UUID request layouts in any adapter that reuses both.
3. Model pointer fields as explicit length-plus-buffer pairs in any Rust or C host reimplementation; never assume inline fixed-size payloads.
4. Keep the three-PHY seek array shape when porting scan logic, and keep 16-bit interval/window widths.
5. Record security-parameter fields even where the WS63 surface omits them; capability gating is a per-part property, not a protocol constant.

## Comparison anchors

- [NEW-NLD-ERPC-CONTRACT](NEW-NLD-ERPC-CONTRACT.md): service and method inventory plus SSAP enums; this report supplies the argument-struct field layouts that report listed as unread.
- [NEW-NLD-LOW-LATENCY-BOUNDARY](NEW-NLD-LOW-LATENCY-BOUNDARY.md): establishes per-part availability; here the shared block is verified identical across the nine BS2X headers.
- [NEW-NLD-ERPC-PROTOCOL](NEW-NLD-ERPC-PROTOCOL.md): transport framing remains the layer below these typed fields.
