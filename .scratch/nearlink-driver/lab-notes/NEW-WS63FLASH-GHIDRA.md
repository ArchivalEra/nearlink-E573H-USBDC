# NEW — ws63flash (UART flasher) + Ghidra_RISCV31 (Huawei custom RISC-V module) harvest

Date: 2026-09-03. Read-only repo-knowledge harvest; no network, no build, no hardware.
Sources on the archive disk:

- `/mnt/hdd/nearlink-stuff/ws63flash/` — git clone of `github.com/goodspeed34/ws63flash`
  (verified via `.git/config` remote), HEAD `5bab2e7`. GPL-3+, authors Gong Zhile
  (Hebei Univ. of Science and Tech.) and William Goodspeed (ChangeLog entries
  2024-12..2025-08). Self-described as "developed from the reverse engineering of
  Hisilicon Official BurnTool" (README.en:5-6, README:5).
- `/mnt/hdd/nearlink-stuff/Ghidra_RISCV31/` — git clone of
  `github.com/NWMonster/Ghidra_RISCV31`, HEAD `68eabf8` ("Improve the translation of
  'stmia&ldmia' instructions"). A Ghidra processor module for the HiSilicon custom
  RISC-V core used in WS63/Hi3863V100 and Hi2821 (README.md:5).

Repo-local cross-references used for comparison:

- `.scratch/nearlink-driver/assets/01-firmware-handshake-spec.md` (our WS73 USB boot
  WRITEM/FILES/QUIT spec — cited below as `handshake-spec §N`).
- `scripts/flash-dongle.sh` (our libusb ws73-probe wrapper).
- `.scratch/nearlink-driver/assets/HHD01-firmware-violin-1.10.102.fwpkg` (real fwpkg,
  used below for an empirical cross-check of the parser).

---

## Part 1 — ws63flash: full flash-protocol teardown

### 1.1 What it is

A small POSIX C utility (autotools + gnulib) that flashes HiSilicon WS63-family boards
over **UART** (`--flash`, `--write`, `--erase`, `--write-program` verbs,
`src/ws63flash.c:56-60`). It speaks the **boot-ROM serial protocol** of the WS63
("WS63E" in the code's own naming, `src/ws63defs.h:58`), reverse-engineered from the
official BurnTool / `libburn.dll` (README:62-63 points at HiSpark Studio's libburn.dll
as the key artifact). Companion tools in the same tree: `ws63fwpkg` (build/inject
fwpkg archives, `src/ws63fwpkg.c:55-57`) and `ws63sign` (machine-code "signing",
`src/ws63sign.c:34-36`).

### 1.2 Transport: plain UART, not USB

- Device opened as a termios TTY at **115200 8N1, no flow control** by default
  (`src/ws63flash.c:872-874`, `src/io.h:60-145`: CS8, no PARENB, no CSTOPB, no
  CRTSCTS, VMIN=0/VTIME=1).
- Supported baud rates: 115200 up to 2000000 from a static table
  (`src/baud.h:33-64`); macOS non-standard rates via `IOSSIOSPEED` ioctl
  (`src/io.h:122-137`).
- Baud can be bumped two ways: in the initial handshake itself (the 0xF0 command
  carries the requested baud, `src/ws63flash.c:294-295`) or *after* loaderBoot is
  running via a 0x5A `CMD_SETBAUDR` command (`--late-baud`, works on Hi3863;
  `src/ws63flash.c:74-75, 345-361`).

### 1.3 Binary frame format (the boot-ROM command layer)

Documented verbatim in the header comment `src/ws63defs.h:36-42` and implemented in
`ws63_send_cmddef()` (`src/io.h:237-274`):

```
+---------+--------+---------+---------------------+----------+
| EF BE AD DE ... see note | LEN16 (LE) | CMD | ~CMD | DATA... | CRC16 (LE) |
```

- Start-of-frame magic written as `htole32(0xdeadbeef)` → on the wire the first four
  bytes are `EF BE AD DE` (`src/io.h:245-246`).
- `LEN16` = total frame length including the 10 non-data bytes
  (`src/io.h:241,247-248`).
- `CMD` is one byte; the next byte is the **low/high nibble swap** of CMD:
  `#define SWAP_CMD(x) (((x) << 4) | ((x) >> 4))` (`src/io.h:50`, used
  `src/io.h:250-251`). So 0xF0 is followed by 0x0F, 0xD2 by 0x2D — a cheap checksum
  byte.
- Trailer is **CRC16/XMODEM** over the whole frame minus the 2 CRC bytes
  (`src/io.h:253-255`), table-driven poly 0x1021 (`src/ymodem.h:36-81`).
- All multi-byte fields little-endian (`src/ws63defs.h:41`).

Receive side: `uart_read_until_magic()` (`src/io.h:147-235`) byte-slurps until the
`EF BE AD DE` magic, then reads `LEN16` at offset 4:5 and consumes the frame,
validating CRC16 (`src/io.h:205-226`); non-magic bytes are echoed as boot-ROM log
output when verbose (`src/io.h:194-201`).

### 1.4 Command catalog (`src/ws63defs.h:58-84`)

| cmd   | name            | payload (LE)                                        | use |
|-------|-----------------|-----------------------------------------------------|-----|
| 0xF0  | CMD_HANDSHAKE   | u32 baud (115200) + u32 "MAGC" 0x0108 (8 B)          | enter YModem/boot download mode (`ws63defs.h:59-64`) |
| 0x5A  | CMD_SETBAUDR    | u32 baud + u32 line-params (8 B)                     | late baud switch after loaderBoot (`ws63defs.h:65-70`) |
| 0xD2  | CMD_DOWNLOADI   | u32 addr, u32 ilen, u32 eras, u16 const 0x00FF (14B) | "download init" — doubles as **erase** command (`ws63defs.h:71-78`) |
| 0x87  | CMD_RST         | u16 0x0000                                           | reset; device answers containing ASCII "Reset" (`ws63defs.h:79-83`, `ws63flash.c:210-211`) |

The device ACK is itself a frame; the tool just greps for the 10-byte sequence
`"\xEF\xBE\xAD\xDE\x0C\x00\xE1\x1E\x5A\x00"` — i.e. a 12-byte frame, cmd **0xE1**
payload 0x5A (`src/ws63flash.c:314-318`, also 502-506, 636-640, 758-762). 0xE1 is the
"handshake accepted / enter YModem" ACK.

### 1.5 The full flashing sequence (verb_flash / verb_write / verb_erase)

All verbs share the same skeleton (`src/ws63flash.c:283-407`, 471-586, 605-672):

1. **Boot-ROM handshake loop**: repeatedly send 0xF0 (with baud patched into the
   payload if != 115200) for up to 10 s while the board is (manually or
   automatically) reset; wait for the 0xE1 ACK magic
   (`src/ws63flash.c:285-325`). Timeout constant `RESET_TIMEOUT 10.0`
   (`src/ws63flash.c:165`).
2. **YModem-send loaderBoot** into RAM: the tool ships a compiled-in signed
   loaderboot blob (`src/blob/ws63_loaderboot_signed.h`, included at
   `src/ws63flash.c:44`; file name sent as `"root_loaderboot_sign.bin"`,
   `src/ws63flash.c:653-656`). The boot ROM executes it.
3. Drain boot log until the next magic frame (`uart_read_until_magic`,
   `src/ws63flash.c:341`); optional late-baud switch (§1.2).
4. For each image: send `CMD_DOWNLOADI` (0xD2) with `burn_addr`, `length`, and
   `eras_size` (`src/ws63flash.c:371-380`), then YModem-transfer the payload, then
   **sleep 100 ms** — the comment says the MCU "won't respond if cmd followed
   immediately by ymodem" (`src/ws63flash.c:396-400`).
   - Erase size is rounded up per 8 KiB: `ceil(len/8192.0)*0x2000`
     (`src/ws63flash.c:374,554`) — 0x2000 = 8192, so eras = length rounded up to the
     8 KiB flash sector.
   - `--erase` sends 0xD2 with an all-zero payload (addr=0,len=0,eras=0xFFFFFFFF
     preserved from the table default) to bulk-erase
     (`src/ws63flash.c:662-666` vs `ws63defs.h:71-78`).
5. **Reset poll**: spam 0x87 until the log contains "Reset"/"reset"
   (`src/ws63flash.c:186-216, 403-405`).
6. TTY restored to 115200 on exit (`src/ws63flash.c:895-896`).

`--write-program` additionally *signs* a raw machine-code image with ws63sign and
burns it at fixed address **0x230000** (`src/ws63flash.c:813`); the README shows the
conventional WS63 flash map: loaderboot + `ws63-liteos-app-sign.bin@0x230000`,
`flashboot_sign.bin@0x220000` (README.en:44-46).

### 1.6 YModem details (`src/ymodem.h`)

- Standard YModem with CRC: wait `'C'` (5 s timeout, `ymodem.h:83,177-198`),
  SOH block 0 (128 B) carrying filename + ASCII-hex size (`snprintf ... "0x%zx"`,
  `ymodem.h:210-215`), then **STX 1024-byte data blocks** with CRC16/XMODEM
  (`ymodem.h:226-256`), EOT with retry (`ymodem.h:258-273`), closing empty block 0
  (`ymodem.h:277-283`).
- Retransmit on NAK or 1.5 s ACK timeout, whole-block retransmit loop
  (`ymodem.h:95-119, 142-164`).

### 1.7 fwpkg container format (`src/fwpkg.h`)

- Header: `mgc(4) crc16(2) cnt(2) len(4)`; then `cnt` × 52-byte
  `fwpkg_bin_info { char name[32]; u32 offset, length, burn_addr, burn_size,
  type_2; }` (`fwpkg.h:51-65`); then concatenated payloads. MAX_PARTITION_CNT 16
  (`fwpkg.h:49`).
- CRC16/XMODEM over `cnt..end of bin_info[]` (`fwpkg.h:114-119`).
- `type_2 == 0` marks the loaderboot entry (must be present for flashing),
  `type_2 == 1` marks flashable images (`fwpkg.h` field used at
  `ws63flash.c:236-238, 364-368`).
- **Empirical cross-check against our own firmware**: hexdump of
  `assets/HHD01-firmware-violin-1.10.102.fwpkg` starts `df ad be ef fb 01 08 00
  4c 3b 15 00 72 6f 6f 74 ...` — i.e. magic bytes `DF AD BE EF` (LE u32
  `0xEFBEADDF`, matching the check at `fwpkg.h:96`), crc=0x01FB, **cnt=8**,
  total len=0x153B4C, and the first bin_info is `root_loaderboot_sign.bin`,
  offset 0x1AC, length 0x7580, burn_addr 0, type 0 — exactly what the parser
  expects. Note the header comment says magic "0xdeadbeef" (`fwpkg.h:52`) but the
  real on-disk value is `0xEFBEADDF`; the *code* is right and the comment wrong.
  Also note 0x0C + 8×52 = 0x1AC confirms the 52-byte bin_info size.
- Implied **wire vs file distinction**: wire frames start `EF BE AD DE`
  (`io.h:246`), fwpkg files start `DF AD BE EF` — two different Hisilicon magics,
  easily confused.

### 1.8 ws63sign: what "signing" really is here (`src/ws63sign.h`)

- Builds a 0x300-byte header blob placed in front of the code: at offset 0x0 a
  root-public-key descriptor (image_id `0x4b0f2d1e`, struct_ver 0x00010000, key_alg
  **Brainpool256/ECC**, `ws63sign.h:29-32, 38-48, 81-94`), at offset 0x100 a
  code-info descriptor (image_id `0x4b0f2d2d`, signature_len 0x40, SHA-256
  `code_hash[32]` over the 16-byte-aligned code, no-encryption flag `0x3c7896e1`,
  `ws63sign.h:34-35, 52-67, 96-127`).
- No actual ECDSA signature is computed — only headers + SHA-256; the 0x40-byte
  signature area is left as written by `ws63sign_init` (zeros),
  `ws63sign.h:100-103` + `ws63sign.c:94-115`. So this validates loaderboot's
  *header/hash* expectations, not a secure-boot chain.

### 1.9 Comparison with our WS73 WRITEM/FILES/QUIT USB boot handshake

Our spec (`assets/01-firmware-handshake-spec.md`) describes the **WS73 dongle boot
over USB bulk** with ASCII commands; ws63flash describes the **WS63 boot ROM over
UART**. They are different protocols for different chips/transport, but with clear
family resemblance:

| aspect | WS63 / ws63flash (UART) | WS73 / our driver (USB boot) |
|---|---|---|
| transport | 115200..2M baud 8N1 UART (`baud.h:33-64`) | USB bulk, 2-EP boot config (`handshake-spec §2`) |
| framing | binary: magic EF BE AD DE + LEN16 + cmd + ~cmd + CRC16-XMODEM (`io.h:245-255`) | ASCII tokens terminated by trailing space 0x20 (`handshake-spec §3.1`) |
| first command | 0xF0 handshake w/ baud+magic 0x0108 (`ws63defs.h:59-64`) | `WRITEM 4 0x40019408 0x<trim>` CMU XO trim (`handshake-spec §4.1`) |
| data move | YModem blocks after 0xD2 DOWNLOADI (`ws63flash.c:371-394`) | `FILES 1 0x<addr> 0x<len> 0x<state>` + READY + raw chunk + FILES OK (`handshake-spec §4.3`) |
| integrity | CRC16 per frame + CRC16 per YModem block + signed loaderboot headers (SHA-256) | host-side SHA-256 64-byte ASCII header per file, nothing on the wire (`handshake-spec §6`) |
| loader staging | boot ROM ← loaderBoot via YModem, loaderBoot flashes rest (`ws63flash.c:327-341`) | none needed; ws73.bin etc. loaded straight to RAM 0x400000/0x430000/0x440000 (`handshake-spec §5`) |
| speed change | in-protocol (0xF0 baud field, 0x5A SETBAUD) (`ws63flash.c:294,349`) | none on USB (`handshake-spec §9`) |
| end of transfer | 0x87 reset poll for "Reset" in log (`ws63flash.c:186-216`) | `QUIT ` then USB re-enumeration wait (`handshake-spec §4.4`) |
| integrity anchor | SHA-256 code hash in 0x100-offset codeinfo header (`ws63sign.h:63,122`) | SHA-256 ASCII header, content-only digest (`handshake-spec §6.1`) |

Shared Hisilicon DNA worth noting: both sides ultimately trust a **SHA-256 over the
image body**, both use half-duplex strict request/response with small retry counts,
and both have a "device goes away and comes back different at the end" step (0x87
reset vs QUIT re-enumeration).

### 1.10 What is reusable for `scripts/flash-dongle.sh`

Our script only wraps `ws73-probe` libusb with the 3-file USB sequence
(`flash-dongle.sh:54-58`), so ws63flash itself (UART-only) is **not directly
pluggable**. Concretely reusable pieces:

1. **HHD-01 CH340K serial flashing path**: the HHD-01 dev board exposes an onboard
   USB-serial (CH340G-class, 115200) for flash/AT
   (`.scratch/nearlink-driver/lab-notes/HHD01-BOARD.md:59`). If we ever want to
   reflash the HHD-01's WS63E without HiSpark Studio, `ws63flash --flash
   /dev/ttyUSB0 assets/HHD01-firmware-violin-1.10.102.fwpkg` is a ready-made,
   GPL open-source replacement — and §1.7 proves our local fwpkg parses with it.
   This gives the lab an independent escape hatch if the USB boot path bricks a
   dongle (boot ROM lives on the chip, independent of flash contents).
2. **CRC16/XMODEM implementation + table** (`ymodem.h:36-81`): if our ws73-probe
   ever needs a boot-ROM-speaking UART mode (e.g. WS63E-based sticks), this is
   drop-in.
3. **fwpkg parser/manifest** (`fwpkg.h:51-147`): we already have a real .fwpkg in
   assets; a 30-line port lets us enumerate partitions/burn addresses of vendor
   packages instead of hard-coding `@0x400000` etc. in flash-dongle.sh
   (`flash-dongle.sh:55-57`).
4. **Timing folklore**: the 100 ms inter-command settle (`ws63flash.c:396-400`) and
   the "wait for device reset for ≤10 s while polling" pattern
   (`ws63flash.c:186-216`) are directly analogous to our ≥5 ms FILES interleave and
   5 s re-enumeration budget (`handshake-spec §7`) — useful priors when tuning
   ws73-probe timeouts.
5. **Erase accounting**: erase granularity 8 KiB, computed as round-up
   (`ws63flash.c:374`) — if we ever implement a WS73 *flash* (not RAM) download
   path, this is the expected sector math.
6. Not reusable: the 0x230000/0x220000 WS63 flash map (README.en:44-46) does not
   apply to our WS73 RAM-load model.

---

## Part 2 — Ghidra_RISCV31: Huawei custom RISC-V processor module

### 2.1 What it is

A Ghidra processor module forked from the stock NSA RISCV module
(README.md:127) adding a language **"RISCV31"** (`RISCV:LE:32:RV31`, variant
`RV31GC`, "RISC-V 32GC with Huawei RISCV extension", `data/languages/riscv.ldefs:4-15`).
README targets Hi3863V100 (= WS63), Hi2821 and other HiSilicon custom RV32 cores
(README.md:5) — the same core family as our WS73/WS63E parts. The author warns the
semantics are **best-effort guesses, untested against a manual** (README.md:1).
Installation is copy-into-`Ghidra/Processors/` (README.md:123).

### 2.2 Composition

The slaspec `data/languages/riscv.ilp31f.slaspec` includes the standard RV32GC set
(rv32i/a/m/b/p/k/f, csr, priv, rvc, rvv, zifencei — `riscv.ilp31f.slaspec:12-28`,
`riscv.zi.sinc:1-7`) and then **`riscv.rv31.sinc` (683 lines)** which defines all
Huawei custom instructions. Context bits enable every standard extension
(`data/languages/RV32GC.pspec:8-18`). A dedicated 48-bit token `hlinstr` is defined
for the long-instruction extension (`riscv.rv31.sinc:101-110`).

### 2.3 Custom instructions added (all from `riscv.rv31.sinc`)

These correspond to the public "Huawei Custom Extension" in the RISC-V code-size
reduction archive (README.md:128):

1. **C.PUSH / C.POP / C.POPRET** — 16-bit push/pop-multiple. Register list is
   fixed-order `ra, s0-s11, a0, a1` (15 regs, selected by 4-bit `rcount`), stack
   adjust `sp16imm` in 16-byte units (`riscv.rv31.sinc:114-208` list builders;
   semantics `:437-457`; encoding table `:427-435`). `popret` restores and
   `return [ra]` (`:451-457`) — this is the compiler's function epilogue.
2. **L.LI** — a **48-bit** instruction that loads a full 32-bit immediate in one go
   (token `hlinstr` fields `hlop0006=0x1f`, `hlop1215=0x0`, imm32 at bits 16..47;
   `riscv.rv31.sinc:101-106, 460-469`). Without this, Ghidra desyncs the
   instruction stream wherever gcc emits it.
3. **C.SB / C.LBU / C.SH / C.LHU** — compressed byte/half load-store encoded into
   the RVC space slots behind C.FSD/C.FLD/C.FSDSP/C.FLDSP (16-bit, 3-bit
   compressed regs s0-s1/a0-a5; `riscv.rv31.sinc:471-510`, table `:475-482`).
4. **MULIADD** — `rd = rs1 + rs2 * uimm9`, opcode 0x5B (custom-2 space)
   (`riscv.rv31.sinc:512-524`).
5. **ADDSHF / SUBSHF / ORSHF / XORSHF / ANDSHF** — pre-shifted ALU ops:
   `rd = rs1 <op> shift(rs2, shamt, shtype)` with shtype ∈ {sll, srl, sra,
   **ror**} (`gshtype` `:417-420`), opcode 0x1B (`:526-571`). ROR inside means the
   core also has bit-rotation — relevant when reading crypto/hash loops.
6. **JAL16 / J16** — long-jump with ~25-bit immediate, opcode 0x7B (custom-3),
   call vs goto (`riscv.rv31.sinc:573-591`). 25-bit displacement reaches ±32 MiB —
   this is how the firmware crosses the big flash/RAM gaps.
7. **BEQI / BNEI / BLTI / BGEI / BLTUI / BGEUI** — compare-register-with-8-bit-
   immediate-and-branch, opcode 0x3B (`riscv.rv31.sinc:593-640`). Huge for loop
   recovery: literal-count loops (`cmpimm 0..255`) become single instructions.
8. **UXTH / UXTB** — zero-extend half/byte in 16-bit encoding
   (`riscv.rv31.sinc:642-659`).
9. **STMIA / LDMIA** — 32-bit store/load-multiple ("increment after"), arbitrary
   register bitmask incl. ra/sp/s0-s11/a0-a7/t0-t6, opcode 0x0B (custom-0)
   (`riscv.rv31.sinc:661-684`, bit-table `:662-672`).

Opcode map of the custom space used: 0x0B (STMIA/LDMIA + UXTH/UXTB in C-space),
0x1B (preshifted ALU), 0x1F (L.LI), 0x3B (immediate conditional branches),
0x5B (MULIADD), 0x7B (JAL16/J16) — i.e. the RISC-V custom-0/1/2/3 quadrants plus
one unused standard opcode slot.

### 2.4 Reversing value for `ws73.bin` / `wow.bin`

- The WS73 firmware blobs we load at 0x400000 (`flash-dongle.sh:55`) are compiled
  for this same HiSilicon RV32 core; gcc for `-march=rv32imafc +Huawei ext` emits
  exactly C.PUSH/POP epilogues, L.LI constants, BEQI loops and J16 far calls.
  With stock Ghidra RISCV these decode as illegal or as wrong RVC instructions,
  which (a) breaks function boundary detection at every epilogue, (b) desyncs the
  stream at every 48-bit L.LI, and (c) kills the decompiler's loop/switch
  recovery. This module fixes all three, which is the difference between noise
  and readable pseudocode for ws73.bin/wow.bin.
- Practical recipe: import ws73.bin as raw at base 0x400000 (its RAM load address,
  `handshake-spec §5`), select language `RISCV:LE:32:RV31`, disassemble; loaderboot
  / flash-resident images would use their burn addresses (0x220000/0x230000
  equivalents) as base.
- The custom-branch and MULIADD encodings also make good **fingerprint/opportunity
  scanners**: MULIADD clusters usually mark hash/whitening code; ROR-capable
  ADDSHF chains mark CRC/cipher inner loops — helpful to locate the SLE/HCC
  handlers inside ws73.bin without symbols.
- Caveats: module is 32-bit little-endian only (`riscv.ldefs:5-7`); instruction
  semantics are author guesses (README.md:1) — the J16/BEI/shift semantics should
  be sanity-checked against gcc output if we ever build for the same core
  (e.g. the fbb_ws63 toolchain referenced in ws63flash README:56); popret's
  `return [ra]` may occasionally over-cut basic blocks in hand-written assembly.

---

## 8-line summary

1. ws63flash (github.com/goodspeed34/ws63flash, GPL-3) is a UART boot-ROM flasher for WS63, RE'd from HiSilicon BurnTool; speaks binary frames `EF BE AD DE + LEN16 + cmd + ~cmd + data + CRC16-XMODEM`.
2. Commands: 0xF0 handshake (baud+magic 0x0108) → YModem-send built-in signed loaderBoot → 0xD2 DOWNLOADI(addr,len,erase) per image → 0x87 reset poll; erase rounds up to 8 KiB sectors; 100 ms settle between command and YModem.
3. Its fwpkg parser was empirically validated against our `assets/HHD01-firmware-violin-1.10.102.fwpkg`: magic `DF AD BE EF` (LE 0xEFBEADDF — code right, header comment wrong), 12-byte header, 52-byte bin_info, first entry root_loaderboot_sign.bin.
4. Our WS73 USB WRITEM/FILES/QUIT boot protocol (assets/01-firmware-handshake-spec.md) is a different transport/framing (ASCII + bulk, SHA-256 file headers, no loader staging); shared DNA: half-duplex req/response and SHA-256 integrity.
5. Reusable for flash-dongle.sh: an open-source escape hatch to flash the HHD-01 over its CH340 serial with ws63flash --flash; drop-in CRC16-XMODEM; a fwpkg manifest parser to replace hard-coded @0x400000 addresses; timing priors (100 ms settle, 10 s reset poll).
6. Ghidra_RISCV31 (github.com/NWMonster/Ghidra_RISCV31) is a Ghidra module for the exact HiSilicon custom RV32 core (RISCV:LE:32:RV31) in WS63/WS73; it adds C.PUSH/C.POP/C.POPRET, 48-bit L.LI, C.SB/C.LBU/C.SH/C.LHU, MULIADD, ADDSHF/SUBSHF/ORSHF/XORSHF/ANDSHF (incl. ROR), JAL16/J16 (±32 MiB), BEQI/BNEI/BLTI/BGEI/BLTUI/BGEUI, UXTH/UXTB, STMIA/LDMIA over opcodes 0x0B/0x1B/0x1F/0x3B/0x5B/0x7B.
7. For ws73.bin/wow.bin reversing: fixes epilogue function boundaries, 48-bit stream desync, and loop/far-call recovery — import raw at 0x400000 with RISCV:LE:32:RV31; MULIADD/ROR clusters fingerprint hash/cipher code. Caveat: semantics are author guesses, 32-bit LE only.
8. Note path: `.scratch/nearlink-driver/lab-notes/NEW-WS63FLASH-GHIDRA.md` (this file).
