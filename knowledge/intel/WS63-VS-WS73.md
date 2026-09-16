---
type: intel
title: "WS63 (fbb_ws63) vs WS73 — Asset Transferability and Interconnect Assessment"
language: zh
created: 2026-08-17
tags: [intel, ws63, ws73, asset]
sources:
  - "/mnt/hdd/nearlink-stuff/fbb_ws63"
trust: B
stale_after: 2027-02-17
---

# WS63 (fbb_ws63) vs WS73 — Asset Transferability and Interconnect Assessment

**Date:** 2026-08-17
**Author:** research subagent (ArchivalEra NearLink)

## Sources

- fbb_ws63 (HiHope/润和 WS63 SDK): `/mnt/hdd/nearlink-stuff/fbb_ws63/`
  - SLE public headers: `src/include/middleware/services/bts/sle/*.h`
  - SLE samples: `src/application/samples/bt/sle/{sle_uuid_server,sle_uuid_client,sle_speed_server,sle_speed_client}/`
  - Boards: `vendor/{BearPi-Pico_H3863,HiHope_NearLink_DK_WS63E_V03}/`
  - Build/chip config: `src/build/config/target_config/ws63/`
  - Prebuilt device-side gle host: `src/protocol/bt/host/gle/ws63-liteos-app/libbth_gle.a`
- WS73 SDK (host-side): `sdk/ws73_sdk_linux_WS73_1.10.110/`
  - Host SLE API headers: `include/bsle/sle/*.h`
  - Kernel SLE driver: `driver/bsle/sle_driver/{sle_hcc,sle_chba,sle_dev}/`
  - HCC config: `driver/platform/drv/device/romable/include/hcc_cfg_comm.h`
  - GLE HCI opcode DB (device firmware side): `build/config/hso/database_es0/cco/system/diag/bt_status_hso_msg_struct_def.txt`
  - Prebuilt host stack: `application/lib/7205_usb/libsle_host.a`
- Our stack: `stack/ssap/` (`include/ssap_pkt.h`, `include/ssap_link.h`, `include/hwsle_transport.h`)
- Prior note: `.scratch/nearlink-driver/lab-notes/BS21-WS63-SDK-COMPARISON.md`

---

## 1. Chip positioning: WS63/WS63E vs WS73

**WS63 is a standalone SoC (full host MCU); WS73 is a radio subcard driven by an external host.**

| | WS63 (Hi3863) | WS73 |
|---|---|---|
| Role | Integrated 2.4GHz WiFi6+BLE+SLE **主控 SoC** | Radio/combo **device**, protocol stack runs on the host |
| CPU | Hi3863 RISC-V 32-bit, up to **240 MHz**, FPU, SWD | None exposed — ROM firmware blob (`firmware/e/ws73.bin`) |
| Memory | SRAM **606 KB**, ROM 300 KB, in-package **4 MB Flash** | N/A (firmware-loaded RAM; host owns all state) |
| OS | LiteOS / OpenHarmony light (in-chip) | Linux / Android on the USB host |
| Stack location | Full gle host **in chip** (prebuilt `libbth_gle.a`) | Host: `sle_soc.ko` kernel + userspace `libsle_host.a`/`sparklinkd` |
| Radio | SLE 1M/2M/4M, max 12 Mbps air rate | same SLE radio, same GLE HCI dialect |

Evidence:
- `fbb_ws63/vendor/BearPi-Pico_H3863/README.md:8` — "2.4GHz Wi-Fi6、BLE、SLE为主控芯片...内置 SRAM 和合封 Flash"
- `fbb_ws63/vendor/BearPi-Pico_H3863/README.md:60-64` — Hi3863 RISC-V 32bit CPU max 240MHz; SRAM 606KB / ROM 300KB / 4MB Flash; BLE up to 2 Mbps; SLE up to 12 Mbps
- `fbb_ws63/vendor/BearPi-Pico_H3863/README.md:88` — "主控芯片 Hi3863, RISC-V 32bit, 合封4MB Flash, WiFi/SLE/BLE 多模并发"
- `fbb_ws63/src/build/config/target_config/ws63/ws63.json:3` — `"chipName": "ws63"`, `"seriesName": "cfbb"`; toolchain is RISC-V (`config.py:125` `CONFIG_PMP_USING_RISCV_31`, `ws63.json` analysis path `cc_riscv32_musl`)
- WS73 role: `docs/SDK-INTEL.md:3-4` ("host-side Linux driver SDK... host CPU drives the WS73 NearLink chip over USB/SDIO/UART via the HCC bus"), `docs/SDK-INTEL.md:59` (host daemon `sparklinkd`), `docs/DEVICE-INTEL.md:44` (boot-stage 2-EP device that re-enumerates after firmware download)

WS63 vs WS63E: both boards build as `"chip": "WS63"` (`vendor/*/build_config.json`); WS63E board (`HiHope_NearLink_DK_WS63E_V03`) is a WS63-partitioned reference board, not a different die. Chip family name is "Hi3863" (BearPi README).

---

## 2. API same-origin: one HiSilicon FBB/SLE API family

The two SDKs share the **same SLE middleware API**. WS73's `include/bsle/sle/` is a byte-near-identical copy of fbb_ws63's `src/include/middleware/services/bts/sle/`. Only diffs are copyright headers, `errcode.h`→`sle_errcode.h` include, and small additive enums/callbacks on the WS73 side.

Header diff lines (fbb_ws63 vs WS73 `include/bsle/sle/`):

| header | diff lines | substantive deltas (WS73 adds) |
|---|---|---|
| `sle_common.h` | 4 | none |
| `sle_ssap_stru.h` | 4 | none |
| `sle_errcode.h` | 4 | none (identical errcode values) |
| `sle_ota.h`, `sle_low_latency.h`, `sle_glp_manager.h`, `sle_hadm_manager.h`, `sle_factory_manager.h` | 4 each | none |
| `sle_connection_manager.h` | 24 | +`SLE_DISCONNECT_UNKNOWN/ERR_PIN_OR_KEY_MISSING/ERR_CONN_TIMEOUT/CONN_TERMINATE_BY_MIC_ERROR/CONN_FAIL_TO_ESTABLISH`; +`SLE_PHY_PILOT_DENSITY_NO`; drops `sle_low_latency.h` include |
| `sle_device_discovery.h` | 19 | +`SLE_MCS_00..` modulation enum |
| `sle_ssap_server.h` | 53 | +`sle_indication_cfm_result_t`, +`ssaps_indicate_cfm_callback` |
| `sle_ssap_client.h` | 8 | none (comment-only) |

Function signatures are **identical** between fbb_ws63 and WS73 `include/bsle/sle/`:

- Server (both 15 funcs): `ssaps_register_server` (`sle_ssap_server.h:454` fbb / `:468` WS73), `ssaps_add_service_sync` (`:574`/`:584`), `ssaps_add_property_sync` (`:601`/`:609`), `ssaps_add_descriptor_sync` (`:629`/`:635`), `ssaps_start_service` (`:651`/`:657`), `ssaps_notify_indicate` (`:731`/`:731`), `ssaps_notify_indicate_by_uuid` (`:763`/`:761`), `ssaps_set_info` (`:787`/`:783`), `ssaps_register_callbacks` (`:808`/`:802`)
- Client (both 9): `ssapc_register_client` (`sle_ssap_client.h:479`/`:495`), `ssapc_find_structure` (`:524`/`:538`), `ssapc_read_req` (`:574`/`:588`), `ssapc_write_req` (`:597`/`:611`), `ssapc_write_cmd` (`:622`/`:634`), `ssapc_exchange_info_req` (`:645`/`:657`), `ssapc_register_callbacks` (`:666`/`:676`)
- Connection: `sle_connect_remote_device` (`sle_connection_manager.h:585`/`:600`), `sle_disconnect_remote_device` (`:604`/`:619`), `sle_pair_remote_device` (`:665`/`:676`), `sle_set_phy_param` (`:837`/`:859`), `sle_set_mcs` (`:860`/`:880`), `sle_connection_register_callbacks` (`:921`/`:935`)
- Discovery: `enable_sle` (`sle_device_discovery.h:572`/`:655`), `sle_set_announce_param` (`:745`/`:826`), `sle_start_announce` (`:766`/`:847`), `sle_set_seek_param` (`:808`/`:889`), `sle_start_seek` (`:827`/`:910`), `sle_announce_seek_register_callbacks` (`:863`/`:946`)

Confirmed by prebuilt symbol tables: WS73 `application/lib/7205_usb/libsle_host.a` exports 1485 T-symbols incl. `ssaps_register_server`, `ssaps_add_service`, `ssaps_notify_indicate`, `ssapc_exchange_info_req`; WS63 `libbth_gle.a` exports 954 T-symbols incl. `ssaps_add_property`, `ssaps_add_service_sync`, `ssaps_notify_indicate`, `ssapc_find_structure`, `ssapc_write_req` — the same HiSilicon gle-host implementation, one in chip (WS63) and one linked on host (WS73).

**Conclusion: our `stack/ssap/` functions (e.g. `ssap_server_add_service/add_property` in `ssap_server.h:85-90`) are a slim host-side re-implementation of exactly the device-side `ssaps_*` contract the WS63 board exposes.**

---

## 3. Protocol layer consistency

### 3.1 SSAP wire protocol (service layer)

Neither SDK ships the full SSAP PDU structs in headers — the PDU layer lives inside the prebuilt gle-host libraries on both sides. Public constants, however, are shared and match our OHOS-derived `ssap_pkt.h`:

- `sle_ssap_stru.h` (fbb and WS73 identical): `SSAP_PERMISSION_READ/WRITE/ENCRYPTION_NEED/AUTHENTICATION_NEED/AUTHORIZATION_NEED` (`sle_ssap_stru.h:52-63`), `SSAP_FIND_TYPE_PRIMARY_SERVICE=0x01`/`PROPERTY=0x03`/`METHOD=0x04`/`EVENT=0x05` (`:72-85`), `SSAP_PROPERTY_TYPE_VALUE=0x00`, `SSAP_DESCRIPTOR_CLIENT_CONFIGURATION=0x02` (`:94-109`), `SSAP_OPERATE_INDICATION_*` bits incl. `BROADCAST=0x20` (`:118-137`), `sle_uuid_t` 16B (`:146-151`), `ssap_exchange_info_t{mtu_size u32, version u16}` (`:160-165`).
  - Matches `stack/ssap/include/ssap_pkt.h`: find types (`SSAP_FIND_PRIMARY_SERVICE` via `SSAP_FIND_TYPE_PRIMARY_SERVICE` in `ssap_codec.h`), operation bits `SSAP_OP_*`, `ssap_exchange_info_t` semantics.
- SSAP error space identical on both sides: `ERRCODE_SLE_SSAP_BASE 0x80006100`, `ERRCODE_SSAP_INVALID_PDU=+0x01` ... `ERRCODE_SSAP_VALUE_OUT_OF_RANGE=+0x0F` (`sle_errcode.h:38,125-172`, byte-identical between fbb_ws63 and WS73 `include/bsle/sle/sle_errcode.h`). These are the peer's `ERROR_RSP` payload codes; ours (`SSAP_ERRCODE_*`, `ssap_pkt.h:188-211`) line up.
- Sample constants cross-check: test service UUID `0xABCD`, property `0x1122`, base UUID `0x37BE...` (`sle_uuid_server.h:18,21`; `sle_uuid_server.c:46`); client uses `SSAP_FIND_TYPE_PRIMARY_SERVICE`, start_hdl 1, end_hdl 0xFFFF (`sle_uuid_client.c:107-111`); server notifies with `SSAP_PROPERTY_TYPE_VALUE` (`sle_uuid_server.c:199`).

Prior note `BS21-WS63-SDK-COMPARISON.md` already established the same conclusion across BS2x/WS63/OHOS: device-side SSAP is the same wire protocol, with constants mapping 1:1 onto OHOS PDUs.

### 3.2 GLE HCI/DLI dialect (link/control plane)

WS73 firmware-side opcode DB (`bt_status_hso_msg_struct_def.txt`) confirms our DLI table:

| our `ssap_link.h` | WS73 diag DB (`bt_status_hso_msg_struct_def.txt`) |
|---|---|
| `DLI_CREATE_CONNECTION 0x1401` (`:21`) | `HCI_GLE_CREATE_CONNECTION = 0x1401` (`:594`) |
| `DLI_CANCEL_CREATE_CONNECTION 0x1402` | `HCI_GLE_CREATE_CONNECTION_CANCEL = 0x1402` (`:595`) |
| `DLI_DISCONNECT 0x1403` | `HCI_GLE_DISCONNECT = 0x1403` (`:596`) |
| `DLI_READ_REMOTE_VERSION 0x1802` | `HCI_GLE_READ_REMOTE_VERSION = 0x1802` (`:598`) |
| `DLI_SET_DATA_LEN 0x1804` | `HCI_GLE_SET_DATA_LENGTH = 0x1804` (`:599`) |
| `DLI_CONNECTION_UPDATE 0x1807` | `HCI_GLE_CONNECTION_UPDATE = 0x1807` (`:602`) |
| `DLI_CMD_STATUS_EVT 0x0001` / `CMD_COMPLETE 0x0002` | `HCI_GLE_COMMAND_STATUS_EVT=0x0001` (`:644`) / `COMMAND_COMPLETE_EVT=0x0002` (`:645`) |
| `DLI_DISCONNECTION_COMPLETE_EVT 0x0005` | `HCI_GLE_DISCONNECTION_EVT = 0x0005` (`:649`) |
| `DLI_CONNECTION_COMPLETE_EVT 0x0015` (`:31`) | ⚠️ DB says `HCI_GLE_CONNECTION_EVT = 0x1401` (`:658`) — see Open Questions |

Scan/adv commands also confirmed: `HCI_GLE_SET_SCAN_PARAMETERS=0x1001`, `SET_SCAN_ENABLE=0x1002` (`:591-592`), `SET_ADVERTISING_PARAMETERS=0x0c02`, `SET_ADVERTISING_DATA=0x0c03`, `SET_ADVERTISING_ENABLE=0x0c05` (`:584-588`), `ADVERTISING_REPORT_EVT=0x180b` (`:661`).

### 3.3 Frame/datatype and SSAP channel

- Frame datatypes identical: `HCI_DATATYPE_SLE_CMD=0xA1`, `EVENT=0xA2`, `ACB=0xA3`, `ICB=0xA4` (`driver/bsle/sle_driver/sle_chba/sle_hci_chba_proc.h:16-22`) == our `stack/ssap/include/hwsle_transport.h:21-24`.
- ACB header layout: tcid at offset 5, 2 bytes (`sle_hci_chba_proc.h:12-13`) == our `[0xA3][tcid u16][len u16][payload]` (`hwsle_transport.h:6-9`).
- SSAP rides ACB on tcid `SLE_SMTC 0x0A` (`hwsle_transport.h:27`; matches prior OHOS/BS21 analysis).
- HCC queue for SLE: `SLE_DATA_QUEUE=8` (`hcc_cfg_comm.h:50`), service `HCC_ACTION_TYPE_SLE=0xA` (`hcc_cfg_comm.h:37`).

**Conclusion: both the SSAP service layer and the GLE HCI/DLI link dialect are consistent between the two HiSilicon stacks; the only unverified number is the connection-complete event opcode.**

---

## 4. Migratable asset inventory from fbb_ws63

### Directly usable for our WS73 host project

1. `src/include/middleware/services/bts/sle/sle_ssap_stru.h` — canonical SSAP types (permissions, property/descriptor types, operate-indication bits, `sle_uuid_t`, `ssap_exchange_info_t`). Our `ssap_pkt.h`/`ssap_server.h` already align; use as the authoritative cross-check.
2. `src/include/middleware/services/bts/sle/sle_errcode.h` — peer `ERROR_RSP`/SSAP errcode space (0x80006100..). Validates our `SSAP_ERRCODE_*` mapping.
3. `src/application/samples/bt/sle/sle_uuid_server/src/sle_uuid_server.c` — the exact device-side server lifecycle our host client must interoperate with: `enable_sle → ssaps_register_server → add_service_sync(0xABCD) → add_property_sync(0x1122, perm READ|WRITE) → add_descriptor_sync(CLIENT_CONFIGURATION {0x01,0x00}) → start_service → announce`. Defines the concrete test vector (UUIDs, perms, notification flow).
4. `src/application/samples/bt/sle/sle_uuid_server/src/sle_server_adv.c` + `inc/sle_server_adv.h` — broadcast/scan-rsp data encoding (local name, tx power, disc/access level AD elements), announce defaults (`SLE_ADV_HANDLE_DEFAULT=1`, interval `0xC8`, `SLE_ADV_DATA_LEN_MAX=251` at `sle_server_adv.c:21-33`).
5. `src/application/samples/bt/sle/sle_uuid_client/src/sle_uuid_client.c` — reference client sequence (seek→connect→pair→`ssapc_exchange_info_req`(MTU 512, ver 1)→`ssapc_find_structure`(PRIMARY_SERVICE, 1..0xFFFF)→write→read→notify). Direct model for our host client FSM and for the interop test script.
6. `sle_connection_manager.h` / `sle_device_discovery.h` — connection-param ranges (conn interval `0x001E-0x3E80`, latency, supervision `0x000A-0x0C80`), disconnect-reason codes, phy/mcs semantics, announce/seek param ranges (interval/window in 0.125ms units).
7. `sle_hadm_manager.h` / `sle_factory_manager.h` / `sle_glp_manager.h` / `sle_low_latency.h` — ranging (channel sounding: `sle_set_channel_sounding_param_ex/enable/disable`), RF factory test (`sle_rf_tx_start/rx_start`), audio (glp), low-latency HID semantics. WS73 `include/bsle/sle/` carries the same files, so these are the semantic spec for our `FEAT_RANGING`/`FEAT_LOW_LAT` feature work.
8. `src/application/samples/bt/sle/sle_speed_{client,server}/` — throughput/latency methodology (MTU 512, `RECV_PKT_CNT` gating, `sle_set_data_len`+`sle_set_phy_param`+`sle_set_mcs`, QoS `gle_tx_acb_data_num_get`).
9. `src/application/ws63_porting_xf_0p2/port_xf/port_xf_sle/` — shows the SSAP API surface re-exported through a port layer (useful shape reference only).

### Not applicable to our project

- Board BSP / MCU HAL: `src/drivers/chips/ws63/`, `src/bootloader/`, `src/kernel/`, `src/open_source/` (LiteOS), `src/middleware/utils/`, `vendor/*/demo/*` peripheral samples, `docs/board/*.pdf`.
- In-chip firmware controller: `src/protocol/bt/controller/bgtp/` (prebuilt `libbgtp.a`), `src/protocol/bt/host/gle/` sources (prebuilt — device-side, not host-side).
- WS63 integrated WiFi service (`src/middleware/services/wifi_service/`) — WS73 uses host `wifi_soc.ko` instead.
- Build system (`src/build/`, CMake/Kconfig target configs) — LiteOS/OHOS-only.

---

## 5. Value grading for our WS73 host stack

**High value (protocol semantics confirmation / interop contract):**
- `sle_ssap_stru.h`, `sle_errcode.h` — confirm our SSAP PDU constants, operation bits and error codes 1:1.
- `sle_uuid_server.c` + `sle_server_adv.c` — define the peer device behavior and the concrete interop test vector (UUIDs 0xABCD/0x1122, perm READ|WRITE, CLIENT_CONFIGURATION descriptor).
- `sle_uuid_client.c` — reference client handshake our host must replicate (seek/connect/pair/exchange/find/read/write).
- `sle_connection_manager.h` param semantics — needed for our `ssap_link.c` DLI param encoding.
- Confirmation that WS73 `include/bsle/sle/` and the GLE HCI opcode DB are the same family (they are the host-side twin of the WS63 device stack).

**Medium value (reference for features/harness):**
- `sle_speed_{client,server}` — throughput/latency test harness semantics (MTU 512, packet gating, MCS/PHY tuning).
- `sle_hadm_manager.h` / `sle_factory_manager.h` — ranging (channel sounding) and RF-test semantics for `FEAT_RANGING`.
- `sle_low_latency.h`, `sle_glp_manager.h` — HID low-latency / audio semantics.
- `sle_device_discovery.h` announce/seek structs — broadcast data format details (ranges, AD element layout).
- `port_xf_sle` — API-surface re-export pattern.

**Low value (hardware/OS-independent code, BSP):**
- Chip drivers, HAL, bootloader, kernel, LiteOS glue, peripheral demos, build system, PDF docs.

---

## 6. Interconnect feasibility: WS63 board + WS73 dongle + our PC stack

**Conclusion: protocol prerequisites hold — the connection is feasible and expected to interoperate, provided our host stack completes the device-expected handshake and a few open items are resolved on the wire.**

Premise that holds:
1. Same HiSilicon FBB gle-host family on both ends. WS63 runs the device-side gle host in chip (`libbth_gle.a`); the WS73 dongle's firmware speaks the same GLE HCI dialect (opcode DB above) and the host-side `include/bsle/sle` API is the identical middleware API. Our `stack/ssap/` mirrors that API and OHOS wire format.
2. SSAP wire protocol identical (shared errcodes 0x80006100.., identical property/find/operate constants, prior `BS21-WS63-SDK-COMPARISON.md`).
3. Frame transport consistent: datatypes 0xA1-0xA4, SSAP over ACB tcid 0x0A, tcid field offset 5.
4. GLE link commands match our DLI table (0x1401/0x1402/0x1403/0x1802/0x1804/0x1807, events 0x0001/0x0002/0x0005).

Required on our side to actually connect (WS63 board runs `sle_uuid_server` sample as peer):
- Host must implement announce/seek commands the peer needs — as **client**: `HCI_GLE_SET_SCAN_PARAMETERS 0x1001` + `SET_SCAN_ENABLE 0x1002` (or the device-side seek API equivalent); as **server** (if we ever host): 0x0c02/0x0c05.
- Client FSM must implement connect (0x1401) → **pair** (`sle_pair_remote_device`) → `ssapc_exchange_info_req` (peer fbb client offers MTU 512, version 1; fbb uuid_server without `ssaps_set_info` defaults to 251) → `ssapc_find_structure` (PRIMARY_SERVICE, 1..0xFFFF) → read/write/notify. Our `ssap_link.c` covers DLI connect; `ssap_codec.c` covers exchange/find/read/write/notify.
- Dongle must be up: firmware loaded + `sle_soc.ko` + `/dev/hwsle` (our `hwsle_transport`).

Expected interop values:
- MTU: peer server default 251 == our `SSAP_STACK_MTU_DEFAULT 251` (`ssap_pkt.h:27`); peer client may request 512/1500 — our max 1024 (`ssap_pkt.h:28`) covers 512.
- Version: peer fbb client sends `version=1`; our `SSAP_EXCHANGE_VERSION=0x0301` (v1.3). The server's `EXCHANGE_INFO_RSP` carries the peer's own version; our codec must accept a peer value different from ours (currently hard-coded version — see Open Questions).

---

## Open questions

1. **Connection-complete event opcode**: our `ssap_link.h:31` uses `DLI_CONNECTION_COMPLETE_EVT 0x0015` (OHOS-derived); WS73 diag DB lists `HCI_GLE_CONNECTION_EVT = 0x1401` (`bt_status_hso_msg_struct_def.txt:658`). 0x1401 may be an internal FSM tag rather than the HCI event — must be confirmed on-wire against the dongle before the connect FSM is trusted.
2. **SSAP version negotiation**: our codec hard-codes v1.3 (0x0301) in exchange_info; the WS63 peer advertises version 1. Is version "1" == `SSAP_VERSION_1_0 0x0001`, and how does the peer treat a v1.3 response (take-min? reject?)? Needs a live exchange or OHOS min-version rule.
3. **Pairing gate**: the reference client pairs unconditionally before `exchange_info`; the WS63 test property is READ|WRITE with no AUTH/ENCRYPT perms (`sle_uuid_server.h:24`). Can exchange/find/read proceed unpaired on WS63, or does its SM gate SSAP? Our `FEAT_SM_SECURE` is currently off by default (`feature_mgr.h`). Try both on the bench.
4. **Full SSAP PDU structs are opaque** (prebuilt libs on both sides). Constants and errcodes strongly indicate byte-identical PDUs with OHOS, but a subtle dialect difference would only surface on the wire — the first find/exchange against the real WS63 board is the definitive check.
5. **WS73 firmware HCI numbering**: the opcode DB matches our DLI table statically, but map.md ticket 02 still lists "固件侧 HCI opcode 编号匹配需实机验证" as a residual risk.
