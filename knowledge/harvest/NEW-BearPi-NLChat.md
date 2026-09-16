---
type: harvest
title: "BearPi Hi2821 Pico 'NLChat' — SLE Passthrough for an Android Chat APP (Research Notes)"
language: zh
created: 2026-09-05
tags: [harvest, bearpi, hi2821, pico]
sources:
  - "https://github.com/Hny0305Lin/Bearpi_Hi2821_Pico_NLChat"
trust: B
stale_after: 2027-03-05
---

# BearPi Hi2821 Pico "NLChat" — SLE Passthrough for an Android Chat APP (Research Notes)

**Date:** 2026-09-03
**Analyst:** Research sub-agent (new repo knowledge harvest)
**Constraints honored:** Read-only, no network, no builds, no hardware. All paths under
`https://github.com/Hny0305Lin/Bearpi_Hi2821_Pico_NLChat/tree/master/` (6633 files; on-disk size measured 705 MB,
largely `interim_binary/` prebuilt per-chip payloads + LiteOS kernel tree).

---

## 1. Sources (primary files opened today)

| File | Role |
|---|---|
| `README.md` | BearPi-Pico H2821 product README (zh): chip/board specs |
| `application/samples/products/sle_uart/sle_uart.c` | Main sample; **carries the NLChat Android modification** |
| `application/samples/products/sle_uart/sle_uart_server/sle_uart_server.c` | SSAP server: service/property/descriptor registration + notify senders |
| `.../sle_uart_server/sle_uart_server.h` | Service/property UUID definitions, permission bitmasks |
| `.../sle_uart_server/sle_uart_server_adv.c` | Announce (advertising) parameters + TLV payload construction |
| `.../sle_uart_server/sle_uart_server_adv.h` | Adv header |
| `.../sle_uart_client/sle_uart_client.c` | SSAP client: seek → connect → pair → MTU exchange → discovery → write |
| `.../sle_uart_client/sle_uart_client.h` | Client-side externs |
| `application/samples/products/sle_uart/Kconfig` | Sample build options (server/client, normal/low-latency, performance) |
| `application/samples/products/ble_uart/ble_uart_server/ble_uart_server.{h,c}` | BLE GATT twin sample (dual-mode reference) |
| `include/middleware/services/bts/sle/*.h` | Complete BS21 host-side SLE API surface (14 headers, 5437 lines total) |
| `build/config/target_config/bs21/menuconfig/acore/standard_bs21_n1100_SLE_{Client,Server}_Config.config` | Prebuilt target configs |

Git provenance (verified in-tree):
- Remote: `https://github.com/Hny0305Lin/Bearpi_Hi2821_Pico_NLChat.git` (blob:none partial clone).
- Single commit `5396e70` "修改注释，添加LICENSE。" dated 2024-06-19 — i.e. the Android-APP
  modification is baked into the squashed initial commit, not visible as a diff.
- The author's own license banner explains exactly what was changed: `sle_uart.c:10-16`
  ("浩瀚银河只对sle_uart.c的修改内容保护… 修改服务端/客户端对文本在串口uart上显示的处理，
  在Android上会做区分以及文本提取等处理" — modified server/client UART text display so the
  Android APP can distinguish and extract chat text; dated 2024-06-19).

---

## 2. Platform: BearPi-Pico H2821 (BS21 family)

From `README.md` (spec tables):
- SoC: **Hi2821**, RISC-V 32-bit, up to 64 MHz, 160 KB SRAM + 512 KB (in-package) Flash
  (`README.md` 通用规格 section).
- Radio: **BLE + SLE dual mode**, 2.4 GHz (`README.md` product intro and features list).
- Peripherals relevant to us: 3x UART (up to 4 Mbit/s), USB 2.0 FS/HS, 29 GPIO, I2S/PCM, PDM,
  NFC Type2 Tag, AES/SM4/TRNG crypto engine (`README.md` features bullet list).
- Board: Type-C 5 V supply, onboard 5V→3.3V DCDC, red power LED + blue user LED (`README.md`
  电源特性 / 功能接口).

Build system target name is **bs21** everywhere: `build/config/target_config/bs21/` and
`interim_binary/bs21*` (plus bs20/bs21e/bs22/bs26 siblings in `interim_binary/` — this SDK image
covers the whole Hi2821/BS2x family). This is the same BS21 family flagged "ignore" in
`HHD01-BOARD.md` §2.1 (HopeRun maps HH-M03 → BS21), yet here it is a first-class low-cost SLE
endpoint. Kernel is **LiteOS v208.6.0_b017** (`kernel/liteos/liteos_v208.6.0_b017/`), not
FreeRTOS/LiteOS-M variants used on WS63 app cores.

---

## 3. SDK structure vs our WS73 host stack

Top-level layout (`ls` of repo root):

```
application/   bs21/standard, samples/{peripheral,products}
build/         config/target_config/bs21 (menuconfig .config per target), build.py, CMakeLists.txt
drivers/       chips/, drivers/ ( HAL + peripherals: uart, pinctrl, adc, i2s, ...)
include/       middleware/services/bts/{ble,br,common,sle}  ← host BT/SLE API surface (headers only)
kernel/        liteos/liteos_v208.6.0_b017 (full LiteOS source)
middleware/    chips/, utils/ (pm, ...)
open_source/   mbedtls, GmSSL3.0, libboundscheck, 7-zip-lzma-sdk
protocol/      bt/, glp/, nfc/   ← controller/protocol layer
interim_binary/ bs20/bs21/bs21e/bs22/bs26 prebuilt blobs, fota, efuse, hdb_config
vendor/        segger
```

Key structural differences vs the WS73/FBB SDKs documented in `WS63-VS-WS73.md` /
`BS21-WS63-SDK-COMPARISON.md`:

1. **Same `include/middleware/services/bts/sle/` header tree as FBB-WS63.** All 14 SLE headers
   (`sle_ssap_client.h` 675 L, `sle_ssap_server.h` 815 L, `sle_connection_manager.h` 1087 L,
   `sle_device_discovery.h` 796 L, `sle_low_latency.h` 360 L, etc.) match the FBB layout we
   cataloged in `HHD01-BOARD.md` §sources (`src/include/middleware/services/bts/sle/*`). API
   names are byte-identical (`ssaps_register_server`, `ssapc_write_req`, `sle_set_announce_param`...).
   → The SSAP "dialect" is shared across Hi2821/BS21 and WS63; anything we learn here transfers
   to our WS73 userspace stack design (`.stack/ssap/`).
2. **Single-chip, no host/controller split.** Unlike our WS73 USB dongle (controller in firmware +
   host on Linux over USB), here the whole stack runs on-chip: `protocol/` (bt/glp/nfc) +
   `middleware/services/bts` + `application/` on one RISC-V core. `#if (CORE_NUMS < 2)
   enable_sle();` in `sle_uart_client.c:115-117` and `sle_uart_server_adv.c:244-247` shows
   single-core builds self-enable SLE (two-core builds get it enabled by the other core's callback).
3. **No USB transport layer at all.** Nothing like our `OSPL-USB-TRANSPORT.md` concerns exist;
   the "transport" to the Android phone is the SLE air interface itself, and to the PC it is the
   UART console (`uapi_uart_write` at `sle_uart.c:120`, `sle_uart.c:266`, `sle_uart.c:282`).
4. **Vendor blobs isolated in `interim_binary/`** — mirrors our own whitelist-gitignore concern
   (AGENTS.md): prebuilt firmware/daemons are kept in a separate tree, not mixed into sources.
5. **Kconfig + CMake + build.py** flow with per-target `.config` files:
   `build/config/target_config/bs21/menuconfig/acore/standard_bs21_n1100_SLE_Client_Config.config`
   and `..._SLE_Server_Config.config` — one firmware image per role, selected by the Kconfig
   choice in `application/samples/products/sle_uart/Kconfig` (`SAMPLE_SUPPORT_SLE_UART_SERVER`
   vs `..._CLIENT`, lines 18-23; normal vs low-latency mode, lines 28-38).

---

## 4. SLE passthrough architecture (the core teaching of this repo)

### 4.1 Server role — registration sequence

`application/samples/products/sle_uart/sle_uart_server/sle_uart_server.c`:

1. **Power-on callback chain:** `sle_power_on_cbk` → `enable_sle()` (line 220-224) →
   `sle_enable_cbk` → `sle_enable_server_cbk()` (line 226-230). So SLE core bring-up is
   strictly event-driven: power-on → enable → (then) register service + start announcing.
2. **Register server with an arbitrary 2-byte app UUID** `{0x12, 0x34}`
   (`sle_uart_server.c:31`, registered at `:240` via `ssaps_register_server(&app_uuid, &g_server_id)`).
   Same value as the NearLinkSLE samples (compare `NearLinkSLE-SAMPLES.md` line 88) — it is a
   HiSilicon sample convention, not a spec constant.
3. **UUID base trick:** the sample defines a 128-bit base
   `g_sle_uart_base = {0x37,0xBE,0xA8,0x80,0xFC,0x70,0x11,0xEA,0xB7,0x20,0,...,0}`
   (`sle_uart_server.c:51-52`). `sle_uuid_setu2()` copies the base and overwrites **bytes 14-15
   little-endian** with the 16-bit short UUID (`sle_uart_server.c:77-82`, `UUID_INDEX 14` at
   `:23`). `SLE_UUID_LEN` is 16 (`sle_ssap_stru.h:23`). So the effective service UUIDs are
   `EA11-70FC-80A8-BE37-...-XXXX` style hybrids — an OnSemi/HiSilicon-style base, *not* the BLE
   SIG base `0000xxxx-0000-1000-8000-00805F9B34FB`. Anyone sniffing or writing an Android client
   must reproduce this base to find the service by UUID.
4. **Service:** 16-bit short UUID **0x2222**, `ssaps_add_service_sync(server_id, &uuid, is_primary=1,
   &g_service_handle)` (`sle_uart_server.h:24`, `sle_uart_server.c:167-178`).
5. **Property (the notify characteristic):** short UUID **0x2323**, permissions
   `SSAP_PERMISSION_READ | SSAP_PERMISSION_WRITE`, operate indication
   `SSAP_OPERATE_INDICATION_BIT_READ | ..._BIT_WRITE` (`sle_uart_server.h:27-33`, property add at
   `sle_uart_server.c:180-204`). Note: the sample does **not** set the NOTIFY operate bit even
   though it sends notifications — matching the NearLinkSLE finding that HiSilicon servers send
   notify regardless (`NearLinkSLE-SAMPLES.md` lines 100-110 discussion).
6. **Descriptor (CCCD equivalent):** `SSAP_DESCRIPTOR_CLIENT_CONFIGURATION` type with value
   `{0x01, 0x02}` (`sle_uart_server.c:205-218`). This is the SLE analog of the BLE CCCD; the
   preset 0x01/0x02 value effectively pre-arms notify/indicate.
7. **Start service:** `ssaps_start_service(g_server_id, g_service_handle)` (`sle_uart_server.c:252`).
8. **Announce (advertise):** `sle_uart_server_adv_init()` → `sle_set_announce_param` +
   `sle_set_announce_data` + `sle_start_announce(1)` (`sle_uart_server_adv.c:266-277`). Details in §5.

### 4.2 Server role — data path (UART → SLE)

- UART RX interrupt handler `sle_uart_server_read_int_handler` (`sle_uart.c:131-146`) forwards raw
  UART bytes directly into `sle_uart_server_send_report_by_handle(buffer, length)`
  (`sle_uart_server.c:293-305`), which wraps `ssaps_notify_indicate(server_id, conn_id,
  &ssaps_ntf_ind_t{handle=g_property_handle, type=SSAP_PROPERTY_TYPE_VALUE, value, value_len})`.
  A by-UUID variant exists too: `ssaps_notify_indicate_by_uuid` with a handle *range*
  (`start_handle=g_service_handle, end_handle=g_property_handle`) + target UUID
  (`sle_uart_server.c:262-290`). Two send flavors (by-handle, by-UUID-range) is a dialect detail
  our WS73 `ssap_server_notify` plan should expose (cf. `SSAP-IMPLEMENTATION-PLAN.md`).
- **No backpressure retry here.** Unlike the NearLinkSLE production samples' 2 ms x100 retry loop
  (`NearLinkSLE-SAMPLES.md` §SLE_UART_HE and line 324), this sample sends fire-and-forget from the
  ISR context. The low-latency build instead double-buffers one packet
  (`g_buff`/`g_uart_buff_len`/`g_buff_data_valid` at `sle_uart.c:126-130`, consumed by the TX
  callback `sle_uart_low_latency_tx_cbk` at `:149-168`).
- Disconnect handling: on `SLE_ACB_STATE_DISCONNECTED` the server pushes the literal string
  `"sle_dis_connect"` into an OSAL message queue (`sle_uart_server.c:336-342`); the main task
  compares it with `strncmp` and re-announces via `sle_start_announce(SLE_ADV_HANDLE_DEFAULT)`
  (`sle_uart.c:240-246`). Queue = 5 entries x 32 bytes (`sle_uart.c:88-89`).

### 4.3 Client role — discovery → write path

`application/samples/products/sle_uart/sle_uart_client/sle_uart_client.c`:

1. Power-on → `enable_sle()` (`:69-73`) → SLE-enable → register SSAPC callbacks + start seek
   (`:75-80`, `:317-325`).
2. Seek params: interval/window 100 (units ≈ 625 µs/125 µs per FBB docs), phy count 1
   (`:55-67`). Match by ASCII name `"sle_uart_server"` inside the raw adv payload via
   `strstr(seek_result_data->data, ...)` (`:31-33`, `:89-98`) — brittle but simple; the name is
   placed in the **scan-response** payload (`sle_uart_server_adv.c:107-128`).
3. On seek-disable: `sle_remove_paired_remote_device()` then `sle_connect_remote_device()`
   (`:100-108`) — deliberately un-pairing each cycle so pairing re-runs (Android APP use-case:
   fresh chat sessions).
4. Connect-state callback (`:153-182`): on `SLE_ACB_STATE_CONNECTED` with `SLE_PAIR_NONE` →
   `sle_pair_remote_device()` (`:163-165`); on disconnect → remove pairing + re-seek (`:175-178`).
   ACB/pair state enums: `SLE_PAIR_NONE=0x01, SLE_PAIR_PAIRING=0x02, SLE_PAIR_PAIRED=0x03`
   (`sle_connection_manager.h:33-37`); `SLE_ACB_STATE_NONE=0, _CONNECTED=1, _DISCONNECTED=2`
   (`sle_connection_manager.h:63-67`).
5. Pair-complete → **MTU exchange**: `ssapc_exchange_info_req(0, conn_id, &info)` with
   `info.mtu_size = SLE_MTU_SIZE_DEFAULT = 520`, `version = 1` (`:22`, `:184-194`).
6. MTU-exchange callback → service discovery: `ssapc_find_structure` with
   `type = SSAP_FIND_TYPE_PROPERTY`, `start_hdl=1, end_hdl=0xFFFF` (`:203-215`).
7. Property-found callback stores the discovered handle into the shared write param and sets
   `type = SSAP_PROPERTY_TYPE_VALUE` (`:230-239`).
8. UART → air: `sle_uart_client_read_int_handler` (`sle_uart.c:286-294`) stuffs UART bytes into
   the global `ssapc_write_param_t` and calls `ssapc_write_req(0, conn_id, param)` — the client
   writes to the server's 0x2323 property (chat uplink).
9. Air → UART: `sle_uart_notification_cb` / `sle_uart_indication_cb` (`sle_uart.c:254-284`) dump
   `data->data` to UART, wrapped in the NLChat framing (§6).

### 4.4 MTU and message-size constants

- SSAP MTU: **520** both sides — client requests it (`sle_uart_client.c:22,190-192`); server
  hard-sets its own view in pair-complete: `parameter.mtu_size = 520; parameter.version = 1;
  ssaps_set_info(g_server_id, &parameter)` (`sle_uart_server.c:345-356`). `ssaps_set_info` is a
  server-side dialect API worth mirroring (declared `sle_ssap_server.h:787`).
- UART RX buffer 256 B (`SLE_UART_TRANSFER_SIZE`, `sle_uart.c:40`); server notify buffer 0x100
  (`UART_BUFF_LENGTH`, `sle_uart_server.c:26`); low-latency buffer 800 B, normal-mode send cap
  40 B, performance-mode cap 250 B (`sle_uart.c:91-96`).
- `ssap_exchange_info_t` is `{uint32_t mtu_size; uint16_t version;}` (`sle_ssap_stru.h:161-163`) —
  32-bit MTU field, unlike BLE ATT's 16-bit.

### 4.5 Low-latency side channel (HS mouse-style path)

`sle_low_latency.h` + `CONFIG_SAMPLE_SUPPORT_LOW_LATENCY_TYPE`:
- Fixed rates enum 125 Hz → 8 kHz (`sle_low_latency.h:33-47`); the client enables
  `sle_low_latency_set(conn_id, true, SLE_UART_LOW_LATENCY_2K)` = 2 kHz (`sle_uart_client.c:30,170`).
- Separate TX/RX callback registration pairs (`sle_low_latency_tx_register_callbacks` at
  `sle_uart.c:170-175`; `sle_low_latency_rx_register_callbacks` at `sle_uart_client.c:308-314`).
- PHY can be forced for throughput: `sle_set_phy_param` with QPSK frame type 2, 4M PHY, pilot
  density 16:1 (`sle_uart_client.c:129-150`) and MCS 10 via `sle_set_mcs` (`sle_uart_server.c:307-320`).
  The throughput measurement loop (1000 packets, µs timer → kbps) is at `sle_uart_client.c:274-301`.
- This is a **bypass of SSAP** (raw scheduled transfers), the same mechanism family as our
  `SLE-MEASURE-QOS.md` / `WS63-SLE-EXAMPLES.md` notes; on BS21 it is exposed as a public header
  API rather than AT commands.

---

## 5. Announce/advertising dialect (BS21 flavor)

From `sle_uart_server_adv.c`:

- Timing units are **125 µs**: conn interval min/max `0x64` = 100 units = 12.5 ms (`:26-28`);
  announce interval `0xC8` = 200 units = 25 ms (`:30-32`).
- Supervision timeout `0x1F4` = 500 units of **10 ms** = 5 s; "max latency" constant misnamed
  `SLE_CONN_MAX_LATENCY = 0x1F3` = 4.99 s (`:34-36`).
- Adv TX power 10 dBm-ish level value carried in scan-rsp TLV (`:38`, `:113-118`).
- Mode `SLE_ANNOUNCE_MODE_CONNECTABLE_SCANABLE = 0x03` (`:136`; enum at
  `sle_device_discovery.h:88-96`, incl. directed 0x07). GT role `SLE_ANNOUNCE_ROLE_T_CAN_NEGO`
  (`:138`; enum `sle_device_discovery.h:70-76`).
- **Adv payload TLVs** built with `struct sle_adv_common_value {length; type; value;}`:
  - announce data: `SLE_ADV_DATA_TYPE_DISCOVERY_LEVEL` = `SLE_ANNOUNCE_LEVEL_NORMAL`, then
    `SLE_ADV_DATA_TYPE_ACCESS_MODE` = 0 (`:78-104`);
  - scan response: `SLE_ADV_DATA_TYPE_TX_POWER_LEVEL`, then
    `SLE_ADV_DATA_TYPE_COMPLETE_LOCAL_NAME` = `"sle_uart_server"` (`:49-70`, `:107-128`).
  - Max adv data len 251 (`:42`); announce level enum (`sle_device_discovery.h:50-58`):
    NONE/NORMAL/PRIORITY/PAIRED/SPECIAL.
- Own address is all-zeros with type 0 (`:135`, `:147-148`) — the chip substitutes its own
  address; `sle_addr_t` = `{uint8_t type; uint8_t addr[6]}` with types PUBLIC=0 / RANDOM=6
  (`sle_common.h:39,66-85`). Address printed with byte order `addr[0]...addr[5]` reversed-ish in
  logs (`sle_uart_server.c:328-329`) — note `BT_INDEX_0`/`BT_INDEX_4` usage.

---

## 6. Android APP bridge protocol ("NLChat" framing)

The repo's whole point (name = "NL Chat"): pair the board with an Android SLE chat APP.
The modification is minimal but reveals the entire application-layer protocol:

- **No binary framing at all.** Payloads are raw UTF-8 chat text moved over the single notify
  property (downlink: server→phone is actually phone→server via writes; see below) and the
  single write target (uplink).
- **Delimiters are the protocol.** Server write-callback prints `"\n"` then
  `"Let's start chatting, This is the content of the client:"` then the payload then `"\n"`
  (`sle_uart.c:109-123`). Client notification/indication callbacks mirror it with
  `"...This is the content of the server:"` (`sle_uart.c:254-284`). These banner strings travel
  **over the air** (they are printed by the peer firmware into its own UART, and the author's
  banner says the Android side "does distinguishing and text extraction" — `sle_uart.c:14`).
  I.e. the Android APP parses the fixed banner strings + newlines out of SLE notification
  payloads to isolate the human-typed text. The commented-out original log lines
  (`sle_uart.c:260-264, 276-280`) show the pre-modification format `"\n sle uart recived data :
  %s\r\n"`.
- **Transport role mapping:** in the default build the *phone-side* counterpart is the SLE
  **client** (it scans by name `"sle_uart_server"`, connects, writes chat text via
  `ssapc_write_req`, and receives server banners via notification callbacks). The board firmware
  here plays the server; the client-side binary is the "dongle" variant of the same sample
  (`SLEUartDongleTask`, `sle_uart.c:333`).
- **Auto-reconnect UX:** client removes pairing and re-seeks on every disconnect
  (`sle_uart_client.c:175-178`); server re-announces on every disconnect via the msg-queue
  sentinel string (`sle_uart.c:240-246`) — chat-session semantics, not persistent pairing.
- **BLE twin for comparison:** the `ble_uart` sample in the same repo defines a classic BLE GATT
  passthrough: service UUID `0xABCD`, char TX `0xCDEF`, char RX `0xEFEF`, CCCD descriptor UUID
  separate (`ble_uart_server.h:22-27`), with NOTIFY|READ / READ|WRITE_NO_RSP characteristic
  property split (`ble_uart_server.c:81-111`). This dual-mode pairing (SLE `0x2222/0x2323` vs BLE
  `0xABCD/0xCDEF/0xEFEF`) is a clean reference for any Android APP that supports both radios on
  Hi2821 hardware.

Takeaway for us: an "Android SLE APP" ecosystem exists in the wild built on exactly these
HiSilicon sample UUIDs (0x2222 service / 0x2323 notify property / base
`37BEA880-FC70-11EA-B720-...`) and newline+banner framing. If our WS73 dongle ever needs to talk
to such APPs, emulating this service layout is the compatibility target — record it in
`docs/DEVICE-INTEL.md` follow-ups.

---

## 7. SSAP dialect details (headers) — what we had not captured before

From `include/middleware/services/bts/sle/`:

- **Permission bitmask** (`sle_ssap_stru.h:53-61`): READ=0x01, WRITE=0x02,
  **ENCRYPTION_NEED=0x04, AUTHENTICATION_NEED=0x08, AUTHORIZATION_NEED=0x10** — the three
  security-granularity bits are how SLE access control is expressed at attribute level (maps to
  our `OHOS-SM-SECURITY.md` findings).
- **Operate-indication bitmask** (`sle_ssap_stru.h:119-135`): READ=0x01, WRITE_NO_RSP=0x02,
  WRITE=0x04, NOTIFY=0x08, INDICATE=0x10, BROADCAST=0x20, **DESCRITOR_WRITE=0x100** (sic),
  `SSAP_OPERATE_INDICATION_MAX`. Note the high-bit descriptor-write flag — no BLE analog.
- **Write types** (`sle_ssap_stru.h:19-26`): `SSAP_WRITE_NO_RSP=0x01`,
  `SSAP_WRITE_DEFAULT_WITH_RSP=0x02`.
- **Property/descriptor types** (`sle_ssap_stru.h:95-107`): VALUE=0x00; descriptors
  USER_DESCRIPTION=0x01, CLIENT_CONFIGURATION=0x02, SERVER_CONFIGURATION=0x03,
  PRESENTATION_FORMAT=0x04, RFU=0x05..0x1F, **CUSTOM=0xFF**.
- **Find types** (`sle_ssap_stru.h:73-83`): SERVICE_STRUCTURE=0x00, PRIMARY_SERVICE=0x01,
  REFERENCE_SERVICE=0x02, PROPERTY=0x03, **METHOD=0x04, EVENT=0x05** — METHOD/EVENT discovery
  types are SLE-specific (profile "methods"/"events" beyond GATT-shaped attributes); our
  `SSAP-DIALECT-COMPARISON.md` tables should gain a row for these.
- **Error-code space** (`sle_errcode.h`): `ERRCODE_SLE_COMMON_BASE = 0x80006000`, SSAP sub-range
  `0x80006100-0x800061FF`, common end `0x800067FF`; named codes SUCCESS=0, CONTINUE,
  DIRECT_RETURN, NO_ATTATION (sic), PARAM_ERR, FAIL, TIMEOUT, UNSUPPORTED, GETRECORD_FAIL,
  POINTER_NULL, NO_RECORD, STATUS_ERR, NOMEM (`sle_errcode.h:25-90+`). `ERRCODE_SLE_CONTINUE`
  and `ERRCODE_SLE_DIRECT_RETURN` are control-flow codes — a stack design detail worth keeping
  for our Rust port error taxonomy.
- **UUID representation**: `sle_uuid_t {uint8_t len; uint8_t uuid[16];}` — length-tagged, with
  16-bit UUIDs stored in the tail bytes 14-15 of the same 16-byte array (see §4.1.3). Printing
  helpers branch on len 2 vs 16 (`sle_uart_server.c:83-103`).
- **SLE version constant** `SLE_VERSION 0x10` ("SLE version 1.0", `sle_common.h:30`); name max
  31 bytes (`sle_common.h:48`); link key 16 bytes (`sle_common.h:57`).
- **Client callback set** (`sle_ssap_client.h:186-437` + registration at
  `sle_uart_client.c:257-268`): exchange_info, find_structure, find_property, find_structure_cmp
  (complete), read_cfm, read_by_uuid_complete, write_cfm, **mtu_changed**, notification,
  indication. The `ssapc_handle_value_t == ssapc_write_param_t` union typedef
  (`sle_ssap_client.h:79`) means notification payloads and write params share one struct shape.
- **Server API set** (`sle_ssap_server.h`): register/unregister server (454), add service sync
  (574), add property sync (601), add descriptor sync (629), start service (651), notify/indicate
  by handle (731) and by UUID range (763), **ssaps_set_info** (787, MTU/version push), plus
  delete-all-service and read/write request callbacks used by the sample
  (`sle_uart_server.c:145-165`).

---

## 8. Cross-check vs existing knowledge

Against `NearLinkSLE-SAMPLES.md` (NearLinkSLE community repo, 2026-08-17):

| Aspect | BearPi NLChat (this repo) | NearLinkSLE samples | Verdict |
|---|---|---|---|
| App UUID | `{0x12,0x34}` (`sle_uart_server.c:31`) | `{0x12,0x34}` (SAMPLES line 88) | Same HiSilicon sample convention |
| Service/property UUID | 0x2222 / 0x2323 (`sle_uart_server.h:24-27`) | 0x3333 / 0x3434 (SAMPLES lines 95-106) | Different short UUIDs, same base-UUID trick |
| MTU | 520, both sides set it (`sle_uart_client.c:22`, `sle_uart_server.c:353`) | 520 (SAMPLES line 176) | Confirmed 520 is the de-facto SSAP MTU |
| CCCD pre-arm | descriptor value `{0x01,0x02}` (`sle_uart_server.c:185`) | client must write 0x0001 (SAMPLES line 199) | This repo's server pre-arms CCCD; SAMPLES relies on client write |
| Backpressure | none (fire-and-forget from ISR) | 2 ms x100 retry loop (SAMPLES line 324) | SAMPLES is production-hardened; this is a demo |
| Framing | newline + banner strings, human text | 0x5A 0x5A header + XOR checksum (SLE_QT) | Two different app-layer dialects over identical SSAP |
| Notify operate-bit | not set, notify still works | same observation (SAMPLES line 110) | Independent confirmation |

Against `HHD01-BOARD.md` (HopeRun WS63 kit):
- Same SLE header API tree, so demo-level SSAP knowledge transfers 1:1 (§3.1).
- WS63 demos name their sample files `sle_uart_server.c` etc. too (HHD01 §2.2 table) — this repo
  is the BS21-origin of that same sample lineage; BearPi's version is where the Android chat
  framing was added.
- BS21 differences that matter for us: LiteOS v208 (not WS63's app-core variant), single-chip
  role-firmware model (one .config per role, §3.5), and UART-passthrough instead of
  sensor/peripheral demos.

What this repo adds that we did not have anywhere: (a) the **Android NLChat framing/UUID
compatibility target** (§6); (b) **SSAP server CCCD pre-arm** via descriptor default value;
(c) `ssaps_set_info` server-side MTU push (§4.4); (d) by-UUID-range notify API (§4.2);
(e) METHOD/EVENT find types + descriptor-write operate bit (§7); (f) public low-latency bypass
API with fixed rates (§4.5); (g) the full BS21 announce unit table (125 µs / 10 ms) with concrete
values (§5).

---

## 9. Action items for our project (proposals, no code touched)

1. Add the BearPi/NLChat service identity (base UUID + 0x2222/0x2323 + banner framing) to the
   ecosystem notes (`docs/ECOSYSTEM.md` follow-up) as a known Android-APP compatibility profile.
2. Feed §7 enum/error-space details into `SSAP-DIALECT-COMPARISON.md` and
   `SSAP-IMPLEMENTATION-PLAN.md` (by-UUID-range notify, `ssaps_set_info`, CCCD default value).
3. The seek-by-name `strstr` pattern (§4.3.2) suggests our discovery layer should expose raw adv
   payload access, not just parsed fields (`WS63-CONN-DISCOVERY.md` follow-up).
4. `interim_binary/` contains BS2x-family prebuilts — potential future source for controller-side
   behavior reference, but per AGENTS.md whitelist rules nothing from there goes into git.
