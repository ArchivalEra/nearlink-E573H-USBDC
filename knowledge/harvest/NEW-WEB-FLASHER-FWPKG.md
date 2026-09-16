---
type: harvest
title: "StarFish nearlink-web-flasher + nearlink-contrib — browser-side LoaderBoot/YMODEM flashing with fwpkg container format; a young community component collection"
language: en
created: 2026-09-13
tags: [web-serial, flasher, fwpkg, loaderboot, ymodem, crc16, nearlink-contrib, mpu6050, harvest]
sources:
  - "/mnt/hdd/nearlink-stuff/nearlink-web-flasher"
trust: verified
stale_after: 2026-12-13
---

# StarFish nearlink-web-flasher + nearlink-contrib — browser-side LoaderBoot/YMODEM flashing with fwpkg container format; a young community component collection

- Inspection date: 2026-09-13 (staleness check: pushed 2026-08-14 / 2025-12-11 — ALIVE)
- Source roots: `/mnt/hdd/nearlink-stuff/nearlink-web-flasher`, `/mnt/hdd/nearlink-stuff/nearlink-contrib`
- Mode: read-only local program inspection; no build, network, hardware, or PCB access

## Executive findings

1. **`nearlink-web-flasher` (StarFish Community) flashes WS63 and BS2x (BS21/BS21E) entirely from the browser**: a zero-dependency, front-end-only implementation on the native Web Serial API (Chrome/Edge/Opera 89+, secure context). The pipeline: parse the `.fwpkg` package → LoaderBoot handshake (user presses reset) → baudrate switch → YMODEM transfer → reset and run, with progress callbacks at every stage. [`README.md`, `src/flasher.js:34-90`]
2. **The `.fwpkg` container format is documented in code**: magic `0xEFBEADDF` (note: the boot-ROM sync magic from our ws63flash knowledge is `0xEFBEADDE` — same `EFBEAD*` family, one nibble apart, marking the container layer vs the transport layer), a header with CRC16 computed over `[6..12+52*binCount]`, a bin-info table (per-partition entries), and a data section located by per-partition offsets. [`src/fwpkg.js:9-57`]
3. The flashing protocol modules: `protocol.js` (170 lines — LoaderBoot handshake/baudrate negotiation), `ymodem.js` (125 lines — standard YMODEM, CRC mode, 1024-byte data blocks), `crc16.js`, `serial-port.js` (317 lines — read buffering, RTS control, baudrate switching, frame sync). Total 903 lines — the complete vendor flash workflow in readable JS. [src/ line counts]
4. **`nearlink-contrib` is an early community component collection** (, ⭐8): WS63 directory currently holds one component family (`sensors/mpu6050` + samples) with a Step1 (driver integration guide) / Step2 (test case guide) documentation convention; BS21E is marked "planned". The scaffolding and contribution process matter more than the current single component. [nearlink-contrib/ tree, README]

## Boundaries and gaps

- The web flasher targets UART LoaderBoot only — no USB or network flashing path.
- nearlink-contrib's mpu6050 driver was not audited against the pet-collar/vehicle integrations of the same chip.
- StarFish Community is not the format owner (their own notice states HiSilicon owns the protocol) — compatibility-implementation status.

## Reusable for our stack

- The **fwpkg container constants** (magic 0xEFBEADDF, CRC16 coverage window, bin-info stride) extend our firmware-format knowledge (`05-firmware-blob-format.md`, ws63flash) with the browser-side parse reference.
- A zero-dependency Web Serial flasher means our dongle's firmware could be updated from a web page with no installed tooling — a distribution channel our E573H project can adopt (the HHD-01 CH340 driver note matches our hardware).
- The Step1/Step2 integration-guide convention is a lightweight pattern for our own sample contributions.

## Comparison anchors (vs existing reports)

- `NEW-WS63FLASH-GHIDRA.md` / `NEW-XF-BURN.md`: the boot-ROM protocol knowledge those reports established is here re-implemented in JavaScript for the browser — third independent implementation of the same flashing protocol.
- `NEW-NLD-ERPC-PROTOCOL.md`: Nld updates dongle firmware over its `Firmware1` D-Bus/eRPC service; the web flasher uses the UART LoaderBoot path — two firmware-update channels now documented.
- `NEW-NLCHAT-WEB.md`: same Web-Serial-in-browser pattern, applied to flashing instead of chatting.
