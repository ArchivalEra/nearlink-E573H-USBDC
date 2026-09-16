---
type: harvest
title: "OHOS DLI snoop format decoded: 9-byte header + 228-opcode DLI namespace + commercial privacy blacklist design"
language: en
created: 2026-09-15
tags: [harvest, ohos, dli, snoop, opcode, privacy, analyzer]
sources:
  - ""https://github.com/openharmony/communication_nearlink_service (services/hardware/src/SleDliSnoop.cpp, services/stack/src/dli/interface/dli_opcode.h)""
trust: A
stale_after: 2027-03-15
---

# OHOS DLI snoop format deep-dive

## Executive findings

`communication_nearlink_service` continues to pay out: the previously-unreported `services/hardware/src/SleDliSnoop.cpp` (704 lines) is a complete **host↔controller capture layer** — the NearLink equivalent of btmon/hcidump — plus `dli_opcode.h` enumerating 228 DLI commands/events.

**1. Snoop record format (reversible from code).** Each packet: 9-byte header (`AssignSnoopHeader`, SleDliSnoop.cpp:627-643) = 8-byte little-endian millisecond timestamp + 1-byte direction flag (0x01 = received); then payload starting with a type byte (`DliSnoopType`, :96): **0xA1 CMD / 0xA2 EVENT / 0xA3 ACB / 0xA4 ICB**. CMD/EVENT payloads carry a 2-byte LE opcode at offset 10 and (for events) length at offset 12, status/complete events carry the responded opcode at offset 14 (`SNOOP_TYPE_OFFSET..SNOOP_EVT_CMD_OPCODE_OFFSET`, :58-62). ACB/ICB frames use a 4-byte sub-header: `lcid/handle(2) + len(2)` (`DLI_ACB_HEADER_LEN`, :63). Storage: `/data/log/nearlink/nearlink_dli_YYYYmmdd-HHMMSS.log` (:45), **hex-text** encoding (2 chars/byte, space separators at string offsets 16 and 19 — after timestamp and direction flag, :107-109), 200-byte per-record display cap with `...` truncation suffix, per-directory size accounting with oldest-file-first deletion.

**2. The DLI opcode namespace (228 symbols in `dli_opcode.h`).** Command families visible through the snoop blacklist: `0x0405/0x0406` set/get public address, `0x040C/0x040D` access-filter-list add/remove, `0x0C02-0x0C06` advertising parameters/data/scan-response/enable/read-max-len, `0x1401` create-connection, `0x180C` read-remote-RSSI event, `0x1811` ACB-param event, `0x1C01/0x1C03/0x1C05/0x1C28` encryption family (encrypt, enable-encryption, param-request-reply, IMG group-multicast encryption), `0x1812` set-controller-data. This namespace **is** the wire protocol our E573H USB dongle speaks at the DLI layer — directly annotates USB-PROTOCOL.md captures.

**3. Commercial privacy anonymization is opcode-blacklist-based.** Two sets (`kSensitiveCmdOpcodes` 13 entries, `kSensitiveEventOpcodes` 2) — on commercial builds (runtime `IsVendorCommercialVersion()` check) matching opcodes are dropped entirely, keeping only opcode+event id; non-listed commands log fully. The maintenance rule is stated in a comment: **blacklist and `dli_opcode.h` are dual-source — a missed sync is a privacy leak, an extra sync is silent truncation**, enforced by dev+review. Design lesson for any capture tool: privacy filtering at named-opcode granularity, with the cost of dual-source drift explicitly accepted and documented.

**4. Adapter plumbing.** `SleDliLayerAdapter.cpp` (162 lines) bridges DLI callbacks to the snoop/upper layers; `SleDliCallbacks.cpp` + `SleDliThreadUtil.cpp` give single-thread-per-snoop discipline (`DoInSnoopThread` funnels all writes onto one thread — file access stays serialized without locks in the hot path).

## Boundaries

- Log format is OHOS-specific (path, HILOG debug gating, commercial-version detection via sysparam) — reusable concept, not binary-compatible with btsnoop.
- The 228-opcode header is read for family structure; full per-opcode semantics need `dli_cmd_struct.h`/`dli_event_struct.h` (next dive if the USB capture needs them).
- Snoop covers DLI only; SSAP-layer traffic arrives pre-assembled as ACB/ICB user payloads, so SSAP analysis needs the app-layer decoder too.

## Reusable

- Snoop record layout (ts8+dir+type+opcode) — adopt as our dongle capture format so Wireshark-style tooling can be shared conceptually.
- ACB/ICB 4-byte lcid/handle+len sub-header — confirms the channel-mux framing seen in USB-PROTOCOL.md.
- Opcode-granularity privacy blacklist with explicit dual-source warning — the right model if we ever ship capture logs from users' dongles.
- Single-thread snoop funnel (`DoInSnoopThread`) — lock-free-ish capture for high-rate USB traffic.

## Comparison anchors

- vs. USB-PROTOCOL.md: DLI opcode namespace fills the "command semantics" column our raw USB capture lacked; 0x0C0x advertising family matches observed USB command shapes.
- vs. btsnoop (Bluetooth): same concept, text-based log instead of binary; OHOS chose debuggability over tooling compatibility.
- vs. Nld dongle eRPC: Nld is the userspace service protocol, DLI is the controller transport — the two reports now bracket the full host stack of communication_nearlink_service.
