---
type: harvest
title: "DLI command/event struct tables decoded: adv PHY/frame-format params, conn timing units, PHY feedback codes, event field maps"
language: en
created: 2026-09-15
tags: [harvest, dli, structs, adv, connection, phy, ohos]
sources:
  - ""/mnt/hdd/nearlink-stuff/communication_nearlink_service/services/stack/src/dli/interface/dli_cmd_struct.h dli_event_struct.h""
trust: A
stale_after: 2027-03-15
---

# DLI struct tables deep-dive

## Executive findings

The 1532-line struct pair (`dli_cmd_struct.h` 632, `dli_event_struct.h` 597) is the parameter dictionary for the 228 DLI opcodes — with the units and ranges documented inline, this is the file that makes raw dongle captures decodable.

**1. Advertising params go far beyond BLE shape.** `DLI_AdvPhyParam`: primary frame format (radio frame type 1 vs 4 with m-sequence 0), primary PHY, secondary frame format, secondary PHY, **secondary pilot density** and **secondary MCS**, and `secondAdvMaxSkip` (0 = prefer data broadcast; 1-255 = max events skippable before sending a data broadcast). `DLI_AdvExtParam` adds filter policy, TX power [-127,20]/0x7F sentinel (same as discovery header), and adv SID grouping. `DLI_AdvEnable`: duration N×10ms (0=unlimited), maxAdvEvents cap. Scan: `DLI_ScanEnable {enable, filterDuplicates}`, `DLI_AdvScanParam` reports scan-req notification enable + per-cycle scan_req count/time caps.

**2. Connection timing has SLE-specific extra fields.** `DLI_ConnParam` (G role): interval [0x001E,0x3E80] × **0.25ms** = [7.5ms, 4s], maxLatency [0,0x1F3], supervisionTimeout [0x000A,0x0C80] × 10ms = [100ms,32s], CE lengths × 0.125ms. `DLI_ConnectionUpdateParam` adds SLE-only knobs: `txRxInterval` (intra-event), `eventInterval` (inter-event), `systemTimeUnit` (system scheduling slot), and **`txRxFlag` (initiator sends first or later in the event)** — T-side ordering control absent from BLE. `DLI_ConnectionCreateParam` includes GT-role negotiation indication and initiatingPhys at creation time.

**3. PHY control exposes HARQ/multicast feedback codes.** `DLI_SetPhyParam`: per-direction frame format, PHY, pilot density, plus `gFeedback` (0-63: 0=CBG-based, 1-25=multicast feedback bit positions without data, 26=TB-based, 27-34=with-data bit positions, 35-63 reserved) and `tFeedback` (0-7: 0-5=semi-reliable multicast with m-sequence index, 6=CBG, 7=TB). This is direct evidence of the HARQ/m-sequence feedback machinery at the DLI boundary — matches the m-sequence mention in AdvPhyParam's frame formats.

**4. Event field maps for decoders.** `DLI_AdvReportEvt.eventType` is a bitfield: bit0 connectable adv, bit1 scannable, bit2 direct, bit3 scan-response. `DLI_ConnectionCompleteEvt` carries the originating advHandle (0 when not via local advertising). `DLI_DisconnectEvt {status 0=ok/1=fail, connHandle, reason}`. `DLI_CommandComplete`/`DLI_EncryptChangeEvt`/`DLI_ConnectionUpdateCmpEvt` provide the standard completion/ack shapes. `DLI_ControllerData {connHandle, opcode, len, data[0]}` = the generic controller-data envelope (opcode 0x1812's payload shape).

**5. Encryption parameter shape.** `DLI_EnableEncryptParam {connHandle, linkKey[SLE_SM_LINK_KEY_LEN], cryptoAlgo, keyDerivAlgo, integrChkInd}` — SM link key + algorithm selectors, confirming SM-key-based link encryption (the 0x1C01/0x1C03 family payload).

## Boundaries

- Structs read for the adv/connection/PHY/encryption families; remaining families (read-remote-RSSI, white-list, RNG, REM...) not itemized.
- radio_frame_type_t / sle_adv_mcs enumerations referenced but defined elsewhere (dli_def.h or SDK headers) — cross-referencing is follow-up work.
- These are OH-side definitions; byte order and padding on the actual dongle wire need capture validation before treating them as binary layouts.

## Reusable

- Parameter dictionary for annotating DLI captures (adv PHY/pilot/MCS, conn timing units 0.25ms/10ms/0.125ms, txRxFlag).
- eventType bitfield + advHandle linkage for the dongle scanner decoder in our tools.
- gFeedback/tFeedback code tables — the SLE HARQ feedback vocabulary, otherwise undocumented in public code.
- Generic `DLI_ControllerData` TLV envelope — template for vendor-extended controller commands.

## Comparison anchors

- vs. USB-PROTOCOL.md: gives semantic field names + units to the command bytes we recorded; the capture decoder can now print parameters, not just hex.
- vs. Nld eRPC report (sync 89): eRPC is the session layer; DLI structs are the controller layer beneath — together they form a complete two-tier dictionary.
- vs. BLE HCI (general): SLE adv gains pilot/MCS/skip dimensions, conn gains txRxInterval/eventInterval/systemTimeUnit/txRxFlag — quantifies the "SLE is not BLE" claim at parameter granularity.
