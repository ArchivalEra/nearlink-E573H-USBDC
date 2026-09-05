# WS73 BT GAP Audit — stack/ssap vs BLE host (TCID 0x1F / 0xA4 ICB, SM, low-latency, GATT)

**Issue:** `.scratch/rust-ws73-tri-mode/issues/02-bt-gap-audit.md:1` (research AFK, read-only)
**Date:** 2026-08-19
**Scope:** Contrast `driver/bsle/ble_driver/linux/ble_soc.ko` + `application/lib/*/libble_host.a` +
`application/sample/ble/ble_gatt_client` + `ble_uuid_server` (7-sample claim in ticket vs 2-sample
reality in `sdk/ws73_sdk_linux_WS73_1.10.110`) against `stack/ssap` (`hwsle_transport` only `0x0A`,
`ssap_link` only `0x14xx/0x18xx`, `feature_mgr` pure bitmask). Propose `hwsle_transport`
extension points and `libble_host.a` FFI binding plan.
**Constraint:** Read-only, no network/build/hardware. All claims carry `file:line` citations.
SD SDK root below means `sdk/ws73_sdk_linux_WS73_1.10.110`.

---

## 1. Sources and inventory

### 1.1 `stack/ssap` as shipped

```
stack/ssap/include/hwsle_transport.h:1   — transport adapter, defines TCID_SLE_CUTC=0x1F etc.
stack/ssap/include/ssap_link.h:1        — DLI connection helper, only 0x14xx/0x18xx
stack/ssap/include/feature_mgr.h:1      — heuristic bitmask, no driver calls
stack/ssap/include/ssap_codec.h:1       — SSAP PDU codec (0x01..0x14)
stack/ssap/include/ssap_pkt.h:1         — OHOS SSAP packet structs (Apache-2.0)
stack/ssap/include/ssap_server.h:1      — SSAP service table + dispatch
stack/ssap/src/hwsle_transport.c:1      — ACB 0x0A-only implementation
stack/ssap/src/ssap_link.c:1            — DLI 0x1401/0x1802/0x1804 state machine
stack/ssap/src/feature_mgr.c:1          — bitmask heuristic, RAM/peer gating
stack/ssap/src/ssap_codec.c:1           — encode/decode, little-endian
stack/ssap/src/ssap_server.c:1          — EXCHANGE_INFO / FIND / READ / WRITE / METHOD dispatch
stack/ssap/Makefile:1                   — builds libssap.a (44 lines)
docs/USB-PROTOCOL.md:1                  — HCC-over-USB 5-EP model, 0xA3/0xA4 framing
docs/SDK-INTEL.md:1, docs/DEVICE-INTEL.md:1
.scratch/nearlink-driver/lab-notes/BLE-WIFI-USERLAND-RESEARCH.md:1
.scratch/nearlink-driver/lab-notes/SLE-CONTROL-PLANE.md:1
.scratch/nearlink-driver/lab-notes/OHOS-SM-SECURITY.md:1
.scratch/nearlink-driver/lab-notes/OHOS-DLI-LAYER.md:1
```

### 1.2 SDK BLE side (what the ticket calls "fbb_ws63 BLE side" and SDK `ble_host`)

Ticket lists `sdk/.../driver/bsle/ble_driver/linux/ble_soc.ko` sources + `libble_host.a` +
`application/sample/ble/ble_gatt_client + ble_uuid_server (7 samples)`.

Reality on `sdk/ws73_sdk_linux_WS73_1.10.110`:

* `driver/bsle/ble_driver/linux/ble_host_hcc.c:1` (22 308 B) + `ble_host_hcc.h:1` + `Makefile:10` (`obj-m:=ble_soc.o`, `ble_soc-objs:=ble_host_hcc.o`) + prebuilt `ble_soc.ko` / `ble_soc.o` / `ble_host_hcc.o` — a **real BlueZ `hci_dev`** via `hci_alloc_dev` / `hci_register_dev` (`BLE-WIFI-USERLAND-RESEARCH.md:10` cites `ble_host_hcc.c:532,547-551,553`). Appears as `hci0` for stock BlueZ; no `/dev/hwble` on Linux (`BLE-WIFI-USERLAND-RESEARCH.md:13` cites `driver/bsle/ble_driver/android/bt_dev/bt_dev.c:323`).
* `driver/bsle/ble_driver/android/ble_host.c:1`, `ble_host.h:1`, `bt_dev/bt_dev.c:1` — Android-only vendor path; `BLE-WIFI-USERLAND-RESEARCH.md:14` notes `H2D_MSG_BT_OPEN=25/CLOSE=26` vs SLE `29`.
* `application/lib/*/libble_host.a` — 15 arch variants enumerated via `find application/lib -name libble_host.a`:
  `1155, 1156, 1156_ont, 1156_iot, 3516V610, 3518_usb, 3519d, 7205_usb, 7205_sdio, t23, A40I, rk3568, stm32mp157` (each `libble_host.a`, plus `t23/libble_host.so`). All are **ARM/MIPS** ELFs; `ar t .../7205_usb/libble_host.a` lists ~120 `.o` (e.g. `hci_core.c.o`, `smp_fsm.c.o`, `gap_ui.c.o`, `att_gatt.c.o`, `sdk_gatt.c.o`, `ble_core.c.o`); `nm` on `7205_usb/libble_host.a` reports `no symbols` / Thumb-only stripped objects — no dynamic export, static SDK.
* `application/sample/ble/ble_gatt_client/{ble_app.c,inc/ble_gatt_client.h:1,inc/ble_client_scan.h:1,src/ble_gatt_client.c:1,src/ble_client_scan.c:1}` and `ble_uuid_server/{ble_app.c,inc/ble_uuid_server.h:1,inc/ble_server_adv.h:1,src/ble_uuid_server.c:1,src/ble_server_adv.c:1}` — **2 samples**, not 7. Each `lib/ReadMe.md:1` is the `placeholder ReadMe` and `Makefile:1` links `${LIBDIR}/*.a` that does not exist for x86_64 (confirmed by `BLE-WIFI-USERLAND-RESEARCH.md:27-28`). Ticket's "7 ble_* samples" belongs to an older/BS21-era tree; `WS63 vs WS73` notes confirm the drift (`.scratch/nearlink-driver/lab-notes/WS63-VS-WS73.md:1`).

No `fbb_ws63/src/protocol/bt/host/bt/*` tree exists on disk (`ls .../fbb_ws63 2>&1` → "No such file"). Ticket's path is an OHOS / `communication_nearlink_service` or `open-spark`s reference; the recoverable analogue is `libble_host.a` + `OHOS-SM-SECURITY.md` (host SM in `cp/bsl/sle/sm/`).

---

## 2. What `stack/ssap` currently implements

### 2.1 `hwsle_transport` — SMTC-only (ticket's `hwsle_transport only 0x0A` confirmed)

* Defines `HCI_DATATYPE_ACB 0xA3`, `HCI_DATATYPE_ICB 0xA4`, `TCID_SLE_SMTC 0x0A`, `TCID_SLE_CUTC 0x1F`, `TCID_SLE_CMTC 0x02` (`stack/ssap/include/hwsle_transport.h:21-28`) — **definitions only**.
* API surface: `hwsle_transport_open:34`, `hwsle_transport_send_ssap:37` (hardwired to `TCID_SLE_SMTC`), `hwsle_transport_send_acb:40` (generic tcid arg exists but unused upstream), `hwsle_transport_send_hci_cmd:43` (opcode `0xA1`), `hwsle_transport_run:46`, `hwsle_transport_close:49`.
* Implementation: `stack/ssap/src/hwsle_transport.c:51-76` — `send_acb` builds `[0xA3][tcid u16 LE][len u16 LE][payload]` (`hwsle_transport.c:56-60`), `send_ssap` delegates to `TCID_SLE_SMTC` (`hwsle_transport.c:75`). `hwsle_transport_run:98-142` parses `[type][...]` frames but **only dispatches `tcid==0x0A`** (`hwsle_transport.c:125-126`: `if (tcid == TCID_SLE_SMTC && g_recv_cb) g_recv_cb(...)`). `0xA4 ICB` frames are not matched; the `HCI_DATATYPE_EVENT 0xA2` branch (`hwsle_transport.c:128-137`) advances by a heuristic `5+elen` without delivering to any callback. `CUTC 0x1F` is never sent or received. `g_fd` is a single global (`hwsle_transport.c:21`), no per-TCID queues, no reassembly for >MTU, no fragmentation.

### 2.2 `ssap_link` — only DLI connection-management (ticket's `ssap_link only 0x14xx/0x18xx` confirmed)

* Declares `DLI_CREATE_CONNECTION 0x1401`, `CANCEL 0x1402`, `DISCONNECT 0x1403`, `READ_REMOTE_VERSION 0x1802`, `SET_DATA_LEN 0x1804`, `CONNECTION_UPDATE 0x1807` (`stack/ssap/include/ssap_link.h:21-26`) and events `CMD_STATUS 0x0001`, `CMD_COMPLETE 0x0002`, `CONNECTION_COMPLETE 0x0015`, `DISCONNECTION_COMPLETE 0x0005` (`ssap_link.h:29-32`).
* Encodes `DLI_ConnectionCreateParam` (23 B, `ssap_link.c:49-67`: `version, localIndex, peerAddr[6], peerAddrType, connIntervalMin/Max, maxLatency, supervisionTimeout, min/maxCeLength, scanInterval/Window/Type, initiatePhys, filterPolicy, ownAddrType`) and sends `0x1401` (`ssap_link.c:69`). Post-connect it fires `0x1802` + `0x1804` (`ssap_link.c:176,178`). Disconnect/cancel use `0x1403`/`0x1402` (`ssap_link.c:91,101`). Watchdog + `find_cmd_echo:124-135` + `on_event:137-248` + `tick:250-295` cover supervision/retries (status `0x06`).
* **Missing opcodes:** no `0x0C02 SET_ADV_PARAMS` / `0x0C03 SET_ADV_DATA` / `0x0C04 SET_SCAN_RSP_DATA` / `0x0C05 SET_ADV_ENABLE` (advertising, `SLE-CONTROL-PLANE.md:24-26`), no `0x1001 SET_SCAN_PARAMS` / `0x1002 SET_SCAN_ENABLE` (`SLE-CONTROL-PLANE.md:26-27`), no `0x1C01/0x1C02` SM / `0x2001..0x2005` HADM/ranging / `0x2803/0x280D` ICB data-path (`SLE-CONTROL-PLANE.md:74-77`), no `0x1805 READ_PHY` / `0x1806 SET_PHY` / `0x180C READ_REMOTE_RSSI` (`SLE-CONTROL-PLANE.md:78`). All are present on hardware per `SLE-CONTROL-PLANE.md:74-88`.

### 2.3 `feature_mgr` — pure bitmask heuristic, no driver calls (ticket's wording exact)

* `stack/ssap/include/feature_mgr.h:23-35` defines `FEAT_SSAP_V1_0 .. FEAT_SM_SECURE` (10 bits, RAM costs annotated), `fm_capacity_t CAP_TINY..CAP_FULL` (`feature_mgr.h:38-43`), `fm_conditions_t {ram_free, connected, peer_version, peer_mtu, use_case}` (`feature_mgr.h:46-52`), `fm_t {capacity, enabled, wanted, on_change}` (`feature_mgr.h:62-69`).
* `stack/ssap/src/feature_mgr.c:2-121` implements `fm_init:33-40` + `fm_update:42-121`: capacity trim (`feature_mgr.c:51-64`), connection-state gating (`feature_mgr.c:66-75`), peer-version gating (`feature_mgr.c:78-79`), RAM-pressure drop-order (`feature_mgr.c:82-115`, order `RANGING, ICB, CONNECT, ADV, SCAN, SM_SECURE, DYN_TCID, V1_3, LOW_LAT`). **Zero calls** to `hwsle_transport` or DLI; `on_change` callback is the only side effect (`feature_mgr.c:118-119`). Consumers poll `fm_has:76` or check `enabled_features` in `ssap_server_apply_config:119-125` (which only clamps `version`/`mtu` in `ssap_server.c:569-581`). ADV/SCAN/ICB/LOW_LAT/RANGING remain phantom toggles.

### 2.4 Codec / server — SSAP only, not BLE GATT

* `stack/ssap/include/ssap_codec.h:19-40` covers all SSAP opcodes `0x01..0x14`; `ssap_codec.c:11-217` implements only `ssap_trans_type_of:11-41`, `ssap_encode_exchange_info:55-74`, `ssap_decode_exchange_info:76-94`, `ssap_encode_find_struct_req:96-116`, `ssap_encode_read_req:118-133`, `ssap_encode_write:135-151`, `ssap_encode_value:153-169`, `ssap_encode_error_rsp:171-183`, `ssap_encode_write_rsp:185-206`, `ssap_encode_value_ack:208-217`. No ATT/GATT PDU handling (cf. `libble_host.a:att_pdu.c.o`, `att_gatt.c.o`).
* `stack/ssap/include/ssap_server.h:16-22` caps tables (`HANDLE_START 0x0001`, `MAX_DESCRIPTORS 8`, `MAX_PROPERTIES 32`, `MAX_VALUE_LEN 1024`). `ssap_server:1-582` dispatches `EXCHANGE_INFO 0x02/0x03` (`ssap_server.c:172-187`), `FIND_STRUCTURE 0x04/0x06 → 0x05/0x07` (`ssap_server.c:188-334`), `READ 0x08 → 0x09` / `READ_BY_UUID 0x0A → 0x0B` (`ssap_server.c:335-433`), `WRITE 0x0C/0x0D → 0x0E` with CCCD auto-logic (`ssap_server.c:435-510`), `CALL_METHOD 0x12/0x13 → 0x14` (`ssap_server.c:512-546`), `notify VALUE_NTF/IND 0x0F/0x10` with `cccd_value` gating (`ssap_server.c:549-567`). No `GAP/GATT` service registry beyond SSAP `SERVICE_CHANGE 0x000E` (`ssap_server.c:33-47,74-87`), no CCCD persistence, no long-read/write fragmentation reassembly, no reliable-write / prep-queue.

---

## 3. BLE feature gap analysis

### 3.1 TCID `0x1F` CUTC (default unicast) + dynamic `0x80-0xDF` — missing

* **What it is:** On WS73 the SLE transport uses ACB datatype `0xA3` with a 2-byte TCID. `0x0A SMTC` carries SSAP; `0x1F CUTC` is the **default unicast channel** for application payload; `0x80-0xDF` are **dynamically allocated** unicasts (`feature_mgr.h:31` comment, `FEAT_DYN_TCID`). Hardware framing is `[0xA3][tcid u16 LE][len u16 LE][payload]` per `USB-PROTOCOL.md:37-40` (also `hwsle_transport.h:7-8`). `OHOS-DLI-LAYER.md:1` models the same via `HCI_DATA_TCID_POS 5`.
* **Gap:** Defined (`hwsle_transport.h:28`) but `ssap_link` never negotiates it, `hwsle_transport` never opens a CUTC receive path, and `ssap_server` only sends/receives on `0x0A` (`hwsle_transport.c:75,125`). No scatter/gather, no per-TCID flow control, no MTU negotiation for `0x1F` (cf. BLE `L2CAP LE CBFC` / `sdk_gatt.c` credit model in `libble_host.a:l2cap_core_ble.c.o,l2cap_gle.c.o`).
* **Needed:** TCID table, open/close verbs, CUTC MTU handshake, demux on `run()` by tcid, back-pressure.

### 3.2 `0xA4` ICB isochronous data path — missing

* **What it is:** `HCI_DATATYPE_ICB 0xA4` (`hwsle_transport.h:24`, `USB-PROTOCOL.md:42`) is the isochronous channel (used on SLE for audio/media or sensor streaming; on BLE for **CIS/BIS**). SDK confirms via `app/lib/*/libble_host.a` isochronous objects (`gap_period_adv.c.o`, `l2cap_gle.c.o`) and `SLE-CONTROL-PLANE.md:77` lists `SETUP_ICB_DATA_PATH 0x280D` / `CREATE_IOB 0x2803` as `accepted`. `BLE-WIFI-USERLAND-RESEARCH.md:44` notes no hard tri-mode exclusion, so ICB coexists with BLE/WiFi.
* **Gap:** `feature_mgr.h:32` / `feature_mgr.c:7,53,57,60,88` track `FEAT_ICB` as `+8 KB` but it has no DLI call. `hwsle_transport_run:98-142` has no `HCI_DATATYPE_ICB` branch; any `0xA4` frame falls through to `off++` (`hwsle_transport.c:137-139`) and is silently dropped. No `hwsle_transport_send_icb()`, no QoS params, no framing for `icb_data_type` / timestamps.
* **Needed:** `0xA4` send/recv, `0x280D`/`0x2803` wrappers, jitter/interval negotiation, loss concealment glue.

### 3.3 SM pairing / security (the ticket's `SM pairing` core) — missing

* **What the SDK provides:** Two independent SM stacks:
  * **BLE SM** inside `libble_host.a` (`smp_fsm.c.o`, `smp_ui.c.o`, `smp_aes.c.o`, `smp_ecc.c.o`, `hci_secu.c.o`) exposed to samples via `gap_ble_pair_remote_device` + `pair_result_cb` (`application/sample/ble/ble_gatt_client/src/ble_gatt_client.c:391-394,411-415` connect-state → `GAP_BLE_PAIR_NONE → gap_ble_pair_remote_device`; `ble_uuid_server.c:368-371` `pair_result_cbk`). GATT permission bits (`GATT_ATTRIBUTE_PERMISSION_READ/WRITE`, CCC checks) gate I/O (cf. `ble_uuid_server.c:147-148` descriptor perms).
  * **SLE SM** per `OHOS-SM-SECURITY.md:1` — host-side 7-state FSM (`sm_stm.c:54-62`, states `INIT→NEGO→AUTH→ENCP→FULL`), opaques `0x0133..0x0147` (`sm_struct.h:67-82`), 6 auth methods (NumCmp, JustWorks, Passcode, Password, PSK, OOB — `sm_auth.c:1`, `sm_numcmp.c:1`, `sm_noentry.c:1`, etc.), crypto registration (`Nlstk_SmRegAlgoFuncs`, `sle_crypto.h:1`), link-key derivation `LK = CMAC(dk||Ra||Rb||GAddr||TAddr)` (`sm_dhkey.c:333`).
* **What `stack/ssap` has:** `FEAT_SM_SECURE 1<<9` (`feature_mgr.h:34`, `+4 KB` in `feature_mgr.c:27`) with gating in `feature_mgr.c:72` (disabled unless connected) but **no SM state machine**, no `0x1Cxx` / `0x0133..` opcode handling, no crypto callbacks, no NV persistence. `ssap_server` does model permission checks (`ssap_server.h:43-44, permission field; ssap_server.c:455-462` error codes `FORBID_READ/WRITE`, `UNAUTHENTICATED 0x06` etc. in `ssap_pkt.h:186-211`) yet never calls `SmIsSLinkAuthComplete` / `SmIsSLinkEncryptComplete` (cf. `OHOS-SM-SECURITY.md:23` quoting `ssaps_server.c:339-351`). `bts_dev_manager_register_file_path` / NV persistence (`ble_gatt_client.c:548-554`, `ble_uuid_server.c:430-436`) has no counterpart.
* **Gap:** Entire pairing negotiation, DH key exchange, LTK/LinkKey derivation, encryption enable (`DLI_EnableEncryption`), re-pair, and per-handle auth/encrypt gates.

### 3.4 Low-latency (subrate / `CONNECTION_UPDATE` / PHY / DLE) — missing/incomplete

* **What the SDK provides:**
  * DLE: `gap_le_set_data_length` / `gap_ble_set_data_length` (`ble_gatt_client.c:312-320`: `maxtxoctets=GAP_MAX_TX_OCTETS, maxtxtime=GAP_MAX_TX_TIME`) — outside `0x1804`-only `ssap_link`.
  * PHY: `gap_le_set_phy` (`ble_gatt_client.c:300-310`: `2M` PHY) — `ssap_link` lacks `0x1805`/`0x1806` (`SLE-CONTROL-PLANE.md:78` shows them `accepted`).
  * Connection update: `DLI_CONNECTION_UPDATE 0x1807` is declared (`ssap_link.h:26`) but **never sent** by `ssap_link.c` (no `connection_update()` API; only supervision-timeout watchdog `tick:279-290`).
  * ICB/subrate for SLE (`CREATE_IOB 0x2803`) — absent.
* **What `stack/ssap` has:** `FEAT_LOW_LAT 1<<8 (+1 KB)` (`feature_mgr.h:33`, `feature_mgr.c:8,60,72,88,113`), `ssap_link_conn_param_t.maxLatency` is settable (`ssap_link.h:52`) but defaults to `0` (`ssap_link.c:58`), `connIntervalMin/Max` default `0x64 (=125 ms, ssap_link.h:50-51)` with no runtime adjustment, MTU defaults `517` in the sample (`ble_gatt_client.c:41`) vs `SSAP_MTU_DEFAULT 251` / `SSAP_MTU_MAX 1024` (`ssap_codec.h:60-61`).
* **Gap:** No DLE/PHY negotiation loop, no subrate/anchor-point control, no latency/interval re-negotiation after connect, no scheduling hint for WiFi/BT coex (`_PRE_WLAN_FEATURE_BTCOEX`, `BLE-WIFI-USERLAND-RESEARCH.md:21`).

### 3.5 GATT (client/server, discovery, ATT) — absent (SSAP ≠ GATT)

* **What the SDK provides:** Full GATT via `libble_host.a` (`att_gatt.c.o`, `att_pdu.c.o`, `att_table.c.o`, `g_sdk.c.o`, `sdk_gatt.c.o`, `sdk_gatt_hlp.c.o`, `sapi_ble_gatt.c.o`) exposed as `gattc_*` / `gatts_*` (`ble_gatt_client.c:76-88,170-260,285-289,292-297`: `gattc_write_cmd`, `gattc_discovery_service/character/descriptor`, `gatts_add_service/characteristic/descriptor`, `gatts_notify_indicate` in `ble_uuid_server.c:485-503`). Plus GAP adv/scan (`gap_ble_set_adv_data/param/start_adv`, `gap_ble_set_scan_parameters/start_scan` in `ble_server_adv.c:133-197` and `ble_client_scan.c:17-31`).
* **What `stack/ssap` has:** SSAP only — `EXCHANGE_INFO 0x02/0x03` (`ssap_codec.c:55-94`), `FIND 0x04/0x06`, `READ 0x08/0x0A`, `WRITE 0x0C/0x0D`, `VALUE_NTF/IND 0x0F/0x10`, `VALUE_ACK 0x11` (`ssap_codec.c:153-217`), `CALL_METHOD 0x12/0x13`, `ERROR_RSP 0x01` (`ssap_pkt.h:121-143`). `ssap_server:1-567` is a minimal OHOS `ssaps_server.c` port without SMP-gated permissions. `hwsle_transport` has no GAP (adv/scan) verb and `ssap_link` has no adv/scan verbs (`ssap_link.h:21-26` is connection-only).
* **Gap:** No GATT client/server, no ATT bearer over L2CAP `0x0004`, no service-change / hash indiction beyond the canned `0x000E` ntf (`ssap_server.c:33-46,74-87`), no 128-bit UUID base handling (SSAP base `0x37BEA880-FC70-11EA-B720-000000000000` in `ssap_codec.h:91` vs BLE base), no prepare/long writes, no flow-control.

---

## 4. `hwsle_transport` extension points (incremental, AFK-safe)

### 4.1 Frame-type & TCID demux expansion

*Current:* `hwsle_transport.c:120-139` discriminates `HCI_DATATYPE_ACB 0xA3` and `0xA2 EVENT`, drops everything else.
*Extension point A — ICB path:* Add `case HCI_DATATYPE_ICB:` parsing `[0xA4][tcid u16][len u16][payload]` (same layout as ACB; `USB-PROTOCOL.md:42` + `SLE-CONTROL-PLANE.md:77` confirm `0xA4` acceptance). New API `hwsle_transport_send_icb(uint16_t tcid, ...)` and `hwsle_transport_register_icb_cb(icb_recv_fn)` alongside `hwsle_transport.h:30-31` `ssap_recv_fn`. Back-end: `write` path mirrors `send_acb:51-71` with `HCI_DATATYPE_ICB`. Tests can inject `0xA4` frames without hardware (like `test/test_link.c:52-180` injects events via `on_event`).

*Extension point B — multi-TCID ACB:* Generalize `tcid==0x0A` (`hwsle_transport.c:125`) into a table `struct tcid_route {uint16_t tcid; void(*cb)(uint8_t*,size_t);}` covering `TCID_SLE_SMTC 0x0A` (`hwsle_transport.h:27`), `TCID_SLE_CUTC 0x1F` (`hwsle_transport.h:28`), `TCID_SLE_CMTC 0x02` (`hwsle_transport.h:26`) and dynamic `0x80..0xDF` (`feature_mgr.h:31`). `send_ssap:75` stays `0x0A`; new `send_cutC` / `send_dyn_tcid` use the same header builder `put_u16:24-29`. `run:98-142` loops with `off+5+len` bounds (already in `hwsle_transport.c:123-124`) — add reassembly for `len` spanning multiple `read()`s (256 B MTU vs `buf[2048]`).

### 4.2 HCI command / event completeness

*Current:* `hwsle_transport_send_hci_cmd:78-96` sends `[0xA1][opcode][plen][params]`; `run` discards most `0xA2` payload.
*Extension point C — adv/scan verbs:* Add typed wrappers `hwsle_transport_send_adv_cmd(opcode∈{0x0C02,0x0C03,0x0C04,0x0C05}, ...)` and `scan_cmd({0x1001,0x1002}, ...)` that build the 49 B `DLI_AdvParam` (`SLE-CONTROL-PLANE.md:38-49`) / 8 B `DLI_ScanParam` (`SLE-CONTROL-PLANE.md:32-37`) explicitly (today `ssap_link` only builds the 23 B connect blob `ssap_link.c:49-67`). Expose the framing confirmation loops documented in `SLE-CONTROL-PLANE.md:59-65,83-88` (fragmentation for `0x0C03` >MTU, status-byte-vs-return-data correction).

*Extension point D — event router:* Parse `0xA2` as `[0xA2][02 00][plen u16][event_struct]` (`SLE-CONTROL-PLANE.md:14-16`) and fan out to `ssap_link_on_event:137` (already handles `0x0001/0x0002/0x0015/0x0005`) plus new handlers for SM `0x1Cxx` and HADM `0x2001..` / ICB `0x280x` (all `accepted` per `SLE-CONTROL-PLANE.md:74-88`).

### 4.3 Back-pressure, poll, and lifecycle

* Current `poll 500 ms` + blocking `read` (`hwsle_transport.c:105,113`) is chatty and single-FD (`g_fd:21`, `g_recv_cb:22`). Issues: no `EPOLLIN|EPOLLERR`, no partial-frame carry-over, no `SIGHUP` handling, `hcc_adapt_bt_alloc/free` alignment absent (cf. `ble_host_hcc.c:62-113` 32-byte sdio alignment).
* Extension point E: Switch `run` to `epoll`/`poll` with carry-over `tail[]` buffer, add `hwsle_transport_set_nonblock`, add `hwsle_transport_fd()` for callers to own the loop (mirrors BlueZ `hci_dev->send` setup `ble_host_hcc.c:547-551`). Gate `pm_ble_enable` / `pm_sle_enable` lifecycle (`BLE-WIFI-USERLAND-RESEARCH.md:16-20`: first service drives firmware download; reload via `hcc_usb_reload()`).

### 4.4 What *not* to change without `feature_mgr` wiring

Any new verb must be gated by `feature_mgr` — today `fm_update:42-121` is pure state. Add a thin `fm_notify_adv/scan/icb/low_lat` helper that turns `enabled` bits (`feature_mgr.h:27-33`) into **actual** `send_hci_cmd` calls (currently none). `ssap_server_apply_config:569-581` shows the pattern for `version`/`mtu`; replicate for transports.

---

## 5. `libble_host.a` FFI binding plan (x86, AFK-safe, Rust-ready)

### 5.1 Why FFI is the only sane path

* The BLE host is **closed-source static lib** (ARM/MIPS only, ~120 `.o`, stripped `nm` output per §1.2). Re-implementing `smp_fsm` / `att_gatt` / `gap_ui_le` from scratch duplicates ~30 kLOC and the 7-state SLE SM (`OHOS-SM-SECURITY.md:15-18` lists `sm.c:710`, `sm_stm.c:653`, `sm_nego.c:436`…). Shipping `ble_soc.ko` gives `hci0` for free via `hci_register_dev` (`ble_host_hcc.c:553`) — BlueZ can drive BLE without `libble_host.a` at all (`BLE-WIFI-USERLAND-RESEARCH.md:42-43`: stock `bluetoothd` path). For SLE, the host **must** be rebuilt for x86 because `libble_host.a` is arch-locked; but the BLE side demonstrates the architecture to reuse: keep `ble_soc.ko` for BLE, layer SSAP/SLE via `stack/ssap` on `/dev/hwsle` (`docs/USB-PROTOCOL.md:55-58`).

### 5.2 Targets inside `libble_host.a` to bind (priority order)

All via `ar t .../7205_usb/libble_host.a` object names; approximate APIs from samples + OHOS notes:

| Priority | Object(s) | API family | Sample evidence | Use in `stack/ssap` |
|---|---|---|---|---|
| P0 | `gap_ui.c.o`, `gap_ui_le.c.o`, `gap_period_adv.c.o`, `sapi_ble_adv.c.o`, `sapi_ble_gap.c.o` | `gap_ble_set_adv_data/param/start_adv`, `gap_ble_set_scan_parameters/start_scan`, `gap_ble_connect_remote_device`, `gap_ble_pair_remote_device`, `gap_ble_set_phy/set_data_length` | `ble_server_adv.c:133-197` / `ble_client_scan.c:17-31` / `ble_gatt_client.c:300-394` | Replace/augment `ssap_link` 0x14xx/0x18xx with typed GAP; wiring for `feature_mgr FEAT_ADV/FEAT_SCAN` |
| P0 | `smp_ui.c.o`, `smp_fsm.c.o`, `smp_aes.c.o`, `smp_ecc.c.o`, `hci_secu.c.o`, `gap_io_security.c.o` | `smp_*`, `NLSTK_SmRegAlgoFuncs`, `Crypto_* (Rand, PubPriKey, SecKey, DerivedKey, Sha256)` | `OHOS-SM-SECURITY.md:11-18` (`sm_algos.c:29`, `sle_crypto.h:53`) | Implement `FEAT_SM_SECURE` — host FSM `INIT→NEGO→AUTH→ENCP→FULL` per `OHOS-SM-SECURITY.md:28` |
| P1 | `att_gatt.c.o`, `att_pdu.c.o`, `att_table.c.o`, `sdk_gatt.c.o`, `sapi_ble_gatt.c.o`, `g_sdk.c.o` | `gatts_add_service/characteristic/descriptor`, `gatts_notify_indicate`, `gattc_discovery_service/character/write_cmd`, `gattc_exchange_mtu_req` | `ble_gatt_client.c:76-296` / `ble_uuid_server.c:119-503` | Provide BLE GATT alongside SSAP; template for any SSAP→GATT bridging |
| P2 | `l2cap_core*.c.o`, `l2cap_gle.c.o`, `l2cap_signal*.c.o` | L2CAP LE credit/CBFC | `libble_host.a:l2cap_core_ble.c.o` etc. | CUTC / dynamic TCID flow-control model |
| P3 | `nv_*.c.o`, `sdk_mem/list` | NV persistence `bts_dev_manager_register_file_path` | `ble_gatt_client.c:548-554` / `ble_uuid_server.c:430-436` | LTK/addr persistence for SM |

### 5.3 Binding approach (Rust `*-sys` crate, no hardware needed to author)

1. **Header extraction** — Collect the public SDK headers that accompany `libble_host.a` on a real SDK installation: `bts_def.h`, `bts_gatt_stru.h`, `bts_gatt_client.h`, `bts_gatt_server.h`, `bts_le_gap.h`, `bts_device_manager.h`, `errcode.h`, `sle_crypto.h` (referenced in `ble_gatt_client.c:11-14` / `ble_uuid_server.c:16-18`). They are not checked in (whitelist `.gitignore`); the `ReadMe.md` in each `application/lib/*:1` confirms the stub. Author `bindgen` input from a snapshot of these headers versioned under `.scratch/` (whitelist allowed for `sources/docs` per `AGENTS.md`). Mark everything `#[repr(C)]`, map `errcode_t → i32`, `bd_addr_t→[u8;6]` (`ble_gatt_client.c:67-72` shows `bd_addr_t {type, addr[6]}`).

2. **Shim for OSAL/HCC** — `libble_host.a` pulls `osal_*` (`osal_mutex.c.o`, `osal_task.c.o`, `osal_timer.c.o`, `osal_addr.c.o` per `ar t` list) and `hcc_adapt_*` (`ble_host_hcc.c:62-113`: `hcc_adapt_bt_alloc/free`, `hcc_bt_rx_proc`, `g_hcc_ble_adapt`). For an x86 `*-sys` build **without** rebuilding `libble_host.a`, provide tiny C shims that satisfy link (stub `osal_kmalloc → malloc`, `osal_kthread_create → pthread_create` — already mapped in the objects per `nm` tails: `U pthread_create`, `U malloc`, `U timerfd_create`). The real SLE transport on x86 will not be `libble_host.a` at all — it will be `stack/ssap` on `/dev/hwsle`; the `libble_host.a` FFI is only for **BLE** reference and for compiling the SLE SM crypto callbacks (`Crypto_RandNumGenerate` etc. in `OHOS-SM-SECURITY.md:13`).

3. **BLE path (reuse kernel, not the lib)** — As `BLE-WIFI-USERLAND-RESEARCH.md:1-16,42-43` proves, `ble_soc.ko` already exposes a standard `hci0`. The FFI plan for BLE is therefore **optional**: link `libble_host.a` only if the product wants the HiSilicon GAP/GATT API surface; otherwise drive `hci0` via `btleplug` / `bluer` / raw HCI (no FFI). Document this as the default in the Rust workspace (`wayfinder` ticket 04/05).

4. **SLE SM FFI (the hard one)** — Extract `nlstk_sm_api.h:1-570` + `sle_crypto.h:1-53` + `sm_struct.h:67-82,309` from OHOS `communication_nearlink_service` (mirrored in `OHOS-SM-SECURITY.md:5-10`). Generate Rust `sm-sys` with `bindgen`, wire `Crypto_*` callbacks to Rust `ring`/`p256` (or keep the C `smp_aes_comm.c.o:smp_ecc.c.o` implementations by linking those objects only — they are in the archive and `nm` shows they are self-contained). The FSM `sm_stm.c:653` + `sm_nego.c:436` / `sm_auth.c:102` etc. would be **ported** to Rust, not FFI'd verbatim, because the chip does not implement the FSM (`OHOS-SM-SECURITY.md:18-22`: "host-side, 100%").

5. **Build gating** — `Cargo.toml` feature `ble-host-ffi` (default off) that:
   * enables `bindgen` for `libble_host.a` headers,
   * links `application/lib/<arch>/libble_host.a` only when `CARGO_CFG_TARGET_ARCH` matches (e.g. `aarch64`), otherwise stubs with `#[cfg(not(target_arch="..."))] compile_error!` guidance to use `hci0`/SSAP instead — matching the SDK reality that no x86 `libble_host.a` exists (`BLE-WIFI-USERLAND-RESEARCH.md:27-28`).

6. **Verification without hardware** — The whole `stack/ssap` test pattern (`test/test_link.c:52-180`, `test/test_codec.c:1-117`, `test/test_feature.c:1-98`, `test/test_server.c:1-290`) injects PDUs/events directly; reuse for FFI: write `ssap_ssap_transport_icb_test`-style unit tests that craft `[0xA4][tcid][len]` frames and `[0xA2][event]` blobs and assert callbacks, exactly as `ssap_link_on_event:137-248` is tested today. `cargo test --features ble-host-ffi` can run on x86 even though `ble_soc.ko` is absent.

### 5.4 Minimal Rust crate sketch

```
crates/
  hwsle-transport-sys/   # bindgen of stack/ssap/include/hwsle_transport.h:1 + ssap_link.h:1
  ble-host-sys/          # bindgen of bts_* headers, links libble_host.a on aarch64 only
  sm-sys/                # bindgen of nlstk_sm_api.h + sle_crypto.h (OHOS snapshot)
  sm-fsm/               # Rust port of sm.c:710 + sm_stm.c:653 + sm_nego.c:436 state machine
  ssap/                 # existing stack/ssap (lifted to Rust or kept as C via hwsle-transport-sys)
```

Link order: `sm-sys` provides `Crypto_*` statics; `sm-fsm` registers them via `NLSTK_SmRegAlgoFuncs` (`OHOS-SM-SECURITY.md:17-20`); `hwsle-transport-sys` supplies `0xA3/0xA4` I/O; `ble-host-sys` is optional and only built on the matching arch.

---

## 6. Consolidated gap matrix (what is missing in `stack/ssap`)

| Feature (ticket wording) | Expected (SDK / spec) | `stack/ssap` today | Gap severity | Fix location |
|---|---|---|---|---|
| `TCID 0x1F CUTC` multi-channel | `hwsle_transport.h:28` + `USB-PROTOCOL.md:42` | defined, never sent/recvd (`hwsle_transport.c:75,125`) | High | §4.1 B + TCID table |
| `0xA4 ICB` isochronous (`FEAT_ICB`) | `hwsle_transport.h:24`, `SLE-CONTROL-PLANE.md:77` (`0x2803/0x280D accepted`) | dropped (`hwsle_transport.c:137-139`), `feature_mgr.c:88` phantom `8 KB` | High | §4.1 A |
| `FEAT_DYN_TCID 0x80-0xDF` | `feature_mgr.h:31` / `ssap_server` needs alloc | bitmask only (`feature_mgr.c:53-60`) | Medium | §4.1 B |
| `SM pairing` (BLE + SLE) | `libble_host.a:smp_fsm.c.o` + `OHOS-SM-SECURITY.md:5-22` (opcodes `0x0133..0x0147`, 6 auth methods, LTK/LK) | `FEAT_SM_SECURE` bitmask only (`feature_mgr.h:34`, `feature_mgr.c:27`) + error codes in `ssap_pkt.h:193-196` unused | **Critical** | §4.2 D + §5.4 `sm-fsm` |
| Low-latency (`LOW_LAT`, subrate, `CONNECTION_UPDATE 0x1807`) | `ssap_link.h:26` + `SLE-CONTROL-PLANE.md:74` / `ble_gatt_client.c:300-320` PHY/DLE | declared never sent, `feature_mgr.c:60,72` gates `FEAT_LOW_LAT 1<<8` | Medium | §4.2 C + DLE/PHY loop |
| `ADV/SCAN` (`0x0Cxx`, `0x1001/0x1002`) | `ble_server_adv.c:133-197` + `SLE-CONTROL-PLANE.md:24-27` | absent; `feature_mgr.h:27-28` phantom `FEAT_ADV/FEAT_SCAN` | **Critical** | §4.2 C |
| `GATT` client/server | `ble_gatt_client.c:170-297` / `ble_uuid_server.c:119-503` (`gattc_/gatts_*`) | SSAP only (`ssap_codec.c:11-217`, `ssap_server:512-546`) | High | §5.2 P1 |
| `RANGING / HADM` (`FEAT_RANGING`, `0x2001..0x2005`) | `SLE-CONTROL-PLANE.md:74` (`READ_MEASURE_CAPS accepted`) | phantom `FEAT_RANGING 1<<5 (+16 KB)` (`feature_mgr.h:30`, `feature_mgr.c:69,87`) | Low* | deferred (needs IQ buffers) |
| Persistence / NV | `ble_gatt_client.c:548-554` / `OHOS-DEVICE-MGR.md:1` | none | Medium | §5.2 P3 |
| MTU / version negotiation | `ssap_codec.c:55-94` + `ble_gatt_client.c:41,335-347` (`mtu 517`, `2M PHY`) | `mtu 251/1024` (`ssap_codec.h:60-61`), `version min` (`ssap_server.c:178`) | Low | tune `ssap_server_apply_config:569-581` |

\* `RANGING` is `Low` because `HADM` is scheduled behind pairing/ICB in the Wayfinder map (`map.md:1`).

---

## 7. Recommendations for `rust-ws73-tri-mode` Wayfinder

1. **Ticket 02 (this audit) → downstream tickets:** File a `hwsle-ext` ticket for §4.1 A/B (`0xA4` + multi-TCID) with acceptance criteria "inject `0xA4` frame → cb fires; `send_cutc(0x1F, ...)` round-trips" and a `sm-bringup` ticket for the FFI/port plan §5.4.
2. **Do not block on `libble_host.a` for BLE:** Keep `BLE = stock BlueZ over hci0` (`BLE-WIFI-USERLAND-RESEARCH.md:42`) as the default; `ble-host-sys` stays `optional` (arch-gated) — this unblocks `rust-ws73-tri-mode` on x86 without rebuilding the closed lib.
3. **Gate every new DLI verb by `feature_mgr`:** Before any `0x0Cxx`/`0x10xx`/`0x280x` call, check `fm_has(FEAT_*)` (`feature_mgr.h:76`) and route through a single `hwsle_transport_send_hci_cmd:43` wrapper that logs the why (mirrors `OHOS-CM-LAYER.md:1` CM layer).
4. **Snapshot OHOS headers** for `sm-sys` — `BLE-WIFI-USERLAND-RESEARCH.md:28` and the missing `fbb_ws63` tree confirm the SDK will not ship them; vendoring the OHOS `cp/bsl/sle/sm/include/nlstk_sm_api.h:1` + `nai/crypto/include/sle_crypto.h:1` snapshot is the only reproducible path.

---

## 8. File inventory for reviewers

All paths absolute under `/home/archivalera/plum/zcode-projects/nearlink`.

* `stack/ssap/include/hwsle_transport.h:1`, `src/hwsle_transport.c:1` — primary `0x0A`-only evidence.
* `stack/ssap/include/ssap_link.h:1`, `src/ssap_link.c:1` — `0x14xx/0x18xx` only.
* `stack/ssap/include/feature_mgr.h:1`, `src/feature_mgr.c:1` — bitmask without driver calls.
* `stack/ssap/include/ssap_server.h:1`, `src/ssap_server.c:1` — SSAP vs GATT scope.
* `sdk/ws73_sdk_linux_WS73_1.10.110/driver/bsle/ble_driver/linux/ble_host_hcc.c:1`, `ble_host_hcc.h:1`, `Makefile:1` — `ble_soc.ko` is `hci0`.
* `sdk/.../application/lib/7205_usb/libble_host.a` + `ar t` — stripped static host (ARM/MIPS only).
* `sdk/.../application/sample/ble/ble_gatt_client/src/ble_gatt_client.c:1` (`:391,411,300,548`), `inc/ble_gatt_client.h:1`, `src/ble_client_scan.c:1`, `ble_uuid_server/src/ble_uuid_server.c:1` (`:368,430,485`), `inc/ble_uuid_server.h:1`, `src/ble_server_adv.c:1` (`:133`) — 2-sample gap vs ticket's 7.
* `docs/USB-PROTOCOL.md:1`, `.scratch/nearlink-driver/lab-notes/BLE-WIFI-USERLAND-RESEARCH.md:1`, `SLE-CONTROL-PLANE.md:1`, `OHOS-SM-SECURITY.md:1`, `OHOS-DLI-LAYER.md:1` — framing and SM reference.
* Ticket: `.scratch/rust-ws73-tri-mode/issues/02-bt-gap-audit.md:1`.

---

*English-only per `AGENTS.md` / `docs/agents/domain.md`. Whitelist `.gitignore` respected — no binaries committed.*
