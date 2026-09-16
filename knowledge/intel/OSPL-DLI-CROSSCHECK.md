---
type: intel
title: "OpenSparklink ↔ WS73 DLI Dialect Cross-check"
language: zh
created: 2026-08-17
tags: [intel, opensparklink, ws73, dialect]
sources:
  - "/mnt/hdd/nearlink-stuff/OpenSparklink-linux"
  - "/mnt/hdd/nearlink-stuff/sparklink"
trust: B
stale_after: 2027-02-17
---

# OpenSparklink ↔ WS73 DLI Dialect Cross-check

Date: 2026-08-17

Scope: Cross-check the WS73 (ffff:3733) DLI HCI dialect we decoded empirically against OpenSparklink, the most complete open host-stack implementation of the NearLink (SparkLink / SLE) spec. All claims cite `file:line`.

## Sources read

OpenSparklink (kernel, `/mnt/hdd/nearlink-stuff/OpenSparklink-linux/net/sparklink/`):
- `sle_dli.rs` (1503 lines) — DLI type/opcode/event/status constants, `SleController` trait, command wrappers
- `sle_transport.rs` — transport protocol registry (H4-UART / USB-bulk / SPI), max PDU sizes
- `sle_uart.rs` — UART framing: parser + `encode_command` / `encode_event` / `encode_data`
- `sle_usb.rs` — USB framing: `build_command_packet`, `build_async_data_packet`, `parse_event_packet`, `event_to_sle`, `raw_to_status`
- `sle_mgmt.rs` — command pending queue (opcode-based correlation + timeout)
- `sle_event.rs` — host-side event ring + `SleEventType` UAPI enum

OpenSparklink (userspace crates, `/mnt/hdd/nearlink-stuff/sparklink/crates/`):
- `slk-protocol/src/types.rs` — UAPI constants `DLI_PKT_*`, `EVT_*`, `SleDliCmd` / `SleDliEvent`
- `slk-protocol/src/ioctl.rs` — ioctl numbers (magic 'S', incl. `sl_dli_send_cmd` 0x84, `sl_dli_poll_event` 0x82)
- `slk-protocol/src/lib.rs` — module layout
- `slk-protocol/src/tests.rs` — wire-format assertions
- `libsparklink/src/adapter.rs` — daemon-side `SleDliEvent` decode

Our decoded WS73 dialect:
- `.scratch/nearlink-driver/lab-notes/SLE-CONTROL-PLANE.md` — verified command/event bytes on hardware
- `.scratch/nearlink-driver/lab-notes/SSAP-DIALECT-COMPARISON.md`
- `.scratch/nearlink-driver/lab-notes/NEARLINK-PROTOCOL-RESEARCH.md`
- `stack/ssap/src/hwsle_transport.c` + `stack/ssap/include/hwsle_transport.h` — our DLI frame adapter
- `stack/ssap/include/ssap_pkt.h` — SSAP PDU definitions
- WS73 SDK: `driver/platform/drv/device/romable/include/hcc_cfg_comm.h`, `driver/bsle/sle_driver/sle_host_register.c`

## 1. DLI packet types

**Verdict: identical byte-for-byte (0xA1–0xA5).**

OpenSparklink defines the type byte in two places, both agreeing with our WS73 decode:

- Kernel enum `DliPacketType`: `Command = 0xA1`, `Event = 0xA2`, `AsyncUnicast = 0xA3`, `SyncUnicast = 0xA4`, `AsyncMulticast = 0xA5` — `sle_dli.rs:99-110`.
- Userspace crates: `DLI_PKT_COMMAND = 0xA1`, `DLI_PKT_EVENT = 0xA2`, `DLI_PKT_ASYNC_UCAST = 0xA3`, `DLI_PKT_SYNC_UCAST = 0xA4`, `DLI_PKT_ASYNC_MCAST = 0xA5` — `slk-protocol/src/types.rs:77-81`, asserted in `slk-protocol/src/tests.rs:119-127`.

Our WS73 decode: `0xA1 CMD / 0xA2 EVENT / 0xA3 ACB / 0xA4 ICB` (ACB = async connection-based data, ICB = isochronous) — `hwsle_transport.h` (`HCI_DATATYPE_CMD 0xA1` … `HCI_DATATYPE_ICB 0xA4`), and `NEARLINK-PROTOCOL-RESEARCH.md:35` ("Datatypes 0xA1 CMD / 0xA2 EVENT / 0xA3 ACB / 0xA4 ICB (identical DLI/WS73)").

Notes:
- `0xA5` (async multicast) exists in the standard and OpenSparklink, but our WS73 lab notes never exercised it (we only used 0xA1–0xA4). Treat as ❓unverified against WS73 hardware.
- Our WS73 name for 0xA4 ("ICB") maps to the standard's "SyncUnicast"; the WS73 SDK's ICB (isochronous) data path is configured via the sync-unicast opcode `0x280D` (`SLE-CONTROL-PLANE.md:98`), which is the DLI `SyncDataPathConfig` (`sle_dli.rs:659`). Same wire value, different vendor vocabulary.

## 2. Command framing

**Header: verdict — same 5-byte command header `[0xA1][opcode u16 LE][plen u16 LE][params]`.**

- OpenSparklink UART `encode_command`: `[0xA1][opcode LE16][plen LE16][params]` — `sle_uart.rs:293-307`; `CMD_HEADER_SIZE = 5` — `sle_uart.rs:64`.
- OpenSparklink USB `build_command_packet`: identical 5-byte header `[0xA1][opcode LE16][plen LE16][params]` — `sle_usb.rs:467-483`.
- Our adapter `hwsle_transport_send_hci_cmd`: `[0xA1][opcode u16 LE][plen u16 LE][params]` — `hwsle_transport.c:78-96`, confirmed on hardware (`SLE-CONTROL-PLANE.md:15`).

**Size limits:**
- OpenSparklink caps command params at 255 B: `MAX_PAYLOAD_LEN = 255` (UART, `sle_uart.rs:73`), `DLI_PARAM_MAX = 255` (USB, `sle_usb.rs:449`); per-protocol `max_pdu = 255` (`sle_transport.rs:326-343`). The userspace ioctl path caps params at 240 B: `params: [u8; 240]` in `SleDliCmd` (`types.rs:718-723`), `CMD_REQ_PARAM_MAX = 240` (`sle_mgmt.rs:255`).
- WS73: our largest verified command is the 49-byte `DLI_AdvParam` (`SLE-CONTROL-PLANE.md:36,52-64`); adv data up to 251 B (`SLE-CONTROL-PLANE.md:113`). No WS73 limit conflict observed; 255-B params fits the standard.

**Parameter byte order:** little-endian for all multi-byte scalars in both stacks (`sle_uart.rs:299-304`, `sle_usb.rs:473-478`; our `put_u16` in `hwsle_transport.c:24-29`). Parameter struct layouts are per-opcode structs on both sides; our WS73-verified `DLI_AdvParam` (49 B, `SLE-CONTROL-PLANE.md:52-64`) and `DLI_ScanParam` (8 B, `SLE-CONTROL-PLANE.md:44-50`) came from the OHOS stack `dli_def.h` — the same layout family OpenSparklink follows (T/XS 10003-2025 section 8).

**Segmentation:** neither OpenSparklink nor DLI segments command frames at the transport level (a single command frame holds up to 255 B of params). Large adv data is handled inside the command params via the broadcast-data "operation" fragment field (FIRST→INTERMEDIATE→LAST) — our WS73 notes require exactly this for >single-slice `SET_ADV_DATA` (`SLE-CONTROL-PLANE.md:37`, `NEARLINK-PROTOCOL-RESEARCH.md:44` DTAP-level frag); OpenSparklink's `SetBroadcastData` (`SleOpcode::SetBroadcastData = 0x0C03`, `sle_dli.rs:467`) passes the same params through unchanged.

## 3. Event framing & command correlation

**OpenSparklink event model (standard):**
- UART event frame: `[0xA2][event_code u16 LE][plen u16 LE][params]` — `sle_uart.rs:310-324`; `EVENT_HEADER_SIZE = 5` — `sle_uart.rs:67`.
- USB event frame (interrupt IN, type byte implicit from endpoint): `[event_code u16 LE][plen u16 LE][params]` — `parse_event_packet`, `sle_usb.rs:546-570`.
- Event codes: `CmdStatus = 0x0001`, `CmdComplete = 0x0002`, `ConnEstablished = 0x0015`, `DisconnectDone = 0x0005`, `BroadcastReport = 0x001A`, etc. — `DliEventCode`, `sle_dli.rs:700-803`.
- `CmdComplete` params: `[opcode:2 LE][status:1][return data:N]` — `sle_usb.rs:628-651`.
- `CmdStatus` params: `[status:1][opcode:2 LE]` — `sle_usb.rs:653-663`.

**Correlation mechanism:** there is **no wire sequence number**. Correlation is by **opcode echo** in the CmdComplete/CmdStatus params, resolved against the oldest pending matching entry in `CmdPendingQueue::resolve()` — `sle_mgmt.rs:194-204`. Each entry carries a host-side `seq: u32` for dedup only (never transmitted) — `sle_mgmt.rs:38,241-247`; `SleDliCmd.seq` is a UAPI field (`types.rs:721`). Pending queue depth 32, per-command timeout 5000 ms — `sle_mgmt.rs:132,31`.

**Our WS73 decode:**
- Command complete: `[0xA2][0x0002 LE][plen u16 LE][num_hci_pkts=0x01][opcode u16 LE echo][status 1B][return data...]` — `SLE-CONTROL-PLANE.md:16-17`.
- Async commands first return a "command status" style event before the real completion — `SLE-CONTROL-PLANE.md:19`.

**Divergences:**
1. **`num_hci_pkts` byte (0x01)** after the plen field in WS73 CmdComplete events is **not** part of OpenSparklink's CmdComplete param layout (OpenSparklink expects `opcode` at params[0..2]; `sle_usb.rs:631-632`). Our own tooling is inconsistent about where it sits: `SLE-CONTROL-PLANE.md:16` places it right after plen (offset 4), while `scripts/ws73-probe/sle-adv.py:48-53` parses the opcode echo at frame offset 5–6 (i.e., treats `num_hci_pkts` as absent and header as `[02 00][plen]`). ⚠️ Must be pinned down on hardware — if the byte really exists, an OpenSparklink-style parser reading params[0..2] as opcode would be misaligned by one byte.
2. **WS73 "command status" event code**: our notes captured `[A2 0a 00 01 00 00]` for the async-ack event (`SLE-CONTROL-PLANE.md:19`) — that reads as event_code `0x000A`, which the DLI spec assigns to `HwError` (`sle_dli.rs:718`), not `CmdStatus` (`0x0001`). Either the capture is mistranscribed, or the WS73 firmware emits a non-standard code for command-status. ❓Unverified — needs re-capture on hardware.
3. **Status code values differ.** OpenSparklink's `SleStatus`: `0x00 Success, 0x01 UnknownCommand, 0x02 InvalidParameters, … 0x08 Timeout` — `sle_dli.rs:681-691`, mapped in `raw_to_status` (`sle_usb.rs:1290-1303`). Our WS73 observed statuses are a different, larger errno table: `0x0B CMD_DISALLOWED`, `0x0F INVALID_PARAMS`, `0x1E UNKNOWN_ADVERTISING_IDENTIFIER`, and our notes explicitly warn "尾部 return data ≠ status!" (`SLE-CONTROL-PLANE.md:17-18,20-22`). ⚠️ Do not reuse OpenSparklink `raw_to_status` for WS73.

## 4. Flow control

**OpenSparklink has no DLI-layer credits or windowing.** Flow control is entirely host-side bookkeeping:
- Outgoing commands: `CmdRequestQueue` depth 64 (`sle_mgmt.rs:268`), drained by a worker; pending resolution via `CmdPendingQueue` depth 32 with a 5 s timeout (`sle_mgmt.rs:132,31`).
- Incoming events: `ControllerEventRing`, 32 slots, drop-on-full with a counter — `sle_dli.rs:30-47`.
- USB bus: events on interrupt IN (`EP_EVENT_IN 0x91`), data on bulk IN (`EP_DATA_IN 0x92`), commands+data on bulk OUT (`EP_CMD_DATA_OUT 0x12`) — `sle_usb.rs:417-421`.
- Data-frame max payload 511 B (9-bit length field) — `DLI_DATA_PAYLOAD_MAX`, `sle_usb.rs:452`.

**WS73 queue split is a transport-layer (HCC) concept OpenSparklink does not have:**
- `SLE_DATA_QUEUE = 8` ("used for gle data") and `BSLE_MSG_QUEUE = 10` ("used for bgle msg") — `sdk/.../hcc_cfg_comm.h:50,52`. Commands/INI customization ride `BSLE_MSG_QUEUE` (`customize_bsle.c:283,654`), SLE data rides `SLE_DATA_QUEUE` (`sle_hcc_proc.c:127`).
- These are HCC queue identifiers on the WS73 firmware's internal message bus, not DLI credits. Nothing in OpenSparklink models them; OpenSparklink treats the controller as a dumb command/data sink.

## 5. Opcode comparison table

WS73 verified opcodes (`SLE-CONTROL-PLANE.md:24-42,92-104,111-125`; `NEARLINK-PROTOCOL-RESEARCH.md:29,41-48`) vs OpenSparklink `SleOpcode` (`sle_dli.rs:404-676`). "✅same-as-WS73" means the opcode *value* matches; WS73 names in quotes are our HCI-flavored labels.

| # | Our WS73 command (name used in lab notes) | WS73 opcode | OpenSparklink `SleOpcode` (value) | Verdict |
|---|---|---|---|---|
| 1 | "SET_EVENT_MASK" (8B mask) | 0x0401 | `ReadCmdLen` = 0x0401 | ⚠️ value matches but semantic differs — DLI 0x0401 is a *read* (读控制器指令长度, `sle_dli.rs:407`), no params. We labeled it with a BLE-HCI name and WS73 accepted an 8-byte mask; must re-verify what 0x0401 really is on WS73 |
| 2 | READ_LOCAL_BUFFER | 0x0402 | `ReadCtrlBuffer` = 0x0402 | ✅ `sle_dli.rs:409` |
| 3 | READ_LOCAL_SUPPORT_FEATS | 0x0403 | `ReadLocalFeatures` = 0x0403 | ✅ `sle_dli.rs:411` |
| 4 | READ_LOCAL_VERSION | 0x0404 | `ReadLocalVersion` = 0x0404 | ✅ `sle_dli.rs:413` |
| 5 | SET_PUBLIC_ADDRESS (6B MAC) | 0x0405 | `SetMacAddr` = 0x0405 | ✅ `sle_dli.rs:415` |
| 6 | GET_PUBLIC_ADDRESS | 0x0406 | `ReadMacAddr` = 0x0406 | ✅ `sle_dli.rs:417` |
| 7 | RESET | 0x0408 | `Reset` = 0x0408 | ✅ `sle_dli.rs:421` |
| 8 | READ_ACCESS_FILTER_SIZE | 0x040A | `ReadWhitelistSize` = 0x040A | ✅ `sle_dli.rs:425` |
| 9 | SET_ADV_PARAMS (49B `DLI_AdvParam`) | 0x0C02 | `SetBroadcastParam` = 0x0C02 | ✅ `sle_dli.rs:465` |
| 10 | SET_ADV_DATA (fragment via operation field) | 0x0C03 | `SetBroadcastData` = 0x0C03 | ✅ `sle_dli.rs:467` |
| 11 | SET_SCAN_RSP_DATA | 0x0C04 | `SetBroadcastScanRsp` = 0x0C04 | ✅ `sle_dli.rs:469` (WS73 returned 0x1E — needs scan context; `SLE-CONTROL-PLANE.md:119`) |
| 12 | SET_ADV_ENABLE | 0x0C05 | `EnableBroadcast` = 0x0C05 | ✅ `sle_dli.rs:471` ⚠️ param layout: we send 5B `[enable][handle][duration u16][maxEvents]` (`sle-adv.py:96`), OpenSparklink wrapper sends 1B `[enable]` (`sle_dli.rs:1355-1357`) — wrapper is a simplification |
| 13 | READ_MAX_ADV_DATA_LEN (implied by 0xFB result) | 0x0C06 | `ReadMaxBcastDataLen` = 0x0C06 | ✅ `sle_dli.rs:473`, `SLE-CONTROL-PLANE.md:114` |
| 14 | SET_SCAN_PARAMS (8B `DLI_ScanParam`) | 0x1001 | `SetScanParam` = 0x1001 | ✅ `sle_dli.rs:489` |
| 15 | SET_SCAN_ENABLE | 0x1002 | `EnableScan` = 0x1002 | ✅ `sle_dli.rs:491` (OpenSparklink wrapper 1B `[enable]`, `sle_dli.rs:1360-1362`; we send 2B `[enable][filterdup]`) |
| 16 | CREATE_CONNECTION | 0x1401 | `CreateConnection` = 0x1401 | ✅ `sle_dli.rs:499` |
| 17 | DISCONNECT | 0x1403 | `Disconnect` = 0x1403 | ✅ `sle_dli.rs:503` (WS73 returned status 06/params needing a live handle; `SLE-CONTROL-PLANE.md:42`) |
| 18 | CONNECTION_UPDATE | 0x1807 | `ConnParamUpdate` = 0x1807 | ✅ `sle_dli.rs:519` |
| 19 | SET_DATA_LEN | 0x1804 | `SetMaxDataLen` = 0x1804 | ✅ `sle_dli.rs:513` |
| 20 | SET_MCS | 0x180A | `SetCodingModulation` = 0x180A | ✅ `sle_dli.rs:525` |
| 21 | READ_REMOTE_VERSION | 0x1802 | `ReadVersion` = 0x1802 | ✅ `sle_dli.rs:511` |
| 22 | READ_PHY (19B return) | 0x1805 | `ReadPhyParam` = 0x1805 | ✅ `sle_dli.rs:515` |
| 23 | SET_PHY | 0x1806 | `SetPhyParam` = 0x1806 | ✅ `sle_dli.rs:517` |
| 24 | READ_REMOTE_RSSI | 0x180C | `ReadRssi` = 0x180C | ✅ `sle_dli.rs:527` |
| 25 | "ENCRYPT"/"RANDOM" | 0x1C01 / 0x1C02 | `HashCompute` = 0x1C01 / `GenSecureRandom` = 0x1C02 | ⚠️ value matches (`sle_dli.rs:547,549`), but our label "ENCRYPT" for 0x1C01 is wrong per DLI (0x1C01 = HashCompute; link-encryption start is 0x1C03 `StartEncrypt` `sle_dli.rs:551`). Only status-06 "present" was seen, no real crypto exchange (`SLE-CONTROL-PLANE.md:104`) |
| 26 | READ_MEASURE_CAPS | 0x2001 (0x2002 variant) | `ReadLocalMeasCap` = 0x2001 | ✅ `sle_dli.rs:619`; ❓ 0x2002 not in DLI enum — vendor sub-variant |
| 27 | SET_MEASURE_EN | 0x2005 | `MeasAction` = 0x2005 | ✅ `sle_dli.rs:623` |
| 28 | SETUP_ICB_DATA_PATH | 0x280D | `SyncDataPathConfig` = 0x280D | ✅ `sle_dli.rs:659` (WS73 calls it ICB, DLI calls it sync-unicast data path) |
| 29 | CREATE_IOB | 0x2803 | `SyncUcastCreate` = 0x2803 | ✅ `sle_dli.rs:639` |
| 30 | SET_IOG_PARAM | 0x2801 | `SyncUcastParam` = 0x2801 | ✅ `sle_dli.rs:637` |
| — | SLE_OPEN / SLE_CLOSE | — (not DLI opcodes) | no equivalent DLI command | ❓ SLE_OPEN=29 / SLE_CLOSE=30 are **HCC transport messages** (`hcc_cfg_comm.h:124-125`), not 0xA1 DLI commands; triggered by `/dev/hwsle` open/close (`sle_host_register.c:126,153`, `docs/USB-PROTOCOL.md:108-113`). OpenSparklink's `SleController::open()/close()` (`sle_dli.rs:1124-1127`) is a host-side lifecycle hook with no wire message — nothing to compare on the wire |

Summary: **all 30 verified WS73 opcode values sit in the standard DLI opcode space and match OpenSparklink 1:1** (the 0x0401/0x1C01/0x1C02 items are naming/semantics mismatches in *our* lab vocabulary, not wire mismatches). No opcode value collides.

## 6. Divergences (things OpenSparklink does differently from our WS73 decode)

1. **Data-plane frame field semantics.** OpenSparklink encodes the async data header as `link_id_seg = [15:4] link_id (12b) | [3:2] segmentation (2b) | [1] reserved | [0] priority`, and length as a 9-bit field — `sle_usb.rs:491-517,608-622`; UART variant `[handle 12b | flags 4b]` — `sle_uart.rs:269-271,339`. Our WS73 ACB frame carries the **raw TCID** in that same 2-byte slot: `[0xA3][tcid u16 LE][len u16 LE][payload]` (`hwsle_transport.c:56-71`; "TCID at frame offset 5 (2B) demuxes data planes" — `NEARLINK-PROTOCOL-RESEARCH.md:35-37`). Both occupy bytes 1–2 after the type byte, but they are different address spaces: a WS73 TCID (e.g. 0x000A = SLE_SMTC, `hwsle_transport.h`) fed into OpenSparklink's parser would decode as `link_id=0, seg=2, prio=0`. Same for length: WS73 uses a plain u16 len, OpenSparklink masks to 9 bits. This is the single most important porting trap.
2. **`num_hci_pkts` byte** in WS73 CmdComplete events — absent from OpenSparklink's param layout (see §3.1).
3. **Status/errno tables differ** (§3.3).
4. **Async "command status" event code** on WS73 looks like 0x000A vs standard 0x0001 (§3.2) — unverified.
5. **OpenSparklink enables broadcast/scan with minimal 1-byte param wrappers** (`sle_dli.rs:1355-1362`), whereas WS73 accepted the full 5-byte `SET_ADV_ENABLE` / 2-byte `SET_SCAN_ENABLE` param blocks (`sle-adv.py:96,105`). The full layout is the standard one; the wrappers are convenience simplifications.

## 7. Actionable takeaways for our stack

1. **Keep our 5-byte command header** `[0xA1][opcode u16 LE][plen u16 LE][params]` — it is byte-identical to OpenSparklink and the standard (`sle_uart.rs:15-19,293-307`; `sle_usb.rs:463-483`). No change needed in `hwsle_transport_send_hci_cmd` (`hwsle_transport.c:78-96`).
2. **Our ACB data framing `[0xA3][tcid u16][len u16][payload]` is WS73-correct and should stay as-is** — do not "standardize" it to OpenSparklink's `link_id<<4|seg|prio` form; that would break against WS73 firmware. Document the mapping if we ever bridge an OpenSparklink-style upper layer.
3. **Adopt OpenSparklink's command-correlation pattern in `hwsle_transport.c`**: track a small pending-command table keyed by **opcode echo** with a timeout (mirror `CmdPendingQueue`, `sle_mgmt.rs:132-204`), instead of the current fire-and-forget send + loose event skip in `hwsle_transport_run` (`hwsle_transport.c:98-141`). The WS73 already echoes the opcode in every CmdComplete (`SLE-CONTROL-PLANE.md:16`), so correlation is free.
4. **Fix the event parser alignment**: today `hwsle_transport_run` skips events by assuming plen at bytes 3–4 and 5-byte header (`hwsle_transport.c:129-134`). Resolve the `num_hci_pkts` question (§3.1) — either parse the WS73 CmdComplete as `[event_code][plen][num_hci_pkts=01][opcode][status][data]` (per `SLE-CONTROL-PLANE.md:16`) and route async completions, or re-capture and correct the note. This also unlocks proper status/errno handling (WS73 errno table, not OpenSparklink `SleStatus`).
5. **Rename our lab command labels to DLI names**: `0x0401` is *ReadCmdLen* (not SET_EVENT_MASK), `0x1C01` is *HashCompute* (not ENCRYPT) — update `SLE-CONTROL-PLANE.md` and any scan tooling to avoid future confusion.
6. **SLE_OPEN/SLE_CLOSE are not DLI commands** — keep them out of the DLI opcode tables; they live at the HCC transport layer (`hcc_cfg_comm.h:124-125`). Our `/dev/hwsle` open/close already handles them (`hwsle_transport_open`/`close`, `hwsle_transport.c:31-49`).
7. **For future interop**, OpenSparklink's `SleController` trait + `DliPacketType` enum is a faithful reference for the standard's framing; use `sle_usb.rs:467-483` as the canonical "standard DLI" command encoder if we ever need to talk to a second, standards-compliant NearLink controller over USB interrupt/bulk endpoints (`sle_usb.rs:417-421`).

## 8. Open questions

- Exact byte position / presence of the `num_hci_pkts` byte in WS73 CmdComplete events — our notes and our probe script disagree (`SLE-CONTROL-PLANE.md:16` vs `sle-adv.py:48-53`).
- What event code the WS73 really emits for async "command status" (`0x000A` captured vs standard `0x0001`)? (`SLE-CONTROL-PLANE.md:19`)
- WS73 status/errno table values (0x0B / 0x0F / 0x1E …) — full enumeration unverified; not covered by OpenSparklink `SleStatus`.
- Semantics of 0x0401 on WS73 (mask write accepted, but DLI standard says read-only).
- 0x2002 measurement-caps variant — vendor extension not in the DLI enum.
- 0xA5 async-multicast data frames: never exercised on WS73 hardware.
