# New repo knowledge harvest: Ai-BS21 SDK + wtsl_app

Date: 2026-09-03 (read-only harvest, no network/build/hardware)
Sources on disk:

- `/mnt/hdd/nearlink-stuff/Ai-BS21_SDK/` — 691 MB, Ai-Thinker official BS21 SDK
  (github.com/Ai-Thinker-Open/Ai-BS21_SDK, single squashed commit `f4f3f28 "add output file"`)
- `/mnt/hdd/nearlink-stuff/wtsl_app/` — 26 MB, "WTSL" SparkLink gateway app
  (github.com/zak1234-git/wtsl_app, single commit `f93998c "modified the api md"`)

Questions asked: (1) structural diff Ai-BS21 vs fbb_ws63 lineage; (2) is wtsl_app's
`slal/node/` SSAP a from-scratch SLE stack (valuable) or just a vendor header copy?

Cross-reference baseline used for all diffs: our in-tree SDK at
`sdk/ws73_sdk_linux_WS73_1.10.110/` (fbb_ws63 lineage, `include/bsle/sle/*.h`).
See also existing note `BS21-WS63-SDK-COMPARISON.md` (fbb_bs2x device-side SSAP dialect).

---

## PART 1 — Ai-BS21_SDK (Ai-Thinker, half-open BS21/BS25 SDK)

### 1.1 What it is

- Self-described "半开源" (half-open) SDK for the Ai-BS21 NearLink module; full source
  requires contacting Ai-Thinker (`README.md:3`). Explicitly restricted to Ai-BS21
  modules/boards (`README.md:3-5`).
- Same corporate skeleton as fbb_ws63: top-level `build.py` + Kconfig `config.in` +
  CMakeLists.txt, `@CompanyNameMagicTag` copyright headers everywhere
  (`build.py:3-4`, `config.in:2-4`). Build entry instantiates a `CMakeBuilder`
  from `build/config` (`build.py:45,53`), vs fbb_ws63's make-based `build.py` —
  so Ai-BS21 moved the build to pure CMake+ninja, otherwise the target/Kconfig
  layout (`Targets/Application/Bootloader/Drivers/Kernel/Middleware/Protocol`,
  `config.in:13-33`) is the fbb_ws63 pattern.
- Toolchain/IDE: HiSilicon "CFBB" DevEco Device Tool plug-in, chip selected as
  **BS21-N1100** (`README.md`, DevEco/CFBB sections). CFBB version is exposed in
  `include/cfbb_version.h:22-28` as 0.9.0.5. "CFBB" appears to be the chip-family
  codename for the BS2x/BS25 generation (our WS73 "fbb" = a sibling family).

### 1.2 Open vs closed — much more closed than fbb_ws63

- Open: `application/samples/` (peripheral demos: adc, blinky, i2c, pwm, spi, uart,
  tasks, watchdog, ...), `application/samples/products/{sle_uart,ble_uart}`,
  `open_source/` (7-zip-lzma-sdk, GmSSL3.0, libboundscheck, mbedtls), `test/`.
- Closed (binary-only, 0 .c files found by `find ... -name '*.c' | wc -l` in all three):
  - `kernel/` — only CMakeLists/Kconfig + `kernel/osal/libosal.a`;
    LiteOS variants referenced at `kernel/liteos/CMakeLists.txt:15-23`
    (liteos_v208.5.0_b004 ... liteos_v208.6.0_b017_cat1, sw39/rom) with no sources.
  - `drivers/`, `middleware/` — header-only `include/` trees + closed libs.
  - `protocol/` — **everything is a prebuilt static lib per product profile**:
    - `protocol/bt/controller/bgtp/<profile>/libbgtp.a` (controller firmware
      delivered as an .a to link), with profiles per chip: bs20-n1200, bs21-n1100,
      bs21e-1100e, bs22-n1200 × {ble-peripheral, sle-peripheral, sle-central,
      sle-ble-peripheral, slem-peripheral/central, rcu, sle-microphone,
      sle-measure-dis, slp, standard-*} (dir listing under `protocol/bt/controller/bgtp/`).
    - `protocol/bt/host/bg_common/<profile>/libbg_common.a` (host-side common lib,
      same profile matrix).
  - `interim_binary/{bs20,bs21,bs21e,bs22}/bin/` — `boot_bin`, `nv`, `partition`
    (boot images + NV defaults per chip).
- Note the profile-per-firmware model: unlike fbb_ws63 (one bgtp firmware + runtime
  profile switching), Ai-BS21 ships a distinct prebuilt controller lib per use-case.
  The `bgtp`/`bg_common`/`bg_` prefix is shared with our WS73 tree's BGTP discussion
  (see `FBB-WS63-BGTP.md`) — same firmware-transport architecture, different packaging.
- `protocol/glp/hiex_v300` exists (GLP = the generic link protocol also present in
  fbb_ws63); include tree also has `services/{wifi,radar,slp,nfc}` and
  `middleware/utils/usb_class` — i.e. the same peripheral/service taxonomy as ws63.

### 1.3 SLE/SSAP API surface (headers only)

- Header set: `include/middleware/services/bts/sle/` — exactly 14 headers:
  sle_common, sle_connection_manager, sle_device_discovery, sle_device_manager,
  sle_errcode, sle_factory_manager, sle_glp_manager, sle_hadm_manager, sle_low_latency,
  sle_ota, sle_ssap_client, sle_ssap_server, sle_ssap_stru, sle_transmition_manager.
  This is the **same 14-header set** as `sdk/ws73_sdk_linux_WS73_1.10.110/include/bsle/sle/`.
- SSAP is byte-for-byte near-identical to ws73: `diff sle_ssap_server.h` against our
  ws73 copy = **4 changed lines**; ssap flow identical (register_server → add_service_sync
  → add_property_sync → add_descriptor_sync → start_service → notify_indicate).
- Connection manager is a **superset of ws73**: 1184 lines vs 962; 28 `errcode_t`
  functions vs 18. Only-in-BS21 functions: `sle_add_device_to_access_filter_list`,
  `sle_clear_access_filter_list`, `sle_create_connection_cancel`, `sle_get_connect_role`,
  `sle_read_access_filter_list_size`, `sle_remove_device_from_access_filter_list`,
  `sle_set_channel_map`, `sle_set_connect_rssi`, `sle_set_nv_smp_keys`,
  `sle_set_save_pair_keys_mode`. Nothing is only-in-ws73 (ws73 is a strict subset here).
  This matches the earlier finding (BS21-WS63-SDK-COMPARISON.md) that BS2x adds
  access-filter/pair-key APIs over ws63.
- sle_device_discovery.h differs by 90 lines (minor), sle_common.h by 9 (trivial).

### 1.4 Sample apps (the only real SLE application code)

- `application/samples/products/sle_uart/` — canonical SLE UART server+client pair,
  essentially the same sample as fbb_ws63/fbb_bs2x `products/sle_uart`:
  - Server (`sle_uart_server/sle_uart_server.c`, 422 lines):
    `ssaps_register_server(&app_uuid,&g_server_id)` at :240,
    `ssaps_add_service_sync` :172, `ssaps_add_property_sync` :199,
    `ssaps_add_descriptor_sync` :218, `ssaps_start_service` :252; callback
    registration via `ssaps_register_callbacks` with add/start/mtu/read/write cbs
    (:150-161). Base UUID `37 BE A8 80 FC 70 11 EA B7 20 ...` (:69-70).
  - Client (`sle_uart_client/sle_uart_client.c`, 325 lines): seek param setup,
    `sle_connect_remote_device` :106, `sle_pair_remote_device` :164,
    `ssapc_exchange_info_req` :192, `ssapc_find_structure` :214 — same client flow
    our SSAP notes already document.
  - `board.json` targets chip `"bs25"` with **PLIC** interrupt settings
    (`board.json:1`, `info.chipName: bs25`, PLIC under IRQ_UART2/3) — i.e. the
    BS25 generation is RISC-V; BS21-N1100 is the ARM one selected in the README.
- No SLE samples beyond sle_uart/ble_uart; everything else is peripheral demos.
- Takeaway: Ai-BS21 exposes the identical device-side SSAP API as fbb_ws63/bs2x but
  with **zero stack source** — it cannot teach us anything new about SSAP internals;
  its value is (a) confirming API-parity, (b) the extra connection-manager functions
  as a second reference, (c) the per-profile bgtp packaging model.

---

## PART 2 — wtsl_app (a "SparkLink gateway" product application)

### 2.1 What WTSL actually is

- README says clone from `git@101.102.1.199:sparklink/wtapp/wtsl_app.git` with
  `wtsl_core` as a git submodule (`README.md:1-9`) — an internal corporate GitLab.
- `docs/wtsl_restful_api.md:3` titles it **"微泰星闪网关API指南"** ("MicroTai/WT
  SparkLink Gateway API guide") — so WTSL = the vendor's (微泰) SparkLink gateway
  product line; REST API base `/api/v1`, JSON, bearer-token auth
  (`docs/wtsl_restful_api.md:5-10`).
- `docs/hisi_FAQ.md:3-5` names the hardware platform: **Hi2921 + Hi2981**, host
  driver version "T215", and shows `iwpriv vap0 cfg ...` commands on a USB-gadget
  netdev (`/sys/kernel/config/usb_gadget/g1`). G-nodes (Grant/gateway) and T-nodes
  (Terminal) form BSS-like domains with channels ~2.4 GHz numbers (2479, 2291, 709
  in `show_bss` output, `docs/hisi_FAQ.md:21-27`) and `start_join`/`view_users`.
  => The primary radio in this product is **SLB** (a WLAN-like SparkLink Basic
  bearer on Hi2981), with SLE as a secondary link — NOT an SLE-only product.
- `docs/ACL_QOS_API_GUIDE.md:1-7`: gateway firmware v1.1.22+, ACL = iptables,
  QoS = tc (HTB/SFQ/TBF) — the app is a Linux network appliance bridging
  Ethernet/TCP/UDP onto the SparkLink bearers.
- Build (`Makefile`): three targets — `arm` with `arm-mix510-linux-` cross compiler
  (the Hi2921 toolchain, `Makefile:12`), `arm64` with `aarch64-none-linux-gnu-`
  (:16), and x64; `envsetup.sh` adds tl3568 (RK3568) and hi2921 toolchain paths.
  Link line: `-lpthread -lcjson -lmicrohttpd -lwtsl_core -lsle_host_arm`
  (`Makefile:13,17`) — **`-lsle_host_arm` is a prebuilt closed SLE host stack**,
  shipped as `lib/libsle_host_arm.a` and `lib/libsle_host_arm64.a`.
- Version stamped from git: `cfg/PreBuild.sh` writes `inc/version.h` from
  `git rev-list` (commit count = VersionCode 50).

### 2.2 Source layout of `src/wtsl_core/`

```
src/wtsl_core/
  api/     wtsl_core_api.c (331 lines) + wtsl_core_api.h — command dispatch + SPLINK_INFO state
  slal/    "stack abstraction layer": node/ (SLE), slb/ (SLB), sle/ (transport glue)
           + wtsl_core_splink_mc.c (312) + wtsl_core_splink_state.c (node service loops)
  slb/     wtsl_core_slb_interface.c (2790 lines!), wtsl_core_slb_acl_core.c,
           wtsl_core_slb_qos_core.c, wtsl_core_slb_init.c, wtsl_core_slb_business.c
  node/    25 files, 14,264 lines total (HiSilicon headers + wtsl glue, see 2.3)
  sle/     sle_conn_client.c (314), sle_client.c (101), sle_tcp_server.c (147),
           sle_udp_server.c (171), sle_simple_ota_* (cli/fops/main/server/server_adv),
           slemain.c (EMPTY 0-byte file)
  protocol/  can/, http/, https/, modbus/, mqtt/, rs485/  (bearer/protocol adapters)
  dataparser/ wtsl_core_dataparser.c + *_ota.c (OTA payload parsing, self-kill+restart cmds)
  db/, user/, utils/  (config manager via nanddump of /dev/mtd10, users, logs, lists)
```

### 2.3 `slal/node/` — what it actually contains (the core question)

Contents (25 files, 14,264 lines):

- **17 HiSilicon headers** with `Copyright (c) @CompanyNameMagicTag 2022` banners:
  `errcode.h` (828), `sle_common.h` (105), `sle_connection_manager.h` (1502),
  `sle_device_discovery.h` (900), `sle_device_manager.h` (218), `sle_errcode.h` (213),
  `sle_factory_manager.h` (444), `sle_glp_manager.h` (44), `sle_hadm_manager.h` (509),
  `sle_iso_manager.h` (829), `sle_low_latency.h` (441), `sle_ota.h` (92),
  `sle_ssap_client.h` (836), `sle_ssap_server.h` (1071), `sle_ssap_server_inner.h`
  (116), `sle_ssap_stru.h` (179), `sle_trans_data_manager.h` (149),
  `sle_transmition_manager.h` (300).
- **5 wtsl-authored .c/.h glue files**: `wtsl_core_node_callback.c` (3272 lines!),
  `wtsl_core_node_manager.c` (711) + `.h` (324), `wtsl_core_node_list.c` (404) + `.h` (83),
  `wtsl_core_node_service.c` (505).

Header provenance — diffed against our ws73 1.10.110 `include/bsle/sle/`:

- These are **NOT copies of ws73 headers and NOT from-scratch**. They are HiSilicon
  originals from a *newer/different* generation of the "SLE host for Linux" SDK
  (the user-space host stack for Hi2921/Hi2981). Changed-line counts vs ws73:
  ssap_server 486, ssap_client 459, connection_manager 2464, device_discovery 272,
  common 21.
- Three headers exist **only** here (absent from ws73 AND from Ai-BS21):
  `sle_iso_manager.h` (ISO channel API), `sle_trans_data_manager.h`,
  `sle_ssap_server_inner.h` (internal server interfaces — a small intel leak about
  the host stack's internal wiring).
- API-generation markers proving "newer than ws73":
  - `sle_ssap_server.h:890,900` documents an `ssaps_call_method_request_callback`
    (call/method RPC over SSAP) — **absent** from ws73 and Ai-BS21 ssap_server.h
    (grep returned nothing in both).
  - `ssaps_notify_indicate_by_uuid` at `sle_ssap_server.h:969` (ws73 has it at
    `sle_ssap_server.h:809`, Ai-BS21 has neither extra doc structure; ws73 line
    count 861 vs 1071 here).
  - `sle_transmition_manager.h` is a different beast: this generation adds a
    **fast-data pipe** API — `sle_tm_send_fast_data` (:222), `fast_data_cbk`
    typedef (:224), `sle_tm_fast_register_cbks` (:241), `sle_tm_set_node_info`
    (:258), `sle_tm_sync_timer` (:275), `sle_tm_custom_set_config` (:292) —
    whereas ws73's `sle_transmition_manager.h` only has
    `sle_tm_signal_capability_req` (:63). Ai-BS21's copy (135 lines) sits in
    between and has neither fast-data nor the ws73 capability call — i.e. three
    distinct generations of the same header across ws73 / Ai-BS21 / wtsl-node.

Verdict on the SSAP question: **`slal/node/` is NOT a from-scratch SLE/SSAP stack.**
The .c files contain no SSAP PDU/state-machine code at all (grep for `ssaps_`/`ssapc_`
inside `node/*.c` finds no protocol handling — only `sle_common.h` includes and the
HiSilicon callback registration pattern). All protocol work lives inside the closed
prebuilt `libsle_host_arm(.a)`. What wtsl wrote is:

- `wtsl_core_node_manager.{h,c}` — node model and state: `WTSLNodeType` with
  `NODE_TYPE_SLB_G=0 / NODE_TYPE_SLB_T=1 / NODE_TYPE_SLE_G=5 / NODE_TYPE_SLE_T=6 /
  NODE_TYPE_SLE_P=7` (`wtsl_core_node_manager.h:12-15`), `WTSLNodeAdvInfo`
  advertising model with `mcs_bound_table[32]`, `mib_params`, `fisa_enable`,
  per-user MCS/RSSI tables (`wtsl_core_node_manager.h:36-100`), a big packed
  `WTSLNodeInfo` config struct (:104+).
- `wtsl_core_node_callback.c` (3272 lines) — REST/command handlers wired to cJSON:
  e.g. `wtsl_core_sle_start_scan` (:2123), `wtsl_core_sle_connect` (:2172, parses a
  MAC into `sle_addr_t` and calls `sle_create_connection`), `wtsl_core_get/set_sle_basicinfo`
  (:2235/:2260), flash-env persistence via `nanddump`/`flash_erase`/`nandwrite` on
  `/dev/mtd10` (:2358-2376).
- `wtsl_core_node_service.c` — inter-node heartbeat/management mesh over UDP port
  8081 (`:52`), DHCP server/client bridge on a `NET_BRIDGE_NAME` alias interface
  (`:93`, embedded `udhcpd` config strings :24-38), NodeDataHeader heartbeat protocol
  (:58-75).
- `wtsl_core_node_list.c` — plain linked-list container.

### 2.4 The prebuilt `libsle_host_arm.a` — what the closed stack is

`nm` on `lib/libsle_host_arm.a`: 1748 unique exported `T` symbols, of which 401 are
`sle_*`/`ssap*`/`enable_sle`/`disable_sle`. Highlights:

- Adapter layer: `sle_adapter_ioctl`, `sle_adapter_hci_send_data`, `gle_adapter_init`,
  `gle_adapter_deinit`, `gle_aa_reg_hci_cbks`, `bth_hci_malloc/free`,
  `bth_conf_set_hci_log_status` — a user-space host stack talking downward through an
  **ioctl-style adapter + HCI transport** into a kernel driver (the "T215" host driver
  of `docs/hisi_FAQ.md:5`). This mirrors our E573H problem shape (host stack in
  user space vs our in-kernel approach on WS73).
- Full AT-command framework inside the lib (`sle_at_*`: `sle_at_cm_register`,
  `sle_at_cmd_ssaps_*` incl. `sle_at_cmd_ssaps_call_method_request_callback`,
  `sle_at_performance_*`, `sle_at_hadm_register`) — same AT infrastructure as the
  BS2x firmware-side stack.
- Utility stratum uses `bg_` prefixes (`bg_dl_list_*`, `bg_data_put_le*`,
  `bg_bitmap_*`, `bgh_init_config`) — the same "bg" naming family as Ai-BS21's
  `libbgtp.a`/`libbg_common.a`, confirming one shared HiSilicon/Bestechnic-lineage
  codebase across the BS2x firmware stack and this Linux host stack.

### 2.5 The genuinely custom and reusable parts (where the value is)

- **SLE↔TCP/UDP data plane bridge** (`slal/sle/`): `sle_client_init` registers
  dev-manager/seek/connection callbacks plus `sle_tm_fast_register_cbks(fast_data_recv_cb)`
  (`sle_conn_client.c:257-263`); `fast_data_recv_cb` fans received SLE fast-data out
  to both `sle_tcp_server_send` and `sle_udp_server_send` (:250-252); the reverse path
  pushes socket bytes into `sle_client_senddata` → `sle_tm_send_fast_data(conn_id,...)`
  per connected peer (`sle_tcp_server.c:42-44`, `sle_conn_client.c:274-296`). This is
  the cleanest working example we have of the Linux-host `sle_tm` fast-data API —
  directly relevant if the E573H dongle firmware exposes a tm/transparent pipe.
- **SLB control plane** (`slal/slb/wtsl_core_slb_interface.c`, 2790 lines): drives the
  Hi2981 SLB netdev purely via `system()`/`popen()` of `iwpriv <dev> cfg "channel %d"`
  (:558), `get_channel` (:585), `bw`/`tfc_bw` (:631/:701), `set_mcs_bound <mac> <min> <max>`
  (:782), `cell_id` (:923), plus iptables/tc glue in `wtsl_core_slb_acl_core.c:214`.
  Documents the whole `iwpriv` verb surface of an SLB radio — useful background for
  any future SLB-capable hardware, not for SSAP.
- **OTA over SLE** (`slal/sle/sle_simple_ota_*`, ~1000 lines) — SLE-server-based OTA
  with adv (`sle_simple_ota_server_adv.c` 275 lines) and file-ops backend.
- **Gateway product architecture** — REST (libmicrohttpd) + cJSON config + mtd env +
  UDP heartbeat mesh + DHCP bridging: a complete reference for how a SparkLink
  gateway appliance is assembled on Linux.

### 2.6 What wtsl_app is NOT

- Not an open SLE stack: the actual SSAP/LL/ISO implementation is the closed
  `libsle_host_arm.a`; the repo cannot be used to replace or reimplement our host.
- Not a ws73/fbb_ws63 derivative: newer API generation (call_method, tm fast-data,
  iso manager), Linux user-space (pthread/sockets) rather than LiteOS device-side.
- Not SSAP-node code in the protocol sense: "node" refers to gateway mesh nodes
  (G/T nodes), and the SSAP headers are just the vendor drop that `sle_conn_client.c`
  compiles against.

---

## PART 3 — Implications for the nearlink E573H driver project

1. Ai-BS21 SDK adds little beyond what `BS21-WS63-SDK-COMPARISON.md` already
   established (identical SSAP wire API). Its one incremental find is the BS21
   connection-manager superset (access-filter list, `sle_set_channel_map`,
   `sle_set_nv_smp_keys`, `sle_set_save_pair_keys_mode` — see 1.3) which we can use
   as a semantic reference if our dongle firmware reports newer stack capabilities.
2. wtsl_app's `slal/node/` is a header drop + glue, not a stack — do not budget any
   "free SSAP implementation" value from it.
3. Real value of wtsl_app: (a) the `sle_tm` fast-data client/server glue as an
   integration template for a Linux-host SLE link; (b) `sle_ssap_server_inner.h` +
   `sle_trans_data_manager.h` as previews of host-stack internals not present in
   ws73 1.10.110 headers; (c) proof that HiSilicon ships the same stack family as a
   user-space lib over an ioctl/HCI adapter — a data point for our own
   user-space-vs-kernel split design for the E573H.
4. If we ever need SLB (SparkLink Basic G/T-node networking), wtsl's iwpriv verb
   list (2.5) is the best documentation of that control surface we have found.
5. Watch item: the three-generation drift of `sle_transmition_manager.h` (ws73 70
   lines / Ai-BS21 135 / wtsl-node 300) suggests HiSilicon is consolidating on the
   fast-data tm API; future firmware updates for our dongle may follow that dialect.

---

## 8-LINE SUMMARY

Ai-BS21_SDK is a half-open fbb_ws63-style SDK (same build.py/Kconfig skeleton, CFBB
0.9.0.5) where kernel/drivers/middleware/protocol are prebuilt .a libs packaged
per-chip-per-profile (libbgtp.a); its 14 SLE headers match ws73's set, SSAP server
header is 4-lines-different, connection manager is a strict superset (+10 APIs), and
sle_uart is the only SLE sample. wtsl_app is a 微泰 SparkLink *gateway* appliance
(REST/iptables/tc/DHCP/heartbeat) for Hi2921+Hi2981 where the radio plane is SLB
(iwpriv-controlled G/T nodes) plus SLE via the closed prebuilt libsle_host_arm.a
(1748 symbols: sle_adapter_ioctl/HCI adapter, AT framework, bg_* stratum). Its
slal/node/ is NOT a from-scratch SSAP stack: 17 HiSilicon headers from a newer
Linux-host SDK generation (adds ssaps_call_method, ssaps_notify_indicate_by_uuid,
sle_tm fast-data, iso manager, ssap_server_inner.h — absent from ws73 and Ai-BS21)
plus ~5k lines of wtsl glue (REST handlers, node list, UDP mesh). The reusable finds
are the sle_tm fast-data TCP/UDP bridge (sle_conn_client.c/sle_tcp_server.c), the
SLB iwpriv verb surface (wtsl_core_slb_interface.c, 2790 lines), and OTA-over-SLE.
Full report: .scratch/nearlink-driver/lab-notes/NEW-BS21-WTSL.md
