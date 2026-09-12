---
type: intel
title: WS63 (HiSilicon SLE SDK) Connection Management & Device Discovery — Cross-Validation
language: zh
created: 2026-08-17
tags: []
---

# WS63 (HiSilicon SLE SDK) Connection Management & Device Discovery — Cross-Validation

Date: 2026-08-17

Scope: cross-check our WS73 `ssap_link.c` connection FSM and DLI broadcast/scan
decodes against the Runhe (润和) WS63 SDK's official HiSilicon SLE API headers and
reference demos. Local files only; no network/build/hardware.

## Sources

WS63 SDK (`/mnt/hdd/nearlink-stuff/fbb_ws63/src/`):
- `include/middleware/services/bts/sle/sle_connection_manager.h` (952 lines)
- `include/middleware/services/bts/sle/sle_device_discovery.h` (871 lines)
- `include/middleware/services/bts/sle/sle_common.h`, `sle_errcode.h`, `sle_transmition_manager.h`,
  `sle_low_latency.h`, `sle_ssap_client.h`, `sle_ssap_server.h`, `sle_ssap_stru.h`
- Demos: `application/samples/bt/sle/sle_uuid_client/src/sle_uuid_client.c`,
  `.../sle_uuid_server/src/sle_uuid_server.c`, `.../sle_uuid_server/src/sle_server_adv.c`,
  `.../sle_speed_client/src/sle_speed_client.c`, `.../sle_speed_server/src/sle_speed_server.c`,
  `.../sle_speed_server/src/sle_speed_server_adv.c`
- Porting layer: `application/ws63_porting_xf_0p2/port_xf/port_xf_sle/port_xf_sle_connection_manager.c`

Our side:
- `stack/ssap/src/ssap_link.c`, `stack/ssap/include/ssap_link.h`
- `.scratch/nearlink-driver/lab-notes/OSPL-CONN-FSM.md`
- `.scratch/nearlink-driver/lab-notes/SLE-CONTROL-PLANE.md`

Note: this SDK ships headers + demos + a thin porting layer only; the SLE stack
itself is prebuilt firmware (no middleware SLE source under `middleware/`), so the
API→DLI wire mapping below is inferred from header docs and demo behavior.

---

## 1. Connection API inventory (`sle_connection_manager.h`)

Link establishment / teardown:
- `errcode_t sle_connect_remote_device(const sle_addr_t *addr)` — connect to a peer; takes **address only**
  (`sle_connection_manager.h:585`). Result arrives via `connect_state_changed` callback.
- `errcode_t sle_disconnect_remote_device(const sle_addr_t *addr)` — disconnect by address (`:604`).
- `errcode_t sle_disconnect_all_remote_device(void)` (`:623`).
- `errcode_t sle_update_connect_param(sle_connection_param_update_t *params)` — param renegotiation
  (`:644`); fields `conn_id/interval_min/interval_max/max_latency/supervision_timeout` (`:150-161`).
- `errcode_t sle_default_connection_param_set(sle_default_connect_param_t *set_param)` — **global** default
  connect params applied to all future connects (`:904`); struct at `:293-312`:
  `enable_filter_policy, initiate_phys (1:1M/2:2M), gt_negotiate, scan_interval, scan_window,
  min_interval, max_interval, timeout`.

Pairing / security (orthogonal to link FSM):
- `sle_pair_remote_device` (`:665`), `sle_remove_paired_remote_device` (`:686`), `sle_remove_all_pairs` (`:705`),
  `sle_get_paired_devices_num` (`:726`), `sle_get_paired_devices` (`:749`), `sle_get_bonded_devices` (`:770`),
  `sle_get_pair_state` (`:793`).

Link tuning / post-connect:
- `sle_read_remote_device_rssi(conn_id)` (`:814`), `sle_set_phy_param(conn_id, sle_set_phy_t*)` (`:837`),
  `sle_set_mcs(conn_id, mcs)` (`:860`), `sle_set_data_len(conn_id, tx_octets)` (`:883`),
  `sle_customize_max_pwr(ble_pwr, sle_pwr)` (`:944`).
- Low-latency (ACB) via `sle_low_latency.h`: `sle_low_latency_set(conn_id, enable, rate)`
  with rates 125 Hz–8 kHz (`sle_low_latency.h:32-54,268`).

Param-update structs:
- `sle_connection_param_update_req_t` — interval_min/max, max_latency, supervision_timeout
  (`:132-141`); comment units: interval/latency **slot**, supervision **10 ms**.
- `sle_connection_param_update_evt_t` — negotiated `interval, latency, supervision` (`:170-177`).

Note: `sle_connect_remote_device` carries NO per-connect parameters. All link params come from either
(1) `sle_default_connection_param_set` (host global), or (2) the **announce** side for the G role via
`sle_announce_param_t.conn_*` fields (`sle_device_discovery.h:204-219`), or (3) a later
`sle_update_connect_param`. This is the key semantic difference from our `ssap_conn_param_t` which embeds
the full `DLI_ConnectionCreateParam` in every 0x1401.

---

## 2. Connect params field comparison

WS63 `sle_default_connect_param_t` (`sle_connection_manager.h:293-312`) vs our `ssap_conn_param_t`
(`ssap_link.h:45-62`):

| Semantic field | WS63 `sle_default_connect_param_t` | Ours `ssap_conn_param_t` (WS73 DLI) | Match |
|---|---|---|---|
| scan interval | `scan_interval` u16 (`:300`) | `scanInterval` u8, 0.125 ms, default 0x20 (`ssap_link.h:56`) | field matches; width/units unverified on WS73 |
| scan window | `scan_window` u16 (`:304`) | `scanWindow` u8, default 0x20 (`:57`) | field matches |
| min/max link interval | `min_interval`/`max_interval` u16 (`:306-309`) | `connIntervalMin/Max` u16, "1.25 ms" (`:50-51`) | **unit mismatch to verify** — see Open questions (U1) |
| supervision timeout | `timeout` u16 (`:310`) | `supervisionTimeout` u16, "10 ms" (`:53`) | match on 10 ms unit |
| PHY | `initiate_phys` (1:1M/2:2M) (`:296`) | `initiatePhys` bit0=1M (`:59`) | match |
| filter | `enable_filter_policy` (`:294`) | `filterPolicy` (`:60`) | match |
| G/T interaction | `gt_negotiate` (`:298`) | (none — role not in param) | WS63 has role; ours implicit |
| max latency / CE length | (not in default struct) | `maxLatency`, `minCeLength`, `maxCeLength` (`:52,54-55`) | ours richer |
| own addr type | (not in default struct) | `ownAddrType` (`:61`) | ours richer |
| version / local index | (not in default struct) | `version`, `localIndex` (`:46-47`) | ours richer |

The WS63 API also carries connect params inside the announce struct when acting as G
(`sle_announce_param_t.conn_interval_min/max`, `conn_max_latency`, `conn_supervision_timeout`;
`sle_device_discovery.h:204-219`), which is exactly how the uuid/speed server demos advertise the
parameters they want (e.g. `sle_server_adv.c:128-131` sets interval 0x64, latency 0x1F3,
supervision 0x1F4).

`ssap_conn_param_t` is a superset of the WS63 default-connect model and mirrors the full
`DLI_ConnectionCreateParam`; nothing in the WS63 API contradicts our wire layout except the
interval-unit question (U1) and scan interval width (u8 vs u16).

---

## 3. Event callbacks & status codes

Callback registration model (WS63):
- `sle_connection_register_callbacks(sle_connection_callbacks_t*)` (`sle_connection_manager.h:921`);
  struct members `:549-566`.
- `sle_announce_seek_register_callbacks(sle_announce_seek_callbacks_t*)` (`sle_device_discovery.h:863`).

The single connect-state callback merges connect-complete and disconnect-complete:
```
typedef void (*sle_connect_state_changed_callback)(
    uint16_t conn_id, const sle_addr_t *addr,
    sle_acb_state_t conn_state, sle_pair_state_t pair_state,
    sle_disc_reason_t disc_reason);                 // sle_connection_manager.h:343-344
```
- `sle_acb_state_t`: NONE=0x00, CONNECTED=0x01, DISCONNECTED=0x02 (`:62-69`).
- `sle_pair_state_t`: NONE=0x01, PAIRING=0x02, PAIRED=0x03 (`:32-39`).
- `sle_disc_reason_t`: `SLE_DISCONNECT_BY_REMOTE = 0x10`, `SLE_DISCONNECT_BY_LOCAL = 0x11` (`:48-53`).
  These are the only two disconnect reasons exposed at API level — a much coarser taxonomy than the
  raw reason bytes we carry (0x08 link-loss, 0x13 local-host, etc.).

Other callbacks: `connect_param_update_req_cb` (`:401-402`), `connect_param_update_cb` (returns
negotiated interval/latency/supervision, `:373-374`), `auth_complete_cb` (`:431-432`),
`pair_complete_cb` (conn_id/addr/status, `:459`), `read_rssi_cb` (conn_id/rssi/status, `:486`),
`low_latency_cb` (`:513`), `set_phy_cb` (`:540`).

Mapping to our FSM callbacks (`ssap_link.h:87-90`):
| WS63 callback | Ours |
|---|---|
| `connect_state_changed_cb` with `conn_state==CONNECTED` | `on_connected(conn_handle, ctx)` |
| `connect_state_changed_cb` with `conn_state==DISCONNECTED` | `on_disconnected(reason, ctx)` |
| (no direct connect-failure callback; failure → DISCONNECTED + `disc_reason`) | `on_connect_failed(status, ctx)` |
| `connect_param_update_cb` (evt interval/latency/supervision) | our 0x0019 conn-param-update handling |
| `pair_complete_cb`/`auth_complete_cb` (status) | N/A in our slim stack |

**Status codes.** The WS63 public API does not expose the wire-level DLI status byte. Error surfacing is
two-level:
1. Synchronous return `errcode_t` from each API call (`errcode_sle_t` enum,
   `sle_errcode.h:65-115`). Relevant values (base `0x80006000`): `ERRCODE_SLE_TIMEOUT` (+0x05),
   `ERRCODE_SLE_STATUS_ERR` (+0x0A), `ERRCODE_SLE_AUTH_FAIL` (+0x0C), `ERRCODE_SLE_RMT_DEV_DOWN` (+0x0E),
   `ERRCODE_SLE_PAIRING_REJECT` (+0x0F), `ERRCODE_SLE_BUSY` (+0x10), `ERRCODE_SLE_CONN_FAIL` (+0x12).
2. Async via callback `status` parameters (`pair_complete`, `auth_complete`, `connect_param_update`,
   `read_rssi`, `set_phy`) — all typed `errcode_t`.

**Our 0x06 / 0x0B / 0x0F / 0x1E wire codes have no numeric counterpart in the WS63 API.**
`ERRCODE_SLE_CONN_FAIL`/`ERRCODE_SLE_RMT_DEV_DOWN`/`ERRCODE_SLE_TIMEOUT` cover the *semantic* classes
(connect failed / peer gone / timeout), which is the strongest available confirmation that our error-code
space is a WS73-controller dialect (per `SLE-CONTROL-PLANE.md:17`: 0x0F=INVALID_PARAMS, 0x0B=CMD_DISALLOWED,
0x06=needs-connection, 0x1E=UNKNOWN_ADVERTISING_IDENTIFIER). The disconnect *reason* enum
(0x10 remote / 0x11 local) is similarly non-overlapping with our 0x08/0x13 reason bytes.

SSAP-level error codes (`sle_errcode.h:124-183`, base `0x80006100`) are GATT-like attribute errors
(PROHIBIT_READING, CLIENT_NOT_AUTHENTICATED, VALUE_OUT_OF_RANGE...) and are irrelevant to link management.

---

## 4. Supervision timeout

- Configurable in three places, always **10 ms units**:
  - `sle_default_connect_param_t.timeout` (`sle_connection_manager.h:310-311`, "链路超时时间").
  - `sle_announce_param_t.conn_supervision_timeout` (`sle_device_discovery.h:216-219`).
  - `sle_connection_param_update_req_t.supervision_timeout` (`sle_connection_manager.h:139-140`).
- Valid range per header: `[0x000A, 0x0C80]` → **100 ms – 32 s** (`sle_device_discovery.h:160-162`).
- Default used by every HiSilicon reference demo: `0x1F4` = 500 → **5 s**:
  - `sle_speed_server_adv.c:26` `SLE_CONN_SUPERVISION_TIMEOUT_DEFAULT 0x1F4`
  - `sle_uuid_server/src/sle_server_adv.c:25` (same), applied at `:131`
  - `sle_speed_client.c:31` `SPEED_DEFAULT_TIMEOUT_MULTIPLIER 0x1f4` → applied in
    `sle_default_connection_param_set` (`:297`) and `sle_update_connect_param` (`:315`)
- Enforcement lives entirely in firmware; the API only forwards the value and reports the resulting
  disconnect via `connect_state_changed` (`disc_reason = SLE_DISCONNECT_BY_REMOTE`).

**Confirmation for us:** the 5 s default and the 10 ms unit in `ssap_link.c:59`
(`supervisionTimeout 0x1F4`) and `SSAP_LINK_SUP_TO_DEFAULT_MS 5000` (`ssap_link.h:37`) are exactly what
the official HiSilicon SDK samples use. Our host-side watchdog in `ssap_link_tick` (`ssap_link.c:279-291`)
has no API-level counterpart — the WS63 SDK trusts firmware to emit the disconnect event — so our
watchdog is a valid defense-in-depth addition (matches OSPL T12 semantics).

---

## 5. Discovery (adv/scan) API — `sle_device_discovery.h`

### Announce (broadcast)
- Lifecycle: `sle_set_announce_param(announce_id, sle_announce_param_t*)` (`:745`),
  `sle_set_announce_data(announce_id, sle_announce_data_t*)` (`:701`),
  `sle_start_announce(announce_id)` (`:766`), `sle_stop_announce` (`:787`), `sle_remove_announce` (`:722`).
  Up to `SLE_ANNOUNCE_ID_MAX = 16` instances (`:31`); handle range [0,0xFF] (`:180`).
- `sle_announce_param_t` (`:178-222`): `announce_mode` (nonconn/non-scan 0, conn-non-scan 1,
  nonconn-scan 2, conn-scan 3, conn-directed 7 — `:87-98`), `announce_gt_role`
  (T/G × negotiable — `:69-78`), `announce_level` (`:49-60`), `announce_interval_min/max`
  (u32, [0x20,0xFFFFFF], **125 µs units** — `:191-194`), `announce_channel_map`
  (bits 0/1/2 = channels 76/77/78 — `:195-196`), `announce_tx_power` (dBm [-127,20], 0x7F = no pref —
  `:197-199`), own/peer `sle_addr_t` (`:200-203`), and the G-role conn params (`:204-219`).
- Data: `sle_announce_data_t` carries both announce data and scan-response data (`:231-240`).
  Demos use 251-byte buffers (`SLE_ADV_DATA_LEN_MAX 251`, `sle_server_adv.c:33`) — matches the 251 B
  max we verified on WS73 (`SLE-CONTROL-PLANE.md` round-4 note).

### Seek (scan)
- Lifecycle: `sle_set_seek_param(sle_seek_param_t*)` (`:808`), `sle_start_seek()` (`:827`),
  `sle_stop_seek()` (`:846`).
- `sle_seek_param_t` (`:267-287`): `own_addr_type`, `filter_duplicates`, `seek_filter_policy`
  (ALLOW_ALL 0 / ALLOW_WLST 1 — `:137-142`), `seek_phys` (1M/2M/4M bits — `:107-114`),
  `seek_type[3]` (PASSIVE 0 / ACTIVE 1 per PHY — `:123-128`), `seek_interval[3]`/`seek_window[3]`
  (u16 per PHY, [0x0004,0xFFFF], **0.125 ms units** — `:281-286`).
- Result callback `sle_seek_result_callback(sle_seek_result_info_t*)` (`:479`); struct
  `sle_seek_result_info_t` (`:296-312`): `event_type`, `addr` (sle_addr_t), `direct_addr`,
  `rssi` ([-127,20] dBm, 0x7F = n/a), `data_status`, `data_length`, `data*`.

### Mapping to our DLI decodes (`SLE-CONTROL-PLANE.md`)
| WS63 API struct | Our WS73 DLI decode | Notes |
|---|---|---|
| `sle_seek_param_t` | `DLI_ScanParam` 8 B `[ownAddrType][scanFilterPolicy][frameFormatInd][scanType][scanInterval u16][scanWindow u16]` | field-for-field match except `frameFormatInd` (WS73-only byte, no API field); 0.125 ms interval/window units agree (400=50 ms) |
| `sle_announce_param_t` | `DLI_AdvParam` 49 B `[advHandle][advMode][advGtRole][intervalMin/Max 3B][channelMap][own/peer addr][advTxPower][frameFormat][...][conn params 14B]` | mode/role/channel_map/interval(125 µs)/tx_power/conn params all agree; API hides frame-format & secondary-PHY tail bytes |
| `sle_seek_result_info_t` | scan-result event (addr + RSSI + data) | RSSI range [-127,20], 0x7F = not provided matches BLE/星闪 convention |
| `sle_set_announce_data` (251 B) | SET_ADV_DATA 0x0C03 with FIRST/INTERMEDIATE/LAST fragmentation when > single frame | capacity agrees |

Demo sequences:
- Server (G role): register announce+conn callbacks → `sle_set_announce_param` (conn-scanable,
  T-CAN-NEGO, channel map, interval 0xC8=100 ms, conn interval 0x64, latency 0x1F3, supv 0x1F4)
  → `sle_set_announce_data` (discovery-level + access-mode + name + tx-power) → `sle_start_announce`
  (`sle_server_adv.c:118-201`).
- Client: register seek+conn+ssap callbacks → `enable_sle()` → on enable-callback
  `sle_set_seek_param` + `sle_start_seek` → on seek-result: save addr, `sle_stop_seek` →
  on seek-disable: `sle_connect_remote_device(&addr)` → on connect-state CONNECTED: if not paired
  `sle_pair_remote_device` → on pair-complete: `ssapc_exchange_info_req` (MTU) →
  `ssapc_find_structure` → read/write. (`sle_uuid_client.c:31-92,208-231`; identical shape in
  `sle_speed_client.c:46-76,287-330`.)

---

## 6. Data transmission

- `sle_transmition_manager.h` is intentionally thin: capability query
  `sle_transmission_signal_capability_req` (relay / trans_mode / measurement / access_slb / access_sle /
  mtu / mps bit flags, `:74-112`) and one busy-callback `sle_trans_data_busy_callback` with QoS states
  IDLE/FLOWCTRL/BUSY (`:30-56`). No raw packet send API — application data flows through SSAP.
- SSAP client data path: `ssapc_write_req` / `ssapc_write_cmd` (with/without response,
  `sle_ssap_client.h:597,622`), `ssapc_read_req` (`:574`), notifications/indications via
  `notification_cb`/`indication_cb` (`:396-427`). MTU negotiated via `ssapc_exchange_info_req`
  (`ssap_exchange_info_t {mtu_size, version}`, `sle_ssap_stru.h:160-165`).
- SSAP server data path: `ssaps_notify_indicate` / `ssaps_notify_indicate_by_uuid`
  (`sle_ssap_server.h:731,763`), server info via `ssaps_set_info` (`:787`).
- Max lengths: client sample requests MTU 300 (`sle_uuid_client.c:18`), speed samples use MTU 512 and
  `sle_set_data_len(conn_id, 512)` (`sle_speed_server.c:56-57,175,362`), then push 236-byte packets per
  notify (`:65,196`). The MPS/MTU ceiling is negotiated per link; 512 B is the sample's chosen cap.
- Low-latency (ACB/ICB) channels live in `sle_low_latency.h` (mouse/dongle/tx/rx modes with report-rate
  scheduling 125 Hz–8 kHz, `:32-54`) and are configured on a per-conn basis via
  `sle_low_latency_set` (`:268`). This corresponds to the WS73 `0x28xx` sync/data-path commands we
  verified (`SETUP_ICB_DATA_PATH 0x280D`, `CREATE_IOB 0x2803`, `SET_IOG_PARAM 0x2801` in
  `SLE-CONTROL-PLANE.md` rounds 3–4).

Note: in the WS63 SDK the terms "ACB" (connection-oriented, e.g. the low-latency mouse channel keyed by
`conn_id`) and "ICB" (connectionless isochronous broadcast) match our WS73 command set; the SDK does not
expose the TCID/channel-set abstraction that OSPL documents (TCID 0x02 CMTC / 0x0A SMTC / 0x1F DUDTC /
0x80-0xBF dynamic SSAP — `OSPL-CONN-FSM.md` §1.1). Our stack rides TCID 0x0A (SMTC) directly on WS73.

---

## 7. 差异清单 (differences vs our stack)

1. **Connect API carries no params.** `sle_connect_remote_device(&addr)` is address-only
   (`sle_connection_manager.h:585`); params come from a global default
   (`sle_default_connection_param_set`, `:904`), from the announce struct (G role), or a later
   `sle_update_connect_param` (`:644`). We embed the full param block in every 0x1401
   (`ssap_link.c:49-68`). Richer, but also means we must re-send on every connect; WS63 applies defaults
   once.
2. **No wire-level 0x1401/0x0015 visible.** The SDK never exposes the DLI opcodes/events we drive
   (0x1401/0x1402/0x1403, 0x0015/0x0005/0x0001/0x0002). Our stack's equivalents of the SDK's
   "connect complete" are: 0x0015 CONNECTION_COMPLETE (`ssap_link.h:31`), disconnect via 0x0005
   (`:32`), command status via 0x0001/0x0002 (`:29-30`). Semantically the WS63
   `connect_state_changed` callback (state + pair + reason) is the folded version of our
   `on_connected`/`on_disconnected`/`on_connect_failed` triple — but the SDK has **no dedicated
   connect-failure callback**; a failed connect arrives as a state change with reason.
3. **Callback (registration) model vs event-loop model.** WS63: register callback structs once
   (`sle_connection_register_callbacks`, `sle_announce_seek_register_callbacks`,
   `ssapc_register_callbacks`) and the firmware thread invokes them. Ours: `ssap_link_on_event` +
   `ssap_link_tick` polled from the app loop. Mapping is 1:1 at the semantic level (see §3 table); the
   WS63 model implicitly gives us "SLE service thread" ordering that we must reproduce with our event
   queue + state gating.
4. **Error/reason code spaces do not overlap.** WS63: `ERRCODE_SLE_*` host errors
   (`sle_errcode.h:65-115`) + two-value `sle_disc_reason_t` (0x10/0x11). Ours: WS73 controller codes
   0x06/0x0B/0x0F/0x1E and disconnect reasons 0x08/0x13. Do not translate numerically; translate
   semantically.
5. **Default supervision agreed:** 0x1F4 = 5 s, 10 ms units on both sides.
6. **Scan/adv wire layouts agree** field-for-field with our `DLI_ScanParam`/`DLI_AdvParam` decodes; the
   API hides `frameFormatInd` and the secondary-PHY tail of DLI_AdvParam.
7. **Interval unit documentation conflict** in the WS63 SDK itself (see U1) — must be resolved against
   live WS73 behavior before trusting our 1.25 ms assumption.
8. **Max adv data 251 B** confirmed on both WS63 demos and WS73 hardware.
9. **Param-update events** (`sle_connection_param_update_evt_t` interval/latency/supervision,
   `sle_connection_manager.h:170-177`) map to our 0x0019 conn-param-update event; the SDK exposes both a
   request-in callback and a negotiated-result callback, i.e. two-phase param update like our
   DLI 0x1807 CONNECTION_UPDATE.

---

## 8. 对我们加固的确认 (confirmation of our hardening)

`ssap_link.c` hardening items from `OSPL-CONN-FSM.md` §5, cross-checked against the official WS63 API:

| Hardening in ssap_link.c | WS63 API counterpart | Verdict |
|---|---|---|
| Connect timeout → cancel 0x1402 → IDLE + `on_connect_failed` (`ssap_link.c:258-268`) | No public "cancel create connection"; firmware handles timeout; API reports via `connect_state_changed`/`ERRCODE_SLE_TIMEOUT` (`sle_errcode.h:78`) | No API-level need — **keep ours** (WS73 controller must be told to cancel; SDK hides this) |
| 0x0015 status gate / rejection revert (`ssap_link.c:152-161`) | No wire status exposed; connect failure → `ERRCODE_SLE_CONN_FAIL` (`sle_errcode.h:105-106`) | Keep; our numeric codes are WS73-specific and unmatched in API |
| 0x1403 reject → revert to CONNECTED (`ssap_link.c:228-233`) | Firmware-internal; param-update/state callbacks only | Keep; no API counterpart |
| Stale-event gating by state & conn_handle (`ssap_link.c:163-165,187-193`) | SDK relies on firmware thread ordering; no app-side guard shown in demos | Keep — our guard is stricter and correct for a host-side FSM |
| Duplicate-peer rejection (`ssap_link.c:41-47`) | SDK firmware likely enforces; demos never re-connect while connected | Keep |
| Supervision watchdog 5 s (`ssap_link.c:279-291`) | Firmware enforces supervision (default 5 s confirmed, `sle_server_adv.c:25`); disconnect surfaced via callback | Keep as defense-in-depth; aligns with confirmed 5 s/10 ms semantics |
| 0x1804 SET_DATA_LEN retry on 0x06 (`ssap_link.c:236-242`) | `sle_set_data_len` exists (`sle_connection_manager.h:883`) but return/retry is firmware-internal | Keep; WS73 returns 0x06 until connected (`SLE-CONTROL-PLANE.md`) |

**Bottom line:** none of our hardening has a direct API-level counterpart — the WS63 SDK delegates all
link-lifecycle management to firmware and only exposes outcome callbacks. Every hardening we added
(connect timeout, reject-revert, event gating, supervision watchdog, retry) operates at a layer the WS63
SDK does not even expose, so nothing in the official API supersedes it. The one place the SDK does
authoritatively confirm us is the supervision default: **0x1F4 = 5 s in 10 ms units** is exactly the
official HiSilicon default, validating `ssap_link.h:37` and the `supervisionTimeout 0x1F4` default in
`ssap_link.c:59`.

---

## 9. Open questions

- **U1 (interval unit on WS73).** WS63 header says conn interval time = N × 0.25 ms, range [0x1E,0x3E80]
  = [7.5 ms, 4 s] (`sle_device_discovery.h:153-157,204-211`), but the reference demo comment claims
  0x64 = "12.5 ms, 单位125us" (`sle_server_adv.c:16-19`) — the two are contradictory (0x64 would be
  25 ms at 0.25 ms units). Our `ssap_conn_param_t` assumes 1.25 ms units (`ssap_link.h:50-51`,
  default 0x64 = 125 ms). Three candidate units (0.125/0.25/1.25 ms) across three sources; needs a live
  WS73 connect capture to pin down which the `DLI_ConnectionCreateParam` connInterval field uses.
- **U2 (scan interval width).** `sle_default_connect_param_t.scan_interval/scan_window` are u16
  (`sle_connection_manager.h:300-305`); our DLI field is u8 (`ssap_link.h:56-57`). WS73
  `DLI_ConnectionCreateParam` may use u8 (values like 0x20) — confirm against firmware struct docs or a
  live probe.
- **U3 (disconnect reason bytes).** WS63 exposes only 0x10/0x11 reasons; we emit/carry 0x08/0x13 and
  read the raw byte from 0x0005. Our 0x0005 layout (`reason=data[0]`) vs OSPL
  (`[handle:2][reason:1]`) is still unverified on WS73 (from `OSPL-CONN-FSM.md` §6). The WS63 API does
  not disambiguate because it never shows the wire.
- **U4 (connect-failure taxonomy).** WS63 has no explicit connect-failure callback. If we want a
  userland API parity layer later, decide whether to map `on_connect_failed` into a
  DISCONNECTED-with-reason event or expose an extra callback.
- **U5 (firmware-enforced supervision vs host watchdog).** Confirm on WS73 whether the controller
  enforces supervision on its own (likely, given the WS63 SDK relies on it) — if yes, our host watchdog
  is pure defense-in-depth; if no (WS73 firmware variant), our watchdog is load-bearing.
- **U6 (scan filter / whitelist).** WS63 has `SLE_SEEK_FILTER_ALLOW_WLST` reserved
  (`sle_device_discovery.h:140-141`); our DLI scan filter policy byte 0 — confirm whether WS73 supports
  a whitelist scan to avoid noisy scans.
