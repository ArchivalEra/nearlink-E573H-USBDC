---
type: intel
title: "OpenSparklink Connection FSM vs Our ssap_link FSM"
language: zh
created: 2026-08-17
tags: [intel, opensparklink, connection, ssap]
sources:
  - "/mnt/hdd/nearlink-stuff/OpenSparklink-linux"
  - "/mnt/hdd/nearlink-stuff/sparklink"
trust: B
stale_after: 2027-02-17
---

# OpenSparklink Connection FSM vs Our ssap_link FSM

Date: 2026-08-17

Sources (local only):
- `/mnt/hdd/nearlink-stuff/OpenSparklink-linux/net/sparklink/sle_conn.rs` (3071 lines — connection FSM, `ConnManager`)
- `/mnt/hdd/nearlink-stuff/OpenSparklink-linux/net/sparklink/sle_mgmt.rs` (333 lines — pending-command queue + timeout)
- `/mnt/hdd/nearlink-stuff/OpenSparklink-linux/net/sparklink/sle_event.rs` (1387 lines — typed event queue / wire events)
- `/mnt/hdd/nearlink-stuff/OpenSparklink-linux/net/sparklink/sle_uapi.rs` (2880 lines — connect/disconnect ioctls, `SleConnectParams`)
- `/mnt/hdd/nearlink-stuff/OpenSparklink-linux/net/sparklink/sle_dli.rs` (opcode/status/event-code enums)
- `/mnt/hdd/nearlink-stuff/OpenSparklink-linux/net/sparklink/sle_usb.rs` (wire event decode, `event_to_sle`)
- `/mnt/hdd/nearlink-stuff/OpenSparklink-linux/net/sparklink/sle_workers.rs` (event pump driving FSM transitions)
- `/mnt/hdd/nearlink-stuff/OpenSparklink-linux/net/sparklink/sparklink_core.rs` (ioctl handlers)
- `/mnt/hdd/nearlink-stuff/sparklink/crates/slk-protocol/src/types.rs` (userland mirror)
- Our side: `/home/archivalera/plum/zcode-projects/nearlink/stack/ssap/src/ssap_link.c`, `include/ssap_link.h`
- Prior notes: `SLE-CONTROL-PLANE.md`, `SSAP-DIALECT-COMPARISON.md`

---

## 1. OpenSparklink connection state machine

### 1.1 States

Kernel `ConnState` enum — `sle_conn.rs:827-841`:

| Value | State | Meaning |
|---|---|---|
| 0 | `Idle` | No active connection (default). |
| 1 | `Connecting` | Access request sent, awaiting access response. |
| 2 | `Connected` | Established; data/SSAP active. |
| 3 | `Disconnecting` | Disconnection in progress. |
| 4 | `ConnectPending` | `CreateConnection` command sent to controller, awaiting command acceptance (host-side state, not on-air). |
| 5 | `DisconnectPending` | `Disconnect` command sent to controller, awaiting acceptance. |

The userland mirror (`types.rs:11-16`) exposes only 4 states (Idle/Connecting/Connected/Disconnecting); the two `*Pending` states are kernel-internal.

`ConnEntry` (per-connection state) — `sle_conn.rs:1010-1082`: handle, state, peer_addr, local_role (`GtRole` T=0/G=1), `AccessCapability`, `NegotiatedParams` (bandwidth/mcs/pilot/schedule_slot/crc/event periods/**supervision_timeout**/latency/max_pdu_size), `PeerCapability` (features/version post-connect), transport `ChannelSet` (TCID 0x02 CMTC reliable, 0x0A SMTC reliable, 0x1F DUDTC; dynamic SSAP reliable channel 0x80-0xBF), `SeqTracker` (async 1-bit ARQ), TX/RX data queues, `last_activity` jiffies (used by supervision timeout), AFH state.

### 1.2 Transitions and triggers

| # | Trigger | Guard | Action / target state | Source |
|---|---|---|---|---|
| T1 | User connect ioctl `SL_IOCTL_CONNECT` | `connections.len() < max(8)` else `EBUSY`; no existing non-Idle entry with same peer else `EEXIST` | alloc handle, `entry.state = ConnectPending`, push entry | `sle_conn.rs:1182-1217` |
| T2 | Controller accepts `CreateConnection` (sync, ioctl path) | state == ConnectPending | `ConnectPending → Connecting` (`confirm_connecting_by_addr`) | `sle_conn.rs:1246-1254`; `sparklink_core.rs:1844-1849` |
| T3 | Controller rejects `CreateConnection` (send error) | state == ConnectPending | remove entry (`abort_connecting`) | `sle_conn.rs:1231-1242`; `sparklink_core.rs:1850-1853` |
| T4 | DLI `ConnComplete` event (wire 0x0015) with `status == Success` | pending entry exists | `ConnectPending → Connecting`; if no pending/no address entry, treat as **incoming**: create entry directly `Connected` (`accept_incoming`) | `sle_workers.rs:195-207`; `sle_conn.rs:1263-1283` |
| T5 | DLI `ConnComplete` event with status != Success | pending entry exists | remove entry (`abort_connecting_by_addr`) — connect attempt failed | `sle_workers.rs:204-206`; `sle_conn.rs:1286-1297` |
| T6 | Access response accepted (ioctl `SL_IOCTL_INJECT_CONN_RESP`, response_type==Accepted) | state == **Connecting** else `EBUSY` | negotiate params, `channels.open_all()`, create `SsapSession`, `→ Connected`; stamps `last_activity` | `sle_conn.rs:1323-1348`; `sparklink_core.rs:1944-1988` |
| T7 | Access response rejected (RoleNegotiationFailed/ResourceLimited/UserRejected) | state == Connecting | `→ Idle`, `Err(EACCES)` | `sle_conn.rs:1350-1357` |
| T8 | User disconnect ioctl `SL_IOCTL_DISCONNECT` | state in {Connected, Connecting, ConnectPending} else `EPIPE` | `→ DisconnectPending` | `sle_conn.rs:1366-1385` |
| T9 | Controller accepts `Disconnect` (sync path) | state == DisconnectPending | remove entry (`confirm_disconnecting`) | `sle_conn.rs:1389-1401`; `sparklink_core.rs:1872-1882` |
| T10 | Controller rejects `Disconnect` command | state == DisconnectPending | revert `→ Connected` (`abort_disconnecting`) | `sle_conn.rs:1405-1411`; `sparklink_core.rs:1883-1885` |
| T11 | DLI `Disconnected` event (wire 0x0005, `DisconnectDone`) | state in {DisconnectPending, **Connected**, **Connecting**} | remove entry (`confirm_disconnecting_by_handle`) | `sle_workers.rs:208-216`; `sle_conn.rs:1301-1320` |
| T12 | Supervision timeout (worker maintenance pass) | state == Connected, `elapsed(last_activity) > supervision_timeout*10ms` | `channels.close_all()`, `→ Idle`, `ssap_session=None`; user event `ConnStateChanged` old=2 new=0 **reason=0x08** | `sle_conn.rs:2103-2125`, `2161-2172`; `sle_workers.rs:687-701` |
| T13 | Data path send/recv | state == Connected else `EPIPE` | enqueue/dequeue | `sle_conn.rs:1414-1460` |
| T14 | Param updates (`ConnParamUpdate` evt 0x0019, `PhyUpdate` evt 0x0018, `DataLenChange` evt 0x0003) | state == Connected | mutate `params`; stamp `last_activity` | `sle_conn.rs:1497-1560` |

### 1.3 How DLI events map to FSM changes

Raw wire decode (`sle_usb.rs:625-687`):
- `0x0002` CmdComplete → `SleEvent::CommandComplete{opcode, status, data}` (opcode u16 LE, status byte, then return params) — `sle_usb.rs:628-651`
- `0x0001` CmdStatus → `SleEvent::CommandStatus{opcode, status}` — `sle_usb.rs:653-663`
- `0x0015` ConnEstablished → `SleEvent::ConnComplete{handle:u16, addr:6B, status}` — `sle_usb.rs:664-678`
- `0x0005` DisconnectDone → `SleEvent::Disconnected{handle:u16, reason:1B}` — `sle_usb.rs:679-687`

Event pump (`sle_workers.rs:140-216`) first resolves the pending-command queue for `CommandComplete`/`CommandStatus` (opcode+status), then drives FSM transitions for `ConnComplete` (T4/T5) and `Disconnected` (T11). Connection lifecycle events are also published to the userland event queue / genl bridge.

Wire event codes in `sle_dli.rs:698-803`: `DisconnectDone = 0x0005`, `CmdStatus = 0x0001`, `CmdComplete = 0x0002`, `ConnEstablished = 0x0015` (line 728), `PhyParamUpdate = 0x0018`, `ConnParamUpdate = 0x0019`, `DataLenChange = 0x0003`, `PeerConnParamReq = 0x0007`, `HwError = 0x000A`.

---

## 2. Error / status codes

OSPL `SleStatus` enum — `sle_dli.rs:681-691` and `raw_to_status` at `sle_usb.rs:1290-1303`:

| Raw | SleStatus | Notes |
|---|---|---|
| 0x00 | Success | |
| 0x01 | UnknownCommand | |
| 0x02 | InvalidParameters | |
| 0x03 | HardwareFailure | also catch-all for unknown raw values |
| 0x04 | ResourceExhausted | |
| 0x05 | NotConnected | |
| 0x06 | AlreadyActive | |
| 0x07 | PermissionDenied | |
| 0x08 | Timeout | |

How these surface in the FSM:
- A non-zero status on `ConnComplete` (0x0015) **aborts the pending connect** (removes entry, T5). No retry.
- A non-zero status on `CommandComplete`/`CommandStatus` for `CreateConnection`/`Disconnect` resolves the pending-command entry; FSM abort/revert is driven in the ioctl path (T3/T10) or via the async event (T5).
- Remote/unexpected disconnect is reported two ways:
  1. `Disconnected` event (0x0005) with a `reason` byte (`sle_usb.rs:686`, `sle_dli.rs:834`), surfaced to userspace (`sle_uapi.rs:1929-1932`); FSM removes the entry from any of DisconnectPending/Connected/Connecting.
  2. Supervision timeout: no data activity for `supervision_timeout * 10ms` → forced teardown, `ConnStateChanged` event reason **0x08** (`sle_workers.rs:693-699`). `last_activity` is stamped on send/recv/param-update (`sle_conn.rs:1429,1448,1518`).

Note: OSPL's status code space (0x01-0x08) is the controller dialect it targets. Our WS73 hardware reports different dli_errno values — from `SLE-CONTROL-PLANE.md`: 0x0F = INVALID_PARAMS, 0x0B = CMD_DISALLOWED, and 0x06 status on commands that need an established connection (e.g. `0x1804 SET_DATA_LEN` → 0x06 "参数需连接", `0x1403 DISCONNECT` → 0x06 "参数"). So the *semantic* classes overlap (invalid-params, not-connected, disallowed) but the *numeric* codes do not; do not reuse OSPL's numbers.

Command-level timeout: every DLI command gets a 5 s default deadline (`CMD_TIMEOUT_MS = 5000`, `sle_mgmt.rs:31`); expired entries resolve as ETIMEDOUT pseudo-status (`sle_mgmt.rs:96-98`, `expire_stale` at 209-221), swept by the worker (`sle_workers.rs:681-685`). Note: expiry only resolves the pending queue — the connection FSM entry for a timed-out `CreateConnection` is **not** automatically aborted by the pump; the ioctl path aborts on synchronous send error only.

---

## 3. Connect parameter handling

`SleConnectParams` (userspace ioctl 0x30) — `sle_uapi.rs:817-844`, mirrored at `types.rs:219-227`:
```
peer_addr[6], gt_role (0=T/1=G), bandwidth (1/2/4 MHz), mcs_index (0-12),
timeout_10ms (supervision timeout in 10 ms units, default 100 = 1 s), reserved[4]
```

How they flow to the DLI layer (this is the important finding):
- The ioctl passes **only `peer_addr` + `gt_role`** to `conn.connect()` → `controller.create_connection(&cp.peer_addr)` sends a **bare 6-byte address** as the DLI command param (`sle_dli.rs:1382-1384`, `sparklink_core.rs:1844-1845`). `bandwidth`, `mcs_index`, `timeout_10ms` are parsed from the struct but **dropped** at connect time.
- `gt_role` → `GtRole` (GNode if 1 else TNode) stored per-connection (`sparklink_core.rs:1836-1840`).
- Effective link parameters arrive later from the controller as async events: `ConnParamUpdate` (interval/latency/timeout, `sle_conn.rs:1497-1520`), `PhyUpdate` (mcs/bandwidth, 1523-1541), `DataLenChange` (max pdu, 1544-1560), or via userspace-injected access response `SL_IOCTL_INJECT_CONN_RESP` (`sparklink_core.rs:1944-1988`, `SleInjectConnResp` at `types.rs:274-283`).

So OSPL does not implement a rich `DLI_ConnectionCreateParam` — the full param set (interval min/max, max latency, supervision timeout, CE lengths, scan interval/window/type, PHY bits, filter policy, addr types) that OUR `ssap_conn_param_t` already encodes (`ssap_link.h:38-55`, `ssap_link.c:31-50`) goes beyond OSPL's minimal model. That is fine — it means our wire dialect is richer; what we lack is the *FSM handling* around those params (see sections 4-6).

---

## 4. Comparison: OSPL FSM vs our ssap_link FSM

Our FSM (`ssap_link.h:58-63`, `ssap_link.c`):
```
IDLE → (0x1401 sent) → CONNECTING → (0x0015 event) → CONNECTED
     → (0x1403 sent) → DISCONNECTING → (0x0005 event) → IDLE
```

| Aspect | OSPL | ssap_link.c | Assessment |
|---|---|---|---|
| States | 6 (Idle/Connecting/Connected/Disconnecting/ConnectPending/DisconnectPending) | 4 (IDLE/CONNECTING/CONNECTED/DISCONNECTING) | We lack the command-pending split; ok for single-inflight, but no `ConnectPending` guard on events |
| Connect send | guards max conns (EBUSY) + duplicate peer (EEXIST) | guards `state != IDLE` only | Missing duplicate-peer rejection |
| Connect ack wait | 5 s CMD_TIMEOUT + abort on failure | **none** — stuck in CONNECTING forever if 0x1401 never completes or is rejected via CmdStatus | GAP |
| 0x0015 with error status | abort connect (T5) | status field not even parsed (data[0] treated as `role`) | GAP / likely layout mismatch |
| Incoming connect | `accept_incoming` creates Connected entry | 0x0015 sets CONNECTED regardless of prior state — works by accident, no gate | RACE |
| Disconnect allowed from | Connected/Connecting/ConnectPending | **Connected only** | GAP: cannot cancel a pending connect |
| 0x1403 rejected | `abort_disconnecting` reverts to Connected | **ignored** — stays DISCONNECTING forever | GAP |
| Remote close mid-open | `confirm_disconnecting_by_handle` removes Connecting entries (T11) | 0x0005 sets IDLE from any state (ok) | we handle, but see races below |
| Supervision timeout | enforced, event reason 0x08 | `supervision_timeout` stored but never enforced | GAP |
| Post-connect 0x1802/0x1804 | separate ops; FSM untouched by their failure | fired blindly on 0x0015; failure ignored | see section 5 |

### (a) Transitions / error paths we don't handle
1. **0x1401 rejected by CmdStatus/CmdComplete with error** — we stay CONNECTING forever; OSPL aborts and removes the entry (T3/T5).
2. **0x1403 rejected (status != 0)** — we stay DISCONNECTING forever; OSPL reverts to Connected (T10).
3. **No connect timeout** — no recovery if neither CmdComplete nor 0x0015 ever arrives.
4. **Supervision timeout / link loss without 0x0005 event** — no fallback teardown; OSPL detects via `last_activity` vs `supervision_timeout*10ms` (T12).
5. **Incoming (peer-initiated) connect** — OSPL explicitly creates a Connected entry; we have no path to distinguish it from our own pending connect.
6. **Duplicate connect to same peer** — OSPL rejects with EEXIST.

### (b) Races
1. **Remote close while we are mid-open** (after 0x1401, before 0x0015): OSPL removes the Connecting entry on 0x0005 (T11). We set IDLE (fine) but *then a late 0x0015 could flip us back to CONNECTED* because our 0x0015 handler has no state guard / no ordering check — OSPL's `confirm_connecting_by_addr` requires a live pending entry and would be a no-op if the entry was already removed.
2. **Remote close racing the 0x0015 handler's post-connect sends**: our handler fires 0x1802 + 0x1804 immediately; if the link already died, 0x1804 returns 0x06 (not-connected, per `SLE-CONTROL-PLANE.md`). We neither check the returned status nor gate on handle validity.
3. **Disconnect ioctl vs 0x0015 arriving late**: `ssap_link_disconnect` requires CONNECTED, so a disconnect issued right as 0x0015 lands can fail with -1 and be silently lost (caller has no error path).
4. **Stale 0x0005 from a previous connection** hitting a new connect: our event handler does not match by conn_handle, so a delayed 0x0005 can tear down a freshly established link. OSPL matches by handle.

### (c) Timeout values / recovery worth copying
- **CMD_TIMEOUT_MS = 5000** for command completion (`sle_mgmt.rs:31`) — apply to 0x1401 ack and 0x1403 ack.
- **supervision_timeout default 100 (= 1 s)** in OSPL `NegotiatedParams` (`sle_conn.rs:787`); enforcement loop with `last_activity` stamping (`sle_conn.rs:2103-2125`). Our default is 0x1F4 = 500 (= 5 s) in `ssap_link.c:41` — fine, but unused.
- **Event-driven abort rather than polling**: OSPL reacts to `ConnComplete(status!=0)` → abort; `Disconnected` → remove regardless of state. Copy the "any-state removal" semantics.

---

## 5. Actionable hardening list for ssap_link.c

Concrete changes (each maps to a gap/race above):

1. **Add a connect timeout / stuck-connecting recovery.** Track a deadline (e.g. 5 s from 0x1401 send, per `sle_mgmt.rs:31`). On expiry with no 0x0015: send `DLI_CANCEL_CREATE_CONNECTION` (0x1402, already defined in `ssap_link.h:20`) and transition to IDLE + fire `on_disconnected`/a new `on_connect_failed` callback.
2. **Parse the status byte in 0x0015.** Verify the WS73 event layout against OSPL's `[status:1][handle:2][addr:6]` (`sle_usb.rs:664-678`). Our `ssap_link.c:80-87` reads `role=data[0], handle=data[1..3], interval=data[3..5]` — if status is data[0], our role/interval fields are wrong and non-zero status is silently treated as success. If status != 0 → go IDLE (do not send 0x1802/0x1804).
3. **Add a LINK_LOSS / failed state or reason tracking.** Introduce at minimum an enum value distinguishing "locally disconnected" vs "connect failed" vs "remote loss", and carry the 0x0005 reason byte (note: OSPL decodes reason at `data[2]` of 0x0005 — `sle_usb.rs:679-687`; we read `data[0]`, so verify the layout).
4. **Gate 0x0015 and 0x0005 by state.** In `ssap_link_on_event`, only accept 0x0015 while CONNECTING (else ignore/drop, or treat as incoming-connect per section 6), and only 0x0005 when the conn_handle matches `link->conn_handle` (stale-event protection, race 4).
5. **Add a disconnect revert path.** If 0x1403 is rejected by CmdStatus/CmdComplete with an error status (or the CmdStatus event shows CMD_DISALLOWED 0x0B / INVALID_PARAMS 0x0F), revert to CONNECTED instead of staying DISCONNECTING (mirror `abort_disconnecting`, `sle_conn.rs:1405-1411`). Also allow `ssap_link_disconnect` from CONNECTING so a pending connect can be cancelled (0x1402 or 0x1403), matching OSPL T8.
6. **Handle CmdStatus/CmdComplete events for 0x1401/0x1403/0x1804.** Add cases for `DLI_CMD_STATUS_EVT 0x0001` and `DLI_CMD_COMPLETE_EVT 0x0002` (`ssap_link.h:27-28`) keyed by opcode; at minimum log/translate non-zero status and drive the abort/revert above.
7. **Timeout for the 0x0015 ack, retry policy for 0x1804.** 0x1804 SET_DATA_LEN returns 0x06 until a connection exists (`SLE-CONTROL-PLANE.md` "SET_DATA_LEN status 06（参数需连接）"). Don't send it blind on every 0x0015: send after 0x1802 completes, and retry 0x1804 (e.g. 2-3 attempts) if it fails with 0x06 while CONNECTED. Also fix the bug where 0x1802 is sent with `data` (event buffer, data[0]=role) as the param — the param should be the conn handle `{data[1], data[2]}` (`ssap_link.c:89`).
8. **Enforce supervision timeout.** Store the negotiated supervision timeout from 0x0015 (or param default 0x1F4=5 s) and run a periodic check (or an idle watchdog) tearing down CONNECTED links whose data activity exceeds `supervision_timeout*10ms`, emitting reason 0x08 (mirror `sle_conn.rs:2103-2125`, `2161-2172`).
9. **Reject duplicate connect to the same peer.** In `ssap_link_connect`, return an error if a non-IDLE link already targets the same 6-byte address (mirror EEXIST check, `sle_conn.rs:1191-1196`).

---

## 6. Open questions

- **0x0015 payload layout on WS73**: role-first (our OHOS-based parse) vs status-first (OSPL `[status:1][handle:2][addr:6]`). Needs one live capture with a real peer to settle; it changes both the status check (item 2) and the stored `role`/`conn_interval`.
- **0x0005 reason byte position**: `[handle:2][reason:1]` (OSPL) vs `reason=data[0]` (ours). Same verification needed.
- **0x1403 param layout**: OSPL sends `[handle:2]` only (`sle_dli.rs:1387-1390`); we send `[handle:2][reason=0x13]`. Does the WS73 controller accept a 3-byte param? Our lab note shows 0x1403 returning 0x06 "参数" — the 0x06 may be a param-format complaint, worth re-testing with `[handle:2]`.
- **Incoming-connect support**: does the WS73 even initiate connects on its own (peer-initiated)? OSPL has `accept_incoming`; our slim stack has no accept/reject path, so an inbound 0x0015 with no pending connect would silently become CONNECTED. Decide whether to support or drop it.
- **Which timeout unit the WS73 firmware uses for `supervisionTimeout`**: OSPL assumes 10 ms units; our param comment says the same. If the WS73 uses ms, our default 0x1F4=500 would be only 500 ms instead of 5 s.
