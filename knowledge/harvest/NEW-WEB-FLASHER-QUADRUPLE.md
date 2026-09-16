---
type: harvest
title: "nearlink-web-flasher: the browser flasher completes the quadruple implementation of the WS63 boot-ROM protocol"
language: en
created: 2026-09-15
tags: [harvest, web-serial, flasher, protocol, fwpkg, synthesis]
sources:
  - "/mnt/hdd/nearlink-stuff/nearlink-web-flasher"
trust: A
stale_after: 2027-03-15
---

# nearlink-web-flasher + protocol quadruple synthesis

## Executive findings

**1. StarFish's `nearlink-web-flasher` (updated 2026-08-14) is a zero-dependency JS library flashing `.fwpkg` in Chrome/Edge/Opera via the Web Serial API.** 903 lines across seven modules: `serial-port.js` (Web Serial open/read/write, default 115200 8N1), `protocol.js` (frame layer), `fwpkg.js` (package parser: `FWPKG_MAGIC = 0xefbeaddf` little-endian at offset 0, header `{magic, crc, binCount, length}`, partition table with CRC validation — the on-disk DF-AD-BE-EF magic, distinct from the wire 0xdeadbeef), `flasher.js` (orchestration: parse → LoaderBoot handshake → `reqBaudrate(baudRate)` switch → YMODEM per partition → reset), `ymodem.js`, `crc16.js`, `index.js`. Explicit provenance notice: the fwpkg format and LoaderBoot protocol belong to HiSilicon; this is a compatible implementation maintained by StarFish.

**2. Synthesis: the WS63 UART boot-ROM protocol now has FOUR independent implementations, and they agree.** (a) C `ws63flash` (goodspeed34, the reference with the most verbs), (b) Python `xf_burn_tools` (geekheart, protocol-identical clone, NEW-XF-BURN), (c) Rust `ws63flash` v4 (xiaohong-ai, NEW-GITCODE-SCAN-0915B: WS63/BS21E/ESP32, GUI, signing), (d) this JS in-browser port (StarFish, no install at all). All four implement: magic `EF BE AD DE` + LEN16(+10) + cmd + complement + CRC16/XMODEM; commands 0xF0 handshake (in-band baud negotiation) → YModem loaderboot (fwpkg type-0 entry) → 0xD2 DOWNLOADI (8KiB-sector erase round-up) → 0x87 reset; ack magic 0xE1. Four independent teams converging on byte-identical wire behavior is the strongest possible validation of the protocol documentation in our fwpkg/boot notes. The vendor tool itself (HiSilicon AutoBurn) remains the fifth, closed, member of the family.

**3. Browser-side specifics worth noting.** Web Serial gives no RTS pin control in the same way native serial does per OS (auto-reset tricks differ); the library compensates with the pure protocol path (handshake via 0xF0 polling, as in our HHD-01 bring-up notes). Baud switch happens mid-session exactly like the C/Python/Rust trio. `test/` directory exists (unit-level checks on parsers).

## Boundaries

- Read at source level; not executed in a browser (needs USB-permission grants in a real Chrome session).
- BS2x support claimed in README (shared LoaderBoot protocol); code paths checked only for the shared layer.
- No PCB/hardware dirs involved.

## Reusable

- The quadruple-implementation agreement table (per-command, per-magic) — cite as final authority in our fwpkg/boot-protocol docs; any future decoder can diff against any of the four.
- Web Serial port wrapper pattern (`serial-port.js`) — directly reusable if we ever ship a browser-based dongle tool (our E573H USB CDC testing UI).
- `reqBaudrate` mid-session switch — the standard handshake ordering to preserve.
- StarFish's HiSilicon-format provenance notice — the correct attribution phrasing for compatible implementations.

## Comparison anchors

- vs. NEW-XF-BURN / NEW-GITCODE-SCAN-0915B: this report closes the flasher-implementation family (C → Python → Rust → JS); each report's quirks table now cross-checks against three more witnesses.
- vs. hinearlink/ws63flash-win (batch 15): the Windows-native shell targets the same protocol — five front-ends, one wire truth.
- vs. our USB WRITEM/FILES/QUIT handshake: distinct transport (USB bulk ASCII vs UART binary), same reset/re-enumeration success philosophy; the web flasher is for the UART family only.
