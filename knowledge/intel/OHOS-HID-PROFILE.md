---
type: intel
title: "OHOS HID-over-SSAP Profile — Server/Client Blueprint for a TV-Box NearLink Remote"
language: zh
created: 2026-08-17
tags: [intel, ohos, ssap, profile]
sources:
  - "https://github.com/openharmony/communication_nearlink_service"
trust: B
stale_after: 2027-02-17
---

# OHOS HID-over-SSAP Profile — Server/Client Blueprint for a TV-Box NearLink Remote

**Date:** 2026-08-17
**Scope:** Read-only local dissection of OpenHarmony `communication_nearlink_service` SSAP-service-layer HID / BAS / ACTM profiles, mapped onto our WS73 host stack. No network/build/hardware.
**Companion notes:** `NEARLINK-CONTROLLER.md` (community gamepad: frame aggregation / dirty retry / reconnect replay + HarmonyOS HID mapping), `HOPERUN-DEMOS.md` (tp78_v3 2K keyboard+dongle product validation), `stack/ssap/` (our stack), `WS63-SSAP-API.md`.

---

## 1. Sources

All `file:line` refer to files under `https://github.com/openharmony/communication_nearlink_service/tree/master/` unless otherwise stated.

| Item | Path |
|---|---|
| HID profile (client) | `services/stack/src/cp/bal/profile/hid/src/hid_ssap.c` (132), `hid_stm.c` (773), `hid_utils.c` (193), `hid_type.h`, `hid_def.h`, `hid_client.c`, `hid_client_api.c`, `hid_common.c` |
| HID includes | `profile/hid/include/hid_def.h`, `profile/hid/src/hid_type.h` |
| BAS profile | `profile/bas/src/bas_ssap_cbk.c`, `bas_stm.c`, `bas_common.c`, `bas_client.c`, `bas_def.h`, `profile/bas/include/nlstk_bas_def.h` |
| Audio control (ACTM) | `audioctl/actm/src/actm_ssap.c` (716), `audioctl/actm/include/inner/actm_ssap.h` |
| SSAP client/server APIs | `services/stack/src/cp/bsl/sle/servm/ssap/include/nlstk_ssap_app_client.h`, `nlstk_ssap_app_server.h`, `nlstk_ssap_app_link.h`, `ssap_type.h`; server reg example `profile/dis/src/dis_server.c` |
| Wire message codes | `services/stack/src/cp/bsl/sle/servm/ssap/include/inner/ssap_pkt.h` |
| HID IT test (wire vectors) | `test/unittest/stack_test/hid_test/hid_test.cpp` |
| Std UUID table | `utils/include/sle_uuid.h`, `interfaces/def/nearlink_def.h:57-58` |
| Our stack | `stack/ssap/` (ssap_server.h/c, ssap_link.h/c, ssap_pkt.h, ssap_codec.h, hwsle_transport, feature_mgr) |

---

## 2. HID 服务架构 — how the HID profile sits on SSAP

### 2.1 The HID profile is a pure **client** on the SSAP link; the profile is a state machine over SSAP app-client calls

`hid_ssap.c` is only callback glue: it registers `NLSTK_SsapAppClientCb_S` (9 callbacks: `onFindServiceByUuid`, `onConnectionStateChanged`, `onReadProperty`, `onGetPropertyNtf`, `onSetPropertyNtf`, `onPropertyChanged`, `onWriteProperty`, `onRegisterApp`, `onGetServices`), each one does `HidFindDeviceByAppId(appId)` then dispatches into the per-device state machine (`hid_ssap.c:36-50`, `hid_ssap.c:52-133`). No profile logic lives in `hid_ssap.c`.

The connection state machine (`hid_stm.c:39-56`) is:

```
HID_STATE_UNINIT --HID_ON_USER_CONNECT--> INIT --NLSTK_SsapClientRegAppAsyn(addr,connParam,cb)--> HID_ON_REGISTER_APP
  --> CREATE_LINK --NLSTK_SsapClientConnect(appId)--> wait onConnectionStateChanged==SSAP_CONNECT_STATE_CONNECTED
  --> GET_SERVICE --NLSTK_SsapClientGetServicesAsyn(appId)--> HID_ON_GET_SERVICE (discover 0x060B service + members)
  --> READ_PROPERTY (read report-map, each report-index, each report property)
  --> SET_NOTIFICATION (NLSTK_SsapClientSetPropertyNtf on every INPUT report handle)
  --> CONNECTED
```

- `NLSTK_SsapClientRegAppAsyn` (`hid_stm.c:79-87`) is the first step — each profile is a separate **SSAP appId** on the same DLI link (`NLSTK_SSAP_CLIENT_APP_MAX_NUM 50`, `nlstk_ssap_app_client.h:26`). BAS opens a second appId on the same link.
- The client initiates the link (`NLSTK_SsapClientConnect`, `nlstk_ssap_app_link.h:35`), so in the OHOS model the **HID server is on the peer peripheral** and the **profile client runs on the central/host**.
- On disconnect, the state machine tears down with `NLSTK_SsapClientDisconnect` + `NLSTK_SsapClientDeregAppAsync` + device removal (`hid_stm.c:148-156, 211-218`).

### 2.2 Service / property layout (server side, what a remote must expose)

UUIDs (`hid_type.h:36-55`; confirmed in `sle_uuid.h:44-45`, `nearlink_def.h:57-58`):

| Role | 16-bit UUID | 16-bit UUID_PEN (big-endian, "old devices") | Value format |
|---|---|---|---|
| Service | **0x060B** | 0x0B06 | primary service |
| Report Map | 0x1039 | 0x3910 | `type(1)` + report-descriptor bytes (`hid_stm.c:389-406`: `desc.type = data[0]`, `descLen = len-1`) |
| Work State | 0x103A | 0x3A10 | 1 byte |
| Report Index | 0x103B | 0x3B10 | **8 bytes**: `reportId(1) + reportType(1) + reportHandle(2 LE) + reportSrcPort(2 LE) + reportDestPort(2 LE)` (`hid_def.h:122-128`, parse `hid_stm.c:315-320`); **one instance per report** |
| Input Report | 0x103C | 0x3C10 | raw report bytes only (no id/type prefix) |
| Output Report | 0x103D | 0x3D10 | raw report bytes only |
| Feature Report | 0x103E | 0x3E10 | raw report bytes only |

- All UUIDs are 16-bit values carried inside the standard 128-bit base `37BE-A880-FC70-11EA-B720-000000000000`, with the 16-bit value in **bytes 14-15 big-endian** (`hid_utils.c:23-24, 64-81`). Both std and big-endian ("PEN") variants are matched (`hid_stm.c:161-180`, `hid_utils.c:31-62`).
- Report types: 0x01 INPUT, 0x02 OUTPUT, 0x03 FEATURE (`hid_def.h:46-51`).
- The test cache shows the canonical server layout: service handle 0x10 (end 0x18), report-map 0x11, work-state 0x12, **three report-index properties 0x13/0x14/0x15** (one per report), input 0x16, output 0x17, feature 0x18 (`hid_test.cpp:175-226`).

### 2.3 Notification channel

- **Client subscribes per INPUT report property**: `NLSTK_SsapClientSetPropertyNtf(appId, reportHandle, true)` for every report with `reportType == HID_INPUT_REPORT_TYPE` (`hid_stm.c:444-460`). Only input reports are subscribed (comment `hid_stm.c:706`).
- The subscribe is a **client-property-config descriptor (CPCD) write**: `NLSTK_SsapClientSetPropertyNtf` → `SsapClientSetCpcd` → `SsapcAppSetCpcd` (`nlstk_ssap_app_client.c:441-462`). On the wire this is the **SET_CPCD PDU (msgCode 0x0E)** — see test vector `{0x0E, 0x03, 0x16, 0x00, 0x02, 0x01, 0x00}` = CPCD write on input-report handle 0x16, enable=1 (`hid_test.cpp:355-357`).
- Incoming report: `onPropertyChanged` → `HID_ON_NOTIFY_PROPERTY` → handler **filters `uuid == HID_INPUT_REPORT_UUID(_PEN)`**, resolves report (id/type) by handle from the report-index table, then delivers `HidReportInfo_S{reportId, reportType, value}` (`hid_stm.c:701-719`). The wire payload of VALUE_NTF (msgCode 0x0F) carries **only the raw report bytes** — id/type are looked up locally, not transmitted per report (`hid_test.cpp:429-432`: `{0x0F, 0x03, 0x16, 0x00, 0x04, 0x00, 0x04, 0x03, 0x02, 0x01}`).
- Descriptor machinery on the server: a property carries `descriptors[]` with `DESC_TYPE_CLIENT_CONFIG` (2), operation bit `SSAP_OPERATE_INDICATION_DESCRIPTOR_CLIENT_CONFIGURATION_WRITE` (0x200) (`ssap_type.h:110-135`; server-side example `dis_server.c:40-52`). This is the SSAP analogue of the BLE CCCD.

---

## 3. 报告流 — report streaming

### 3.1 Input reports (remote → host), SSAP notify

- One `VALUE_NTF` per report instance. There is **no batching / no frequency shaping in the profile itself** — the profile is a dumb pipe; rate control lives in the app (see §6/§7, where the community controller's seq-dedup + dirty-retry state machine is the required upper layer).
- Inbound notify PDU is decoded by the client into `NLSTK_SsapClientReadPropertyInfo_S{handle, uuid, errorCode, value}` (`nlstk_ssap_app_client.h:80-86`). Per-notify `SDF_MemZalloc` copies and a per-device vector of `HidReport_S` (`hid_type.h:112-119`) are the only bookkeeping.
- The host app receives each report as a callback (`HidNotifyPropertyCbk`, `hid_def.h:62`); `HidGetInformation` hands the whole descriptor + report table up (shallow copy, `hid_stm.c:521-541`).

### 3.2 Output reports (host → remote), write callback

- Host → remote: `NLSTK_SsapClientWriteProperty(appId, reportHandle, value, true)` — the trailing `true` = **write without response** (`nlstk_ssap_app_client.h:280-281`; `hid_stm.c:590-610`). Payload is raw report bytes (`hid_utils.c:169-185`). Applies to OUTPUT and FEATURE reports; work-state writes also use without-response (`hid_stm.c:589-595`).
- Remote → host: on the server, a WRITE_REQ/WRITE_CMD PDU hits the property's `write_cb`; WRITE_REQ requires a WRITE_RSP (result 0 success / 1 error items) while WRITE_CMD (0x0C) is fire-and-forget (`ssap_pkt.h:133-136, 394-426`; our server `ssap_server.c:190-206`).
- Read path is symmetric: `NLSTK_SsapClientReadProperty(appId, handle)` per property; report index read parses the 8-byte struct and chains to reading each `reportHandle` (`hid_stm.c:278-343`).

---

## 4. 与 BLE HID 异同 — vs BLE HID over GATT

| Aspect | BLE HID over GATT (standard) | SSAP HID (this profile) |
|---|---|---|
| Service UUID | 0x1812 | **0x060B** (own allocation in NearLink std space; `sle_uuid.h:44-45`) |
| Report map | characteristic 0x2A4B | property 0x1039, `type(1)+descriptor` |
| Report | characteristics 0x2A4D/0x2A4E/0x2A4F | properties 0x103C/0x103D/0x103E |
| **Report reference** | Report Reference char 0x2908 (id+type 2 bytes) | **Report Index property 0x103B (8 bytes: id+type+reportHandle+srcPort+destPort)** — an explicit indirection level; handle→report table, no per-report id/type on the wire |
| **Protocol/control** | Protocol Mode 0x2A4E, HID Control Point 0x2A4C, Boot info | collapsed into a single **Work State** property 0x103A (1 byte) |
| Information / ext refs | HID Information 0x2A4A, External Report Reference | none — type byte in report map covers descriptor-vs-other formats |
| Notification gating | CCCD 0x2902 write | **CPCD** descriptor write (SET_CPCD PDU 0x0E); same semantics (`ssap_type.h:130`) |
| Write | WRITE without response typical | `writeWithoutRsp=true` default (`hid_stm.c:590-610`) |
| UUID encoding | 16-bit GATT | 16-bit folded into 128-bit base, big-endian at bytes 14-15; both byte orders accepted (`hid_utils.c:23-24, 64-81`) |
| Discovery | GATT primary-service discovery | `NLSTK_SsapClientGetServicesAsyn` (SSAP find service / member enum) (`nlstk_ssap_app_client.h:258-267`) |
| Service model | characteristics only | **properties + methods + events** (`nlstk_ssap_app_client.h:46-58`) — extra kinds unused by HID but used by ACTM |

**Verdict: HID-over-SSAP is a direct structural transplant of BLE HID over GATT** (report map / input+output+feature reports / CCCD-equivalent / write-no-rsp), with the additions of a Report-Index indirection (with vestigial data-plane port fields — set to 0xFFFF in the reference test, `hid_test.cpp:340-342`) and a collapsed control/information model. Report descriptor bytes, report payload bytes and report-id semantics are identical to HID-over-GATT; the same HID report descriptor can be reused verbatim.

---

## 5. BAS — battery service

- **Separate profile = separate appId** on the same link. `bas_ssap_cbk.c:32-43` registers only 6 callbacks (registerApp, connectStateChanged, getServices, readProperty, propertyChanged, setPropertyNtf) — no write needed.
- Service 0x060A; properties: 0x1034 REMAIN_BATTERY_PERCENTAGE (mandatory, 1 byte), 0x1035 REMAIN_BATTERY, 0x1036 BATTERY_CAPACITY, 0x1037 BATTERY_RATED_CAPACITY, 0x1038 REMAINING_WORKING_TIME (`bas_def.h:41-49`). Non-percentage values are 4 bytes (`BAS_PROPERTY_LENGTH 4`, `bas_def.h:32`).
- Flow mirrors HID: connect → `NLSTK_SsapClientGetServicesAsyn` → read every property (`bas_stm.c:248-262`) → **subscribe notify only on the percentage property** (`bas_stm.c:346-357`) → connected. Notify handler checks `handle == remainBatPctHdl` and **requires exactly 1 byte**, then `BasNotifyCbk(addr, BAS_BATTERY_PERCENTAGE, value)` (`bas_stm.c:368-385`).
- Host-initiated poll: `BasGetBatteryLevelTask` → `NLSTK_SsapClientReadProperty(appId, lastReadHandle)` (`bas_client.c:94-101`). So battery is available both push (notify on change) and pull (read).
- Value layout is thus dead simple for a TV-box remote: `0x1034` = current percent (0-100).

---

## 6. 音频控制 — audio control is also an SSAP service (and shows the methods/events model)

`actm_ssap.c` demonstrates the richer SSAP service model and is the template for any control/feedback channel on the remote:

- **Two services**: Audio Stream Management **0x0605** and Audio Public Property **0x0606** (`actm_ssap.h:28-29`). Properties 0x1015-0x1021 (`actm_ssap.h:33-45`).
- **Control is a METHOD, not a property write**: `AUDIO_CONTROL_POINT_UUID 0x1017` is found in `service->methods[]` and invoked via `NLSTK_SsapClientCallMethod(appId, handle, value, withoutRsp=false)` (`actm_ssap.c:109-114, 209-236`). Payload: `opcode(1) + pointNum(1) + per-point {pointId(1) + data}`; reply comes back on `onCallMethod` (`actm_ssap.c:280-312`). This is the right pattern for remote volume/media-key commands that need an ACK.
- **Async feedback is an EVENT**: `AUDIO_STREAM_STATE_CHANGE_UUID 0x1018` lives in `service->events[]`; the client receives it on `onEvent` (`actm_ssap.c:115-119, 667-706`). Events are the SSAP-native way to push unsolicited state (volume-step/bitrate/state transitions) without a notify subscription.
- Property notifications are also used (location, available stream type) via `NLSTK_SsapClientSetPropertyNtf` (`actm_ssap.c:157-169, 652-665`).
- **Batch reads**: `ReadDeviceProperties` uses `NLSTK_SsapClientReadProperties(appId, handles, num)` when `SsapcIsSupportMultiProcessing` (`actm_ssap.c:84-97, 590-609`) — multi-read exists in SSAP.

For the TV-box remote: volume keys / media keys can ride the same HID output/feature report path, or — if ack or state feedback is wanted — a tiny control **method** + **event** pair on the HID service (or a vendor service). That is much closer to BLE's HID control point than anything in the plain HID profile.

---

## 7. 电视盒遥控蓝本 — TV-box remote blueprint

### 7.1 Roles and topology

- **Remote (peripheral)** = SSAP **server** exposing the standard HID service 0x060B (+ optional BAS 0x060A). This is exactly what a Hi2821/BS21/WS63 chip-side firmware builds with `SsapsRegisterServer`/`SsapsNotifyIndicate` (WS63-SSAP-API.md / HHD01-BOARD.md §4 `23_sle_uart` pattern).
- **TV box + WS73 dongle (central/host)** = SSAP **client** running the hid profile (the role `hid_stm.c` implements). This is the direction our stack must add (see §8).
- The OHOS reference contains **only the client** for HID (`profile/hid/`); the server side is implied by the client's expectations + `dis_server.c`'s registration pattern (`NLSTK_SsapServerRegAppAsyn` + `NLSTK_SsapServerAddService` + per-property `SSAP_OPERATE_INDICATION_*` ops, `dis_server.c:118-135`). We must build both.

### 7.2 Server property table for the remote (target design)

Base UUID `37BE-A880-FC70-11EA-B720-0000-0000-0000`, 16-bit UUID big-endian at bytes 14-15.

| Service | Handle (suggested) | Property | UUID | Op bits | Value |
|---|---|---|---|---|---|
| HID 0x060B (primary) | 0x10-0x1A | Report Map | 0x1039 | READ | `0x01` + HID report descriptor (e.g. 15-byte gamepad layout from NEARLINK-CONTROLLER §3, or a slim keyboard/mouse descriptor) |
| | | Work State | 0x103A | READ+WRITE_NORSP | 1 byte |
| | | Report Index (1) | 0x103B | READ | `{reportId=1, type=0x01, reportHandle, 0xFFFF, 0xFFFF}` (8B LE) |
| | | Input Report | 0x103C | READ+NOTIFY, CPCD descriptor (type 2, op 0x200) | raw report bytes (e.g. 15 B / ≤ payload) |
| | | Output Report | 0x103D | WRITE_NORSP | raw bytes (LED / rumble / IR feedback) |
| | | Feature Report | 0x103E | READ+WRITE | raw bytes (config) |
| BAS 0x060A (primary) | 0x20+ | Remain Battery Percentage | 0x1034 | READ+NOTIFY, CPCD descriptor | 1 byte percent; push on change, pull via read |

Notes:
- One report-index per report; multiple reports → multiple 0x103B instances + matching report properties. A TV remote realistically needs 1 input report (+1 output for LED).
- **Follow the exact 8-byte Report Index format** so an OHOS phone or our dongle client interoperates; per-report id/type are NOT on the data path (wire carries raw bytes).
- CPCD descriptor on the input report is required for notification gating (`ssap_type.h:130`; `dis_server.c:40-52` for the registration recipe).
- Battery: percent push + read, 1 byte (`bas_stm.c:346-385`).

### 7.3 Data plane: reuse the community state machine on top of SSAP notify

Per NEARLINK-CONTROLLER §2, the SSAP profile gives a dumb pipe; the TV-box remote needs the **chip-side** (server) and **host-side** (client) state machine:

- Chip side (`controller.c` pipeline, ported): poll inputs → filter → build one compact HID report → seq-dedup (send only on change) → dirty retry (retry failed frame before sampling new input, no backoff timer, poll tick is the throttle) → three-report cache (processed/sent/pending) → 30 s idle → power-saving.
- Host side (dongle): SSAP notify receive → per-report **dedup by content/reconnect replay** (reconnect = send latest state frame, per the implemented semantics — the controller.c behavior, which is better than the spec's replay-last-sent) → map to evdev/uinput.
- The 2K keyboard+dongle product (tp78_v3) proves the route end-to-end (HOPERUN-DEMOS §2.4, COMMUNITY-PROJECTS). For Linux TV boxes the right-stick Z/Rz mapping caveat of NEARLINK-CONTROLLER §3 does not bite (remotes have no sticks); for HarmonyOS TV boxes the ABS_* table applies directly.

### 7.4 What the WS73 dongle side (our stack, acting as host) must implement

Concretely, port of the OHOS hid profile client onto our stack:

1. **SSAP client role** — our `stack/ssap/` currently has server (`ssap_server.h/c`), link (`ssap_link.h/c`), codec/pkt, transport; **there is no client module**. Needed: service discovery (find primary service + members), `ReadProperty`/`ReadProperties` (batch), `SetPropertyNtf` (CPCD write, SET_CPCD PDU 0x0E), `CallMethod` (0x1017-style), and **VALUE_NTF (0x0F) / VALUE_IND reception dispatch** (`ssap_server.c:212-222` encodes notify; the client must decode).
2. **HID profile state machine** — a direct port of `hid_stm.c` (7 states) + `hid_ssap.c` (callback→STM glue) + `hid_type.h` UUID/format tables, driving our `ssap_link` (DLI connect 0x1401/0x1804 per `ssap_link.h:21-26`).
3. **Report layer** — the three-cache/dedup/reconnect-replay state machine (NEARLINK-CONTROLLER §2) on the notify receive path, then evdev/uinput injection.
4. **BAS client** — the 6-callback mini-profile (`bas_stm.c`) for battery percent; trivial.

---

## 8. 结论 — migration cost vs BLE HID, and what our stack must add

- **HID-over-SSAP is structurally ≈ HID-over-GATT**: same report semantics (map + input/output/feature + raw payloads + id/type), same descriptor bytes, same notification gating (CPCD ≈ CCCD), same write-no-rsp default. A BLE HID device server-side translates to SSAP nearly 1:1; only the UUIDs, the 8-byte Report Index indirection and the folded 128-bit encoding differ.
- The **uniquely SSAP parts** are the service model extras — **methods** (control point, acked commands like volume) and **events** (async state feedback) — which are *cheaper and cleaner* than BLE's char+indicate workarounds.
- The **client-side cost is the real work for us**: OHOS ships the client (`hid_stm.c`/`hid_ssap.c`) but our stack has no SSAP client. We must add: service-discovery client, read (single+batch), CPCD write, VALUE_NTF decode, plus the HID profile STM port, plus the report dedup/reconnect-replay layer, plus uinput injection. The server side (remote firmware) is small and template-shaped after `dis_server.c` + `23_sle_uart`.
- End-to-end product validation already exists in the market (tp78_v3 SLE 2K keyboard+dongle; Hi2821 4K gamepad), so the HID-over-SSAP remote route for the TV box is low-risk given the OHOS reference is byte-level portable.

---

## Open questions

1. CPCD exact PDU layout on the wire (SET_CPCD 0x0E: control/type/enable byte meanings) — our client encoder must match `ssaps_server` decoding; only the test vector `{0x0E,0x03,handle,0x02,0x01,0x00}` is available (`hid_test.cpp:355`).
2. Whether the WS73 dongle should also expose a **server** role (e.g., to mirror remote state to a second device) — `ssap_server.h` exists but its find/read/write/notify path has not been exercised against an OHOS client.
3. Whether reportSrcPort/reportDestPort (0xFFFF in the reference) matter for any in-market remote, or are pure vestiges — affects whether we can emit 0xFFFF unconditionally.
4. Multi-connection policy: OHOS caps at 50 apps / single link per appId; a TV box with 2 remotes needs per-addr appId handling in our link layer (`ssap_link` currently holds one target address, `ssap_link.h:78`).
5. Notify frequency ceiling: VALUE_NTF min PDU 7 bytes (`ssap_pkt.h:102`); achievable HID report rate over SLE 12 Mbps with 1-4K polling depends on conn-interval/MTU negotiation — needs hardware measurement (matches NEARLINK-CONTROLLER OQ-T3).
6. Whether OHOS HID client requires the Report Index before it subscribes (yes, by STM order) — remote firmware must expose it even if the index data is static.
