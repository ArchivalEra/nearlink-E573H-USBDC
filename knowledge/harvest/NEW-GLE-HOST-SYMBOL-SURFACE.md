---
type: harvest
title: "device_soc ws63v100 protocol layer deep-dive — device-side SLE host ships closed as libbth_gle.a; symbol surface reveals full SM/TM/DM/CM layering and the authoritative uapi_ssaps surface"
language: en
created: 2026-09-13
tags: [ws63, gle, ssap, sm, security-manager, closed-source, symbol-surface, uapi, harvest]
sources:
  - "ArchivalEra/nearlink-E573H-USBDC workspace"
trust: A
stale_after: 2026-12-13
---

# device_soc ws63v100 protocol layer deep-dive — device-side SLE host ships closed as libbth_gle.a; symbol surface reveals full SM/TM/DM/CM layering and the authoritative uapi_ssaps surface

- Inspection date: 2026-09-13; resolution of the issue-11 observation item "protocol/middleware deep-dive"
- Method: sparse checkout of `ws63v100/sdk/protocol` (bt/controller+host, radar, wifi) from the metadata clone; `nm` symbol-table extraction from the distributed static archives
- Scope: what is open vs closed in the device-side BT/SLE protocol stack, and the layering the closed archive exposes

## Executive findings

1. The device-side BT/SLE host ships **closed**: `protocol/bt/host/gle` contains only `libbth_gle.a` (+CMake/Kconfig), and `bt/host/bt/sdk` only `libbth_sdk.a` — the SLE host implementation is not source-public even inside the OHOS vendor tree; only the controller transport (`bgtp`) builds from source. [`ws63v100/sdk/protocol/bt/host/` file lists]
2. The symbol table of `libbth_gle.a` (1035 unique symbols) exposes the internal layering of the closed SLE host: `gle_sm` (77 symbols — security manager), `sle_at` (69 — the AT command layer internals), `gle_hci` (55 — controller transport), `sapi_gle`/`uapi_gle` (50/49 — the two API boundary planes), `gle_dd` (45), `gle_aa` (45), `gle_tm` (37 — transport manager), `gle_dm` (35 — device manager), `gle_access` (19), `gle_cm` (16 — connection manager), plus `uapi_ssaps`/`uapi_ssapc` and `sle_announce_seek_*` exports. [`nm libbth_gle.a`]
3. **The device stack's SM layer is real and complete**: `gle_sm_authentication_procedure`, `gle_sm_authentication_start_g_node`/`_t_node` (two pairing roles), `gle_sm_authentication_recv_number_compare` (numeric-comparison association), `gle_sm_cmd_enable_encryption`, `gle_sm_cmd_encrypt_param_req_reply`, `gle_sm_cmd_encrypt_block` — the full SLE pairing + encryption handshake exists behind the closed archive. This is the concrete reference for the SM layer our userspace SSAP stack has not yet implemented. [`/tmp/gle-syms.txt` gle_sm_* set]
4. The authoritative device-side SSAP server UAPI surface (`uapi_ssaps_*`) includes functions our `assets/stack/ssap` API does not model: `_ex` variants of add_property/add_descriptor (extended property forms) and the `update_item_value_by_handle/by_uuid(_by_addr)` family — server-initiated value updates without a client write, i.e. the notify path at the server API level. [`uapi_ssaps_*` symbol set]
5. The `sle_at_*` symbols (69, including `sle_at_cmd_ssaps_add_service_callback` etc.) show the AT dialect is implemented INSIDE the host archive with callbacks into the SSAP server — the AT command layer of the YL63/DS10 dialects we documented sits on exactly this machinery. [`sle_at_*` symbol set]

## Boundaries and gaps

- Symbol names only — no disassembly, no decompilation; internal data layouts and call graphs remain unknown.
- The `libbth_sdk.a` (BT classic/BR-EDR side?) and wifi/radar protocol trees were inventoried but not symbol-analyzed in this pass.
- Symbol visibility (T/D/B) mixes exports and local strong symbols; the true public boundary is the `uapi_*`/`sapi_*` prefixes.

## Reusable for our stack

- The `uapi_ssaps` surface is the most authoritative reference for completing our SSAP server API: add the `_ex` property/descriptor forms and the `update_item_value_*` server-initiated update family to our implementation plan.
- The gle_sm function inventory is a buildable spec for our missing SM layer: two authentication roles, numeric-compare association, encryption-parameter negotiation, per-block encryption.
- The sapi/uapi dual-boundary naming is a clean pattern for our host stack's internal/external API separation.

## Comparison anchors (vs existing reports)

- `FBB-WS63-GLE.md`: that intel doc studied the GLE layer from headers; this closes the loop with the archive's actual exported surface and internal module names.
- `OHOS-SM-SECURITY.md`: the OHOS-side SM knowledge now has a device-side counterpart mapping (gle_sm_*).
- `NEW-OHOS-DEVICE-SOC-WS63.md`: the API-parity proof covered public headers; this adds the closed-underbelly layering the headers do not show.
