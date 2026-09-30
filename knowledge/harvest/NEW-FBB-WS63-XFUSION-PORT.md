---
type: harvest
title: "ws63_porting_xf_0p2 — the XFusion-to-FBB port layer inside x-eks-fusion/fbb_ws63: in-tree build bridge, SLE SSAP port contracts, and byte-verified porting defects"
language: en
created: 2026-09-17
tags: [fbb_ws63, xfusion, port-layer, sle, ssap, ota, build-system, ws63, harvest]
sources:
  - "https://github.com/x-eks-fusion/fbb_ws63"
trust: A
stale_after: 2027-03-17
---

# ws63_porting_xf_0p2 — the XFusion port layer inside x-eks-fusion/fbb_ws63: the build bridge NEW-XFUSION.md could only infer, plus the SLE SSAP implementation contracts and four byte-verified porting defects

- Inspection date: 2026-09-17; read-only inspection of a local mirror of
  `x-eks-fusion/fbb_ws63` (shallow clone, depth 1, working tree clean except one
  untracked build artifact).
- Pinned revision: commit `f0fbaef71197650e7a25695bb814e8fadca511ba`
  (2025-03-13T06:41:24Z, author `dotchan`, master). All citations below are
  byte-verified against that commit's git blobs; the blob SHAs given in the
  source table uniquely pin the cited bytes. GitHub URLs are constructed from
  the verified remote (`origin = https://github.com/x-eks-fusion/fbb_ws63`) plus
  commit/blob SHAs and were not fetched (no network this round) — they are
  self-validating once opened.
- Scope: the in-tree port layer `src/application/ws63_porting_xf_0p2/` (~11.0k
  lines tracked, measured 10,956) that binds the XFusion component stack onto the HiSilicon FBB
  WS63 SDK. Read fully this round: the whole `port_xf_sle/` SLE port (8 files: 7 code + CMakeLists),
  `tasks_xf_entry/` bootstrap (4 files), and the Kconfig/CMake selection glue.
  Inventoried only: the BLE, WiFi and peripheral ports (`port_xf_ble/`,
  `port_xf_wifi/`, gpio/i2c/pwm/spi/uart/ringbuf/utils) and `platform_def/`
  headers.

## Executive findings

1. **The port layer is fork-added, not vendor code.** The official HiSpark
   upstream (`gitee.com/HiSpark/fbb_ws63`, HEAD `aa8f856be4a9253a4518b80399b5da16fba76bd1`,
   2026-08-14) contains zero `porting_xf` paths; `src/application/` there holds
   only `samples/` and `ws63/`. The x-eks-fusion fork injects
   `ws63_porting_xf_0p2/` and selects it with a Kconfig choice:
   `PORTING_XF_ENABLE` defaults to **y**, and the single choice member
   `WS63_PORTING_XF_V0_2_X` ("XF V0.2.x") sets `WS63_PORTING_XF_VERSION = 2`
   (`src/application/Kconfig:19-41`). CMake picks the port when
   `CONFIG_WS63_PORTING_XF_V0_2_X` is defined and otherwise falls back to
   `ws63_porting_xf_null` — an empty component kept only so the vendor linker
   script's expectations are untouched (`src/application/CMakeLists.txt:28-32`).
2. **The build bridge is an environment-variable handshake, confirming what
   NEW-XFUSION.md could only infer.** The port root CMake **fatals** if
   `XF_PROJECT_PATH` is unset — error text "please run in xf command line!" —
   then `include("$ENV{XF_PROJECT_PATH}/build/build_environ.cmake")`, which
   exports `XF_SRCS_STR` / `XF_INCS_STR` / `XF_CFLAGS_STR`; these become
   `XF_SOURCES` / `XF_HEADERS` for the sub-builds
   (`src/application/ws63_porting_xf_0p2/CMakeLists.txt:6-29`). None of the XFusion component
   sources exist in the fbb_ws63 tree — the enumerated stack (xf_utils, xf_sle,
   xf_heap, xf_init, xf_ota_client headers, xfconfig.h) is injected from
   *outside* the vendor tree at build time (only the port's own
   `platform_def/xf_hw_rsrc_def.h` matches the xf_ prefix locally). The
   platform-def include path is exported by
   `platform_def/platform_def.cmake:6-9` (`XF_PLATF_DEF` = the dir plus
   `hw_rsrc_def/`).
3. **The application component can be built from source or linked as a
   prebuilt archive.** `xf_app/CMakeLists.txt:16-50`: with
   `CONFIG_XF_APP_BUILD_FROM_SOURCE`, `SOURCES = ${XF_SOURCES}` and
   `COMPONENT_CCFLAGS = "-Wno-error" ${XF_CFLAGS_STR}`; otherwise
   `LIBS = libxf_app.a`. Both branches set `WHOLE_LINK true`,
   `MAIN_COMPONENT false`, and `install_sdk(... "*")` — the port uses the
   vendor component API verbatim. Notably line 11 (the xf_app empty branch) documents avoiding edits to vendor
   build/linker files (`ws63_liteos_app_linker/function.h`; the null
   component's own comment cites `build/config/target_config/ws63/config.py`). Every port CMakeLists also carries
   `"-Wno-error"` (`port_xf/port_xf_sle/CMakeLists.txt:41`,
   `tasks_xf_entry/CMakeLists.txt:37`) — which is exactly how the defects in
   finding 8 survive a `-Werror` vendor build.
4. **The whole XFusion runtime is one LiteOS task running a never-returning
   loop.** `tasks_xf_entry/tasks_xf_entry.c:34-57` (blob
   `857805de...`): `app_run(tasks_xf_entry)` registers the vendor auto-run
   entry; it creates a kernel thread named `TasksXFPremain` via
   `osal_kthread_create(..., TASKS_XF_PREMAIN_STACK_SIZE)`, sets
   `CONFIG_TASKS_XF_PREMAIN_PRIORITY`, frees the handle immediately, and the
   thread body is `xfusion_init(); while (1) { xfusion_run(); }` — all
   XFusion application code runs on that single task; there is no XFusion-side
   scheduler below it (XFusion tasks/timers map onto OSAL primitives inside
   the injected components).
5. **Port services are auto-init'ed at two XFusion boot levels.**
   `port_xf_log.c:33-47`: the XFusion log sink is
   `xf_log_register_obj(xf_log_out, NULL)` where `xf_log_out` forwards to
   `osal_printk("%.*s")`, registered via `XF_INIT_EXPORT_SETUP(port_log_init)`.
   `port_xf_sys.c:104-110`: `port_sys_init` calls
   `xf_sys_time_init(_port_xf_sys_get_us)` where the microsecond source is
   `uapi_tcxo_get_us()`, registered via `XF_INIT_EXPORT_BOARD(port_sys_init)`.
   The sys port also implements the watchdog/reboot contracts:
   `xf_sys_watchdog_enable/disable/kick` wrap `uapi_watchdog_*` with
   `WDT_MODE = WDT_MODE_INTERRUPT` (`port_xf_sys.c:32-75`), and
   `xf_sys_reboot()` disables the watchdog then `hal_reboot_chip()` — with a
   doc-comment pointing at the vendor AT reset handler
   (`src/middleware/utils/at/at_plt_cmd/at/at_plt.c:1029`,
   `at_exe_reset_cmd`) and an intended 3000 us wait for the AT print, whose
   delay call is commented out (`port_xf_sys.c:83-86`).
6. **SLE is ported as one event-callback funnel per role, and the two roles
   fight over a single vendor registration slot.** Both
   `port_xf_sle_ssap_server.c:78-83` and `port_xf_sle_ssap_client.c:116-120`
   call `sle_connection_register_callbacks()` with their own private
   `connect_state_changed_cb` / `connect_param_update_cb` implementations. The
   vendor API has one registration slot per host, so in a dual-role app the
   **last registrant wins** and the other side silently stops receiving
   connect/disconnect/param-update events. Only the client registers
   `sle_announce_seek_register_callbacks({seek_result_cb})`
   (`port_xf_sle_ssap_client.c:125-130`); the server never does. The advertised
   event-mask parameter is ignored on both sides (`unused(events)` —
   `port_xf_sle_ssap_server.c:71`, `port_xf_sle_ssap_client.c:109`): one
   global `s_sle_ssaps_evt_cb` / `s_sle_ssapc_evt_cb` receives every event of a
   discriminated-union param struct.
7. **The client's service discovery is sync-over-async with a 10-second
   budget and a shared result race.** `port_xf_sle_ssap_client.c:170-227`:
   `xf_sle_ssapc_discover_service` mallocs a `port_discover_service_node_t`
   `{link, uuid, start_hdl, end_hdl, is_cmpl, is_specific_uuid}`
   (`:32-39`), appends it to a static `xf_list_t s_queue_find_struct`
   (`:91,190`), issues `ssapc_find_structure`, then polls `task->is_cmpl`
   every `INTERVAL_MS_CHECK_ATTR_ADD` = 20 ms up to
   `TIMEOUT_CNT_CHECK_ATTR_ADD` = 500 iterations — i.e. a hard **10 s cap**
   before returning `XF_ERR_TIMEOUT` (constants at `port_sle_ssap.h:20-21`).
   Two contract wrinkles: (a) the per-result callback marks **every** queued
   task complete with the same service result, so overlapping discoveries
   cross-contaminate (`:424-439`); (b) the completion callback is a no-op
   (`:442-451`), and completion is signaled only from the per-result callback —
   a discovery that returns zero services never completes and burns the full
   10 s. A "specific UUID" test uses `strncmp` against a zeroed UUID over
   `param->uuid.type` bytes, so `type == 0` always classifies as non-specific
   (`:184`).
8. **Four byte-verified implementation defects** (all reachable, all muted by
   `-Wno-error`):
   a. **Infinite loop on the first service descriptor**:
      `port_xf_sle_ssap_server.c:171-193` — the descriptor sentinel loop
      `while (prop->desc_set[cnt_desc].desc_uuid != XF_SLE_ATTR_SET_END_FLAG)`
      never increments `cnt_desc` (only `++cnt_prop` at `:193` is inside the
      property loop). Any server service declaring at least one descriptor
      (e.g. a CCCD) re-adds `desc_set[0]` forever.
   b. **Missing return in a non-void function**:
      `port_xf_sle_connection_manager.c:232-251` —
      `xf_sle_set_max_pwr_level_by_pwr` ends after
      `uapi_nv_write(NV_ID_BTC_TXPOWER_CFG, ...)` + `XF_CHECK` with no return
      statement; the success path returns garbage. The function writes the
      TX-power **level index** (table `{-6,-2,2,6,10,14,16,20}` dBm,
      `:229-230`) into NV via `btc_power_type_t.btc_max_txpower` — see
      dedup note on NEW-HISI-NVS-PERSISTENCE.md.
   c. **Copy-paste field swap in the PHY setter**:
      `port_xf_sle_connection_manager.c:169` —
      `.tx_pilot_density = sle_phy->rx_pilot_density` (the xf-level tx field
      is fed from the rx input; there is no `tx_pilot_density` input at all).
   d. **Copy-paste error message in OTA write**:
      `port_xf_ota_client.c:231-234` — a `uapi_upg_write_package_sync`
      failure logs "uapi_upg_prepare error". Additionally the OTA `write_to`
      path returns `XF_ERR_NOT_SUPPORTED` (`:241-249`) and `xf_ota_abort` too
      (`:187-193`).
9. **Flow control before every SSAP data operation is a busy-wait that kicks
   the watchdog, and its default backend reaches into the closed GLE host.**
   `port_sle_transmition_manager.h:19-22` defines three modes
   (`NONE` / `BY_CALLBACK` / `BY_EXTERN_API`) and hard-selects
   `PORT_SLE_QOS_FLOWCTRL_MODE = BY_EXTERN_API`. In that mode
   `port_sle_flow_ctrl_state_get(conn_id)` ignores `conn_id` and returns
   `gle_tx_acb_data_num_get()` — an **extern declaration of a closed GLE-host
   internal** (`port_sle_transmition_manager.c:31,57-59`), i.e. the available
   ACB TX-buffer count (nonzero = send-OK, per vendor sample usage) from inside `libbth_gle.a` (the same closed archive
   NEW-GLE-HOST-SYMBOL-SURFACE.md mapped). All three data-plane entry points
   spin `while (port_sle_flow_ctrl_state_get(conn_id) == false)
   uapi_watchdog_kick();` — server response `:242-245`, server
   notify/indicate `:294-297`, client write-req `:285-288`, client write-cmd
   `:309-312`. The count-vs-boolean doc mismatch is benign only because a
   zero count (no available buffer; header documents 0 = busy / 1 = available,
   `port_sle_transmition_manager.h:29-36`) matches the spin's continue
   condition, and vendor samples treat nonzero as OK-to-send; the wait is
   unbounded (watchdog kept alive but the calling task stalled), global
   across links in the default mode, and only bounded (8-link QOS state
   table, `PORT_SLE_LINK_MAX`) in the non-default callback mode
   (`:20-22,36-38,67-73`).
10. **Error mapping is pass-through casting, and two API families are
    honestly stubbed.** Every wrapper casts the vendor `errcode_t` result
    1:1 into `xf_err_t` (e.g. `ssaps_register_server`,
    `port_xf_sle_ssap_server.c:105-107`) — no numeric translation, valid only
    because `ERRCODE_SUCC == 0 == XF_OK`. The one intentional offset is the
    application response status: `status = err_code + ERRCODE_SLE_SSAP_BASE`
    (`:237`, the 0x80006100 SSAP error base already documented in
    intel/WS63-SSAP-API.md). Stubs: `xf_sle_ssaps_stop_service` returns
    `XF_ERR_NOT_SUPPORTED` (`:212-218`); `xf_sle_ssapc_discovery_property`
    carries a TODO and returns `XF_ERR_NOT_SUPPORTED` at `:229-238` — exactly
    the line range the earlier intel note cited, now re-verified byte-exact at
    this revision.

## Substantive contracts

### Build/selection contract (fork side)

| Concern | Contract | Evidence |
|---|---|---|
| Port selection | Kconfig choice, default-on | `application/Kconfig:19-41` |
| Component selection | `CONFIG_WS63_PORTING_XF_V0_2_X` → port, else null | `application/CMakeLists.txt:28-32` |
| XFusion injection | env `XF_PROJECT_PATH` + `build_environ.cmake` exports | `ws63_porting_xf_0p2/CMakeLists.txt:6-25` |
| Platform defs | `platform_def/` + `hw_rsrc_def/` added to header path | `platform_def/platform_def.cmake:6-9` |
| App component | from source (`XF_SOURCES`/`XF_HEADERS`) or `libxf_app.a`; `WHOLE_LINK true`; `install_sdk "*"` | `xf_app/CMakeLists.txt:16-64` |
| Warning policy | `-Wno-error` on every port component | `port_xf_sle/CMakeLists.txt:41`, `xf_app:42`, `tasks_xf_entry:37` |

### SLE port contracts (the deep-read core)

- **UUID bridging** (`port_sle_ssap.h:29-50`): statement macros map
  `xf_sle_uuid_info_t {type, uuid128}` to vendor `sle_uuid_t {len, uuid[16]}`
  and back — `PORT_SLE_DEF_WS63_UUID_FROM_XF_UUID` (defines + memcpy),
  `PORT_SLE_SET_WS63_UUID_FROM_XF_UUID`, and the reverse. The xf `type` field
  is the vendor `len` (2 or 16).
- **Announce data packing with documented SDK workarounds**
  (`port_xf_sle_device_discovery.c:95-151`): TLV struct sets are packed by
  `xf_sle_adv_data_packed_size_get` / `..._packed_by_adv_struct_set` into
  stack VLAs sized from the computed packed length (`:108,:129`; a `len==0 →
  1` idiom avoids zero-length arrays). Two FIXMEs record vendor-SDK defects:
  announce data or scan-response NULL/zero-length makes
  `sle_set_announce_data`/start fail with `0x8000600C`, worked around by
  sending a 1-byte payload (`:117-123,:138-144`); and a third FIXME records
  that scan-response data appears ineffective (`:146`). Announce param mapping
  is field-for-field with `announce_type → announce_mode` (`:154-186`); seek
  params map per-PHY type/interval/window over `XF_SLE_SEEK_PHY_NUM_MAX`
  entries (`:207-226`).
- **Pair/bond enumeration with heap copy-out**
  (`port_xf_sle_connection_manager.c:120-159`): `xf_malloc` a
  `sle_addr_t[*max_num]` array, call `sle_get_paired_devices` /
  `sle_get_bonded_devices`, copy back in reverse, free. Note the `XF_CHECK`
  early-exit paths do not free the buffer (leak on vendor-error paths).
- **Server service tree** (`port_xf_sle_ssap_server.c:120-199`):
  `ssaps_add_service_sync` (primary iff
  `service_type == XF_SLE_SSAP_SERVICE_TYPE_PRIMARY`), then sentinel-terminated
  property loop (`XF_SLE_ATTR_SET_END_FLAG`) with
  `ssaps_add_property_sync` writing back `prop_handle`, then the descriptor
  loop (defect 8a). Event forwarding: CONNECT carries `conn_id` + peer addr +
  `pair_state` (no reason); DISCONNECT carries `disc_reason`; the disconnect
  branch memcpys the address through the `.connect` member of the union-style
  param (`:360,:372`).
- **Client data plane** (`port_xf_sle_ssap_client.c:241-333`): read by
  handle → `ssapc_read_req(app_id, conn_id, handle, type)`; read by UUID →
  `ssapc_read_req_by_uuid`; write req/cmd with the flow-control gate;
  MTU exchange → `ssapc_exchange_info_req({mtu_size, version})`. Notify /
  indication callbacks forward `handle, type, data, data_len` to the single
  event callback (`:538-585`).
- **OTA client** (`port_xf_ota_client.c`): single static context
  `{b_inited, update_partition_size, update_package_size, write_len}`
  (`:32-49`); running partition is fixed `XF_OTA_PARTITION_ID_OTA_0`, update
  partition fixed `XF_OTA_PARTITION_ID_PACKAGE_STORAGE` with "dual OTA
  partition not supported yet" TODOs (`:75-94,:203-216`); platform app
  descriptor/digest sizes are 0 and their fetch paths `XF_ERR_NOT_SUPPORTED`
  (`:124-150`). Write path bounds-checks against `update_partition_size`
  (`:225-230`) then `uapi_upg_write_package_sync(ctx()->write_len, src, size)`
  with sequential-only semantics (`sequential_write` ignored, `:159`).

## Runtime limits

- One XFusion task for the whole framework loop (`tasks_xf_entry.c:34-43`);
  stack/priority from Kconfig defaults `CONFIG_TASKS_XF_PREMAIN_*`.
- `xf_sle_enable()` unconditionally sleeps **800 ms** before `enable_sle()`
  (`port_xf_sle_device_discovery.c:33-40`) — a hard-coded boot-order
  workaround, paid on every enable.
- Service discovery: 20 ms poll × 500 = **10 s** worst case, plus the
  zero-result discovery always times out (finding 7b).
- Data-plane busy-waits are unbounded (finding 9); in the default
  `BY_EXTERN_API` mode they drain the **global** ACB TX queue, not the
  addressed link's.
- OTA: single-partition, single static context, no abort, no random-access
  write; `update_partition_size` from `uapi_upg_get_storage_size()` caps the
  package.
- BY_CALLBACK flow-control mode (not default) tracks at most
  `PORT_SLE_LINK_MAX = 8` links.

## Dedup / novelty

- Not covered by any harvest report: `knowledge/harvest/index.md` has no
  porting_xf entry; corpus-wide search for `ws63_porting_xf` / `port_xf_sle` /
  `port_xf_ble` matches only three intel documents that use the port as a
  cross-validation witness, never as a subject:
  - `knowledge/intel/WS63-SSAP-API.md:29` lists the SSAP port pair as "2nd
    official wrapper" and cites `port_xf_sle_ssap_client.c:229-238` — verified
    byte-exact here at the pinned commit; this report adds the implementation
    contracts and the defect census the intel note never attempted.
  - `knowledge/intel/WS63-CONN-DISCOVERY.md:32` names
    `port_xf_sle_connection_manager.c` once; its analysis is header/demo-level.
  - `knowledge/intel/WS63-VS-WS73.md:145,170` cites the port as an "API-surface
    re-export pattern" shape reference only.
- Complements, does not reslice, the closed topics: NEW-XFUSION.md reconstructed
  the xf_sle API from four examples and explicitly recorded the port glue as
  unfetched ("the actual glue lives in a separate repo ... not fetched") — this
  report reads the *other* fork's in-tree realization of that same glue and
  confirms the env-var/build_environ mechanism it inferred. NEW-GLE-HOST-SYMBOL-SURFACE.md
  mapped the closed host archive from symbols; here the port makes a
  closed-internal (`gle_tx_acb_data_num_get`) load-bearing from open code.
  NEW-HISI-NVS-PERSISTENCE.md covered the NV store itself; the port is a
  consumer (BTC TX-power NV write), not a re-slice. None of the other closed
  topics (hisi-nvs/alloc/crypto/fwpkg, hisiflash, hisi-registers, hisi-rom-sys,
  hisi-rtos scheduler, hisi-rf, hispark-rs, DSoftBus, Ai-BS21 OSAL/NV, RSSI,
  toolbox docs, NLChat web) touch this tree.

## Reusable for our stack

- A second, complete, open SLE host-consumer reference (next to the OHOS SSAP
  engine and the vendor samples) — useful for cross-checking our device-side
  SSAP semantics: callback fan-in per role, MTU-exchange ordering, notify
  gating.
- The defects are a checklist of what to avoid in our own adaptation layers:
  sentinel loops without increment, non-void functions falling off the end,
  union-style event params addressed through the wrong member, and
  `-Wno-error` hiding all of it.
- The `XF_PROJECT_PATH` → `build_environ.cmake` injection is a working
  template for layering an external component tree onto a vendor SDK without
  touching vendor files (the null-port fallback pattern included).
- The flow-control contract documents where the vendor SLE host actually
  applies backpressure (ACB TX depth), observable from open code even though
  the counter itself lives in the closed archive.

## Comparison anchors (existing reports/intel)

- `NEW-XFUSION.md` — the xf_sle API surface and the unfetched-port caveat this
  report resolves for the fbb_ws63 fork.
- `NEW-GLE-HOST-SYMBOL-SURFACE.md` — the closed host archive whose internal
  ACB counter the port externs.
- `NEW-HISI-NVS-PERSISTENCE.md` — the NV layer the port's TX-power write rides.
- `NEW-SSAP-SERVM-MODULE-MAP.md` / `NEW-SSAPC-CLIENT-OBJECT-MODEL.md` — the
  OHOS SSAP engine; this port is the embedded-vendor counterpart.
- `knowledge/intel/WS63-SSAP-API.md`, `knowledge/intel/WS63-CONN-DISCOVERY.md`
  — header-dialect cross-validations that cite the port files as witnesses.

## Boundaries and gaps

- Read fully: `port_xf_sle/` (8 files, ~1.8k lines), `tasks_xf_entry/` (4
  files, ~465 lines), the selection/bridge CMake+Kconfig glue. Inventoried
  only (no line-level claims): `port_xf_ble/` (3 files, ~1.8k lines — gap/gatt
  client/server entirely unanalyzed), `port_xf_wifi/` (~1.5k lines), the
  peripheral ports, `port_utils/`, and `platform_def/` headers. The Kconfig
  and CMake drift noted below means the BLE/WiFi Kconfig gating could not be
  confirmed from the vendor-side Kconfig alone.
- Build drift: `port_xf/Kconfig:6-21` or-sources Kconfigs for
  `port_xf_event`/`port_xf_systime`/`port_xf_timer` which do not exist in the
  tree, and `port_xf/CMakeLists.txt:27-37` issues four `build_component()`
  calls for `port_xf_event`/`port_xf_systime`/`port_xf_test`/`port_xf_timer`
  with no sources — phantom components; the buildable set is the
  `add_subdirectory_if_exist` list (`:12-25`).
- History is one commit deep locally (shallow clone); older evolution of the
  port (the `port_xf_for_nearlink` standalone repo lineage) was not traced.
- No build, no hardware, no network; GitHub pin-URLs constructed, not fetched.

## Pinned sources (commit `f0fbaef71197650e7a25695bb814e8fadca511ba`)

Base: `https://github.com/x-eks-fusion/fbb_ws63/blob/f0fbaef71197650e7a25695bb814e8fadca511ba/`

| Path | Blob SHA (git) |
|---|---|
| `src/application/ws63_porting_xf_0p2/port_xf/port_xf_sle/port_xf_sle_ssap_server.c` | `08bbda8a45bfb6f9d4cd8beb0cecf17820dd80e4` |
| `src/application/ws63_porting_xf_0p2/port_xf/port_xf_sle/port_xf_sle_ssap_client.c` | `eb2f6b903b7f2a68e5c249df5934b5c27db34065` |
| `src/application/ws63_porting_xf_0p2/port_xf/port_xf_sle/port_xf_sle_device_discovery.c` | `40f7e83fe695cc1fa389b51b60852847e26885ab` |
| `src/application/ws63_porting_xf_0p2/port_xf/port_xf_sle/port_xf_sle_connection_manager.c` | `f16556f78a301a310819d990c52624e8266a56c2` |
| `src/application/ws63_porting_xf_0p2/port_xf/port_xf_sle/port_sle_transmition_manager.c` | `11c8cf2b766beee90afef8610ff9354e69086d5a` |
| `src/application/ws63_porting_xf_0p2/port_xf/port_xf_sle/port_sle_transmition_manager.h` | `2b1c77a8d12f915b56028e990d761ed79d0f1054` |
| `src/application/ws63_porting_xf_0p2/port_xf/port_xf_sle/port_sle_ssap.h` | `6313aac0c4fdf7e268add5a48e9193a50ad20072` |
| `src/application/ws63_porting_xf_0p2/tasks_xf_entry/tasks_xf_entry.c` | `857805de6f673fd60a0d8acd9fe5a9327983d0a2` |
| `src/application/ws63_porting_xf_0p2/tasks_xf_entry/port_xf_sys.c` | `cb80f17d22f2ad3ee1b760551651c4c1317960e1` |
| `src/application/ws63_porting_xf_0p2/tasks_xf_entry/port_xf_log.c` | `9faea8355455633230e40d636b7c96443daf3034` |
| `src/application/ws63_porting_xf_0p2/tasks_xf_entry/port_xf_ota_client.c` | `cc946acf48da24fa9502ff0f46ac15d60bd958bc` |
| `src/application/ws63_porting_xf_0p2/CMakeLists.txt` | `5f1cf9f738cd6ac6b85a72e34c65b9cd4980ce2c` |
| `src/application/ws63_porting_xf_0p2/xf_app/CMakeLists.txt` | `acc44ef20e9787f35a5521c60f74adb194482c7e` |
| `src/application/ws63_porting_xf_0p2/platform_def/platform_def.cmake` | `8af94767ad994b804ffd26a3129b4b9de4056c99` |
| `src/application/ws63_porting_xf_0p2/port_xf/CMakeLists.txt` | `ea0db2a2af2e79a193673b05e05313ff8f782167` |
| `src/application/ws63_porting_xf_0p2/port_xf/Kconfig` | `7bf05940f57f2299de8dca1cb825ec0d91b221b9` |
| `src/application/ws63_porting_xf_0p2/port_xf/port_xf_sle/CMakeLists.txt` | `76e93b961680b9ac5ea4d7c135e529cf793a9fc4` |
| `src/application/ws63_porting_xf_0p2/tasks_xf_entry/CMakeLists.txt` | `e699eae39ab698d0d0f50ddd0a4fabc85b6640e0` |
| `src/application/CMakeLists.txt` | `9282435fcab6a6168ad5537eff677b4c975243d5` |
| `src/application/Kconfig` | `e6ddd4e9db0a6639568aac064a31c78cace745ba` |

## Ten-line summary

1. `x-eks-fusion/fbb_ws63` (pinned `f0fbaef71197650e7a25695bb814e8fadca511ba`,
   2025-03-13) carries an in-tree XFusion port layer
   `src/application/ws63_porting_xf_0p2/` (~11.0k lines) absent from the
   official HiSpark upstream.
2. Selection is default-on Kconfig (`application/Kconfig:19-41`), CMake switch
   to a null fallback component (`application/CMakeLists.txt:28-32`).
3. The bridge fatals without env `XF_PROJECT_PATH` and imports the XFusion
   build via `build_environ.cmake` exports
   (`ws63_porting_xf_0p2/CMakeLists.txt:6-29`) — no xf_* sources exist in the
   vendor tree.
4. XFusion runs as one LiteOS task (`TasksXFPremain`) executing
   `xfusion_init(); while(1) xfusion_run();` (`tasks_xf_entry.c:34-57`).
5. The SLE port funnels all events through one callback per role and ignores
   the event mask; server and client overwrite each other's connection-callback
   registration (`ssap_server.c:78-83` vs `ssap_client.c:116-120`).
6. Client discovery is sync-over-async, 20 ms × 500 (10 s cap), with a shared
   result queue that cross-completes overlapping tasks and a no-op completion
   callback (`ssap_client.c:170-227,424-451`).
7. Every SSAP data op busy-waits on flow control with watchdog kicks; the
   default backend externs the closed GLE-host counter
   `gle_tx_acb_data_num_get` (`port_sle_transmition_manager.c:31,57-59`).
8. Byte-verified defects: descriptor loop never increments its index
   (`ssap_server.c:171-193`), missing return after the NV TX-power write
   (`connection_manager.c:232-251`), rx-pilot-density assigned into the tx
   field (`:169`), wrong OTA error message (`port_xf_ota_client.c:233`);
   `-Wno-error` everywhere explains their survival.
9. Error mapping is pass-through casting with one deliberate
   `+ ERRCODE_SLE_SSAP_BASE` offset; stop-service and property-discovery are
   `XF_ERR_NOT_SUPPORTED` stubs.
10. Novelty: no harvest report covers this tree; three intel docs cite the
    port only as a cross-validation witness. The natural follow-up slice is
    the uncited BLE port (`port_xf_ble/`, gap/gatt client/server) plus the
    WiFi port's netif glue.
