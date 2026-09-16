---
type: harvest
title: "device_soc ws63v100 middleware map — HCC architecture headers open (flow-ctrl + DFX loss counters), AT framework open except BT/SLE command table"
language: en
created: 2026-09-13
tags: [ws63, hcc, at-framework, flow-control, dfx, open-closed-map, middleware, harvest]
sources:
  - "ArchivalEra/nearlink-E573H-USBDC workspace"
trust: A
stale_after: 2026-12-13
---

# device_soc ws63v100 middleware map — HCC architecture headers open (flow-ctrl + DFX loss counters), AT framework open except BT/SLE command table

- Inspection date: 2026-09-13; continuation of the issue-11 observation item "middleware deep-dive"
- Method: sparse checkout of `ws63v100/sdk/middleware` (chips/ws63, services/wifi_service, utils/{algorithm,app_init,at,common_headers,dfx,error_code,fs,hcc})
- Scope: which device-side middleware is source-open vs closed, and what the open parts teach about our host stack's counterparts

## Executive findings

1. The **HCC transport architecture is header-open**: 19 files under `middleware/utils/hcc/` define the full device-side structure — `hcc.h` (handler API: `hcc_sched_transfer`, `hcc_change_state`, `hcc_check_header_vaild`, `hcc_enable_switch`), `hcc_bus.h`/`hcc_bus_types.h` (bus abstraction), `hcc_channel.h` (channel multiplexing), `hcc_list.h` (queue primitives), and `hcc_flow_ctrl.h` (`hcc_flow_ctrl_sched_check/process/pre_proc/module_init`). Implementation `.c` files are absent — closed like the GLE archive. [`middleware/utils/hcc/comm/`]
2. HCC tracks **queue loss as a first-class DFX metric**: `hcc_dfx_queue_total_pkt_increase` and `hcc_dfx_queue_loss_pkt_increase` counters in the open header — the device side measures dropped-queue packets, the exact signal our host-side DLI diagnostics see as byte drops during link noise. [`hcc.h` DFX section]
3. The **AT framework is open except the BT/SLE command table**: `middleware/utils/at/at/` (9 core .c files), `at_plt_cmd` (2), `at_wifi_cmd` (7) ship sources, while `at_bt_cmd/src/` holds only `at_bt_cmd_register.h` — the BT/SLE AT command bodies live behind the closed boundary (consistent with the `sle_at_*` symbols inside `libbth_gle.a` from `NEW-GLE-HOST-SYMBOL-SURFACE.md`). [`middleware/utils/at/` tree]
4. The remaining middleware is support infrastructure: `services/wifi_service`, `utils/{algorithm, app_init, common_headers, dfx, error_code, fs}` — OHOS-side glue and utility layers, no additional SLE content. [`middleware/` tree]
5. Net open/closed map for the device stack now reads: **open** = adapter/HAL, AT framework core, WiFi command set, HCC architecture headers, all public headers; **closed** = GLE/SLE host (`libbth_gle.a`), BT/SLE AT command bodies, HCC implementation.

## Boundaries and gaps

- Header-only analysis for HCC; the flow-control algorithm (credit windows vs stop-and-wait) is not visible from prototypes.
- The wifi_service middleware (services/) was inventoried, not audited — WiFi is adjacent to our scope only for tri-mode coexistence.
- No build validation (read-only inspection per standing rules).

## Reusable for our stack

- `hcc_flow_ctrl.h`'s three-phase API (sched_check → pre_proc → process) is a direct architectural mirror for our USB transport's pending-frame/credit gating — the device side genuinely implements flow control as a distinct module, validating our host-side split.
- The DFX queue-loss counters give us the device-side metric name to watch in logs when debugging our USB link drops.
- The open/closed boundary map tells us exactly where to spend reverse-engineering effort (closed GLE host + bt_cmd table) versus where headers suffice.

## Comparison anchors (vs existing reports)

- `02-dli-hcc-dialect.md` / `kernel-init-seq.md`: our HCC dialect intel from the WS73 SDK now has the device-side architecture headers to cross-check channel and flow-control semantics.
- `NEW-GLE-HOST-SYMBOL-SURFACE.md`: the closed boundary continues — GLE host archive and BT AT table are two halves of the same closure.
- `WS63-AT-FRAMEWORK.md`: that report's framework knowledge gains the precise open/closed split of the command sets.
