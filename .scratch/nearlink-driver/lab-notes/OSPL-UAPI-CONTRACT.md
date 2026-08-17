# OpenSparklink Host-Kernel ABI Contract — Reference Digest

Date: 2026-08-17

Purpose: reference digest of the OpenSparklink host-stack's kernel-ABI contract, for comparison with our `/dev/hwsle` DLI char-device design on the ArchivalEra NearLink TV-box stack. All claims cite `file:line`.

## Sources

- `sparklink.h` = `/mnt/hdd/nearlink-stuff/OpenSparklink-linux/include/uapi/linux/sparklink.h` — Generic Netlink ABI (family `"sparklink"`).
- `sparklink_ioctl.h` = `/mnt/hdd/nearlink-stuff/OpenSparklink-linux/include/uapi/linux/sparklink_ioctl.h` — `/dev/sparklink` ioctl ABI.
- `sle_uapi.rs` = `/mnt/hdd/nearlink-stuff/OpenSparklink-linux/net/sparklink/sle_uapi.rs` (2880 lines) — kernel-side mirror of the ioctl constants + all `repr(C)` structs.
- `sle_event.rs` = `/mnt/hdd/nearlink-stuff/OpenSparklink-linux/net/sparklink/sle_event.rs` — event wire format + per-fd event queue + global broadcast ring.
- `sle_netlink.rs` = `/mnt/hdd/nearlink-stuff/OpenSparklink-linux/net/sparklink/sle_netlink.rs` — Generic Netlink TLV build/parse code.
- `libsparklink/` = `/mnt/hdd/nearlink-stuff/sparklink/crates/libsparklink/src/` — userland client (`adapter.rs`, `event.rs`, `ffi.rs`).
- `slk-protocol/` = `/mnt/hdd/nearlink-stuff/sparklink/crates/slk-protocol/src/` — userland mirror of the ABI (`ioctl.rs`, `types.rs`, `genl.rs`).
- `slctl/` = `/mnt/hdd/nearlink-stuff/sparklink/crates/slctl/src/` — CLI tool (`main.rs`, `commands.rs`).
- Ours: `hwsle_transport.c` = `/home/archivalera/plum/zcode-projects/nearlink/stack/ssap/src/hwsle_transport.c`; `SSAP-DIALECT-COMPARISON.md` = `/home/archivalera/plum/zcode-projects/nearlink/.scratch/nearlink-driver/lab-notes/SSAP-DIALECT-COMPARISON.md`.

Note: OpenSparklink exposes the SAME operation set through two parallel userland paths: (a) the `/dev/sparklink` char device via ~131 ioctls, and (b) a Generic Netlink family `"sparklink"` v1 (`sparklink.h:20-21`) that "parallels the ioctl interface" (`sparklink.h:8-10`). The ioctl ABI is the one actually consumed by the userland tooling (libsparklink drives ioctls exclusively via `slk-protocol/src/ioctl.rs`), so this digest centers on it.

## 1. ioctl surface inventory

### Naming/structure pattern

- One char device `/dev/sparklink` (path constant `DEFAULT_DEV_PATH = "/dev/sparklink"`, `libsparklink/src/adapter.rs:15`).
- Magic byte `'S'` = 0x53 (`sparklink_ioctl.h:649`, `sle_uapi.rs:95`).
- Naming convention: `SL_IOCTL_<GROUP>_<OP>` (`sparklink_ioctl.h:653-914`), e.g. `SL_IOCTL_DEV_INFO`, `SL_IOCTL_SSAP_REMOTE_READ`, `SL_IOCTL_DLI_POLL_EVENT`.
- Encoded with the standard `_IO/_IOW/_IOR/_IOWR` direction macros (`sparklink_ioctl.h:653-914`), so argument struct size is baked into the ioctl number.
- Groups are allocated in hex blocks with reserved gaps; structure is table-driven and each struct carries `_reserved`/`_pad` bytes, and the kernel *rejects non-zero padding* from userspace (`CheckReserved` trait + `impl_check_reserved!` list, `sle_uapi.rs:37-63` and `sle_uapi.rs:2806-2881`).

### Group-by-group inventory (131 ioctls total, `sparklink_ioctl.h:653-914`)

| Group | Range | Count | Representative ioctls |
|---|---|---|---|
| Device mgmt | 0x01–0x08 | 8 | `DEV_REGISTER/UNREGISTER`, `DEV_COUNT`, `DEV_INFO` (→ `struct sci_dev_info`), `DEV_SWITCH`, `DEV_SELECT` (per-fd affinity), `DEV_GET_ACTIVE` |
| Adv / scan (basic) | 0x10–0x13 | 4 | `START_ADV` (→ `struct sle_adv_params`), `STOP_ADV`, `START_SCAN` (→ `struct sle_scan_params`), `STOP_SCAN` |
| Extended adv | 0x14–0x1C | 9 | `EXT_ADV_CONFIGURE/SET_DATA/ENABLE/DISABLE/REMOVE/INFO/ENABLE_EX/TICK/SET_SCAN_RSP` |
| Scan injection / filter | 0x20–0x24 | 5 | `INJECT_ADV`, `SCAN_RESULT_COUNT`, `INJECT_RAW_ADV`, `SET_SCAN_FILTER` (→ `struct sle_scan_filter`, up to 4 service UUIDs), `CLEAR_SCAN_FILTER` |
| Connection mgmt | 0x30–0x39 | 10 | `CONNECT` (→ `struct sle_connect_params`), `DISCONNECT`, `CONN_INFO`, `CONN_SEND` / `CONN_RECV` (→ `struct sle_conn_data`), `CONN_COUNT`, `CONN_LIST`, `SET_CONN_MTU`, `INJECT_CONN_RESP/DATA` |
| AFH | 0x3A–0x3F | 6 | `AFH_SET_MAP/GET_MAP/REPORT_RSSI/CLASSIFY/HOP_NEXT/REPORT_RETX` |
| Security | 0x40–0x4F | 16 | `SEC_SET_PSK`, `SEC_PAIR`, `SEC_INFO`, `SEC_ENCRYPT_ON`, `SEC_GET_PASSKEY`, `SEC_CONFIRM_PASSKEY`, `SEC_REJECT_PASSKEY`, `SEC_SET_OOB`, `SEC_INPUT_PASSKEY`, `SEC_SET_PASSWORD`, `SEC_RESET`, plus SM3/SM4/HMAC self-tests (`SEC_SM3_TEST`, `SEC_SM4_BLOCK_TEST`, `SEC_HMAC_TEST`, …) |
| SSAP service (local) | 0x50–0x59 | 10 | `SSAP_REGISTER_SVC`, `SSAP_INFO` (→ `struct ssap_summary`), `SSAP_READ/WRITE`, `SSAP_FIND_SVC`, `SSAP_NOTIFY`, `SSAP_DEQUEUE_NTF`, `SSAP_ADD_SVC` / `SSAP_ADD_PROP` (dynamic registration), `SSAP_REMOVE_SVC` |
| SSAP service (remote client) | 0x5A–0x5F, 0x6F, 0x72 | 8 | `SSAP_EXCHANGE_INFO`, `SSAP_REMOTE_DISCOVER`, `SSAP_REMOTE_READ/WRITE`, `SSAP_REMOTE_EVENT` (dequeue inbound ntf), `SSAP_CALL_METHOD`, `SSAP_FIND_BY_UUID`, `SSAP_READ_BY_UUID` |
| Power mgmt | 0x60–0x65 | 6 | `PM_INFO`, `PM_SET_STATE`, `PM_SET_INTERVAL`, `PM_FORCE_ACTIVE`, `PM_TICK`, `PM_ACTIVITY` |
| Sync link (iso) | 0x66–0x6E, 0x73–0x77 | 14 | `SYNC_UCAST_PARAM/CREATE/REMOVE` (CIG), `SYNC_MCAST_PARAM/CREATE/REMOVE` (BIG), `SYNC_DATAPATH_CFG/REMOVE`, `SYNC_INFO`, `SYNC_UCAST_ACCEPT/REJECT`, `SYNC_MCAST_ACCEPT/REJECT`, `SYNC_DATA_SEND` |
| Events | 0x70–0x71 | 2 | `EVENT_COUNT`, `EVENT_STATS` (→ `struct sle_event_stats`) |
| DLI controller | 0x80–0x86 | 7 | `DLI_INFO`, `USB_DEV_COUNT`, `DLI_POLL_EVENT` (→ `struct sle_dli_event`), `DLI_RESET`, `DLI_SEND_CMD` (→ `struct sle_dli_cmd`), `MGMT_STATS`, `SUBSYS_STATS` |
| PHY | 0x90–0x97 | 8 | `PHY_INFO`, `PHY_SET_MCS`, `PHY_SET_TXPOWER`, `PHY_MCS_SELECT`, `PHY_HOP_NEXT`, `PHY_SET_BW`, `PHY_GET_SINR`, `PHY_SET_SINR` |
| Conn capability | 0x98–0x9B | 4 | `CONN_READ_PEER_FEATURES`, `CONN_READ_PEER_VERSION`, `CONN_UPDATE_PARAMS`, `CONN_PHY_UPDATE` |
| Role | 0xA0–0xA1 | 2 | `SET_ROLE`, `GET_ROLE` (G-node 0 / T-node 1) |
| RAL / RPA | 0xB0–0xB7 | 8 | `RAL_ADD/REMOVE/CLEAR/SIZE`, `RAL_READ_PEER_RPA`, `RAL_READ_LOCAL_RPA`, `RPA_ENABLE`, `RPA_SET_TIMEOUT` |
| Narrowband measurement | 0xC0–0xC3 | 4 | `MEAS_READ_CAP`, `MEAS_SET_LINK_PARAM`, `MEAS_ACTION`, `MEAS_ENABLE` |

(Verified: 131 unique `SL_IOCTL_*` defines in `sparklink_ioctl.h`.)

### Parallel Generic Netlink surface

Family `"sparklink"`, version 1 (`sparklink.h:20-21`), multicast group `"events"` (`sparklink.h:24`). Command enum `SPARKLINK_CMD_*` has 35 entries including device/adv/scan/conn/security/SSAP/PM/event/DLI/version/role and dynamic SSAP-add commands (`sparklink.h:31-90`). Attribute enum `SPARKLINK_ATTR_*` has 79 entries (`sparklink.h:97-184`); kernel-side builder/parser uses NLA-style TLVs, 4-byte aligned, LE (`sle_netlink.rs:158-364`). Kernel comment admits the genetlink family registration is not yet fully wired into the Rust kernel binding (`sle_netlink.rs:14-17`) — the ioctl path is the battle-tested one.

## 2. Event / notification mechanism

Three mechanisms exist; the userland client relies on the ioctl-poll + char-device-read hybrid.

### (a) read() on `/dev/sparklink` — typed SLE events

- Wire format `struct sle_wire_event` / `SleWireEvent`: `[event_type u8][payload_len u8][payload[40]][pad[2]]` = 44 bytes fixed (`sle_event.rs:716-737`; C mirror `sparklink_ioctl.h:629-634`). Payloads are `repr(C)` typed structs, truncated to `EVENT_PAYLOAD_MAX = 40` (`sle_event.rs:717`).
- 52 kernel event type codes `0x01..0x34` (`sparklink.h:191-257`; `sle_event.rs:30-147`): conn-state, adv report, data-received indication, security-changed, power-changed, HW error, cmd-complete/status, PHY update, conn-param update, data-len change, buffer overflow, pairing (0x17-0x21), narrowband measurement (0x22-0x28), SLB (0x29-0x2E), sync link (0x2F-0x34).
- Delivery: each open fd has a per-fd `EventQueue` (fixed 64-slot ring, drops oldest when full — `sle_event.rs:966-1282`), fed from a global 128-slot `BroadcastRing` with a monotonic `seq`; each fd tracks `last_seq` and drains `seq > last_seq` on poll/read (`sle_event.rs:1284-1387`). This is a `/dev/kmsg`-style multi-listener model — no global fd registry.
- The read path delivers *notifications*; data payloads are NOT in the event (see below).

### (b) `SL_IOCTL_DLI_POLL_EVENT` (0x82) — raw DLI events

- Returns `struct sle_dli_event` (`sparklink_ioctl.h:525-534`, `sle_uapi.rs:1769-1792`): `event_type u8, status u8, handle u16, opcode u16, data_len u16, data[240], addr[6], pad[2]`.
- The kernel translates every DLI controller event into this struct via `sle_dli_event_to_wire()` (`sle_uapi.rs:1878-2502`) — 52 mapping arms with per-type byte packing.
- Non-blocking: returns `EAGAIN` when no event pending (`libsparklink/src/adapter.rs:626-634`).

### (c) Generic Netlink multicast — `SPARKLINK_CMD_EVENT`

- Multicast group `"events"` delivers `SPARKLINK_CMD_EVENT` messages with `SPARKLINK_ATTR_EVENT_TYPE` / `EVENT_PAYLOAD` / pending / total / dropped counters (`sparklink.h:70-71, 158-163`). Described as the async channel for listeners subscribed to the multicast group (`sparklink.h:23-24`, `sle_netlink.rs:19-20, 36`).

### How an app consumes events (libsparklink)

`Adapter::next_event()` loops: non-blocking `poll_event()` ioctl; if empty, wait on fd readability (epoll via tokio `AsyncFd`) with a 50 ms fallback timeout to cover events that do not wake poll (`adapter.rs:642-668`). Raw `SleDliEvent` is decoded into typed `Event` variants (ConnStateChanged, AdvReport, DataReceived, SecurityChanged, PowerChanged, HwError, RawDli, ConnInfoUpdate — `libsparklink/src/event.rs:5-50`) via `decode_event()` (`adapter.rs:887-934`). Data is consumed separately via `CONN_RECV` — the `EVT_DATA_RECV` event only carries `{handle, rx_bytes}` as an availability hint (`sle_event.rs:186-194`).

## 3. Object model

- **Devices** (SCI controllers): each has a `u16 index` + `u8 state` + `u8 bus` + 6-byte SLE addr + 32-byte name (`struct sci_dev_info`, `sparklink_ioctl.h:24-31`). One global active device; per-fd affinity via `DEV_SELECT` (`sparklink_ioctl.h:660-669`). Kernel states Idle/Advertising/Scanning/Connected (`sle_uapi.rs:550-563`).
- **Connections**: identified by `u16 handle`, assigned on connect (kernel returns `handle > 0`, `sle_uapi.rs:195`); `CONN_LIST` returns up to 8 handles (`struct sle_conn_list`, `sparklink_ioctl.h:177-182`). Connections are the addressable unit for data (handle in `sle_conn_data`), SSAP client ops (conn_handle in `ssap_remote_*`), sync links, AFH, peer-capability queries.
- **Services / properties** (SSAP): addressed by `u16 handle` ranges. Services have `start_handle`/`end_handle` + `uuid16`/`uuid128` + primary flag (`struct ssap_service_entry`, `sparklink_ioctl.h:355-361`). Properties have a `u16 handle` and are read/written/notified by that handle (`struct ssap_read_write`, `struct ssap_add_property`). Discovery result list caps at 15 services (`struct ssap_service_list`, `sparklink_ioctl.h:363-367`).
- **Addressing summary**: one 6-byte SLE addr per peer (`sparklink_ioctl.h:28`), `u16` handles for everything post-connection, `u16` handles for SSAP services/properties, and UUID (16-bit or 128-bit) as the symbolic service identity.

## 4. Key ABI structs (boundary types)

| Struct | Where | Key fields |
|---|---|---|
| `sci_dev_info` | `sparklink_ioctl.h:24-31` | index, state, bus, addr[6], name[32] |
| `sle_adv_params` | `sparklink_ioctl.h:35-40` | dev_index, interval_ms, discovery_level |
| `sle_scan_params` | `sparklink_ioctl.h:42-48` | dev_index, window_ms, interval_ms, filter_discovery_level |
| `sle_scan_filter` | `sparklink_ioctl.h:55-59` | uuid_count, uuids[4] (service-UUID match, adv TLV types 0x05/0x07) |
| `sle_ext_adv_data` | `sparklink_ioctl.h:75-80` | handle, data_len, data[252] |
| `sle_connect_params` | `sparklink_ioctl.h:120-128` | peer_addr[6], gt_role, bandwidth, mcs_index, timeout_10ms |
| `sle_conn_info` | `sparklink_ioctl.h:130-157` | handle, state, peer_addr, tx/rx bytes, negotiated MCS/bandwidth, `data_mtu`, `data_mps`, `svc_mtu`, `data_mode`, SSAP fields (`ssap_mtu`, `ssap_info_exchanged`, `ssap_reliable_mode`, `ssap_version_major`), SMTC/DUDTC credit counters |
| `sle_conn_data` | `sparklink_ioctl.h:159-164` | handle, length, data[255] (the data-plane unit for `CONN_SEND`/`CONN_RECV`) |
| `sle_dli_info` | `sparklink_ioctl.h:508-523` | bus, firmware_version, features, max_connections, max_mtu, max_mps, transport_modes, measurement_cap, security_cap, name[32] |
| `sle_dli_event` | `sparklink_ioctl.h:525-534` | event_type, status, handle, opcode, data_len, data[240], addr[6] |
| `sle_dli_cmd` | `sparklink_ioctl.h:536-541` | opcode, param_len, seq (assigned by kernel), params[240] |
| `ssap_summary` | `sparklink_ioctl.h:340-347` | service_count, property_count, total_entries, mtu, notification_count |
| `ssap_read_write` | `sparklink_ioctl.h:349-353` | handle, length, data[252] |
| `ssap_service_entry` / `ssap_service_list` | `sparklink_ioctl.h:355-367` | start/end handle, uuid16, primary; list of 15 |
| `ssap_notification` | `sparklink_ioctl.h:369-374` | handle, indication flag, length, data[252] |
| `ssap_add_service` / `ssap_add_property` | `sparklink_ioctl.h:376-392` | uuid16/uuid128, primary; uuid16, ops bitmask, value[248], assigned handle |
| `ssap_remote_discover` | `sparklink_ioctl.h:772-777` | conn_handle, start/end handle range, count (out) |
| `ssap_remote_read_write` | `sparklink_ioctl.h:779-785` | conn_handle, handle, length, data[248] |
| `ssap_uuid_op` | `sparklink_ioctl.h:787-794` | conn_handle, uuid16/uuid128, matched handle, length, data[232] (READ_BY_UUID) |
| `sle_wire_event` | `sparklink_ioctl.h:629-634` | event_type, payload_len, payload[40] — 44-byte read() event |
| `sle_conn_peer_cap` | `sparklink_ioctl.h:862-871` | handle, features[10], version, manufacturer, subversion, validity flags |
| `sle_conn_param_update` / `sle_conn_phy_update` | `sparklink_ioctl.h:873-886` | interval min/max/latency/timeout; mcs_index/bandwidth |
| `sle_pm_info` | `sparklink_ioctl.h:396-410` | state, force_active, power_pct, current_interval, latency, counters |

Buffer ceilings: adv/scan data 252 (`sle_uapi.rs:77`), SSAP data 252 / property value 248 (`sle_uapi.rs:85-87`), DLI params/event payload 240 (`sle_uapi.rs:89`), `sle_conn_data.data` 255 (`sle_uapi.rs:922`).

## 5. Comparison to our design

Our design: `/dev/hwsle` char device carries raw DLI frames — `[0xA1]` HCI command, `[0xA2]` HCI event, `[0xA3]` ACB data `[tcid u16][len u16][payload]`; the SSAP userland speaks SSAP PDUs directly on TCID `0x0A` (`hwsle_transport.c:3-9, 51-96, 120-139`; our comparison doc confirms SSAP over `TCID_SLE_SMTC 0x0A`, `SSAP-DIALECT-COMPARISON.md:24`). The kernel/device is a dumb byte pipe; ALL protocol logic (connection manager, discovery, SSAP service tables, event correlation) is in userland.

What OpenSparklink's higher-level ABI gives us that we lack:

1. **Kernel-side connection manager** — a stateful conn state machine with `u16` handles, `CONN_INFO`/`CONN_LIST`/`CONN_MTU` introspection (`sparklink_ioctl.h:713-722`), and typed `ConnStateChanged` events with old/new state + reason (`sle_event.rs:154-168`). We track conn state ourselves from raw DLI events.
2. **Structured service discovery** — remote SSAP client ops: `SSAP_EXCHANGE_INFO` (MTU negotiation), `SSAP_REMOTE_DISCOVER` (FindStructure), `SSAP_FIND_BY_UUID`, `SSAP_READ_BY_UUID`, `SSAP_CALL_METHOD`, `SSAP_REMOTE_EVENT` (`sparklink_ioctl.h:796-803`). Our stack must implement these SSAP procedures in userland.
3. **Dynamic service registration** — `SSAP_ADD_SVC`/`SSAP_ADD_PROP`/`SSAP_REMOVE_SVC` with kernel-assigned handles (`sparklink_ioctl.h:761-763`), plus a built-in service registry summary (`ssap_summary`).
4. **Security stack** — pairing methods (JustWorks/PSK/NC/passkey/OOB/password), passkey confirm/reject round-trips, key fingerprints, RAL/RPA resolution lists, and even SM3/SM4/HMAC self-test ioctls (`sparklink_ioctl.h:735-750, 900-907`). The pairing event set 0x17–0x21 (`sle_event.rs:421-516`) implies kernel-side state machines for each method.
5. **Capability/PHY/PM management** — peer features/version exchange (`0x98-0x9B`), MCS/bandwidth/SINR control (`0x90-0x97`), power-state machine with counters (`0x60-0x65`).
6. **Out-of-band test plumbing** — adv/conn injection ioctls for loopback without hardware.
7. **Per-fd, multi-listener event delivery** with backpressure counters (`sle_event.rs:984-990`) — an event-queue discipline we currently do not have (our transport just byte-streams DLI frames).

What is overkill for a low-resource TV box: sync-link/isochronous audio (14 ioctls, 0x66-0x77), AFH channel-map surgery (6 ioctls), narrowband measurement/ranging (4 ioctls), raw-PDU injection, SM3/SM4 crypto self-tests, PHY SINR-threshold tuning, subsystem counters.

## 6. Minimal ABI for a TV-box app (chat / file-transfer)

For a peer-to-peer chat or file-transfer app we should mirror, in our userland stack, roughly this ABI surface:

- **Device info**: name, addr[6], state, bus (`sci_dev_info` clone).
- **Discovery**: `start_scan(window, interval, discovery_level)`, `stop_scan`, `start_adv(interval, discovery_level, adv_data)`, `stop_adv`; scan results as `{addr, rssi, discovery_level, name}` events (`AdvReportEvent`, `sle_event.rs:173-184`).
- **Connection**: `connect(peer_addr, role, timeout)` → handle; `disconnect(handle)`; `conn_info(handle)`; conn-state events with `{handle, old/new state, peer_addr, reason}`.
- **Data plane**: `conn_send(handle, data)` / `conn_recv(handle)` with 255-byte payloads, plus a data-available event (`DataReceivedEvent {handle, rx_bytes}`, `sle_event.rs:189-194`).
- **SSAP server side**: register service (`{uuid, start/end handle}`), add property (`{uuid, ops bitmask, value}`), read/write by handle, `notify(handle)`, inbound notification dequeued `{handle, data}`.
- **SSAP client side** (to browse the peer's services): `exchange_info(conn)`, `remote_discover(conn, range)`, `remote_read(conn, handle)`, `remote_write(conn, handle, data)`, inbound remote notification dequeue.
- **Security (JustWorks/PSK at minimum)**: pair(method), sec_info, encrypt_on; optionally passkey get/confirm/reject.

That is a subset of ~15-20 operations (the 0x01-0x08, 0x10-0x13, 0x30-0x34, 0x40-0x43, 0x50-0x5E, 0x70 blocks) — roughly 40 of the 131 ioctls, implemented as our own userland-facing API rather than kernel ioctls.

## 7. Actionable takeaways for our stack

1. **Adopt the handle model**: assign a `u16` handle per connection and per SSAP service/property; keep `conn_handle` as the key for all per-peer SSAP ops (as `ssap_remote_*` structs do). The 6-byte address identifies a peer only until connect.
2. **Adopt the 44-byte typed event discipline**: define a fixed `{type u8, len u8, payload[40], pad}` event envelope for our own notification path instead of leaking raw DLI events to apps; a bounded ring (drop-oldest + counters) matches `EventQueue`/`BroadcastRing` (`sle_event.rs:966-1287`).
3. **Separate "data available" from "data bytes"**: emit `{handle, rx_bytes}` events and pull bytes via a `recv(handle)` call (like `EVT_DATA_RECV` + `CONN_RECV`), so app event loops never stall on payload framing.
4. **Standardize the discovery-result record** as `{addr, rssi, discovery_level, name}` and filter by service UUID in the scanner (mirror `sle_scan_filter`), because chat/file apps want "find devices that expose service X".
5. **Put an `ssap_summary`-style registry** (service_count, property_count, mtu, pending notifications) in our SSAP layer — cheap to maintain and it makes debugging/CLI far easier (see `slctl` commands `services`/`read`/`write`, `commands.rs:257-301`).
6. **Adopt `seq`-tagged DLI command submission**: kernel assigns `seq` on `SL_IOCTL_DLI_SEND_CMD` so userspace can correlate async `CmdComplete/CmdStatus` events (`sle_uapi.rs:1794-1816`). We should tag our HCI-command writes with sequence numbers even though the device is a byte pipe.
7. **Validate padding/reserved fields on input** (`CheckReserved`, `sle_uapi.rs:37-63`) — zero-cost hardening and keeps the ABI forward-compatible.
8. **Keep the DLI raw path as the internal layer**: OpenSparklink's own userland still exposes raw DLI (`SleDliEvent`, `RawDli` variant, `event.rs:46`) beneath the typed API — same layering as our `hwsle_transport` under the SSAP engine.
9. **Reuse the pairing event vocabulary** (0x17-0x21, `sle_event.rs:421-516`) as the state machine for our own pairing module rather than inventing our own codes — it maps 1:1 to the DLI pair events (`sparklink.h:355-407`) our WS73 firmware already emits.
10. **Do not port**: sync links, AFH, measurement, injection, PHY tuning, crypto self-tests. They add code and state with zero TV-box value; keep the raw `DLI_SEND_CMD`-style escape hatch if the WS73 firmware ever needs a direct command.

## 8. Open questions

- OpenSparklink's `sle_conn_data.data[255]` but SSAP `data[252]` and MTU hints up to `max_mtu` (e.g. 247): our file-transfer framing should confirm the WS73's negotiated `data_mtu`/`data_mps` from `CONN_INFO`-equivalent state before choosing a segment size.
- The kernel's `sle_dli_event_to_wire` uses event_type values 0x01-0x34 that differ from the `sparklink_dli_event_code` wire codes (`sparklink.h:355-407`) — i.e. there are two distinct event numbering namespaces (DLI wire vs kernel event subsystem). Our stack should pick one canonical namespace internally and translate at the boundary.
- `SsapNotification` dequeue (`SSAP_DEQUEUE_NTF`/`SSAP_REMOTE_EVENT`) is a polled queue, not pushed — for a TV-box app the push-vs-poll choice depends on whether our SSAP engine runs in its own thread (push) or in the app's event loop (poll).
- OpenSparklink only implements async-UCAST data (`sle_conn_data`); sync/isochronous and multicast data are separate (`SYNC_DATA_SEND`). A TV-box chat/file stack only needs async-UCAST, but NearLink spec also defines BROADCAST (adv-data broadcast) which this ABI only half-exposes (`SLE_EVT_BROADCAST_END`, `sparklink_ioctl.h:926`). Whether to support broadcast-based "send to everyone in range" is an open product question.
