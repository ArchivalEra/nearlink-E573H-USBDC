---
type: harvest
title: "NEW — xf_burn_tools (Python AutoBurn): WS63 UART flash protocol confirmation + fwpkg.py manifest harvest"
language: zh
created: 2026-09-05
tags: [harvest, burn, tools, python]
sources:
  - "https://github.com/geekheart/xf_burn_tools"
trust: B
stale_after: 2027-03-05
---

# NEW — xf_burn_tools (Python AutoBurn): WS63 UART flash protocol confirmation + fwpkg.py manifest harvest

Date: 2026-09-03. Read-only repo-knowledge harvest; no network, no build, no hardware.

Source on the archive disk:

- `https://github.com/geekheart/xf_burn_tools/tree/main/` — git clone of
  `github.com/geekheart/xf_burn_tools` (verified via `.git/config` remote), HEAD
  `2192a99` "docs: 更新说明文档" (2024-10-11). Author "kirto"
  (`sky.kirto@qq.com`, `setup.py:8-9`), package `AutoBurn` v0.3.0, console
  script `burn` (`setup.py:6,22-25`), claimed MIT license (`setup.py:14`).
  639 total Python/README lines: `AutoBurn/autoBurn.py` (38),
  `AutoBurn/ws63flash.py` (233), `AutoBurn/pymodem.py` (113),
  `AutoBurn/fwpkg.py` (81), `AutoBurn/CRC.py` (95). README is Chinese and
  explicitly credits `goodspeed34/ws63flash` as the reference
  (`README.md:52-54`) — i.e. this is an independent **Python re-implementation
  of the same RE'd protocol** that our C teardown covered.

Repo-local cross-references used for comparison:

- `.scratch/nearlink-driver/lab-notes/NEW-WS63FLASH-GHIDRA.md` — the C
  ws63flash teardown (cited below as `C-note §N`).
- `.scratch/nearlink-driver/assets/01-firmware-handshake-spec.md` — our WS73
  USB WRITEM/FILES/QUIT boot spec (cited as `handshake-spec §N`).
- `.scratch/nearlink-driver/assets/HHD01-firmware-violin-1.10.102.fwpkg` —
  our real vendor fwpkg, used for an empirical fwpkg.py cross-check (§5).

All line references below are to the xf_burn_tools tree unless marked otherwise.

---

## 1. Provenance and intent

- The README states the tool was written "通过解析 ws63 的烧录时序" (by parsing
  the WS63 burn timing sequence) with pyserial, replacing an earlier hack that
  piped com0com into the official burn tool (`README.md:3-5`). Automatic
  burning is achieved by wiring the programmer's RTS to the board RESET pin
  (`README.md:7`).
- CLI (`AutoBurn/autoBurn.py:8-34`): `burn [-v] [-p PORT] [-b BAUD] [-s]
  FIRMWARE_FILE`; `-b` defaults to **921600** (`autoBurn.py:11`); `-s` only
  prints the fwpkg partition table and exits (`autoBurn.py:26-28`).
- Dependencies: click + pyserial (`setup.py:18-21`) plus `rich` for logging /
  progress tables (imported `ws63flash.py:9`, `fwpkg.py:3-4`,
  `pymodem.py:5`) — note `rich` is missing from `install_requires`, a small
  packaging bug.

## 2. Protocol confirmation: the wire frames are byte-identical to the C version

### 2.1 Frame transmit (`ws63_send_cmddef`, `ws63flash.py:71-93`)

- Magic `0xdeadbeef` packed `<I` at offset 0 → on the wire `EF BE AD DE`
  (`ws63flash.py:75`) — exactly the C frame (`C-note §1.3`).
- `LEN16` = data length + 10 (the non-data overhead), packed `<H` at offset 4
  (`ws63flash.py:72,77`) — same rule as C `io.h:241,247-248`.
- Byte 6 = cmd, byte 7 = the complement byte (`ws63flash.py:79-80`).
- Trailer = CRC16/XMODEM over the frame minus the last 2 bytes
  (`ws63flash.py:84-85`), table poly 0x1021 (`CRC.py:4-44`) — same as C
  `io.h:253-255` / `ymodem.h:36-81`.
- **Nuance**: the complement byte is computed as `cmd ^ 0xFF`
  (`ws63flash.py:80`) whereas C uses a nibble swap `SWAP_CMD`
  (`C-note §1.3`). The two agree for every command actually used — 0xF0, 0xD2,
  0x87, 0x5A all have hi-nibble = ~lo-nibble (0xF0↔0x0F, 0xD2↔0x2D, 0x87↔0x78)
  — so the observed wire bytes are identical, but the C version's nibble swap
  is the semantically correct decoding; `^0xFF` only *coincides* for this
  command set. If HiSilicon ever uses a command like 0x12, the two diverge.

### 2.2 Frame receive (`uart_read_until_magic`, `ws63flash.py:97-158`)

- Byte-wise FSM: hunt for magic `EF BE AD DE`, then read `LEN16` at offset
  4:5, consume until `framelen-1`, then CRC16-verify (`ws63flash.py:99-156`)
  — structurally the same as C `io.h:147-235`.
- 5 s overall timeout, refreshed on every valid byte (`ws63flash.py:19,107,
  124`); bad CRC only logs a warning and returns -1 (`ws63flash.py:154-156`).
- Unlike C, **boot-log bytes before the magic are silently discarded** (no
  verbose echo), so you lose the loaderboot console output C shows.

### 2.3 Command table (`WS63E_FLASHINFO`, `ws63flash.py:38-57`)

Byte-for-byte the same three commands as C-note §1.4:

| cmd | payload bytes in source | meaning | C equivalent |
|-----|--------------------------|---------|--------------|
| 0xF0 (`ws63flash.py:39-43`) | baud u32 LE + `08 01 00 00` = 0x0108 "MAGC" | handshake | `ws63defs.h:59-64` |
| 0xD2 (`ws63flash.py:44-51`) | addr u32, ilen u32, eras u32 (default 0xFFFFFFFF), const `00 FF` (14 B) | DOWNLOADI / erase | `ws63defs.h:71-78` |
| 0x87 (`ws63flash.py:52-57`) | `00 00` u16 | reset | `ws63defs.h:79-83` |

The 0xF0 handshake data defaults to `[0x00,0xc2,0x01,0x00,...]` = 0x0001C200 =
**115200** baud patched in at runtime with the user baud
(`ws63flash.py:183-184`) — same in-band baud negotiation as C
(`ws63flash.c:294-295`). The handshake **ACK** is matched by grepping for the
8 bytes `EF BE AD DE 0C 00 E1 1E` (`ws63flash.py:189-190`) — i.e. a 12-byte
frame, cmd 0xE1, ~cmd 0x1E. C matches a *10-byte* prefix that additionally
includes the payload `5A 00` (`C-note §1.4`), so the Python check is slightly
**less strict** (does not verify the 0x5A payload nor the ack's CRC).

### 2.4 Full flashing sequence (`Ws63BurnTools.flash`, `ws63flash.py:160-226`)

1. Open TTY at **115200**, `setRTS(False)` — the RTS edge on open is the
   automatic-reset trick promised by the README (`ws63flash.py:161-162`,
   `README.md:7`).
2. Pick the `type == 0` entry (loaderboot) from the fwpkg; abort if absent
   (`ws63flash.py:163-171`) — same loaderBoot-first staging as C.
3. Poll 0xF0 for up to 10 s (`RESET_TIMEOUT`, `ws63flash.py:17,177-193`)
   while the board resets; on 0xE1 ack, immediately switch the serial port to
   the negotiated baud (`ws63flash.py:191`) — note the switch happens *before*
   YModem, matching C's in-handshake baud change.
4. YModem-send loaderboot (`ws63flash.py:195-200`), then drain the response
   with `uart_read_until_magic` (`ws63flash.py:202`).
5. For each `type == 1` image: compute `eras_size = ceil(len/8192)*0x2000`
   (`ws63flash.py:209` — the identical 8 KiB sector round-up as C
   `ws63flash.c:374`), fill the 0xD2 payload with burn_addr / length /
   eras_size (`ws63flash.py:210-215`), send, drain, YModem the payload, then
   `time.sleep(0.1)` (`ws63flash.py:216-222`) — the same 100 ms settle C
   documents ("won't respond if cmd followed immediately by ymodem").
6. Send 0x87 reset once and drain (`ws63flash.py:223-225`). C additionally
   *polls* until "Reset" appears in the boot log (`C-note §1.5` step 5); the
   Python version is fire-and-forget here.

**Not implemented vs C**: no 0x5A late-baud (`--late-baud`), no standalone
`--write`/`--erase` verbs, no `--write-program` signing path, no "Reset"
string poll, no built-in loaderboot blob (it must come from the fwpkg's own
type-0 entry). It is strictly a `--flash`-verb-only clone.

## 3. pymodem.py — the YModem sender (`pymodem.py:1-113`)

- Standard YModem-CRC: wait `'C'` with 5 s timeout (`pymodem.py:7,51-59`),
  SOH 128-byte block 0 carrying filename + size as an ASCII **hex string**
  `hex(file_size)` e.g. `"0x7740"` (`pymodem.py:63-75`) — matches C's
  `snprintf "0x%zx"` (`C-note §1.6`); then **STX 1024-byte** data blocks with
  big-endian CRC16 at the tail (`pymodem.py:83-97`); EOT with retry-until-ACK
  (`pymodem.py:98-101`); closing empty block 0 (`pymodem.py:103-112`).
- Retransmit: `ymodem_blk_timed_xmit` rewrites the whole block every 1.5 s
  (`YMODEM_ACK_TIMEOUT`, `pymodem.py:8,34-41`) for up to 30 s
  (`YMODEM_XMIT_TIMEOUT`, `pymodem.py:9`) — same whole-block retry semantics
  as C (`C-note §1.6`).
- File data is read **out of the .fwpkg itself** at the entry's `offset`
  (`pymodem.py:80-89`: `f.seek(offset)` where the "file_path" argument is the
  fwpkg name, see `ws63flash.py:197,218`) — a neat trick: one file handle
  serves both manifest and payloads.
- **Quirk**: the last block is zero-padded to 1024 bytes and CRC'd over the
  full padded range (`pymodem.py:84-92`: `blkbuf` is preallocated 1029 zeros,
  only `rlen` bytes overwritten, CRC over `blkbuf[3:1027]`). Standard YModem
  pads with 0x1A; the boot ROM evidently accepts zero fill (C does the same
  with its fixed 1024-byte buffer).
- Block counter is `i_blk % 0x100` (`pymodem.py:87`) — 8-bit wrap, standard.

## 4. CRC.py (`CRC.py:1-95`)

- `calc_crc16` — CRC-16/XMODEM (CCITT, poly 0x1021, init 0, no reflect) via a
  256-entry table (`CRC.py:4-44`). This is the single primitive used for
  wire frames, YModem blocks, and fwpkg files alike. Identical to the C
  `ymodem.h` table (`C-note §1.3/§1.6`).
- Also ships an unused-by-this-tool `calc_crc32` (CRC-32/IEEE 802.3 table,
  `CRC.py:55-95`) — same family as the HCC-layer CRC32 we may need later.

## 5. fwpkg.py — manifest format, and the HHD-01 empirical cross-check

### 5.1 Parsed layout (`fwpkg.py:12-52`)

- 12-byte header `mgc u32, crc u16, cnt u16, length u32`, all `<` little-endian
  (`fwpkg.py:19-20`); magic must be **0xEFBEADDF** (`fwpkg.py:23`) — the
  on-disk bytes are `DF AD BE EF`, the *other* Hisilicon magic (wire frames
  use 0xdeadbeef; same file-vs-wire distinction as `C-note §1.7`).
- `cnt ≤ 16` = `MAX_PARTITION_CNT` (`fwpkg.py:10,27-28`); then `cnt` × 52-byte
  bin_info: `name[32]` + `<5I` = offset, length, burn_addr, burn_size, type
  (`fwpkg.py:32-45`) — identical struct to C `fwpkg.h:51-65`.
- CRC16/XMODEM over header bytes **6..end of bin_info[]**
  (`fwpkg.py:46-51`: `CRC.calc_crc16(buf[6:])` over `12 + cnt*52` bytes) —
  same coverage as C (`C-note §1.7`).
- `type` semantics in use: 0 = loaderboot (flashed first, `ws63flash.py:166`),
  1 = flashable image (`ws63flash.py:206`), anything else silently skipped.

### 5.2 Empirical check against `assets/HHD01-firmware-violin-1.10.102.fwpkg`

I replicated fwpkg.py's parse + CRC in a throwaway pure-python script (same
struct layout, same table CRC-16/XMODEM over bytes 6..0x1AC). Results —
**fwpkg.py agrees completely**:

- Header: magic `0xefbeaddf`, crc `0x01fb`, **cnt=8**, length `0x153b4c`;
  computed CRC over `buf[6:]` = `0x01fb` — **match** (validates both the CRC
  coverage rule and the table).
- All 8 partitions decode to sane, ordered data (name / offset / length /
  burn_addr / burn_size / type):

| # | name | off | len | burn | bsz | type |
|---|------|-----|-----|------|-----|------|
| 0 | root_loaderboot_sign.bin | 0x1ac | 0x7580 | 0x000000 | 0x200000 | 0 |
| 1 | root_params_sign.bin | 0x773c | 0x780 | 0x200000 | 0x780 | 1 |
| 2 | ssb_sign.bin | 0x7ecc | 0x5c00 | 0x202000 | 0x5c00 | 1 |
| 3 | flashboot_sign.bin | 0xdadc | 0xba00 | 0x20a000 | 0xba00 | 1 |
| 4 | flashboot_backup_sign.bin | 0x194ec | 0xba00 | 0x21b000 | 0xba00 | 1 |
| 5 | ws63_all_nv.bin | 0x24efc | 0x1000 | 0x22c000 | 0x4000 | 1 |
| 6 | ws63-liteos-app-sign.bin | 0x25f0c | 0x12dbc0 | 0x235000 | 0x12dbc0 | 1 |
| 7 | efuse_cfg.bin | 0x153adc | 0x60 | 0x000000 | 0x200000 | **3** |

- This confirms and extends `C-note §1.7` (which verified magic/cnt/first
  entry): the CRC also checks out and we now have the **full partition map**
  of the HHD-01 violin firmware — loaderboot@0, params@0x200000,
  ssb@0x202000, flashboot(+backup)@0x20a000/0x21b000, NV@0x22c000,
  liteos-app@0x235000. It also exposes the **type=3** entry (`efuse_cfg.bin`,
  burn 0) which *both* the Python and C flashers ignore (neither handles
  type 3) — so a naive `burn` of this package would skip efuse config.
- Note burn_size ≠ length for entries 0 and 5-6 (reserved padding regions);
  eras computation uses `length`, not `burn_size` (`ws63flash.py:209`).

## 6. Python vs C ws63flash — which is more useful for our tooling

- **Readability**: the Python version is dramatically more readable — the
  entire protocol is ~230 lines with named constants (`ws63flash.py:21-57`)
  vs C's macro-heavy `io.h`/`ws63defs.h`. For onboarding someone to the WS63
  boot-ROM protocol, reading `ws63flash.py` end-to-end is the fastest path;
  the C version remains the authority for the extra verbs and the verbose
  boot-log decode.
- **Protocol equivalence**: frames, commands, ack magic, YModem dialect,
  erase math, and the 100 ms settle are all confirmed identical. Two
  independent implementations (C from goodspeed34, Python from geekheart,
  which credits the former `README.md:52-54`) agreeing on every wire byte is
  strong triangulation that `C-note §1.3-§1.6` describes the real WS63E boot
  ROM protocol.
- **Completeness**: C wins (erase/write verbs, late-baud, signing, reset
  poll, boot-log visibility). Python wins for quick forks (e.g. a WS63E
  HHD-01 serial flasher with our own logging, or adding the type-3 efuse
  entry handling).
- **License caveat**: setup.py claims MIT (`setup.py:14`) but the tool is a
  direct port of GPL-3 ws63flash concepts and credits it; treat as
  GPL-contaminated for copying purposes (fine for lab use, not for vendoring
  into a proprietary tree).

## 7. Relation to our WS73 USB WRITEM/FILES/QUIT handshake

- No new information about our USB transport: xf_burn_tools is UART-only
  (`serial.Serial`, `ws63flash.py:161`) and never touches USB. It neither
  contradicts nor extends `handshake-spec §1-§4`; the protocols remain
  distinct (binary framed UART boot ROM vs ASCII bulk USB boot).
- The family resemblance noted in `C-note §1.9` holds unchanged: loader-based
  staging, in-band speed negotiation on UART (absent on USB,
  `handshake-spec §9`), reset/re-enumeration as the success path.
- Tangible new asset from this harvest: the **HHD-01 flash partition map**
  (§5.2) — previously we only knew loaderboot@0 + app@0x230000-ish from the
  ws63flash README (`C-note §1.5`); we now have the exact violin-1.10.102
  layout including the backup flashboot slot and NV placement, useful if we
  ever build fwpkg-aware flashing for the HHD-01's CH340 serial port.

---

## 8-line summary

1. xf_burn_tools (github.com/geekheart/xf_burn_tools, HEAD 2192a99, MIT-claimed, author "kirto") is a ~560-line Python re-implementation of ws63flash's WS63 UART boot-ROM protocol; its README credits goodspeed34/ws63flash as the reference.
2. Protocol fully confirmed identical to the C version: frames `EF BE AD DE + LEN16(+10) + cmd + ~cmd + data + CRC16/XMODEM`; commands 0xF0 handshake (baud+magic 0x0108) → YModem loaderboot (type-0 entry) → 0xD2 DOWNLOADI(addr,len,ceil(len/8192)*0x2000) per type-1 image with 100 ms settle → 0x87 reset; same 10-rate baud table, in-band baud switch, and 0xE1 ack magic.
3. Two deliberate deviations, both benign: complement byte computed as `cmd^0xFF` instead of C's nibble swap (coincides for all commands used since hi-nibble=~lo-nibble), and the 0xE1 ack match is 8 bytes without the 0x5A payload (less strict than C's 10).
4. pymodem.py is a textbook YModem-CRC sender: 'C' wait (5 s), SOH block 0 with filename+hex size, STX 1024-byte blocks with big-endian CRC16, EOT retry, empty closing block 0; data is read straight out of the .fwpkg at each entry's offset; last block zero-padded to 1024.
5. Missing vs C: no 0x5A late-baud, no --write/--erase/--write-program verbs, no "Reset"-string poll, boot log discarded — it is a --flash-only clone, but far more readable as protocol documentation.
6. fwpkg.py parses header `<IHHI` (magic 0xEFBEADDF on disk as DF AD BE EF), 52-byte `{name[32], offset, length, burn_addr, burn_size, type}` entries, CRC16/XMODEM over bytes 6..12+52*cnt — empirically validated against our `assets/HHD01-firmware-violin-1.10.102.fwpkg`: CRC 0x01fb matches, cnt=8, full map extracted (loaderboot@0, params@0x200000, ssb@0x202000, flashboot@0x20a000 + backup@0x21b000, NV@0x22c000, liteos-app@0x235000, and an ignored type=3 efuse_cfg entry).
7. For our tooling: use this Python tree as the quick-reference implementation and partition-map extractor; keep C ws63flash as the complete flasher; the WS73 USB WRITEM/FILES/QUIT spec is unaffected (UART-only tool), but we now hold the exact HHD-01 violin firmware flash layout for any future serial-path flashing.
8. Note path: `.scratch/nearlink-driver/lab-notes/NEW-XF-BURN.md` (this file).
