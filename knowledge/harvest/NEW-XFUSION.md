---
type: harvest
title: "NEW-XFUSION — What x-eks-fusion (XFusion) teaches us"
language: zh
created: 2026-09-05
tags: [harvest, xfusion, what, fusion]
sources:
  - "/mnt/hdd/nearlink-stuff/xfusion"
trust: B
stale_after: 2027-03-05
---

# NEW-XFUSION — What x-eks-fusion (XFusion) teaches us

- Local snapshot: `/mnt/hdd/nearlink-stuff/xfusion/` (7.1 MB incl. `.git`), 28★ x-eks-fusion/xfusion.
- Snapshot commit: `517af4a 2025-05-23 "refactor: 更新 SLE 例程 (#62)"` (latest commit on the checkout).
- IMPORTANT caveat discovered up front: **all git submodules are uninitialized** (`.gitmodules` lists
  `components/xf_sle/xf_sle` and `ports/nearlink/port_xf_for_nearlink` among 16 submodules; `git submodule status`
  shows every one prefixed `-` = not checked out). So the *implementation* of the xf_sle abstraction and the
  nearlink port layer are NOT on disk. What we have: the framework skeleton, build glue, board configs, and —
  most valuably — four complete SLE example applications that exercise the full xf_sle API surface.
- XFusion self-description: "轻量级、跨平台、组件式的嵌入式开发框架" — a lightweight, cross-platform,
  componentized embedded dev framework ("write once, deploy on many MCUs") (`README.md:36`).
- Supported platforms (README.md:93-96): esp32 (esp-idf v5.0.7), **ws63 (HI3863 chip)**, **bs21 (HI2821 chip)**,
  linux simulator. Note these are *hostless SoCs running their own RTOS*, not Linux-host dongles.

## 1. Repository architecture (the layering lesson)

Top-level dirs (`ls /mnt/hdd/nearlink-stuff/xfusion/`): `boards/ components/ docs/ examples/ plugins/ ports/
sdks/ tools/`, plus `XFKconfig`, `export.sh`, `CHANGELOG.md`, `DETAILS.md`.

Layer split (each role is decoupled — this is the core design idea, spelled out in `DETAILS.md:5-9`):

- **components/** — portable middleware: xf_osal, xf_hal, xf_task, xf_log, xf_heap, xf_utils, xf_sys, xf_init,
  xf_fal, xf_vfs, xf_wal (wireless abstraction: `xf_wifi` + `xf_sle`), xf_net_apps, xf_ota, xf_nal, xf_lp, xf_ble.
  Most are git submodules (e.g. xf_sle points to `https://github.com/x-eks-fusion/xf_sle.git`,
  `.gitmodules` entry for `components/xf_sle/xf_sle`).
- **boards/nearlink/{ws63,bs21}/** — tiny per-target definitions: `target.json` (SDK pin) + `XFKconfig`
  (Kconfig menu) + `xfconfig.defaults`. Nothing else.
- **ports/nearlink/{ws63,bs21}/** — build glue only: `xf_collect.py` + `XFKconfig` + `README.md`.
  `ports/nearlink/ws63/README.md:1` says explicitly: "为了与 XFusion 减少耦合， nearlink 系列芯片对接 XFusion
  的代码不在此处. 见 `ports/nearlink/port_xf_for_nearlink`" — the actual glue lives in a separate repo
  (`port_xf_for_nearlink`, submodule, not fetched).
- **plugins/{ws63,bs21,...}/build.py** — python build plugin that drives the *vendor SDK's own build system*.
- **sdks/** — vendor SDK landing zone; `sdks/README.md` says the dir is gitignored except the README itself
  ("此文件夹用于保存各厂商的 SDK. 此文件夹默认屏蔽除了本文件外的所有文件").

Lesson for our repo: the vendor-SDK pin lives in a 9-line JSON, not in a fork; the adapter lives in its own repo;
examples compile against the *abstraction headers only*. Our `sdk/ws73_sdk_linux_WS73_1.10.110/` whitelist-gitignore
approach matches this; the "adapter in a separate submodule" idea is a cleaner split than what we have.

## 2. The xf_sle component as shipped in this tree

`components/xf_sle/` contains exactly three files:

- `components/xf_sle/XFKconfig:1-3` — a single Kconfig toggle: `config XF_SLE_ENABLE  bool "Enable SLE Module"
  default "n"`.
- `components/xf_sle/xf_sle_config.h:23-35` — maps `CONFIG_XF_SLE_ENABLE` to `XF_SLE_ENABLE (1)` and declares
  `XF_SLE_ADV_ID_INVALID` / `XF_SLE_CONN_ID_INVALID` passthroughs from board pre-config.
- `components/xf_sle/xf_sle/` — empty (unfetched submodule holding the real headers: the examples include
  `xf_sle_connection_manager.h`, `xf_sle_device_discovery.h`, `xf_sle_ssap_server.h`, `xf_sle_ssap_client.h`
  — see `examples/wireless/sle/sample_ssap_server/main/xf_main.c:9-11`).

CHANGELOG.md:172-176 documents xf_sle as "星闪接口抽象" (NearLink interface abstraction) and notes a breaking
change: "修改错误命名。调整参数顺序，不向前兼容" (renamed errors, reordered params, not forward compatible) —
i.e. the API is still churning; treat it as a moving reference, not a stable spec.

So the API surface below is **reconstructed from example call sites**, which is actually the most honest view:
it's the subset a real app needs.

## 3. Reconstructed xf_sle API surface (from all 4 SLE examples)

Distinct `xf_sle_*` functions called in `examples/wireless/sle/` (grep over all four examples, 28 total):

Common / control plane:
- `xf_sle_enable()` — first call in every app (`sample_ssap_server/main/xf_main.c:126`, client `:76`).
- `xf_sle_set_local_name(name, len)` (server `xf_main.c:140`).
- Power control: `xf_sle_set_max_pwr_level_by_pwr(expected_pwr)` and
  `xf_sle_set_max_pwr(tx_pwr, rx_pwr)` (speed server `xf_main.c:278-280`).

Advertising ("announce" — direct loanword of the SLE spec term):
- `xf_sle_set_announce_param(adv_id, xf_sle_announce_param_t*)` (server `xf_main.c:325`).
- `xf_sle_set_announce_data(adv_id, xf_sle_announce_data_t*)` (server `xf_main.c:300`).
- `xf_sle_start_announce(adv_id)` / implicit stop via disconnect handler (server `xf_main.c:167,201`).

The `xf_sle_announce_param_t` fields (server `xf_main.c:307-322`): `announce_type`
(`XF_SLE_ANNOUNCE_TYPE_CONNECTABLE_SCANABLE`), `announce_handle`, `announce_gt_role`
(`XF_SLE_ANNOUNCE_ROLE_T_CAN_NEGO` — GT = grand transmit/central role negotiation), `announce_level`
(`XF_SLE_ANNOUNCE_LEVEL_NORMAL` — the SLE discovery-level byte), `announce_channel_map`
(`XF_SLE_ADV_CHANNEL_MAP_DEFAULT` / `..._MAP_77` in speed server `xf_main.c:533`), `announce_interval_min/max`,
plus *embedded default connection parameters*: `conn_interval_min/max` (units of 125 us),
`conn_max_latency`, `conn_supervision_timeout` (units of 10 ms), and `announce_tx_power`.

The `xf_sle_adv_struct_t` TLV model (server `xf_main.c:277-298`) mirrors the SLE announce-struct list:
array of `{ad_type, ad_data union, ad_data_len, is_ptr}` terminated by `{0}`, with types like
`XF_SLE_ADV_STRUCT_TYPE_DISCOVERY_LEVEL` and `..._COMPLETE_LOCAL_NAME`, plus a parallel `seek_rsp_struct_set`
(scan-response) list. Note the hand-rolled `is_ptr` flag choosing between inline value and pointer — a deliberate
no-malloc pattern for MCUs.

Scanning ("seek"):
- `xf_sle_set_seek_param(&xf_sle_seek_param_t)` then `xf_sle_start_seek()` / `xf_sle_stop_seek()`
  (client `xf_main.c:163,92,236`).
- `xf_sle_seek_param_t` (client `xf_main.c:150-162`): `filter_duplicates`, `seek_filter_policy`
  (`XF_SLE_SEEK_FILTER_ALLOW_ALL`), `seek_phy` (`XF_SLE_SEEK_PHY_1M`), `own_addr_type`
  (`XF_SLE_ADDRESS_TYPE_PUBLIC`), and a `phy_param_set[]` array (per-PHY seek interval/window/type —
  `XF_SLE_SEEK_TYPE_PASSIVE`). The per-PHY array is an SLE-specific evolution of the BLE scan API.
- Scan results arrive as `XF_SLE_COMMON_EVT_SEEK_RESULT` with `xf_sle_common_evt_param_seek_result_t`:
  `evt_type, rssi, peer_addr, data_status, data, data_len` (client `xf_main.c:204-216`).

Connection manager:
- `xf_sle_connect(&result->peer_addr)` — connect by address after filtering on the announce payload
  (client `xf_main.c:238-239`; by MAC in speed client `xf_main.c:418`).
- `xf_sle_set_default_conn_params(&xf_sle_conn_param_def_t)` — central-side default link params incl. scan
  interval/window/phy used during announce handling (speed server `xf_main.c:332-346`).
- `xf_sle_update_conn_params(&xf_sle_conn_param_update_t)` — post-connect update: `conn_id, interval_min/max,
  max_latency, supervision_timeout` (speed server `xf_main.c:462-472`).
- Link tuning: `xf_sle_set_data_len(conn_id, bytes)`, `xf_sle_set_phy_params(conn_id, &xf_sle_set_phy_t)`
  (tx/rx `format` [radio frame format 1/2], `tx_phy`/`rx_phy` [1M/4M], `tx/rx_pilot_density`
  [`_NO`/`_16_TO_1`], `g_feedback`, `t_feedback`), `xf_sle_set_mcs(conn_id, mcs)` — all post-connect
  (speed server `xf_main.c:367-390`).

SSAP server (ssaps):
- `xf_sle_ssaps_event_cb_register(cb, XF_SLE_EVT_ALL)` — ONE global event callback with an event-mask arg
  (server `xf_main.c:137`).
- `xf_sle_ssaps_app_register(&uuid, &app_id)` — app/profile registration, stack-allocated id
  (server `xf_main.c:145`).
- `xf_sle_ssaps_add_service_to_app(app_id, &xf_sle_ssaps_service_t)` — **declarative** service description:
  service_uuid (`XF_SLE_DECLARE_UUID16(0x2222)`), service_type (`_PRIMARY`), nested compound-literal arrays of
  `xf_sle_ssaps_property_t` (prop_uuid, permissions `XF_SLE_SSAP_PERMISSION_READ|WRITE`, operate_indication bits
  READ/NOTIFY/INDICATE/WRITE/WRITE_NO_RSP, value ptr+len, nested `desc_set[]` of `xf_sle_ssaps_desc_t` with
  `XF_SLE_SSAP_DESC_TYPE_CLIENT_CONFIGURATION`) — server `xf_main.c:73-115`. The stack writes back
  `service_handle` and per-prop `prop_handle` into the struct, which the app then reads
  (server `xf_main.c:150-158, 153-155`).
- `xf_sle_ssaps_start_service(app_id, service_handle)` (server `xf_main.c:158`).
- `xf_sle_ssaps_set_info(app_id, &xf_sle_ssap_exchange_info_t{mtu_size, version})` — MTU negotiation, noted
  "注意需要在连接后" (must happen after connect) (speed server `xf_main.c:317-324`).
- `xf_sle_ssaps_send_notify_indicate(app_id, conn_id, &xf_sle_ssaps_ntf_ind_t{handle, type,
  XF_SLE_SSAP_PROPERTY_TYPE_VALUE, value, value_len})` (server `xf_main.c:179-189`).
- `xf_sle_ssaps_send_response(app_id, conn_id, trans_id, XF_OK, &xf_sle_ssaps_response_value_t)` — answers
  READ/WRITE_REQ that had `need_rsp` (server `xf_main.c:258-265`).
- ssaps events: `XF_SLE_SSAPS_EVT_WRITE_REQ` (param carries `need_rsp, handle, conn_id, trans_id, value,
  value_len`), `XF_SLE_SSAPS_EVT_READ_REQ`, plus common events CONNECT/DISCONNECT/CONN_PARAMS_UPDATE
  (server `xf_main.c:196-271`, speed server `xf_main.c:440-488`).

SSAP client (ssapc):
- `xf_sle_ssapc_event_cb_register(cb, XF_SLE_EVT_ALL)`, `xf_sle_ssapc_app_register(&uuid, &app_id)`
  (client `xf_main.c:80-83`).
- `xf_sle_ssapc_discover_service(app_id, conn_id, &xf_sle_ssapc_find_struct_param_t{type=
  XF_SLE_SSAP_FIND_TYPE_PRIMARY_SERVICE, start_hdl, end_hdl})` — discovery is *synchronous-looking* (returns
  result in the same struct), driven from a 500 ms `xf_task` loop guarded by `is_need_discovery` flag set in
  the CONNECT event (client `xf_main.c:103-119, 173-178`). Second find type: `XF_SLE_SSAP_FIND_TYPE_PROPERTY`.
- `xf_sle_ssapc_request_write_cmd(app_id, conn_id, handle, XF_SLE_SSAP_PROPERTY_TYPE_VALUE, data, len)` —
  write-without-response; a `request_write` (with rsp) variant is implied by the `WRITE_CFM` event
  (client `xf_main.c:127-129`).
- `xf_sle_ssapc_request_read_by_handle(app_id, conn_id, XF_SLE_SSAP_DESCRIPTOR_CLIENT_CONFIGURATION, handle)`
  (client `xf_main.c:138-139`).
- ssapc events: `XF_SLE_SSAPC_EVT_WRITE_CFM`, `XF_SLE_SSAPC_EVT_READ_CFM` (data/data_len),
  `XF_SLE_SSAPC_EVT_NOTIFICATION` (conn_id, handle, data, data_len) (client `xf_main.c:182-196`).

Shared conveniences: `XF_SLE_ADDR_LEN` (6), `XF_SLE_ADDR_PRINT_FMT` / `XF_SLE_ADDR_EXPAND_TO_ARG` printf
helpers (client `xf_main.c:176-177`), `XF_SLE_ADV_STRUCT_TYPE_FILED_SIZE` + `XF_SLE_ADV_STRUCT_LEN_FILED_SIZE`
(sic — "FILED" typo) for manual TLV walking (client `xf_main.c:246`).

### SSAP-stack comparison (vs our WS63 SSAP notes and OSPL SSAP plan)

- The naming is a thin veneer over the vendor API, not a redesign: announce/seek/conn intervals in the same
  125 us / 10 ms units, same app_id/conn_id/transport-handle triple, same `trans_id` correlation for responses.
  Compare `WS63-SSAP-API.md` in our lab-notes: function names differ (e.g. vendor `ssaps_add_service` →
  `xf_sle_ssaps_add_service_to_app`) but the *shape* is identical. XFusion did NOT hide the SLE dialect
  (announce vs adv, seek vs scan, GT role, discovery level) — it standardized spelling.
- Biggest genuine abstractions: (a) the declarative nested service/property/descriptor struct with
  stack-written handles; (b) a single event callback union (`xf_sle_ssaps_evt_cb_param_t` /
  `xf_sle_ssapc_evt_cb_param_t`) selected by event enum, with `XF_SLE_EVT_ALL` mask registration; (c) the
  TLV announce-struct builder with `is_ptr`; (d) invalid-id constants (`XF_SLE_ADV_ID_INVALID` = 255) exposed
  as board pre-config so ports can differ (boards/nearlink/*/XFKconfig "pre-config" menu, `visible if
  XFKCONFIG_DEBUG_MODE`).
- Our OSPL UAPI design (`OSPL-UAPI-CONTRACT.md`, `SSAP-IMPLEMENTATION-PLAN.md`) goes further (async
  completion tokens, kernel-safe buffers). XFusion is a userspace MCU app layer with no memory-safety
  constraints — useful as a *naming and ergonomics* reference (e.g. `announce`/`seek` vocabulary, the
  compound-literal service table), not as a concurrency model.

## 4. examples/wireless/sle — the four patterns

Directory: `sample_ssap_server/`, `sample_ssap_client/`, `ssap_speed_test_server/`, `ssap_speed_test_client/`
(each with `main/xf_main.c`, `xfconfig.defaults`, `xf_project.py`; xfconfig.defaults is a single line:
`CONFIG_XF_SLE_ENABLE=y` — the whole radio enable story is one Kconfig symbol).

**Server pattern** (`sample_ssap_server/main/xf_main.c`): enable → register cb → set name → app_register →
add_service → start_service → set adv param+data → start_announce; on CONNECT enable a GPIO IRQ; on GPIO IRQ
send a notify (HID-style button report, `xf_main.c:175-190`); on WRITE_REQ immediately notify back; on READ_REQ
send_response; on DISCONNECT restart announce (`xf_main.c:197-206`). Defaults worth stealing:
conn interval 0x64 = 12.5 ms (units 125 us), adv interval 0xC8 = 25 ms, supervision timeout 0x1F4 = 5000 ms
(units 10 ms), adv tx power 10 dBm (`xf_main.c:21-34`). Static local MAC `{0x33,0x22,0x22,0x66,0x11,0x22}`
(`xf_main.c:70`).

**Client pattern** (`sample_ssap_client/main/xf_main.c`): passive seek, 100/100 interval/window; parse announce
TLVs manually, filter on `COMPLETE_LOCAL_NAME == "XF_SSAPS"`, stop seek, connect by address
(`xf_main.c:218-247`); on CONNECT set `is_need_discovery`; a 500 ms `xf_ttask_create_loop` task performs
service discovery then write-cmd then read-by-handle as a small state machine of bool flags
(`xf_main.c:103-146`). The README (`sample_ssap_client/README.md:3-11`) documents the flow and expected log.

**Speed-test pair**: the interesting one for throughput work (compare `PERF-WIFI-THROUGHPUT.md`,
`SLE-MEASURE-QOS.md` in our notes).
- Five compile-time profiles (speed server `xf_main.c:28-36`): WS63_MAX_SPEED, WS63_MAX_DISTANCE,
  WS63_MAX_DISTANCE_MAX_SENSITIVITY, BS2X_MAX_SPEED, BS2X_MAX_DISTANCE (+WS63E variant) — each is a full
  parameter tuple: conn interval (0x14 = 2.5 ms for ws63 max-speed; 0x0F for bs21 max-speed; 400*125us for
  bs21 max-distance), radio frame format 1/2, PHY 1M/4M, pilot density (none vs 16:1), MCS 0 vs 10, MTU
  (1500 / 512 / 23), payload len, TX power (20 dBm ws63, 6-8 dBm bs21) (`xf_main.c:54-186`).
  This table is empirical evidence of achievable per-profile configs on WS63 and BS21 — directly relevant
  to our dongle throughput targets.
- Server side does: max-power ramp (`set_max_pwr_level_by_pwr` + `set_max_pwr`), MTU exchange after connect,
  default conn params for incoming links, conn-param update on CONNECT event, then a one-shot delayed task
  (1 s after CONN_PARAMS_UPDATE) applies data_len/PHY/MCS ("preinit") and a loop task floods notify with
  PKT_DATA_LEN payloads and a 2-byte packet counter for loss detection (`xf_main.c:400-433`).
- Client side (`ssap_speed_test_client/main/xf_main.c`, 429 lines): filters by *MAC* instead of name
  (`xf_main.c:381`), aggregates RSSI over the run, measures wall-clock via `xf_sys` time in us, computes
  B/s and b/s with Ki/Mi divisors and prints fixed-point (no float printf) (`grep` hits at lines 343-358
  region), and counts received packets for loss rate.
- Watchdog is disabled for the test (`xf_sys_watchdog_disable()`, speed server `xf_main.c:275`) — a reminder
  that saturating notify loops can starve vendor SDK idle threads.

**Common idioms across all four**: `XF_CHECK(cond, ret, TAG, fmt, ...)` error macro; state as file-scope
bools (`is_connected`, `is_need_discovery`...) advanced by event callbacks with work done in a low-priority
loop task — i.e. events only set flags, never do long work. This is a clean pattern for our USB-transport
work queues too (`OSPL-USB-TRANSPORT.md`).

## 5. Board configs: ws63 / bs21 vs our HHD-01

- **ws63** (`boards/nearlink/ws63/target.json`): pins SDK `https://github.com/x-eks-fusion/fbb_ws63` commit
  `e3d7154`, branch `ws63_1.10.102/v2.0`, dir `fbb_ws63`. `XFKconfig` offers public fbb_ws63 vs internal
  1.10.101/1.10.102 SDK choices, default target `ws63-liteos-app`. `xfconfig.defaults` enables
  `CONFIG_XF_NET_APPS_ENABLE=y` and `CONFIG_XF_OSAL_CMSIS=y`.
- **bs21** (`boards/nearlink/bs21/target.json`): pins `https://github.com/Im-eks-dev/bs2x_sdk` commit
  `a14b06a`, branch `bs2x_1.10.15/v2.0`. `XFKconfig` default target `bs21e-sle-ble-slp-central-peripheral`
  (a single firmware target combining SLE + BLE + slep + central/peripheral), plus `standard-bs20-n1200`,
  `standard-bs21-n1100`, `standard-bs21e-1100e`. `xfconfig.defaults` disables net apps.
- Port glue: `ports/nearlink/ws63/XFKconfig:1-3` just `orsource`s the port submodule's Kconfig;
  `ports/nearlink/ws63/xf_collect.py:26-33` calls `get_target_build_lists()` from the submodule to gather
  srcs/incs/requires/cflags — i.e. the port injects XFusion *on top of the SDK tree* and the SDK's own
  build drives compilation.
- Build plugin (`plugins/ws63/build.py`): `build()` runs `python build.py -c -nhso -release <target>` inside
  the SDK dir and copies `output/*/fwpkg/<target>` into the project; `flash()` shells out to a `burn` command
  (auto-`pip install xf_burn_tools`) over a serial port; `menuconfig sub` delegates to the SDK's own
  menuconfig. Same template (`cmake_project.j2`, 2161 bytes, identical for ws63 and bs21).
- vs our HHD-01 (`HHD01-BOARD.md` in our notes): HHD-01 is a *USB dongle* whose WS73 runs controller firmware
  while our Linux kernel driver is the host — there is no "board target" in the XFusion sense for us. What
  transfers: (1) the `target.json` SDK-pin format (URL+commit+branch in 9 lines) is nicer than our loose
  sdk/ dir; (2) the "port in a submodule that contributes file lists + cflags" model maps well onto how our
  driver already wraps the WS73 SDK headers; (3) their conflation of SLE+BLE into one firmware target
  (bs21e-sle-ble-slp-central-peripheral) confirms chip vendors ship combined stacks — while our dongle's
  3733 firmware is SLE-focused, so BLE coexistence questions (`WS73-BT-GAP-AUDIT.md`) stay host-side.

## 6. Could XFusion serve as our TV-box app layer? — No; chip-side only

Evidence:

1. Every nearlink-enabled target is a bare-metal/LiteOS SoC: ws63 = HI3863, bs21 = HI2821 (README.md:94-95).
   The SLE examples run as `xf_main()` firmware inside the chip's LiteOS app target (`ws63-liteos-app`),
   flashed over serial with `burn` (plugins/ws63/build.py flash()).
2. The linux *simulator* port (`ports/simulator/sim_linux/`) implements only peripheral HAL shims:
   `port_adc.c, port_dac.c, port_gpio.c, port_i2c.c, port_log.c, ...` — there is **no SLE/xf_sle backend**
   for the simulator. SLE exists only where a nearlink radio + vendor firmware exists.
3. The xf_sle port contract is `port_xf_for_nearlink` (per-board, builds *into* the vendor SDK tree —
   `ports/nearlink/ws63/xf_collect.py:7-15`); there is no IPC/USB transport variant that would let an
   xf_sle app talk to a *remote* radio. Our architecture is the inverse: radio behind USB, host stack in
   the kernel driver (`OSPL-USB-TRANSPORT.md`, `NEARLINK-CONTROLLER.md`).
4. XFusion's value proposition (xf_hal/xf_task/xf_osal on 64 kB flash MCUs, README.md:87-92) targets the
   chip side of a SLE link — mouse/keyboard/sensor firmware — not a Linux TV-box userspace.

What we can legitimately take:

- **API vocabulary** for our OSPL UAPI: `announce`/`seek` (spec terms, better than gap-ish adv/scan),
  `xf_sle_ssaps_*`/`ssapc_*` split, `app_id` → `conn_id` → `handle` addressing, event-enum + union params,
  `ADDR_PRINT_FMT` debug helpers.
- **Defaults and profiles**: the speed-test parameter table (section 4) as starting points for our
  throughput characterization; server-side restart-adv-on-disconnect; MTU-after-connect ordering note.
- **Declarative GATT-like service tables** if our TV-box app ever needs an SSAP server role exposed to
  chip-side peripherals.
- If we ever build a *companion chip firmware* (e.g. a SLE remote or sensor around the TV box), XFusion on
  ws63/bs21 is a ready-made way to build it, and it would interoperate against our SSAP stack with the
  exact flows in section 4.

## 7. Misc observations

- `xf_collect.py` files are scattered through components/examples — they are the build-system hooks
  (`xf_build.collect(srcs, inc_dirs, requires, cflags)`, `ports/nearlink/ws63/xf_collect.py:29-35`).
- The ssap_server README (`examples/wireless/sle/sample_ssap_server/README.md:5-13`) is stale — it
  describes the BLE gatt server (logs say `sample_gatts`), copied before the SLE split; the client README is
  correct. Another sign the SLE side is young (last touched 2025-05-23).
- Docs are Chinese-first (`README.md`, examples comments); the doxygen setup (`docs/manual/mainpage.md`)
  is a stub. Their docs portal is external (coral-zone.cc).
- License: Apache-2.0 (`README.md` features list, ~line 82) — safe to lift patterns and even code snippets
  with attribution.

---

## 8-line summary

1. XFusion (x-eks-fusion, snapshot @ 517af4a 2025-05-23) is an MCU dev framework; SLE support targets ws63/HI3863 and bs21/HI2821 bare-metal targets only.
2. `components/xf_sle/` is a stub here (Kconfig + config header); the real abstraction lives in unfetched submodule `xf_sle` + port `port_xf_for_nearlink` — API reconstructed from 4 complete SLE examples.
3. xf_sle API = thin standardization of the vendor SLE dialect (announce/seek/GT-role/discovery-level kept as-is): 28 functions covering enable, announce TLVs, seek params, connect, conn-param/PHY/MCS/data-len tuning, and ssaps/ssapc app-register + declarative service tables + single masked event callback.
4. Examples teach: server (adv→service→notify on GPIO/write, restart adv on disconnect), client (name-filtered TLV parse→connect→discover→write→read state machine in a loop task), and a speed-test pair with 5 empirical ws63/bs21 profiles (MTU 23-1500, PHY 1M/4M, MCS 0/10, pilot density, TX pwr 6-20 dBm).
5. Board configs are 9-line SDK pins (fbb_ws63 @ ws63_1.10.102/v2.0; bs2x_sdk @ bs2x_1.10.15/v2.0) + Kconfig; build plugin drives the vendor SDK's own `build.py` and flashes fwpkg via `burn`.
6. NOT usable as our TV-box app layer: no USB/IPC radio transport, linux simulator has no SLE backend — it is chip-side (companion-device firmware) only.
7. Take for OSPL: spec-native vocabulary (announce/seek), app_id/conn_id/handle addressing, event-enum+union cb design, invalid-id constants as port pre-config, and the speed-profile table for our throughput tests.
8. Notes file: `/home/archivalera/plum/zcode-projects/nearlink/.scratch/nearlink-driver/lab-notes/NEW-XFUSION.md`
