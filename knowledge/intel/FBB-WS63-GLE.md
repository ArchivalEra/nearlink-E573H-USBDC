---
type: intel
title: "FBB-WS63 GLE (Generic Link Engine) Analysis"
language: en
created: 2026-08-17
tags: [intel, ws63, generic, link]
sources:
  - "ArchivalEra/nearlink-E573H-USBDC workspace"
trust: B
stale_after: 2027-02-17
---

# FBB-WS63 GLE (Generic Link Engine) Analysis

Date: 2026-08-17

Scope: Reverse-engineer the architecture, DLI command/event flow, and link-layer state machine of the GLE layer from the fbb_ws63 SDK, then cross-reference with our WS73 dongle driver's DLI-level state machine.

**Key finding**: GLE source is **not available** in the SDK — only precompiled static libraries (`libbth_gle.a`) for RISC-V LiteOS. All analysis below is derived from symbol tables (`nm`), build configs (Kconfig/cmake), object-file names, and cross-referencing with OpenSparklink and our hardware-verified DLI dialect.

---

## Sources

| Source | Path | Notes |
|---|---|---|
| GLE library | `fbb_ws63/src/protocol/bt/host/gle/ws63-liteos-app/libbth_gle.a` | Precompiled RISC-V static library; ~90 object files, ~800 exported symbols |
| GLE cmake | `fbb_ws63/src/protocol/bt/host/gle/bth_gle.cmake` | Build component "bth_gle"; subdirs: host, ahi, dft, samples, sdk, service |
| GLE Kconfig | `fbb_ws63/src/protocol/bt/host/gle/Kconfig` | Features: `FEATURE_GLE_LOW_LATENCY`, `FEATURE_GLE_HADM` |
| Build configs | `fbb_ws63/src/build/config/target_config/ws63/menuconfig/acore/ws63_liteos_bgle_all_asic.config` | BGLE RAM 32K; AT_GLE modules: SSAPC, SSAPS, DD, CM, TM_SIGNAL |
| bg_common lib | `fbb_ws63/src/protocol/bt/host/bg_common/ws63-liteos-app/libbg_common.a` | Shared infrastructure: IPC, AES, BLE key storage, BT address, queue ops |
| bt host libs | `fbb_ws63/src/protocol/bt/host/bt/` | `libbt_host.a`, `libbt_app.a`, `libbth_sdk.a` — BLE-side host stack |
| bt controller | `fbb_ws63/src/protocol/bt/controller/bgtp/` | `libbgtp.a` — BLE/GLE coexistence controller firmware |
| Our SSAP driver | `stack/ssap/src/ssap_link.c`, `include/ssap_link.h` | DLI-level connect/disconnect FSM |
| Our transport | `stack/ssap/src/hwsle_transport.c`, `include/hwsle_transport.h` | DLI frame adapter for `/dev/hwsle` |
| Cross-check | `.scratch/nearlink-driver/lab-notes/OSPL-DLI-CROSSCHECK.md` | WS73 DLI dialect vs OpenSparklink standard |
| Cross-check | `.scratch/nearlink-driver/lab-notes/OSPL-CONN-FSM.md` | OpenSparklink connection FSM vs our ssap_link FSM |
| Our DLI notes | `.scratch/nearlink-driver/lab-notes/SLE-CONTROL-PLANE.md` | Hardware-verified DLI command/event bytes |

---

## 1. GLE Architecture

### 1.1 What GLE Is

GLE (Generic Link Engine) is the **chip-side link layer** of the NearLink/SLE (SparkLink Enhanced) protocol stack. On WS63, it runs on the RISC-V A-core under LiteOS as a host-side protocol stack component — it is NOT a baseband processor firmware blob, but rather a user-space library that talks to the radio controller (bgtp) via IPC.

This is a critical distinction for our WS73 analysis: on WS63 (standalone SoC), GLE runs on the same die as the application. On WS73 (RF subcard for a Linux host), the equivalent GLE logic runs **inside the chip's firmware** — we never see it. The DLI command interface (`/dev/hwsle`) on WS73 is the boundary where GLE's equivalent logic begins processing our commands.

### 1.2 Source Organization (from .c.obj names in libbth_gle.a)

GLE is organized into **functional modules**, each with `_core.c` / `_ui.c` / `_config.c` file patterns:

| Module | Object files | Role |
|---|---|---|
| **Manager** | `gle_manager.c`, `gle_manager_ui.c` | Top-level init/cleanup, callback dispatch, module registry |
| **HCI** | `gle_hci_cmd.c`, `gle_hci_ev.c`, `gle_hci_data.c`, `gle_hci_ui.c`, `gle_hci_qos.c`, `gle_hci_send_data.c` | HCI command/event encode/decode, flow control, data recombination |
| **CM** (Connection Manager) | `gle_cm_core.c`, `gle_cm_ui.c`, `gle_cm_config.c` | Connection establishment/release/update FSM |
| **DD** (Device Discovery) | `gle_dev_discovery.c`, `gle_dev_discovery_ui.c`, `gle_dev_discovery_cfm.c` | Advertising + scanning lifecycle |
| **SM** (Security Manager) | `gle_sm_proc.c`, `gle_sm_cmd.c`, `gle_sm_key.c`, `gle_sm_timer.c`, `gle_sm_ui.c` | Pairing, encryption, key management |
| **TM** (Transport Manager) | `gle_tm_conn.c`, `gle_tm_data.c`, `gle_tm_hlp.c`, `gle_tm_manager.c`, `gle_tm_signal.c`, `gle_tm_ui.c` | Transport channel allocation (TCID), data routing, signal capability |
| **AA** (Application Adapter) | `gle_aa_connect_ui.c`, `gle_aa_dev_discovery_ui.c`, `gle_aa_trans_ui.c`, `gle_aa_secu_ui.c`, `gle_aa_factory_ui.c`, `gle_aa_glp_ui.c`, `gle_aa_hadm_ui.c` | DLI command wrappers, factory RF test, HADM ranging, GLP |
| **DM** (Device Management) | `gle_dm_link_ui.c`, `gle_dm_secu_ui.c`, `gle_dm_trans_ui.c` | Device link table, security link tracking, TCID/LCID bookkeeping |
| **SSAP** | `gle_ssap_client.c`, `gle_ssap_client_ui.c`, `gle_ssap_server.c`, `gle_ssap_server_ui.c`, `gle_ssap_manager.c`, `gle_ssap_manager_ui.c` | SSAP protocol engine (service discovery, read/write, notify/indicate) |
| **SSAP Low Latency** | `gle_ssap_low_latency.c` | Low-latency data path optimization |
| **SAPI** (Service API) | `sapi_gle_*.c` (8 files) | Application-facing service API layer |
| **UAPI** (User API) | `uapi_gle_*.c` (8 files) | Kernel/user boundary API layer |
| **AT** (AT commands) | `sle_at.c`, `sle_at_common.c`, `sle_at_cm.c`, `sle_at_dd.c`, `sle_at_sm.c`, `sle_at_ssapc.c`, `sle_at_ssaps.c`, `sle_at_service_low_latency.c` | AT command test interface |
| **OHOS SLE Srv** | `oh_sle_srv_cm.c`, `oh_sle_srv_dd.c`, `oh_sle_srv_ssap_client.c`, `oh_sle_srv_ssap_server.c` | OpenHarmony SLE service adapter |
| **SLE Srv** | `sle_srv_cm.c`, `sle_srv_dd.c`, `sle_srv_sm.c`, `sle_srv_tm.c`, `sle_srv_glp.c`, `sle_srv_ota.c`, `sle_srv_rf.c`, `sle_srv_dfx.c`, `sle_srv_ssap_client.c`, `sle_srv_ssap_server.c`, `sle_srv_ssap_server_cb.c` | SLE service layer (connection mgmt, OTA, RF test, diagnostics) |
| **Factory** | `gle_factory_core.c`, `gle_factory_ui.c` | Manufacturing RF test mode |
| **GLP** (Giant Low Power?) | `gle_glp_core.c`, `gle_glp_ui.c` | Low-power management |
| **Samples** | `gle_sample_*.c` (7 files) | Reference implementations for CM, DD, SM, SSAPC, SSAPS, low-latency dongle/mouse |

Source file count: **~90 .c.obj modules** in libbth_gle.a.

### 1.3 Three-Layer API Architecture

GLE exposes three nested API layers (visible from symbol prefixes):

```
Application
    |
    v
UAPI (uapi_gle_*)  — kernel/user boundary, used by OHOS SLE service
    |
    v
SAPI (sapi_gle_*)  — service-level API, used by AT commands and samples
    |
    v
Internal (gle_*)   — core implementation, callback-driven
```

This is analogous to a Linux kernel subsystem: UAPI is the ioctl-like boundary, SAPI is the in-kernel service API, and `gle_*` internals are the implementation.

### 1.4 Initialization Chain

The entry point is `gle_init`, which calls `gle_stack_init` then `gle_manager_init`. The full init sequence extracted from undefined-symbol dependencies:

```
gle_init
  +-> gle_stack_init
  |     +-> sle_init_transport_layer   (transport to radio controller)
  |     +-> gle_hci_init               (HCI command/event layer)
  |     +-> gle_cm_init                (connection manager)
  |     +-> gle_dd_init                (device discovery: adv+scan)
  |     +-> gle_sm_init                (security manager)
  |     +-> gle_tm_init                (transport manager: TCID channels)
  |     +-> gle_factory_init           (RF test mode)
  |     +-> gle_glp_init               (low power)
  |     +-> gle_ssap_init              (SSAP client+server)
  |     +-> gle_dm_trans_mgt_init      (device link + TCID tracking)
  |     +-> gle_dm_secu_mgt_init       (security link tracking)
  +-> gle_manager_init
        +-> gle_cm_reg_cbk             (register CM callbacks)
        +-> gle_dd_reg_hci_cbks        (register DD HCI callbacks)
        +-> gle_aa_reg_hci_cbks        (register AA HCI callbacks)
```

The `bth_gle.cmake` file (line 9) reveals that GLE also has `ahi/` (Application Host Interface — the DLI boundary layer), `dft/` (Design For Test), `samples/`, `sdk/`, and `service/` subdirectories — but these are all compiled into the same `libbth_gle.a` static library.

### 1.5 Kconfig Features

From the Kconfig file:
- `FEATURE_GLE_LOW_LATENCY` (default n) — low-latency data path (CB/IOG)
- `FEATURE_GLE_HADM` (default n) — High-Accuracy Distance Measurement

From the build configs, the AT_GLE test modules are:
- `AT_GLE_MODULE_SSAPC` — SSAP client AT commands
- `AT_GLE_MODULE_SSAPS` — SSAP server AT commands
- `AT_GLE_MODULE_DD` — Device discovery AT commands
- `AT_GLE_MODULE_CM` — Connection management AT commands
- `AT_GLE_MODULE_TM_SIGNAL` — Transport signal AT commands
- `AT_GLE_MODULE_LOW_LATENCY` — Low-latency AT commands (optional)
- `AT_GLE_MODULE_DFX_CONFIG` — Diagnostics AT commands (optional)

BGLE RAM allocation: 16K / 32K / 64K configurable (`CONFIG_BGLE_RAM_SIZE_*`).

---

## 2. GLE and the DLI Relationship

### 2.1 DLI Command Reception (Host -> Chip Direction)

On WS63 (standalone), GLE receives DLI commands through its HCI layer. The flow is:

```
Application (OHOS/AT)
  -> UAPI/SAPI layer
    -> gle_general_command_deliver()     (command routing by module ID)
      -> gle_hci_command_send()           (HCI command queue)
        -> gle_hci_command_encode_send_tl()  (encode + send to transport layer)
          -> sle_init_transport_layer()   (IPC to radio controller)
```

Key symbols for this path:
- `gle_general_command_deliver` — generic command dispatch by module ID (`gle_general_command_deliver.c.obj`)
- `gle_hci_command_send` — enqueue HCI command for sending
- `gle_hci_command_encode_send_tl` — encode command and send to transport layer (TL = Transport Layer)
- `gle_hci_command_check_flow_control` — flow control before sending
- `gle_hci_command_check_support` — feature/capability check
- `gle_hci_command_rtx` — retransmission on timeout
- `pack_hci_gle_par` — parameter packing helper

Each module registers HCI command/event callbacks via:
- `gle_hci_event_reg_cbks` / `gle_hci_event_unreg_cbks` — global event callback registration
- Module-specific: `gle_aa_reg_hci_cbks`, `gle_dd_reg_hci_cbks` — per-module HCI callback registration

### 2.2 DLI Event Production (Chip -> Host Direction)

Events flow back through the transport layer into the HCI event processing chain:

```
Radio controller (bgtp)
  -> Transport layer
    -> hci_tl_gle_recv_event_proc()       (event reception)
    -> hci_tl_gle_recv_acb_data_proc()    (async connection-based data)
    -> hci_tl_gle_recv_icb_data_proc()    (isochronous data)
      -> gle_hci_cbk()                     (callback dispatch)
        -> gle_hci_cbk_traverse()          (traverse registered callbacks)
          -> module-specific callback (e.g., gle_cm_connection_cbk, gle_dd_*_cbk)
```

Key symbols:
- `hci_tl_gle_recv_event_proc` — receives transport-layer events
- `hci_tl_gle_recv_acb_data_proc` — receives ACB (async connection-based) data frames
- `hci_tl_gle_recv_icb_data_proc` — receives ICB (isochronous) data frames
- `gle_hci_cbk` / `gle_hci_cbk_traverse` — event callback dispatch
- `gle_hci_cmd_find_by_cmd_status_evt` — correlate CmdStatus events with pending commands
- `gle_hci_command_rtx` — retransmit on timeout

### 2.3 How This Maps to WS73

On WS73 (our USB dongle), the DLI interface is `/dev/hwsle`:

```
Our host (Linux)
  -> ssap_link.c (sends DLI commands via hwsle_transport_send_hci_cmd)
    -> hwsle_transport.c (writes 0xA1 frames to /dev/hwsle)
      -> Kernel sle_soc driver (HCC transport over USB)
        -> WS73 firmware (GLE equivalent runs here)
          -> bgtp (radio controller)
```

The WS73 firmware's GLE-equivalent layer is **the black box** that processes our DLI commands. The symbol `oh_sle_srv_cm.c.obj` / `oh_sle_srv_dd.c.obj` / `oh_sle_srv_ssap_*.c.obj` in the GLE library suggest that OpenHarmony (OHOS) has a service adaptation layer that sits between the DLI command interface and the GLE internals — this is exactly the layer that runs inside the WS73 firmware and processes our commands.

### 2.4 Command Correlation

GLE on WS63 uses a **callback-based correlation** model, not opcode-echo correlation:
- Each module registers callbacks for specific event types
- The HCI layer dispatches events to registered callbacks
- There is no explicit pending-command queue with opcode-based matching visible in the exported symbols (unlike OpenSparklink's `CmdPendingQueue`)

This is significant: on WS63, the GLE stack is a single process with shared memory — callbacks are direct function pointers. On WS73, the DLI command/event correlation is done by the **host-side driver** (`ssap_link.c`) using opcode echoes in CmdComplete events — a fundamentally different pattern necessitated by the transport boundary.

---

## 3. Link Layer State Machine

### 3.1 GLE Connection Manager (CM) States

The CM module handles the link-layer connection FSM. From the exported symbols, the CM operations are:

| Operation | Command function | Confirm function | Event callback |
|---|---|---|---|
| Create connection | `gle_connection_create` | `gle_connect_create_cfm` | `gle_cm_connection_cbk` |
| Cancel connection | `gle_connection_cancel` | `gle_connect_cancel_cfm` | — |
| Release (disconnect) | `gle_connection_release` | `gle_disconnect_create_cfm` | `gle_cm_disconnection_cbk` |
| Update parameters | `gle_connection_param_update` | `gle_connect_update_cfm` | `gle_cm_connection_update_cbk` |
| Update request | `gle_connection_param_update_request` | `gle_connect_update_req_cfm` | `gle_cm_connection_update_replay_cbk` |
| Set data length | `gle_link_set_data_len` | `gle_set_data_len_cfm` | `gle_cm_data_length_change_cbk` |
| Set PHY | `gle_link_set_phy` | `gle_set_phy_cfm` | — |
| Read remote version | `gle_link_read_remote_version` | `gle_read_remote_version_cfm` | `gle_cm_read_remote_version_cbk` |
| Read remote features | `gle_link_read_remote_feature` | `gle_read_remote_feature_cfm` | `gle_cm_read_remote_features_cbk` |
| Read remote RSSI | `gle_link_read_remote_rssi` | `gle_read_remote_rssi_cfm` | `gle_cm_read_remote_rssi_cbk` |
| Set MCS | `gle_link_set_mcs` | — | — |
| Set ACB latency | `gle_link_set_acb_latency` | — | — |
| Private conn params | `gle_private_connection_param_set` | — | — |

The CM manages device links through the DM layer:
- `gle_device_link_add` / `gle_device_link_remove` / `gle_device_link_get_by_handle` / `gle_device_link_get_by_rmt_addr` / `gle_device_link_get_by_status`

The CM registers as a module with the manager via `gle_cm_reg_cbk` / `gle_cm_reg_cbk_cfm`, receiving connection/disconnection/update events as callbacks.

### 3.2 GLE Security Manager (SM) States

The SM module has a rich pairing/encryption FSM. From symbols, the key states are:

**Pairing flow** (initiator = G-node, responder = T-node):
```
gle_sm_pairing_start -> gle_sm_pairing_req -> gle_sm_pairing_rsp
  -> gle_sm_pairing_negotiate -> gle_sm_pairing_cfm
  -> gle_sm_authentication_start_{g,t}_node
  -> gle_sm_authentication_procedure
  -> gle_sm_authentication_succeed_cbk
```

**Encryption flow**:
```
gle_sm_enable_encryption
  -> gle_sm_cmd_enable_encryption
  -> gle_sm_encrypt_param_req_reply_cbk / _negative_reply
  -> gle_sm_encrypt_change_cbk
  -> gle_sm_done
```

**Key management**:
- `gle_sm_link_key_generate` / `gle_sm_generate_pub_pri_keys` / `gle_sm_dhkey_confirm_code_key_generate`
- `gle_sm_recover_key` / `gle_sm_recover_key_core`
- `gle_sm_link_release` — release per-link security state

**Timers**:
- `gle_sm_timer_rtx` — retransmission timer
- `gle_sm_timer_repeated_attempts` — pairing attempt limit
- `gle_sm_timer_link_restart` — link restart timer

### 3.3 GLE Transport Manager (TM) States

The TM manages TCID (Transport Channel ID) allocation — the equivalent of BLE's L2CAP channels:

```
gle_tm_init
  -> gle_tm_lcid_init      (logical channel init)
  -> gle_tm_tcid_init      (transport channel init)
  -> gle_tm_reg_hci_cbk    (register HCI data callbacks)

Data path:
  gle_tm_data_send -> gle_tm_data_send_core -> gle_tm_controller_data_send
  gle_tm_data_recv -> gle_tm_data_recv_core -> gle_tm_recv_controller_data_cfm
```

TCID types:
- `gle_tm_default_tcid_activate/release` — default channels (0x02 CMTC, 0x0A SMTC)
- `gle_tm_dynamic_tcid_activate/release` — dynamic channels
- `gle_tm_find_tcid_by_local_tcid` — TCID lookup
- `gle_tm_lcid_dispatch_tcids` — map LCID to TCIDs

Signal capability negotiation:
- `gle_tm_signal_capability_req/rsp/ind/cfm` — transport signal capability exchange
- `gle_tm_unicast_connect_req/rsp` — unicast data path setup

### 3.4 Comparison with Our ssap_link.c FSM

Our `ssap_link.c` implements a **host-side** 4-state FSM that drives the **chip-side** GLE CM FSM through DLI commands:

| Our state | DLI command sent | GLE CM state (inferred) | GLE CM event received |
|---|---|---|---|
| `SSAP_LINK_IDLE` | `DLI_CREATE_CONNECTION (0x1401)` | Idle -> create request | — |
| `SSAP_LINK_CONNECTING` | — (waiting) | Connection pending | `gle_connect_create_cfm` -> connection complete |
| `SSAP_LINK_CONNECTED` | `DLI_DISCONNECT (0x1403)` or `DLI_READ_REMOTE_VERSION (0x1802)` + `DLI_SET_DATA_LEN (0x1804)` | Established | `gle_cm_connection_cbk`, `gle_cm_disconnection_cbk` |
| `SSAP_LINK_DISCONNECTING` | `DLI_CANCEL_CREATE_CONNECTION (0x1402)` (if was connecting) | Disconnect in progress | `gle_disconnect_create_cfm` |

**Critical differences** between our FSM and GLE's internal FSM:

1. **Granularity**: GLE's CM has at least 5 states (Idle, CreatePending, Connecting, Connected, Disconnecting — inferred from OpenSparklink's ConnState plus GLE symbols). Our ssap_link.c has only 4 states and collapses the GLE-internal pending states.

2. **Post-connect actions**: On `DLI_CONNECTION_COMPLETE_EVT (0x0015)`, our driver immediately sends `DLI_READ_REMOTE_VERSION (0x1802)` and `DLI_SET_DATA_LEN (0x1804)`. On WS63, GLE handles this internally: `gle_cm_connection_cbk` is called, and the CM module itself triggers `gle_link_read_remote_version` and `gle_link_set_data_len` as post-connect actions (visible from the `gle_aa_cmd_read_remote_version` and `gle_aa_cmd_set_data_len` dependencies in `gle_cm_core.c`).

3. **Security**: Our ssap_link.c has **no security state machine** — no pairing, no encryption. GLE has a full SM with pairing, DH key exchange, encryption start/change, and key storage. For WS73, security is handled by the firmware and the OHOS SLE service layer (`sle_srv_sm.c`, `sle_srv_ssap_server_cb.c`), not by our host driver.

4. **Supervision timeout**: Both our FSM and GLE implement supervision timeouts, but differently:
   - Our `ssap_link_tick()` checks `last_activity_ms` against `supervision_timeout_ms` in a periodic tick (`ssap_link.c:279-290`)
   - GLE uses timer-based supervision via `gle_sm_timer_link_restart` and the CM's `last_activity` tracking

5. **Command correlation**: Our driver uses **opcode echo** in CmdComplete events (via `find_cmd_echo` in `ssap_link.c:124-135`) to correlate responses. GLE uses **direct callbacks** since it runs in the same address space as the caller.

---

## 4. Functional Capabilities Revealed by Symbols

### 4.1 Connection Management
- Accept filter lists (whitelist): `gle_link_add_device_to_accept_filter_list`, `gle_link_clear_accept_filter_list`, `gle_link_remove_device_from_accept_filter_list`
- Connection parameter update: `gle_connection_param_update` / `_request`
- Private connection parameters: `gle_private_connection_param_set`
- Data length: `gle_link_set_data_len`
- PHY: `gle_link_set_phy`
- MCS (Modulation and Coding Scheme): `gle_link_set_mcs`
- Low-latency ACB: `gle_link_set_acb_latency`, `gle_set_acb_evt_param`

### 4.2 Advertising and Scanning (DD Module)
- Advertising: `gle_dd_set_advertising`, `gle_dd_enable_advertising`, `gle_dd_remove_advertising`, `gle_dd_clear_advertising`
- Scanning: `gle_dd_set_scanning`, `gle_dd_enable_scanning`
- Manager-level: `gle_dd_mgr_set_adv`, `gle_dd_mgr_enable_adv`, `gle_dd_mgr_enable_scan`, `gle_dd_mgr_set_scan`, `gle_dd_mgr_create_adv_item`, `gle_dd_mgr_free_adv_item`, `gle_dd_mgr_remove_adv`
- Public address: `gle_dd_set_public_address`, `gle_dd_get_public_address`
- Max data: `gle_dd_mgr_read_maximum_adv_data_len`, `gle_dd_mgr_read_adv_data_sets_num`

### 4.3 Security (SM Module)
- Pairing: full initiation (G-node and T-node), parameter negotiation, confirmation, authentication
- Encryption: enable, parameter request/reply, key generation, DH key check
- Key storage: `sle_srv_sm.c` / `sle_srv_ota.c` — paired device info persistence
- Crypto: `gle_sm_code_algorithm_choose`, `gle_sm_confirm_number_generator`, `gle_aa_read_support_crypto_algo`

### 4.4 Data Path (TM + HCI)
- ACB (Async Connection-Based) data: `gle_hci_acb_data_recv`, `gle_hci_acb_data_send_pending`, `gle_hci_recv_acb_data_header_proc`, `gle_hci_recv_acb_data_recombination`
- ICB (Isochronous) data: `hci_tl_gle_recv_icb_data_proc`, `gle_hci_sync_data_send`
- QoS: `gle_qos_link_available_data_buffer_num_get`, `gle_qos_link_hci_buffer_data_num_add/sub`, `gle_qos_list_*`
- TX buffer management: `gle_tx_acb_data_len_get/set`, `gle_tx_acb_num_*`

### 4.5 SSAP Service Layer
- Client: `gle_ssapc_discovery_services`, `gle_ssapc_exchange_info_req`, `gle_ssapc_read_req`, `gle_ssapc_write_req/cmd`, `gle_ssapc_indication_ack`
- Server: `gle_ssaps_init/deinit`, `gle_ssaps_find_hdl_by_uuid`, `gle_ssaps_mtu_set`, `gle_ssaps_reg_tree`, `gle_ssaps_send_response`, `gle_ssaps_update_item_value_by_handle/uuid`
- Transport link management: `gle_ssap_create_tl_link_by_addr`, `gle_ssap_delete_tl_link_by_addr`

---

## 5. Implications for Our WS73 Driver

### 5.1 We Do NOT Need GLE Source

The WS73 is an RF subcard — the full GLE stack (CM, DD, SM, TM, SSAP, HCI) runs inside the chip's firmware. Our host-side driver communicates exclusively through the DLI command interface (`/dev/hwsle`). The GLE architecture analysis confirms this is by design: the DLI is the defined API boundary between host and chip.

### 5.2 GLE Architecture Explains DLI Semantics

| GLE internal | DLI command equivalent | What happens on chip |
|---|---|---|
| `gle_connection_create` | `0x1401 CREATE_CONNECTION` | CM allocates handle, starts connection attempt |
| `gle_connection_release` | `0x1403 DISCONNECT` | CM sends disconnect to peer |
| `gle_dd_enable_advertising` | `0x0C05 SET_ADV_ENABLE` | DD starts advertising on PHY |
| `gle_dd_enable_scanning` | `0x1002 SET_SCAN_ENABLE` | DD starts scanning |
| `gle_sm_enable_encryption` | `0x1C03 START_ENCRYPT` | SM starts encryption procedure |
| `gle_hci_command_send` | any 0xA1 frame | HCI encodes and sends to transport |
| `gle_tm_controller_data_send` | 0xA3 ACB data frame | TM routes data to correct TCID |

### 5.3 Our Host-Side FSM Mirrors GLE CM (with simplifications)

Our `ssap_link.c` FSM (`ssap_link.h:65-70`) is a **simplified mirror** of GLE's connection manager:

```
GLE CM (chip-side, full):       Our ssap_link (host-side, simplified):
  Idle                           SSAP_LINK_IDLE
  CreatePending                  (collapsed into CONNECTING)
  Connecting                     SSAP_LINK_CONNECTING
  Connected                      SSAP_LINK_CONNECTED
  DisconnectPending              (collapsed into DISCONNECTING)
  Disconnecting                  SSAP_LINK_DISCONNECTING
```

The simplification is valid because:
- DLI commands are synchronous on the transport layer (command sent -> CmdComplete received)
- The chip handles all intermediate states internally
- Our driver only needs to track whether we're waiting for a connect/disconnect response

### 5.4 Post-Connect Sequence Differs

On WS63 (GLE runs locally):
1. CM creates connection
2. CM fires `connection_cbk`
3. CM itself calls `read_remote_version` and `set_data_len` as post-connect actions

On WS73 (GLE runs in firmware):
1. Our host sends `DLI_CREATE_CONNECTION (0x1401)`
2. Firmware CM creates connection
3. Firmware CM fires `ConnComplete (0x0015)` event back to host
4. **Our host** (`ssap_link.c:176-178`) sends `DLI_READ_REMOTE_VERSION` + `DLI_SET_DATA_LEN`
5. Firmware processes these as separate DLI commands

This is why our post-connect sequence (`ssap_link.c:173-181`) sends commands that GLE would handle internally on WS63 — the host-chip boundary forces the split.

### 5.5 Security is Firmware-Side

GLE's SM module (`gle_sm_*` symbols) has ~40 functions for pairing/encryption. None of this is in our host driver. On WS73:
- Pairing is initiated by the OHOS SLE service layer or the SSAP server application
- The firmware's SM handles the crypto (DH key, encryption start/change)
- Key storage is managed by `sle_srv_sm.c` / `sle_srv_ota.c` in firmware
- Our `sle_at_sm.c` AT commands (`sle_at_pair_remote`, `sle_at_rm_pair_remote`, `sle_at_get_paired_devices`) exercise this firmware-side SM through DLI

---

## 6. Open Questions

1. **GLE internal state machine layout**: Without source code, the exact state-to-state transitions in GLE's CM, DD, and SM are inferred from OpenSparklink's kernel implementation and symbol dependencies. The true GLE FSM may have additional states or different transition guards.

2. **GLE transport layer on WS63 vs WS73**: On WS63, `sle_init_transport_layer()` likely uses IPC (`bt_acore_ipc_receive_data` in bg_common). On WS73, the equivalent transport is HCC over USB (`hcc_cfg_comm.h` queues). The exact mapping between GLE's transport abstraction and WS73's HCC queue model (`SLE_DATA_QUEUE = 8`, `BSLE_MSG_QUEUE = 10`) is not directly visible.

3. **OHOS SLE Srv adaptation**: The `oh_sle_srv_*.c.obj` files in GLE suggest an OpenHarmony service adaptation layer. Is this the exact layer that runs inside the WS73 firmware, or is it a separate host-side component? The presence of both `sle_srv_*.c.obj` and `oh_sle_srv_*.c.obj` suggests two parallel service interfaces.

4. **GLE BLE coexistence**: The `bt/controller/bgtp/` directory contains `libbgtp.a` and `libbgtp_rom_data.a` — a BLE/GLE coexistence controller. On WS63, BLE and SLE share the radio; on WS73, the radio is dedicated to SLE. How the coexistence logic was stripped or adapted for WS73 is unknown.

5. **Low-latency data path**: `FEATURE_GLE_LOW_LATENCY` and the `gle_ssap_low_latency.c` module, plus `gle_tm_unicast_connect_req/rsp` for isochronous channels, suggest a dedicated low-latency path (IOG/ICB). Our WS73 hardware verification accepted `CREATE_IOB (0x2803)` and `SET_IOG_PARAM (0x2801)` — but the full low-latency data flow through our host driver is unimplemented.

6. **GLE QoS module**: The `gle_qos_*` symbols suggest per-connection QoS tracking (buffer counts, data flow control). On WS73, this is transparent to the host — the firmware handles flow control internally. But understanding the QoS model could help explain `DLI_SET_DATA_LEN (0x1804)` rejection patterns observed in our hardware tests.
