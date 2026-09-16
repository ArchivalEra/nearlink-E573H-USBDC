---
type: intel
title: "OpenSparklink USB Transport Teardown — sle_usb.rs / sle_usb_ffi.c"
language: en
created: 2026-08-17
tags: [intel, opensparklink, transport, teardown]
sources:
  - "knowledge/intel/kernel-init-seq.md"
trust: B
stale_after: 2027-02-17
---

# OpenSparklink USB Transport Teardown — sle_usb.rs / sle_usb_ffi.c

Date: 2026-08-17
Author: research subagent (issue 06 context: in-house `ws73usb` kernel driver)

## Sources

Primary (OpenSparklink-linux, kernel 7.x in-tree Rust + C FFI):
- `net/sparklink/sle_usb.rs` (1625 lines) — Rust DLI-over-USB controller, packet builders/parsers, USB driver registration
- `net/sparklink/sle_usb_ffi.c` (1094 lines) — the real URB machinery (all USB I/O lives here in C)
- `net/sparklink/sle_transport.rs` (360 lines) — protocol registry + attach/detach framework (hci_uart_proto + hci_register_dev model)
- `net/sparklink/sle_serdev.rs`, `sle_uart.rs`, `sle_spi.rs` — other-bus shapes of the same `SleController` abstraction
- `net/sparklink/sle_fw.rs` (185 lines) — firmware download orchestration
- `net/sparklink/sle_dli.rs` — DliPacketType, SleBus, SleController trait

Our side (WS73 contract):
- `.scratch/nearlink-driver/assets/kernel-init-seq.md` — 5-EP WORK-mode decode
- `sdk/ws73_sdk_linux_WS73_1.10.110/driver/platform/hcc/host/hcc_usb_host.h` — HCC USB EP indices, package formats, URB pool sizes
- `.scratch/nearlink-driver/issues/06-kernel-driver-skeleton.md` — our driver design questions

## Target chip ID

- **No HiSilicon chip is bound by VID:PID.** The USB id table has exactly two entries (sle_usb.rs:1466-1480):
  1. `0x1234:0x5678` — a **QEMU `usb-sle-dli` virtual controller** (comment at sle_usb.rs:1471). Not real hardware.
  2. A **class/subclass/protocol match**: `bInterfaceClass 0xE0` (Wireless Controller), `bInterfaceSubClass 0x01` (RF Controller), `bInterfaceProtocol 0x05` (SparkLink DLI) — sle_usb.rs:406-410, 1475-1478.
- So OpenSparklink binds *any* device that declares itself a SparkLink DLI interface; it is a **generic SLE-DLI-protocol driver** implementing the Chinese SparkLink standard T/XS 10003-2025 (USB binding), not a driver for a specific HiSilicon chip.
- The only chip family named in the tree is **"WS63-SLE"** — as a *doc comment example* for a controller name field (sle_dli.rs:278). WS63 is the previous HiSilicon SLE-generation sibling of our WS73. No BS21/BS25/WS73 strings anywhere in net/sparklink.
- Probe does not read the device until after attach; the MAC is first synthesized (`0x5E:00:00:00:01:NN`, sle_usb.rs:1505) and later overwritten by a real ReadMacAddr response (sle_usb.rs:1533-1546). Firmware file names are generic `sparklink/sle_usb_v1.bin` (sle_usb.rs:1555, sle_fw.rs:58).

**Conclusion for item 1:** OpenSparklink targets the **SLE DLI standard transport (T/XS 10003-2025)**, attached to a QEMU virtual device or any class-compliant device. It is *not* the WS73 HCC/boot-mode USB contract. WS63/WS73 share the SLE radio family, but the USB transport OpenSparklink implements is a *different, spec-defined binding* — not the HiSilicon HCC `ffff:3733` pipe. Same radio-family DLI command set (opcodes/events), completely different USB framing.

## EP layout

OpenSparklink (host perspective; sle_usb.rs:17-20, 417-421; defaults in sle_usb_ffi.c:351-353):

| EP | addr | type | purpose | size |
|----|------|------|---------|------|
| 0 | EP0 | Control | "DLI instructions (standard path)" — **declared, never used** | — |
| 1 | 0x91 | INT IN | DLI events from controller | 64 B (EVENT buf 64, ffi.c:358) |
| 2 | 0x92 | BULK IN | async data from controller | RX buf 520 B (ffi.c:359) |
| 3 | 0x12 | BULK OUT | async data + commands to controller | TX cmd buf 260 / data buf 520 (ffi.c:356-357) |

- Endpoint addresses are *discovered* at probe with `usb_find_common_endpoints()` (ffi.c:433), falling back to hardcoded 0x91/0x92/0x12 defaults if not found (ffi.c:437-444). The `ep_bulk_in_size` wMaxPacketSize is read from the descriptor (ffi.c:449) and the RX URB buffer is sized from it (ffi.c:465-466).
- `SLE_USB_REQ_TYPE = 0x20` (sle_usb.rs:431) is defined but **grep shows it is never used anywhere**; there is no `usb_control_msg` in the entire net/sparklink tree. All host→device DLI traffic (commands and data) is bulk OUT on 0x12. The "EP0 control" row of Table 3 is doc-only.
- Interface class 0xE0/0x01/0x05 is *advertised* by the driver's own match table, not parsed from the device to alter behavior.

**EP model is 3 endpoints (0x91/0x92/0x12)**, versus our WS73 kernel-mode **5 endpoints** (BULK_IN 0x81, BULK_OUT 0x01, INT_IN 0x83, RW_REG 0x02/0x82, hcc_usb_host.h:58-64) — plus WS73 has a separate 2-EP boot mode (BULK 0x01/0x81?) used for firmware download. OpenSparklink has no boot-mode/writable-register EPs and no concept of a register-access pair.

## RX path

- Two auto-resubmitting URBs, submitted on `open()` via `sle_usb_dev_start_evt` / `sle_usb_dev_start_data` (sle_usb.rs:1365-1377):
  - INT IN URB (64 B), resubmit in callback on status==0 or -EOVERFLOW (ffi.c:98-112, sle_usb_ffi.c `sle_usb_intr_cb`).
  - BULK IN URB (520 B), same auto-resubmit rule (ffi.c:82-96, `sle_usb_bulk_in_cb`).
- Both callbacks funnel into the **same** Rust `sparklink_usb_complete()` (ffi.c:73-79 passes `urb->actual_length`; sle_usb.rs:323-399). No endpoint tagging — `rust_ctx` only carries `dev_id+1` (ffi.c:643, 683), so the parser cannot know which pipe delivered a packet.
- Parsing in the completion callback (sle_usb.rs:354-398):
  1. If `len >= 5 && slice[0] == 0xA3` → treat as **async data**: header `[0xA3][link_id_seg:2 LE][plen:2 LE][payload...]`, handle = `(link_id_seg >> 4) & 0x0FFF` (sle_usb.rs:355-373). This is only the *first* packet of the URB — **any additional packets in the same 520-byte buffer are silently ignored**.
  2. Else attempt `parse_event_packet` (sle_usb.rs:546-570): `[event_code:2 LE][param_len:2 LE][params...]` (no type byte — implicit from INT EP), param_len capped at DLI_PARAM_MAX=255 (sle_usb.rs:556-558). Mapped to `SleEvent` via `event_to_sle` (sle_usb.rs:625-1143).
  3. Any other first byte → debug-logged and dropped (sle_usb.rs:391-397).
- Parsed events/data are pushed into a global `USB_EVENT_RING` (`ControllerEventRing`) and an event pump kthread is kicked (sle_usb.rs:368-372, 378-382); high-level consumers drain via `drain_usb_events` (sle_usb.rs:61-75). Sync command *responses* do **not** go through this ring: during init, `sle_usb_send_cmd_sync` does a blocking `usb_bulk_msg` on bulk IN and parses the response directly (ffi.c:739-777) — meaning the auto-resubmit bulk-in URB **races with the synchronous bulk_msg** reading the same endpoint.
- No aggregation, no scatter header, no per-URB multi-packet loop, no credit/queue parsing. Each 520 B URB is expected to hold at most one DLI frame; the UART parser (`UartParser.feed_bytes`, byte-by-byte state machine, sle_uart.rs:157-283) is *not* applied to USB.

## TX path

- Commands: `sle_usb_dev_send_cmd` builds `[0xA1][opcode:2 LE][param_len:2 LE][params]`, param_len capped at 255, then **synchronous** `usb_bulk_msg(..., SLE_CMD_TIMEOUT_MS=5000)` on bulk OUT 0x12 (ffi.c:523-564). Rust's `build_command_packet` (sle_usb.rs:467-483) duplicates this builder but `UsbController::send_command` ignores it and calls the C path (sle_usb.rs:1389-1418).
- Data: `sle_usb_dev_send_data` builds `[0xA3][link_id_seg:2 LE][data_len_field:2 LE][payload]`, payload capped at 511; `link_id_seg = (handle & 0x0FFF) << 4` (ffi.c:605) — **segmentation and priority bits are not set by the C path** (the Rust builder `build_async_data_packet` supports seg/priority, sle_usb.rs:499-524, but is unused for TX). Also synchronous `usb_bulk_msg` (ffi.c:616-618).
- Everything is one packet = one URB = one synchronous call. **No queue-switch frames, no aggregation, no padding, no credit management, no fragmentation across URBs.** UART `encode_data` uses the same 5-byte header and 12-bit handle (sle_uart.rs:339-348), so TX framing is shared across buses at the DLI layer; only the byte conduit differs.

## Firmware / boot handling

- Present and *modeled after BT HCI firmware download* (ffi.c:885-1002, sle_fw.rs):
  1. `FW_DOWNLOAD_START` DLI command opcode `0xF810`, param = LE32 total size (ffi.c:951-962).
  2. Chunks on bulk OUT: `[0xA5 marker][offset:4 LE][chunk_len:2 LE][data]` (ffi.c:971-978), default chunk 240 B safe for full-speed 64 B MTU (ffi.c:906), auto chunk = `wMaxPacketSize - 7` (ffi.c:942-944), timeout 10 s.
  3. `FW_DOWNLOAD_DONE` opcode `0xF811`, then sync `usb_bulk_msg` wait for CommandComplete (ffi.c:992-997).
- Blob loaded via `request_firmware()` from `sparklink/sle_usb_v1.bin` (sle_fw.rs:108, sle_usb.rs:1555-1561); missing blob is non-fatal (driver proceeds on already-programmed controller).
- **No boot-mode / re-enumeration concept.** No 64-byte header stripping, no `WRITEM/FILES/QUIT` text protocol, no transition from 2-EP boot mode to 5-EP work mode. That machinery exists only in the HiSilicon SDK (ours), not in OpenSparklink.
- Init sequence after attach: Reset (0x0408) → ReadLocalVersion (0x0404) → ReadMacAddr (0x0406), responses parsed from bulk IN (ffi.c:793-851).

## Comparison table: OpenSparklink DLI-USB vs our WS73 HCC contract

| Aspect | OpenSparklink (T/XS 10003-2025 USB) | WS73 HCC kernel mode (ours) |
|---|---|---|
| Device binding | generic class 0xE0/0x01/0x05 + QEMU 0x1234:0x5678 | VID:PID ffff:3733 (also boot-mode id) |
| EP count | 3 (INT IN 0x91, BULK IN 0x92, BULK OUT 0x12); EP0 control unused | 5 (BULK_IN 0x81, BULK_OUT 0x01, INT_IN 0x83, RW_REG_OUT 0x02, RW_REG_IN 0x82) |
| Boot mode / fw download EP | none (fw downloaded on normal bulk OUT) | 2-EP boot mode: WRITEM/FILES/QUIT text protocol, then re-enumerate to 5-EP |
| TX frame header | 5 B: `[type][hdr:4]` per DLI packet (0xA1 cmd / 0xA3 data) | 12 B `usb_package{msg_type,len,reserve}` (hcc_usb_host.h:272-276) + netbuf w/ 5 B `hcc_header`; optional queue-switch descr frame |
| RX frame header | none beyond the DLI packet itself (implicit by EP) | 92 B scatter header `HIUSB_PACKAGE_HEARDER_SIZE` w/ `usb_package{msg_type,len,reserve}` + `aggr_len[0..23]` per-packet u16 lengths (kernel-init-seq.md:5) |
| Aggregation | none; 1 frame per URB, multi-packet URBs truncated | scatter/aggregation with per-packet lengths; USB_ARRG_DEV_HOST_DATA (2) |
| Queues/credits | none at USB layer | queue_id in package reserve + hcc_header byte 1; BSLE_MSG_QUEUE=10, SLE_DATA_QUEUE=8; TX queue-switch descr frames |
| Control transfers | none (constant defined, never sent) | `0x21/req=0/4-byte u32` H2D_MSG (SLE_OPEN=29) via `hcc_usb_send_msg` |
| INT EP payload | 64 B DLI event frames (event_code+params) | 8 B `usb_dev_notification{notification,dev_mem_highpri_pool}` D2H bitmask (hcc_usb_host.h:278-281) |
| TX sync/async | all synchronous `usb_bulk_msg` | URB pool (URB_RX_MAX_NUM=3, URB_TX_MAX_NUM=8, 20 KB bufs) + RX/TX threads |
| Padding | none | each payload padded to >= USB_MIN_PACKAGE_LEN 0x40 (kernel-init-seq.md:3) |
| Min/max PDU | param<=255, payload<=511, MTU 512 | HCC netbuf up to 800 B (customize) / 20 KB URB cap |
| Events on INT vs bulk | events INT, async data bulk (but single parser) | D2H msg bits on INT; data/acks on bulk queues |
| Init | Reset→ReadLocalVersion→ReadMacAddr over bulk | power-on→fw download→BSLE_MSG service init→customize/INI push→H2D_MSG_SLE_OPEN→poll status acks |

**Shared DNA:** both are DLI/HCI-style transports where the radio command set (SLE opcodes/events) is identical; both use bulk for data and an interrupt/notification IN; both have a per-packet length field and a message/type byte at the front. The differences are all in the *transport container*: OpenSparklink wraps each DLI frame in a tiny 5-byte header and sends it raw; WS73 wraps everything in a 12-byte `usb_package` (+ 92-byte scatter header on RX + queue-switch descriptors on TX) and pushes control-plane messages through class control transfers with a 8-byte D2H notification bitmask on the INT EP.

## Takeaways for ws73usb (our in-house C module)

What to **copy**:
1. **DLI/controller abstraction as a trait-like ops table.** `SleController` (sle_dli.rs) + per-bus `SleProtoEntry` registry (sle_transport.rs:76-168) is the cleanest part of this codebase — exactly what our issue-02 "transport adapter" and issue-06 `/dev/ws73hci` design needs. In C: an `struct ws73hci_ops { send_cmd, send_data, ... }` per backend (USB/serial), registered into a small table, with attach/detach mirroring `hci_register_dev`. OpenSparklink's separation of *framing* (per-bus) from *DLI semantics* (shared `event_to_sle`) is the model; we already share the DLI command/event vocabulary with them, so our char device can expose the same "raw DLI frame" semantics.
2. **Discovery of EPs from descriptors with hardcoded fallback** (ffi.c:433-444) — resilient probe.
3. **Auto-resubmit INT/BULK IN URBs** that tolerate `-EOVERFLOW` (ffi.c:90, 106) — good practice for our INT 0x83 (8-byte notification) and bulk 0x81 (92-byte scatter header + data) pipes.
4. **Init handshake is non-fatal degraded-mode** (ffi.c:785-791, sle_usb.rs:1528-1562): if the radio does not answer, continue with placeholder values. Valuable for our bring-up too.
5. **request_firmware() + versioned filename fallback** (sle_fw.rs:166-185) — we already decided this for ws73usb (issue 05); OpenSparklink confirms the pattern, though their wire protocol (0xF810/0xF811 chunks) is *not* ours (we use WRITEM/FILES/QUIT).

What to **avoid**:
1. **A single completion callback that cannot tell which EP delivered data** (ffi.c:73-79 → sle_usb.rs:323). They guess by first byte; a 0xA2 event could arrive on bulk or INT. We should tag URBs per-EP (int vs bulk) and route accordingly.
2. **Synchronous `usb_bulk_msg` for every TX including async data** — serializes TX, no queueing, no throughput. Their whole data path would collapse under WS73-style load. Use a URB pool + TX thread like `hcc_usb_host.c` (URB_TX_MAX_NUM=8, 20 KB).
3. **Ignoring extra packets in one URB** (sle_usb.rs:355-373 parses only packet #1). Our 92-byte scatter header *requires* per-packet length parsing; we must loop over `aggr_len[]` — OpenSparklink is a cautionary example of the naive alternative.
4. **Dead constants documented as protocol** (`SLE_USB_REQ_TYPE` 0x20, EP0 control path, `build_command_packet`/`build_async_data_packet` unused for TX) — misleading. Keep one encoder per direction.
5. **Dev_id-encoding in pointers** `(dev_id+1)` cast to `void*` (ffi.c:643) — fragile; a real device table (like `struct hcc_usb` in hcc_usb_host.h) is better.

Rust-in-kernel vs C — ABI/semantics notes:
- OpenSparklink's Rust layer owns *no* URBs; it wraps C via `#[no_mangle]` FFI (sle_usb.rs:101-156, 322-399). So "Rust USB transport" is mostly C URB plumbing behind a Rust API — the Rust value-add is memory safety at the *framing/parsing* layer (KVec slices, checked parsers) and trait polymorphism. Our C module has no such safety, so compensate with explicit length checks (they already do this: caps at 255/511, sle_usb.rs:449-452, 556-558).
- They use `GFP_KERNEL` allocations and `Mutex`/`global_lock` (sle_usb.rs:43-46) in a kernel 7.x tree; semantics we replicate in C are: spinlock-protected URB queues, per-device state table, completion contexts. Nothing in their design depends on Rust-specific kernel APIs we cannot express in C (`usb::Driver` probe/disconnect/suspend/resume map 1:1 to `struct usb_driver` callbacks).
- Their `usb::DeviceId::from_id(0x1234,0x5678)` is proof that Rust-in-tree kernel USB is still driven by the same id tables; our `ffff:3733` table entry is equivalent. Coexistence with the vendor `wireless_usb` driver (issue 06 point 5) is the real bind-management problem, and OpenSparklink gives no answer (it matches by class, which would conflict even harder with a vendor driver that owns ffff:3733).

## Open questions

1. Does the WS73 (or WS63) silicon actually implement the T/XS 10003-2025 USB binding (0xE0/0x01/0x05, EPs 0x91/0x92/0x12) as an alternative to the HCC 5-EP mode? If so, a class-compliant mode could be a much simpler /dev/ws73hci backend than the 92-byte scatter header path — worth probing on real hardware.
2. Their fw opcodes 0xF810/0xF811 and 0xA5 chunk marker — are these used by HiSilicon SLE chips in any mode, or purely OpenSparklink invention? Our boot-mode WRITEM/FILES/QUIT is the WS73-true path; the 0xF8xx DLI "test/vendor" group (sle_dli.rs:673-675) is the only overlap hint.
3. Sync-init `usb_bulk_msg` racing the auto-resubmit bulk-in URB on the same EP (ffi.c:739-777 vs 670-684) — is that a real race they hit, and does WS73 have the same hazard during our init if we follow suit? (We plan to use RW_REG + INT notification instead, so likely avoidable.)
4. The DLI event set OpenSparklink parses (0x0001..0x002E, sle_usb.rs:626-1141) is standard SLE; confirm the WS73 SLE firmware's event codes for open-ack arrive on BSLE_MSG_QUEUE as `bsle_msg_tag` frames rather than DLI events — kernel-init-seq.md:4 already shows that (type=4 DEVICE_ACTION_STATUS u32=2), which is *not* a T/XS 10003 event. This is the biggest semantic gap between the two stacks.
5. Should /dev/ws73hci expose raw DLI frames (their model) or HCC netbufs (hcc_header + bsle_msg_tag, our SDK model)? kernel-init-seq.md suggests the SLE-open ack is only visible as a BSLE_MSG_QUEUE packet, so a pure DLI framing would miss status acks — the char device likely needs to carry the hcc_header/queue_id too (type byte + queue id in our framing, as issue 06 notes).
