# SSAP Dialect Comparison: OHOS host-stack vs HiSilicon BS21/WS73 firmware

Sources: OHOS ssap/ (local clone), BS21 API (ili9320-i80-hi2821e-spi-bridge/include/middleware/services/bts/sle/), WS73 API (sdk/include/bsle/sle/), fbb_bs2x/fbb_ws63/sle_measure_sdk.

## VERDICT: SAME WIRE DIALECT (confirmed)

OHOS SSAP engine = standard HiSilicon NearLink SSAP protocol, matches BS21/BS2x/WS63 public-API dialect:
- Opcode table 0x01-0x14 1:1; find/read/write/notify/ind PDU layouts aligned
- Operation-indication bits byte-identical: READ 0x01, WRITE_NO_RSP 0x02, WRITE 0x04, NOTIFY 0x08, INDICATE 0x10, BROADCAST 0x20, DESCR_WRITE 0x100 (+0x200/0x400 newer)
- Error codes 0x01-0x11 map 1:1 (OHOS SSAP_PduErrCode_E == fbb_bs2x ssap_errcode_t)
- UUID wire: 2B standard (base 0x37BEA880-FC70-11EA-B720-000000000000) / 16B custom
- Version negotiation 1.0→1.3 (OHOS default 1.3 = 0x0301); older peers get init FIND primary-service req
- API mapping: HiSilicon ssaps_register_server/add_service/add_property/add_descriptor/start_service/notify_indicate/set_info ≈ OHOS NLSTK_SsapServer*

## WS73-specific risks (port watch points)
1. **WS73 server API LACKS read-by-UUID request callback** — OHOS READ_BY_UUID_REQ (0x0A) has no WS73 device handler
2. WS73 lacks descriptor-configuration-write op bits 0x200/0x400
3. WS73 headers are OLDER subset than BS21/BS2x/WS63 (fewer connection-manager functions, smaller structs)
4. Multi-value write/read only after v1.3 multiProcessing negotiation
5. Permission bits differ at APP level (OHOS auth/encr/authz bits 0-2 vs HiSilicon READ/WRITE/ENCR...0x01-0x10) — NOT wire fields, host must translate

## Connection model
- OHOS CM (CM_LINK_STATE_* 0-3, remote/local disconnect 0x10/0x11) == HiSilicon sle_acb_state_t / sle_disc_reason_t (same 5-state, host/device split)
- SSAP hooks CM as module 0x4, data on TCID_SLE_SMTC 0x0A

## Implication
Our slim SSAP server interoperates with BS2x/WS63 devices. For WS73-firmware-as-peer: avoid READ_BY_UUID (or handle 0x0A specially), watch descriptor-write gating, negotiate version conservatively (send exchange_info with 1.0 flags first if firmware seems old).
