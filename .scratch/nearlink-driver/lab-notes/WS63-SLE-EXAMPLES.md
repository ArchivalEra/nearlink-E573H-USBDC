# WS63 SLE SSAP Examples — Reverse-Engineered for PC-Dongle Interop

**Date:** 2026-08-17
**Author:** research subagent (ArchivalEra NearLink)

## Sources

Primary (WS63 SDK `fbb_ws63`, read-only):

| File | Path |
|---|---|
| uuid server main | `/mnt/hdd/nearlink-stuff/fbb_ws63/src/application/samples/bt/sle/sle_uuid_server/src/sle_uuid_server.c` |
| uuid server adv | `/mnt/hdd/nearlink-stuff/fbb_ws63/src/application/samples/bt/sle/sle_uuid_server/src/sle_server_adv.c` |
| uuid server hdr | `/mnt/hdd/nearlink-stuff/fbb_ws63/src/application/samples/bt/sle/sle_uuid_server/inc/sle_uuid_server.h`, `inc/sle_server_adv.h` |
| uuid client | `/mnt/hdd/nearlink-stuff/fbb_ws63/src/application/samples/bt/sle/sle_uuid_client/src/sle_uuid_client.c` |
| speed server | `/mnt/hdd/nearlink-stuff/fbb_ws63/src/application/samples/bt/sle/sle_speed_server/src/sle_speed_server.c`, `src/sle_speed_server_adv.c` |
| speed client | `/mnt/hdd/nearlink-stuff/fbb_ws63/src/application/samples/bt/sle/sle_speed_client/src/sle_speed_client.c` |
| SSAP server API | `/mnt/hdd/nearlink-stuff/fbb_ws63/src/include/middleware/services/bts/sle/sle_ssap_server.h` |
| SSAP client API | `/mnt/hdd/nearlink-stuff/fbb_ws63/src/include/middleware/services/bts/sle/sle_ssap_client.h` |
| SSAP structs | `/mnt/hdd/nearlink-stuff/fbb_ws63/src/include/middleware/services/bts/sle/sle_ssap_stru.h` |
| adv/seek API | `/mnt/hdd/nearlink-stuff/fbb_ws63/src/include/middleware/services/bts/sle/sle_device_discovery.h` |
| conn mgr (PHY/MCS) | `/mnt/hdd/nearlink-stuff/fbb_ws63/src/include/middleware/services/bts/sle/sle_connection_manager.h` |
| sample Kconfig/CMake | `/mnt/hdd/nearlink-stuff/fbb_ws63/src/application/samples/bt/sle/Kconfig`, `CMakeLists.txt` |
| vendor demo (throughput) | `/mnt/hdd/nearlink-stuff/fbb_ws63/vendor/HiHope_NearLink_DK_WS63E_V03/demo/sle_throughput/` (README.md, Kconfig, sle_server_speed/, sle_client_speed/) |
| build config | `/mnt/hdd/nearlink-stuff/fbb_ws63/vendor/HiHope_NearLink_DK_WS63E_V03/build_config.json` |
| build docs | `/mnt/hdd/nearlink-stuff/fbb_ws63/tools/README.md`, `vendor/.../demo/sle_led/README.md:121` |
| build entry | `/mnt/hdd/nearlink-stuff/fbb_ws63/src/build.py` |

Comparison (our side):

- `/home/archivalera/plum/zcode-projects/nearlink/stack/ssap/include/ssap_pkt.h`, `ssap_codec.h`, `ssap_server.h`, `ssap_link.h`, `hwsle_transport.h`
- `/home/archivalera/plum/zcode-projects/nearlink/scripts/hwsle-probe.sh`
- OHOS reference: `/mnt/hdd/nearlink-stuff/communication_nearlink_service/services/stack/src/cp/bsl/sle/servm/ssap/src/ssap_handle.h` (SSAP_HANDLE_MIN = 0x0010), `ssaps_service.c` (handle allocation), `ssaps_server.c`

---

## 1. Sample inventory and call flows

Four samples under `src/application/samples/bt/sle/`, selected mutually-exclusively via Kconfig `choice` (`samples/bt/sle/Kconfig:6-21`). Two are duplicated/evolved in the vendor demo `demo/sle_throughput` (server+client) which is what `build_config.json:94-106` actually builds for the HiHope board.

### 1.1 sle_uuid_server — SSAP peripheral (adv + service + notify)

Init path (`sle_uuid_server.c:269-278`):

```
app_run(sle_uuid_server_entry)                     :297
 └─ task "sle_uuid_server" prio 26                 :283-294
    └─ sle_uuid_server_init()                      :269
       ├─ enable_sle()                             :271
       ├─ sle_conn_register_cbks()                 :272 (conn_state_changed / pair_complete)
       ├─ sle_ssaps_register_cbks()                :273 (start_service/mtu/read/write)
       ├─ sle_uuid_server_add()                    :274
       │   ├─ ssaps_register_server(app_uuid={0,0}, &g_server_id)   :173
       │   ├─ ssaps_add_service_sync(0xABCD, primary=1) → g_service_handle   :99-110
       │   ├─ ssaps_add_property_sync(0x1122, perms=READ|WRITE, value=6×00) → g_property_handle   :112-137
       │   ├─ ssaps_add_descriptor_sync(perms=READ|WRITE, value={0x01,0x00})   :138-157
       │   └─ ssaps_start_service(g_service_handle)   :186
       └─ sle_uuid_server_adv_init()               :275 → sle_server_adv.c:193-202
           ├─ sle_announce_register_cbks()
           ├─ sle_set_announce_param(handle=1, mode=CONNECTABLE_SCANABLE, ...)
           ├─ sle_set_announce_data(adv: discovery level + access mode;
           │                         scan-rsp: tx power + "sle_uuid_server")
           └─ sle_start_announce(1)
```

Server callbacks are log-only (`sle_uuid_server.c:62-87`); **the sample never calls `ssaps_send_response()`** — behaviour of the bts when an app skips the response is an open question (see Open Questions).

Data egress (used by the sample's public API, not wired to a timer):
- `sle_uuid_server_send_report_by_handle()` → `ssaps_notify_indicate(server, conn, {handle=g_property_handle, type=VALUE})` (`:220-240`)
- `sle_uuid_server_send_report_by_uuid()` → `ssaps_notify_indicate_by_uuid(... uuid=0x1122, start=g_service_handle, end=g_property_handle)` (`:196-217`)

### 1.2 sle_uuid_client — SSAP central (scan → connect → discover → write → read)

```
app_run(sle_uuid_client_entry)                     :250
 └─ sle_client_init()                              :208
    ├─ register seek/connect/ssapc callbacks       :210-215
    └─ enable_sle()                                :216
       └─ cbk sle_enable → sle_start_scan()        :31-36
          └─ sle_set_seek_param({phys=1M, passive, interval=100, window=100})  :219-231
             └─ sle_start_seek()
                └─ seek_result_cbk: copy first addr, sle_stop_seek()  :52-58
                   └─ seek_disable_cbk → sle_connect_remote_device(&addr)  :45-50
                      └─ conn_state_cbk (CONNECTED): if PAIR_NONE → sle_pair_remote_device  :68-80
                         └─ pair_complete_cbk → ssapc_exchange_info_req(mtu=300, ver=1)  :82-92
                            └─ exchange_info_cbk → ssapc_find_structure(type=PRIMARY_SERVICE,
                                               start=1, end=0xFFFF)  :100-112
                               └─ find_structure_cbk: store start_hdl/end_hdl  :114-132
                                  └─ find_structure_cmp_cbk → ssapc_write_req(
                                     {0x11,0x22,0x33,0x44} → handle=**start_hdl**)  :134-156
                                     └─ write_cfm_cbk → ssapc_read_req(same handle)  :179-184
                                        └─ read_cfm_cbk: dump bytes  :186-196
```

**Critical quirk:** the client writes to the **service start handle** (`param.handle = g_find_service_result.start_hdl`, `:151`), never to a property handle, and never runs property discovery (`SSAP_FIND_TYPE_PROPERTY`). Whatever the bts does with a write to the service handle is what makes the demo pass — see Open Questions.

### 1.3 sle_speed_server / sle_speed_client — throughput test

Server (`sle_speed_server.c:391-408`): `enable_sle` + conn/ssaps callbacks + service add (same 0xABCD/0x1122) + adv_init + `ssaps_set_info({mtu=512, ver=1})` (`:359-365`) + `sle_default_connection_param_set({interval=0x0A, scan_interval=400, scan_window=20, timeout=0x1F4})` (`:367-379`) + `sle_set_local_addr(11:22:33:44:55:66)` (`:381-388`).

Timing chain (the interesting part):

```
server conn_state_cbk (CONNECTED) → sle_update_connect_param(interval 0x0A)  :302-319
client (sle_speed_client.c):
  seek_result_cbk: keep only MAC 11:22:33:44:55:66  :67-76
  pair_complete → exchange_info_req(mtu=512)        :142-152
  exchange_info_cbk → find_structure(PRIMARY, 1..0xFFFF)   :176-188
  find_cmp_cbk → write {11 22 33 44} → start_hdl   :210-232
  write_cfm_cbk → read_req(start_hdl)              :255-260
server read_request_cbk → spawn send_data_thread_function  :88-103
  send_data_thread_function:                       :173-204
    sle_set_data_len(conn, 512)                    :175
    sle_set_phy_param({frame=SLE_RADIO_FRAME_2, phy=SLE_PHY_4M, pilot=16:1})  :176-186
    sle_set_mcs(conn, 10)                          :187
    loop SEND_PKT_TIMES(1000) × SEND_PKT_NUMS(1000):
      if flow_ctrl OK: notify_indicate(handle=g_property_handle, 236 B)  :190-199
client notification_cb: count 1000 pkts, compute speed = len×1000×8/time  :88-109
```

So the server fires a **1,000,000-packet notification burst** the moment the client issues a READ. Each packet is 236 bytes of 'A' with a 16-bit counter in `data[0..1]` (`:194-196`).

Vendor demo variant (`demo/sle_throughput/`) differs: server `sle_set_data_len(250)` (demo sle_uuid_server.c:124), client MTU 250, client has **no MAC filter** (connects to first seek result), local addr `13:67:5c:07:00:51`; client speed calc uses fixed `235×1000×8/time`.

### 1.4 Text sequence diagrams

UUID server ↔ uuid client:

```
WS63(uuid_server)                      WS63(uuid_client)
  [adv: sle_uuid_server, 25ms]  --seek-->  [passive scan 1M]
                                       ----connect(addr)---->
  <----pair---->
  <--exchange_info(mtu=300, ver=1)-->
  <--find_structure(primary,1..FFFF)-->
  ---find_rsp(start_hdl,end_hdl,uuid=ABCD)-->
  <--write_cmd/req({11 22 33 44}→start_hdl)-->
  ---write_rsp-->
  <--read_req(start_hdl)-->
  ---read_rsp-->
```

Speed pair:

```
WS63(speed_server)                       WS63(speed_client)
  [adv: sle_uart_server, 11:22:33:44:55:66]  --seek(filter MAC)-->  connect
  <--pair--> <--exchange_info(mtu=512)--> <--find_structure(primary)-->
  <--write→start_hdl--> <--read_req(start_hdl)-->
  read_request_cbk → spawn sender
  → 1000×1000 notify(VALUE_NTF, 236B, handle=prop) -->
                                  notification_cb → throughput @
                                  (data_len×1000×8/time)
```

---

## 2. sle_uuid_server details (service/property declaration)

- Service UUID: **0xABCD** — `sle_uuid_server.h:18` (`SLE_UUID_SERVER_SERVICE`).
- Property (attribute) UUID: **0x1122** — `sle_uuid_server.h:21` (`SLE_UUID_SERVER_NTF_REPORT`).
- Property permissions: **SSAP_PERMISSION_READ|SSAP_PERMISSION_WRITE = 0x03** (`sle_uuid_server.h:24`).
- Descriptor permissions: READ|WRITE, value `{0x01, 0x00}` (CCCD-style "notification allowed") (`sle_uuid_server.c:117,138`).
- Property initial value: 6 zero bytes (`sle_uuid_server.c:34`).
- UUID encoding on the wire: a **128-bit base** `{37 BE A8 80 FC 70 11 EA B7 20 00 00 00 00 00 00}` with the 16-bit value written **little-endian into bytes [14..15]** (`sle_uuid_server.c:46-60`), i.e. 0xABCD → uuid[14]=0xCD, uuid[15]=0xAB; 0x1122 → uuid[14]=0x22, uuid[15]=0x11. `out->len = 2` → on the wire this is a **standard (2-byte) UUID**.
- Read/write handling: the server registers `read_request_cb` / `write_request_cb` but only logs (`:62-74`); no explicit `ssaps_send_response()`. Notifications use `SSAP_PROPERTY_TYPE_VALUE` (=0, `sle_ssap_stru.h:95`).
- App UUID used to register the server: `{0x00,0x00}` len 2 (`sle_uuid_server.c:32`).

**Handle expectations (interop critical):** the sample prints `service_handle`/`property_handle` at runtime (`sle_uuid_server.c:184`). Do **not** hardcode them. In the OHOS reference, service handle = first allocated (`SSAP_HANDLE_MIN=0x0010`, `ssap_handle.h:27`; service gets `range.start`, properties get sequential handles from `start+1`, `ssaps_service.c:252-259, 304-315`). The WS63 bts is closed-source; treat discovery results as authoritative and verify against the printed handles on the bench.

---

## 3. sle_uuid_client — call sequence + how it maps onto our stack

Call sequence (in SSAP-PDU terms, opcodes from our `ssap_pkt.h`):

1. `EXCHANGE_INFO_REQ (0x02)` with mtu=300, version=1 (`sle_uuid_client.c:87-91`).
2. `FIND_STRUCTURE_REQ (0x04)` type=PRIMARY_SERVICE, start=1, end=0xFFFF (`:107-111`). No UUID filter.
3. On find complete: `WRITE_REQ (0x0D)` (or CMD) `{11 22 33 44}` to the discovered service `start_hdl`, type=VALUE (`:148-155`).
4. On write cfm: `READ_REQ (0x08)` of the same handle, type=VALUE (`:179-184`).
5. `READ_RSP (0x09)` bytes dumped (`:186-196`).

**Mapping to our PC stack (as central connecting to the WS63 server):**

- Our opcodes/PDUs are byte-identical to OHOS/HiSilicon (`ssap_codec.h:19-40`, `ssap_pkt.h:121-143`) — no codec changes needed for the message types used by this sample.
- Find structure: our codec `ssap_encode_find_struct_req(find_type=SSAP_FIND_PRIMARY_SERVICE, item_type=0, rsp_mode=?, start=1, end=0xFFFF, uuid=NULL, uuid_len=2)` (`ssap_codec.h:107-113`). Match `findType=0b001` and `start_hdl=1,end_hdl=0xFFFF`.
- The expected service UUID in the find response is **0xABCD** (2-byte standard, bytes 14-15 little-endian on the WS63 side).
- Read/write format: items are `{handle u16 LE, type u8}`; read response carries either a raw value (single, ctrl.multi=0) or `{length:15|success:1, value}` items. **Known gap:** our server emits READ_RSP with `handle(2)+vlen(2)+value` (noted in `OSPL-SSAP-COMPARE.md`); as a client we only need to *decode* the WS63 READ_RSP, which our codec already does per `ssap_pkt.h:317-335`.
- For the write to the service start handle to work like the sample, our client must send `WRITE_REQ` with the **service start handle** discovered from find-structure (the sample's convention), not a property handle.
- MTU: WS63 uuid client negotiates 300 (`:18`); our `SSAP_STACK_MTU_MAX=1024` / default 251 (`ssap_pkt.h:27-28`) accepts 300 fine.

**Our side caveats:** `ssap_link` connects via DLI 0x1401 using the peer's 6-byte MAC (`ssap_link.h:21-27`); the WS63 uuid-server sample **does not set a fixed local address** (only the speed server does: `11:22:33:44:55:66`). So for a deterministic PC→WS63 connect with the uuid sample you must first learn the board's MAC (see §7).

---

## 4. Speed samples — channel/tcid config and how to measure

- Service/property: identical 0xABCD/0x1122 table (`sle_speed_server.h:18-21`).
- MTU: server advertises 512 (`sle_speed_server.c:359-365`); client requests 512 (samples) or 250 (vendor demo).
- Max payload: `sle_set_data_len(512)` samples / `250` vendor (`sle_speed_server.c:175`, demo sle_uuid_server.c:124).
- PHY/MCS: radio frame type 2, PHY 4M, pilot density 16:1, MCS=10 (`:176-187`).
- Packet: 236 B ('A' fill, 16-bit counter in first 2 bytes), 1000×1000 packets, flow-controlled (`:190-199`).
- Connection: interval 0x0A, supervision 0x1F4, scan interval 400 / window 20 for the *initiating* role (`:61-64, 367-379`).
- Trigger: a READ request from the peer (`:88-103`). Client measures 1000 notifications' elapsed time and reports `data_len×1000×8/time` bps (`sle_speed_client.c:88-109`).

For our PC: receiving the burst means our transport must accept **VALUE_NTF (0x0F)** on our SSAP tcid. Our transport uses **tcid 0x0A (SLE_SMTC)** for SSAP (`hwsle_transport.h:26-29`). **Verify on the wire** that the WS63 emits SSAP frames on the same tcid (see Open Questions). Payload per NTF is `{handle u16, len u16, value}` (`ssap_pkt.h:507-524`) — a raw 236-byte value inside a 512-MTU frame, so no fragmentation is exercised by default.

---

## 5. Broadcast/scan parameters (exact values)

From `sle_server_adv.c` (uuid) and `sle_speed_server_adv.c` (speed):

| Parameter | uuid server | speed server | units |
|---|---|---|---|
| name (scan-rsp) | `sle_uuid_server` (`:35-36`) | `sle_uart_server` (`:36`) | — |
| announce mode | CONNECTABLE_SCANABLE=0x03 (`:121`) | same | — |
| gt role | T_CAN_NEGO=0 (`:123`) | same | — |
| announce level | NORMAL=1 (`:124`) | same | — |
| channel map | 0x07 → ch 77/78/79 (`:125`) | same | — |
| adv interval min/max | 0xC8/0xC8 (`:126-127`) = 25 ms | same | 125 µs units |
| conn interval min/max | 0x64/0x64 (`:128-129`) = 12.5 ms | 0x0A/0x0A (`:18-19`) = 1.25 ms | 125 µs per SDK comment |
| conn max latency | 0x1F3 (`:130`) | same | — |
| supervision timeout | 0x1F4 (`:131`) = 5 s | same | 10 ms units |
| adv tx power | 10 dBm (scan-rsp, `:101-105`) | same | — |
| adv data TLVs | discovery level (0x01, val 1), access mode (0x02, val 0) (`:67-92`) | same | — |
| speed-server own MAC | — | `11:22:33:44:55:66` (`:121,133-134`) | — |

Scan side (uuid_client `:219-231` / speed_client `:318-330`): `own_addr_type=0`, `filter_duplicates=0`, `seek_filter_policy=0`, `seek_phys=1` (1M), `seek_type[0]=0` (passive), `seek_interval[0]=seek_window[0]=100` (12.5 ms, 0.125 ms units).

**For our dongle to see the WS63:** scan passively on **1M PHY, channels 77/78/79, no duplicate filter**; the peer MAC for the speed server is fixed at `11:22:33:44:55:66`. Note our current stack has **no exposed scan API** (`ssap_link.h` only offers connect); `hwsle-probe.sh` is raw-event only.

---

## 6. Broadcast name / service UUID constants (must-match table for live probing)

| Constant | Value | Location |
|---|---|---|
| Service UUID | **0xABCD** | `sle_uuid_server.h:18`, `sle_speed_server.h:18` |
| Property UUID | **0x1122** | `sle_uuid_server.h:21`, `sle_speed_server.h:21` |
| UUID 128-bit base | `37 BE A8 80 FC 70 11 EA B7 20 00 00 00 00 00 00` (16-bit value at bytes 14-15 LE) | `sle_uuid_server.c:46-47` |
| App/register UUID | `{00 00}` (len 2) | `sle_uuid_server.c:32` |
| Adv name (uuid server) | `sle_uuid_server` | `sle_server_adv.c:35-36` |
| Adv name (speed server) | `sle_uart_server` | `sle_speed_server_adv.c:36` |
| Speed server MAC | `11:22:33:44:55:66` (type 0) | `sle_speed_server.c:384` |
| Speed client MAC | `13:67:5c:07:00:51` | `sle_speed_client.c:303` |
| MTU (uuid client / speed) | 300 / 512 (samples), 250 (vendor) | `sle_uuid_client.c:18`, `sle_speed_client.c:23` |
| Property permissions | READ|WRITE = 0x03 | `sle_uuid_server.h:24` |

To scan-match the WS63: match adv data TLVs `{len=2, type=0x01, value=0x01}` (discovery level NORMAL) and `{len=2, type=0x02, value=0x00}` (access mode); name is in the **scan response** (type 0x0B), not the announce payload.

---

## 7. Interop runbook for tomorrow (board + PC dongle)

### Board firmware (two WS63 boards, HiHope_NearLink_DK_WS63E_V03)

Build (from the SDK `src/` tree, per `vendor/.../demo/sle_led/README.md:121` and `tools/README.md`):

```sh
cd /mnt/hdd/nearlink-stuff/fbb_ws63/src
python3 build.py -c ws63-liteos-app menuconfig
# menu: Application → Enable Sample → (SLE) pick one of:
#   SAMPLE_SUPPORT_SLE_UUID_SERVER_SAMPLE
#   SAMPLE_SUPPORT_SLE_UUID_CLIENT_SAMPLE
#   SAMPLE_SUPPORT_SLE_SPEED_SERVER_SAMPLE / SAMPLE_SUPPORT_SLE_SPEED_CLIENT_SAMPLE
# (samples/bt/sle/Kconfig:9-19; for the vendor throughput demo instead use the
#  build_config.json entries: CONFIG_SAMPLE_SUPPORT_SLE_SERVER_SPEED=y / ..._CLIENT_SPEED=y)
python3 build.py -c ws63-liteos-app
```

Flash: HiSpark Studio → 工程配置 → 程序加载 → serial (CH340G COM port) → 程序加载 → press board reset when prompted (`tools/README.md:89-113`).

**Step 0 (board-only sanity):** flash uuid_server on board A, uuid_client on board B; reset both; watch UART (test_suite_uart) for `[uuid server] init ok`, service/property handles, and the client's read dump. This validates both boards and the stock flow with zero PC involvement.

### PC ↔ WS63 (our stack as client)

1. **Board:** flash `sle_speed_server` (fixed MAC `11:22:33:44:55:66`, name `sle_uart_server`) — the only server variant with a deterministic address.
2. **Verify adv:** run `bash scripts/hwsle-probe.sh --seconds 10` and confirm a seek/announce event carrying channels 77-79 / discovery-level TLV appears from MAC `11:22:33:44:55:66`. (`hwsle-probe.sh` only observes raw events; it cannot connect.)
3. **Connect:** use `ssap_link_connect(link, peer={11 22 33 44 55 66}, &param)` — this drives DLI_CREATE_CONNECTION 0x1401 → CONN_COMPLETE 0x0015 → READ_REMOTE_VERSION 0x1802 → SET_DATA_LEN 0x1804 (`ssap_link.h:6-8`). Set `param.connIntervalMin/Max` to match the server's negotiated 1.25 ms expectation (speed server uses 0x0A); supervision 0x1F4.
4. **SSAP negotiation:** send `EXCHANGE_INFO_REQ` mtu=512 ver=0x0301 (`ssap_codec.h:99-105`); expect the WS63 to answer 512/1.x.
5. **Discover:** `FIND_STRUCTURE_REQ` type=PRIMARY_SERVICE start=1 end=0xFFFF; parse response members `{start u16, end u16, type u8, uuid u16}` (`ssap_pkt.h:37-40`). Assert UUID = 0xABCD.
6. **Trigger throughput:** `READ_REQ {handle=service_start_hdl, type=0}` → the WS63 read_request_cbk spawns the 1,000,000-notification burst (§1.3). Optionally first send a `WRITE_REQ` of `{11 22 33 44}` to mirror the sample.
7. **Measure:** count `VALUE_NTF (0x0F)` frames on tcid 0x0A in `hwsle_transport` recv callback; throughput = `236 × N × 8 / t`. Also do a plain `READ_RSP` read first to validate bidirectional SSAP on a single handle.

### PC ↔ WS63 where *we* are the server (later)

- Flash WS63 `sle_uuid_client` (or speed client); it scans and connects to the first server it sees (uuid) or MAC 11:22:33:44:55:66 (speed client). Our server must then allocate a service 0xABCD with a property 0x1122 and answer find/read/write.
- **Handle-base mismatch to watch:** our `ssap_server` allocates from 0x0001 (`ssap_server.c:16`), OHOS/WS63 from 0x0010 (`ssap_handle.h:27`). WS63 clients discover handles dynamically, so a nonzero base is tolerated, but our READ_RSP layout (`handle+len+value`, per `OSPL-SSAP-COMPARE.md:93`) and missing WRITE_RSP (we reply ERROR_RSP to WRITE_REQ, `ssap_server.c:181-183`) will break the WS63 client — fix both before attempting this direction.

---

## 8. Open questions

1. **Response semantics:** the uuid server never calls `ssaps_send_response()`; does the WS63 bts auto-respond to READ/WRITE with the cached property value, with an error, or with silence? Determines what our PC client sees on a bare read. (`sle_uuid_server.c:62-74`)
2. **Write-to-service-handle semantics:** the sample client writes to the *service* start handle; is the bts lenient about writing any attribute in the service range, or does it map it to the first property? Affects whether our client must target start_hdl or the property handle. (`sle_uuid_client.c:151`)
3. **SSAP tcid on WS63:** do WS63 SSAP PDUs ride tcid 0x0A (our assumption, `hwsle_transport.h:27`) or another channel? Must be confirmed by sniffing the ACB stream.
4. **Conn-interval units:** SDK comments are inconsistent (125 µs in the sample adv files vs 0.25 ms in `sle_device_discovery.h:204-211` vs 1.25 ms in our `ssap_link.h:50-51`); the actual DLI units must be calibrated against the WS63 speed numbers.
5. **Handle base:** our server base 0x0001 vs WS63/OHOS 0x0010 — only matters for the "we are server" direction, but affects any hardcoded handle assertions in tests.
6. **Fixed MAC:** the uuid-server sample sets no local address; to make PC→uuid_server deterministic, either use the speed server or capture the board's MAC from the client seek log.
7. **Notification gating:** the server's notify is gated on the client CCCD (`sle_ssap_server.h:703-763`) but the samples never write the CCCD descriptor — need to confirm the bts allows notify without it (the speed demo obviously works, so likely it does).
