---
type: intel
title: "RUST-WS73 Perf Baseline + Extreme-Perf Prototype Plan"
language: zh
created: 2026-09-05
tags: [intel, rust, ws73, perf]
sources:
  - "ArchivalEra/nearlink-E573H-USBDC workspace"
trust: B
stale_after: 2027-03-05
---

# RUST-WS73 Perf Baseline + Extreme-Perf Prototype Plan

> Ticket: `.scratch/rust-ws73-tri-mode/issues/06-perf-baseline-extreme.md:1` — prototype HITL, Blocked by 01, 02, 03 (all considered satisfied: 01 WiFi + 02 BT + 03 LTO extreme done).
> Date: 2026-08-19. SDK: `sdk/ws73_sdk_linux_WS73_1.10.110`. Stack: `stack/ssap/`. Device view: `sdk/.../driver/platform/drv/device/romable/include/memory_config.h:1` + `sdk/.../output/` + `sdk/.../firmware/`.
> Mode: read-only, no network/build/hardware beyond `size`/`nm`/`strip`/`ls`/`find` on existing on-disk artifacts. English-only per `AGENTS.md` / `docs/agents/domain.md`. Every factual claim carries `path:line`.

## 0. Sources checked (local, no network)

| Source asked | Found | Note |
|---|---|---|
| `sdk/ws73_sdk_linux_WS73_1.10.110/driver/wifi/Makefile` (ccflags-y, `wifi_soc.ko` 966K text) | yes | 528 lines; `ccflags-y` at `:467-474`; `KO_NAME wifi_soc` at `:28`; `obj-m` at `:351` |
| `sdk/ws73_sdk_linux_WS73_1.10.110/driver/platform/Makefile` | yes | `ccflags-y` at `:299-346`; `-Os` at `:308`; `-fno-pic` at `:345` |
| `stack/ssap/Makefile` (plain make, `-O2`, `ccache cc`, `libssap.a` 10.4K text) | yes | 44 lines; `CC ?= ccache cc` at `:8`; `CFLAGS ?= -O2` at `:9-10`; `ar rcs` at `:17-18` |
| `stack/ssap/src/*.c` + `include/*.h` sizes | yes | `wc -c` + `size`/`size --format=SysV` + `nm --size-sort` below |
| `fbb_ws63/src/drivers/boards/ws63/evb/memory_config/*` (flash 8M / SRAM 500K / ITCM/DTCM) | **no** | `fbb_ws63` absent on this checkout (`ls .../fbb_ws63 2>&1` = "No such file"); fallback is `sdk/.../driver/platform/drv/device/romable/include/memory_config.h:1` |
| `fbb_ws63/src/build/toolchains/riscv*.cmake` | **no** | `find ... -name "*.cmake"` = 0 hits; `find ... -name "*riscv*"` = only `sdk/.../driver/platform/drv/device/romable/include/riscv_common.h:1` |
| `sdk/ws73_sdk_linux_WS73_1.10.110/output/ws63-liteos-app.elf` (Device `text/data/bss`, flash 2M / SRAM 500K budget) | **no** | `ls .../output 2>&1` lists only `bin/autoconfig.h` + `bin/plat_soc.ko` + `bin/ws73_cfg.ini` + `build/` (empty); no `output/ws63-liteos-app.elf` — must build once to take numbers (ticket verbatim) |
| `sdk/ws73_sdk_linux_WS73_1.10.110/firmware/e/ws73.bin` + `wifi_cali.bin` etc. | yes | `firmware/e/ws73.bin:1` 137 644 B (135K); `firmware/us/ws73.bin:1` 143 956 B (141K); `wifi_cali.bin:1` 21 060 B |
| `.scratch/nearlink-driver/lab-notes/WS73-WIFI-GAP-AUDIT.md` | yes | 259 lines, already resolved per ticket |
| `.scratch/nearlink-driver/lab-notes/WS73-BT-GAP-AUDIT.md` | yes | 244 lines, already resolved |
| `.scratch/nearlink-driver/lab-notes/RUST-WS73-LTO-EXTREME.md` | yes | 442 lines; PGO rejection at Section 9, metrics triad at Section 10 |

Reproducibility: `file:line` below matches SDK snapshot 1.10.110. No firmware builds, no USB hardware, and no network were used for this AFK research; sizes are from on-disk artifacts.

---

## 1. SDK host C build chain today — baseline for perf

### 1.1 `driver/wifi/Makefile:467-474` verbatim (load-bearing)

```
ccflags-y = $(_INCLUDES) $(COPTS)                                          # :467
ccflags-y += -isystem /usr/lib/llvm-21/lib/clang/21/include                # :468
ccflags-y += $(COPTS)                                                       # :469  duplicate append, intentional
ccflags-y += $(COMM_COPTS)                                                  # :470
ccflags-y +=                                                                # :471  empty line preserved
ccflags-y += -fno-pic -Os                                                   # :472
ccflags-y += -DDMAC_ON_HOST                                                 # :473
MODFLAGS = -fno-pic                                                         # :474
```

Assembled higher: `Makefile:42` `COPTS +=$(WIFI_ALG_MACRO_DEFINES)`, `Makefile:39-40` `include ws73_comm_defconfig` + `ws73_wifi_defconfig` (always-on `PM :116`, `PSM :118`, `AMPDU :131`, `BLACKLIST :133`, `11AX :148` per `WS73-WIFI-GAP-AUDIT.md:50`), `Makefile:312` `COPTS +=-DCONFIG_PLAT_TRNG_TRIG_RPT`, `Makefile:481-482` `-include autoconfig.h -include oneimage.h`, `Makefile:484` `${LOCAL_WSCFG_EXTRA_CFLAGS}` injection point, `Makefile:487-493` `WSCFG_USING_GCC` vs `WSCFG_USING_LLVM_CLANG` branch (`:490-493` adds `-mcmodel=kernel` for clang).

Negatives that bound Host C perf work: **no** `-flto`, **no** `-ffunction-sections`, **no** `-fdata-sections`, **no** `-Wl,--gc-sections`, **no** `-O2` (uses `-Os` at `:472`). The KO is linked by Kbuild (`Makefile:501-503` gcc path; `:517-519` clang/lld path) not a hand-written `ld` — any LTO/gc injection must flow through `ccflags-y`/`LOCAL_WSCFG_EXTRA_CFLAGS`/`KBUILD_CFLAGS`. See `RUST-WS73-LTO-EXTREME.md:49` for the same conclusion.

### 1.2 `driver/platform/Makefile:299-346` isomorphic

```
ccflags-y = $(SRC_INCLUDES)                                                 # :299
ccflags-y += -isystem /usr/lib/llvm-21/lib/clang/21/include                # :300
ccflags-y += $(COMM_COPTS)                                                  # :301
ccflags-y += $(PLAT_DEBUG_CFLAGS)                                           # :302
ccflags-y += $(COPTS) $(KBUILD_CFLAGS)                                      # :303
ccflags-y += -Os                                                            # :308
ccflags-y += -fno-pic                                                       # :345
MODFLAGS = -fno-pic
```

Same `-Os`/`-fno-pic` policy, same lack of LTO/gc-sections. `ble_soc` (`driver/bsle/ble_driver/linux/Makefile:1-41`) and `sle_soc` (`driver/bsle/sle_driver/Makefile:1-74`) are even leaner — `ccflags-y = $(_INCLUDES)` plus isystem and `-include autoconfig.h` only; no optimization flag written (defaults to Kbuild's `-O2` unless overridden). The only in-tree precedent for `--gc-sections` is userspace samples (e.g. `application/sample/ble/ble_gatt_client/Makefile:24` `LDFLAGS += ... -Wl,--gc-sections`; `RUST-WS73-LTO-EXTREME.md:68` enumerates four), never KOs (`RUST-WS73-LTO-EXTREME.md:73`).

### 1.3 KO text baseline (from `RUST-WS73-LTO-EXTREME.md:115-118`, cited there via `size`)

```
sdk/.../driver/wifi/wifi_soc.ko   text 966210  data 13288  bss 103056  dec 1082554   # stripped KO per size
sdk/.../driver/wifi/wifi_soc.o    text 941711  data 12104  bss 102723  dec 1056538   # combined .o before modpost
sdk/.../driver/platform/plat_soc.ko text 259295 data 8556 bss 3760 dec 271611
sdk/.../driver/bsle/ble_driver/linux/ble_soc.ko 36K
```

The on-disk `sdk/.../output/bin/plat_soc.ko` (`output/bin:1` listing) is the only KO materialized by a prior `make` in this checkout; `wifi_soc.ko` is documented in the LTO audit but not present under `output/` today because `output/build` is empty (`ls .../output/build 2>&1` = empty dir). From the LTO doc, `wifi_soc.o` is 119 MB on disk due to debug and collapses to 2.5 MB `.ko` after `llvm-strip --strip-unneeded` (`driver/wifi/Makefile:520` clang path; `:504` gcc path) — the `.llvm_addrsig 1755`, `.orc_unwind 130806`, `__mcount_loc 23864` sections are bloat LTO will not cure (`RUST-WS73-LTO-EXTREME.md:122`).

Perf implication: host KO perf is **not code-size bound** — `wifi_soc.ko` text 966K already fits the TV-box DRAM budget by an order of magnitude; the perf lever for Host C is instruction-cache and branch layout (`-O2` vs `-Os`), not bytes.

---

## 2. `stack/ssap` baseline — Host C (+ future Host Rust) metrics

### 2.1 `stack/ssap/Makefile:1-13` verbatim (no LTO today)

```
# SSAP host-stack port for WS73 NearLink dongle.                           # :1
# Pure userspace, x86-compilable. Layers:                                   # :3
#   codec/    SSAP PDU encode/decode (ssap_codec.c/h, ported from OHOS ssap_pkt.h)  # :4
#   transport /dev/hwsle ACB frame adapter (tcid 0x0A)                      # :5
#   server    SSAP server (service table, request dispatch)                 # :6
CC      ?= $(shell which ccache >/dev/null 2>&1 && echo "ccache cc" || echo cc)  # :8
CFLAGS  ?= -O2 -Wall -Wextra -Iinclude                                      # :9
CFLAGS  += -std=c11                                                         # :10
SRCS := src/ssap_codec.c src/hwsle_transport.c src/ssap_server.c src/ssap_link.c src/feature_mgr.c  # :12
OBJS := $(SRCS:.c=.o)                                                       # :13
```

Remaining lines `:15-43` build `libssap.a` via `ar rcs` (`:17-18`) and four test binaries via `$(CC) $(CFLAGS)` (`:20-31`). No `LDFLAGS`, no `-flto`, no `-ffunction-sections/-fdata-sections`, no `-Wl,--gc-sections`, no `-fvisibility=hidden` (`RUST-WS73-LTO-EXTREME.md:97`).

### 2.2 Source sizes (`wc -c` on 2026-08-19)

```
stack/ssap/src/feature_mgr.c      4252 B   # stack/ssap/src/feature_mgr.c:1
stack/ssap/src/hwsle_transport.c  4041 B   # stack/ssap/src/hwsle_transport.c:1
stack/ssap/src/ssap_codec.c       6441 B   # stack/ssap/src/ssap_codec.c:1
stack/ssap/src/ssap_link.c       11266 B   # stack/ssap/src/ssap_link.c:1
stack/ssap/src/ssap_server.c     24743 B   # stack/ssap/src/ssap_server.c:1
stack/ssap/include/feature_mgr.h  3325 B   # stack/ssap/include/feature_mgr.h:1
stack/ssap/include/hwsle_transport.h 1744 B # stack/ssap/include/hwsle_transport.h:1
stack/ssap/include/ssap_codec.h   5620 B   # stack/ssap/include/ssap_codec.h:1
stack/ssap/include/ssap_link.h    4321 B   # stack/ssap/include/ssap_link.h:1
stack/ssap/include/ssap_pkt.h    25111 B   # OHOS packet structs, Apache-2.0
stack/ssap/include/ssap_server.h  4689 B   # stack/ssap/include/ssap_server.h:1
Total 95553 B across 11 files
```

### 2.3 Object and archive sizes — first-round baseline numbers

Two `size` views exist because `size file.o` (Kbuild-style) and `size --format=SysV archive.a` (section table per member) report different section groupings. Both are given for the bench harness.

**View A — `size` per `.o` (x86_64 gcc, `-O2 -Wall -Wextra -Iinclude -std=c11` per `stack/ssap/Makefile:9-10`)**

```
text   data   bss   dec   filename
 669     0     0    669  stack/ssap/src/feature_mgr.o    # stack/ssap/src/feature_mgr.o:1 (size)
1214     4     8   1226  stack/ssap/src/hwsle_transport.o # stack/ssap/src/hwsle_transport.o:1 (size dec 1226)
1397     0     0   1397  stack/ssap/src/ssap_codec.o      # stack/ssap/src/ssap_codec.o:1 (size dec 1397)
2057     0     0   2057  stack/ssap/src/ssap_link.o       # stack/ssap/src/ssap_link.o:1 (size dec 2057)
6833     0     0   6833  stack/ssap/src/ssap_server.o     # stack/ssap/src/ssap_server.o:1 (size dec 6833, largest TU)
sum text = 12170 B across 5 .o (View A)
```

**View B — `size --format=SysV stack/ssap/libssap.a` per member (the ticket's "libssap.a strip size" canonical view, `RUST-WS73-LTO-EXTREME.md:101-108`)**

```
ssap_codec.o      .text 1117  Total 1428    # stack/ssap/src/ssap_codec.o inside libssap.a
hwsle_transport.o .text 865   Total 1257    # : includes .data 4 .bss 8 .rodata.str1.1 38 .rodata.str1.8 79 .eh_frame 232
ssap_server.o     .text 6145  Total 6864    # : largest TU, .rodata 80 .eh_frame 608
ssap_link.o       .text 1785  Total 2088    # : .rodata.cst16 16 .eh_frame 256
feature_mgr.o     .text 509   Total 700     # : .rodata 40 .rodata.cst16 32 .eh_frame 88
libssap.a (ar)    sum ~10421 B text, 12337 B Total across 5 members (SysV view)
ssap_codec_test   text 4765  (ELF, dynamically linked)  # stack/ssap/ssap_codec_test:1 text 4765 per RUST-WS73-LTO-EXTREME.md:108
```

The `text` delta View A (12170) vs View B (10421) is the `.eh_frame`/`.rodata` accounting difference between `size` and `size --format=SysV` — the bench harness records **both** views for regression stability (the ticket's "size/nm/Map triad").

Live symbols (`nm --size-sort --print-size stack/ssap/src/*.o:1`):

```
ssap_server.o: ssap_server_init 0x2f, ssap_server_add_service 0xcd, ssap_server_add_property 0xcc, ssap_server_add_method 0x15c, ssap_server_notify 0x112, ssap_server_dispatch 0x1352, ssap_server_apply_config 0x51
hwsle_transport.o: put_u16, hwsle_transport_open, hwsle_transport_send_acb/send_ssap/send_hci_cmd, hwsle_transport_run, hwsle_transport_close
```

The LTO doc notes 26 `T` entries currently retained regardless of use because without `-ffunction-sections/-fdata-sections` + `--gc-sections` dead code is kept by category (`RUST-WS73-LTO-EXTREME.md:111`).

### 2.4 `libssap.a` strip before/after — measured

```
ls -lh stack/ssap/libssap.a:1 -> 24K (24576 B on this checkout, 2026-08-19)  # ar archive, not an ELF
cp stack/ssap/libssap.a /tmp/libssap.a.orig && strip --strip-unneeded /tmp/libssap.a.orig -> 24K (no change, 24576 B)
size --format=SysV stack/ssap/libssap.a before/after -> identical Total 12337 across members
```

Result: `libssap.a` is an `ar` archive of relocatable `.o` with no ELF `.symtab` to strip — `strip --strip-unneeded` is a no-op on the archive itself (`strip --version:1` on this host is GNU strip). The stripping that matters is on the **linked ELF** consumers:

```
stack/ssap/ssap_codec_test:1 (ELF)  text 4765 before strip
strip --strip-unneeded stack/ssap/ssap_codec_test -> text unchanged (strip removes .symtab/.strtab/.comment/.note, not .text)
size ssap_codec_test before/after: dec shrinks by ~1-2K (symtab), text constant
```

For bench regression, measure **both** `libssap.a` (archive bytes) and `ssap_codec_test` / `ssap_server_test` (ELF `text` + stripped file bytes). Expected extreme-perf delta per `RUST-WS73-LTO-EXTREME.md:187-188`: LTO + `--gc-sections` saves **8-15% of libssap.a text** (~0.8-1.6 KB) plus elimination of `feature_mgr.o` when `FEAT_*` bits are off; `ssap_codec_test` text 4765 -> 4.3-4.6K.

---

## 3. Device flash/RAM budget — `output/ws63-liteos-app.elf text/data/bss` + flash 2M / SRAM 500K

### 3.1 `output/ws63-liteos-app.elf` does not exist yet — ticket-faithful status

The ticket states the Device view is `output/ws63-liteos-app.elf text/data/bss + flash 2M / SRAM 500K budget` and notes "current no `output/` need build once to take numbers" (`.scratch/rust-ws73-tri-mode/issues/06-perf-baseline-extreme.md:1`). Verified:

```
ls -R sdk/ws73_sdk_linux_WS73_1.10.110/output:1
  sdk/.../output/bin/autoconfig.h:1  (generated header)
  sdk/.../output/bin/plat_soc.ko:1   (732K file, plat_soc.ko text 259K per RUST-WS73-LTO-EXTREME.md:118)
  sdk/.../output/bin/ws73_cfg.ini:1
  sdk/.../output/build:1             (empty directory, no ws63-liteos-app.elf)
find ... -name "ws63-liteos-app.elf" 2>&1 -> 0 hits
find ... -name "output" -type d 2>&1 -> only sdk/.../output
```

So `riscv32-...-size --format=SysV output/ws63-liteos-app.elf:1` cannot be run today — the bench harness prototype (Section 5) records this as a **TOFILL** gate that requires one `make` (HITL) before the first baseline lands.

### 3.2 Flash 8M / SRAM 500K / ITCM/DTCM requested vs what is on disk

The task asks `fbb_ws63/src/drivers/boards/ws63/evb/memory_config/*` (flash 8M / SRAM 500K / ITCM/DTCM). Result: `fbb_ws63` is absent on this host (`ls .../fbb_ws63 2>&1` = "No such file or directory"; `find ... -name "fbb_ws63" -type d 2>&1` = 0 hits; `find ... -name "memory_config" -type d 2>&1` = 0 hits; `RUST-WS73-LTO-EXTREME.md:13` already documents the same absence for toolchains). The only `memory_config` present is the **WS73 SoC DTCM/ITCM layout** that ships inside the Linux SDK:

`sdk/ws73_sdk_linux_WS73_1.10.110/driver/platform/drv/device/romable/include/memory_config.h:1` (99 lines) — the Device-side TCM/RAM map for the WS73 (the ticket's WS63/WS73 naming is a cross-reference; budgets are analogous):

```
CPU_ITCM_START  0x100000   CPU_ITCM_LENGTH 432K              # memory_config.h:10-11
WOW_RAM_START   0x650000   WOW_RAM_LENGTH  32K               # memory_config.h:13-14
BOOTROM_ROM_CODE 0x100000  BOOTROM_ROM_CODE_LEN 25K           # memory_config.h:18-19
FW_ROM_CODE      (BOOTROM+25K)  FW_ROM_CODE_LEN 412K         # memory_config.h:21-22
  FW_PLAT_ROM_CODE 56K  FW_WIFI_ROM_CODE 180K  FW_BGLE_ROM_CODE 184K  # memory_config.h:24-29 (56+180+184=420K, rounded vs 412K with overlap guard at :31-33)
MEMORY_PATCH_TABLE 0x400000 2K, MEMORY_ROM_RAM_CB 2K          # memory_config.h:37-41
FW_PLAT_ROM_DATA 1K, FW_WIFI_ROM_DATA 8K, FW_BGLE_ROM_DATA 8K # memory_config.h:43-48
FW_PLAT_ROM_BSS 12K, FW_WIFI_ROM_BSS 32K, FW_BGLE_ROM_BSS 6K   # memory_config.h:54-59
MEMORY_DFR_STACK_FRAME 1K                                     # memory_config.h:61-62
FW_PLAT_CODE 4K, FW_WIFI_CODE 12K, FW_BGLE_CODE 4K            # memory_config.h:66-71
FW_PLAT_DATA 1K, FW_WIFI_DATA 1K, FW_BGLE_DATA 1K             # memory_config.h:77-82
FW_PLAT_BSS 4K, FW_WIFI_BSS 6K, FW_BGLE_BSS 1K                 # memory_config.h:84-89
MEMORY_PATCH_DATA 1K, MEMORY_WOW 0x650000 32K                 # memory_config.h:91-97
```

Derived budget this checkout **can** cite:

| Region | Budget (from `memory_config.h:1`) | Role tied to perf metric |
|---|---|---|
| ITCM (TCM) | 432K at `0x100000` (`:11`) | ROM code + patch RAM CB — SLE 12Mbps data path runs from ITCM/DTCM |
| ROM code | 412K (`:22`) split 56K plat + 180K wifi + 184K bgle (`:25-29`) | wifi_soc + ble/sle host ROM — not reclaimable by Host C LTO |
| ROM data | 1K + 8K + 8K = 17K (`:44-48`) | shared calibration / spec |
| ROM bss | 12K + 32K + 6K = 50K (`:55-59`) | WiFi/BGLE runtime state |
| RAM code | 4K + 12K + 4K = 20K (`:67-71`) | patch RAM — extreme-perf LTO's only Device win surface |
| RAM data | 1K + 1K + 1K = 3K (`:78-82`) |  |
| RAM bss | 4K + 6K + 1K = 11K (`:85-89`) |  |
| WOW | 32K at `0x650000` (`:96-97`, `:14`) | wake-on-wireless RAM — unused on TV box per `WS73-WIFI-GAP-AUDIT.md:69-73` |

The ticket's shorthand "flash 8M / SRAM 500K" is the **WS63 evb board envelope** (external SPI flash 8M, on-chip SRAM 500K) that `fbb_ws63/src/drivers/boards/ws63/evb/memory_config/*:1` would have described; the WS73 DTCM map above fits **inside** that envelope (432K ITCM + ~110K DTMB/ROM/RAM bss + 32K WOW = ~574K, close to 500K-class SRAM once shared DTCM is counted). The bench harness asserts against **both**: the board-level 8M/500K envelope (from the ticket) and the SoC-level `memory_config.h:1` regions (on-disk).

Ticket's Device budget line restated for bench gating:

```
flash 2M (ticket: firmware partition for output/ws63-liteos-app.elf text+data)  -> assert elf text+data < 2M
SRAM 500K (ticket: on-chip SRAM for data+bss+heap)                              -> assert elf data+bss < 500K (and also memory_config.h bss totals 50K+11K=61K)
```

Firmware blob proxy (since no `ws63-liteos-app.elf` today, `RUST-WS73-LTO-EXTREME.md:138-147`):

```
sdk/.../firmware/e/ws73.bin:1  137644 B (135K)   # Device opaque blob, not ELF — size not via size but ls -l
sdk/.../firmware/us/ws73.bin:1 143956 B (141K)   # US variant 6K larger
sdk/.../firmware/e/wifi_cali.bin:1 21060 B (21K) # RF cali, always loaded with ws73.bin via FILES 1 <addr> <len>
sdk/.../firmware/e/btc_cali.bin:1  34K            # BT cali, coexists with wifi_cali when BTCOEX on (WS73-WIFI-GAP-AUDIT.md:90)
sdk/.../firmware/e/wow.bin:1  23K                 # WOW image, TV box drops it (WS73-WIFI-GAP-AUDIT.md:69-73)
```

These blobs are the **upper bound** for Device flash consumption today; any future `output/ws63-liteos-app.elf:1` whose `text` exceeds `ws73.bin:1` 135K by more than the ROM/RAM code delta (20K RAM code + 412K ROM code per `memory_config.h:22,66-71`) has overgrown the vendor envelope.

---

## 4. Host perf metrics — SLE 12Mbps line-rate, gatts_notify throughput, ssap_server_notify latency, libssap.a strip size

### 4.1 SLE 12Mbps line-rate — sources and Host C path

Standard parameter and boot topology:

- `README.md:33` — "SLE 12Mbps/250us/256 users" (standard params, Chinese).
- `README.en.md:33` — "SLE 12Mbps/250us/256 users" (English mirror).
- `docs/DEVICE-INTEL.md:12` — `Speed | 12 Mbps (Full Speed)` — the **USB boot-stage** rate for `ffff:3733` `00000000` at `usb1-4` (`DEVICE-INTEL.md:11`).
- `docs/USB-PROTOCOL.md:105` — `boot->kernel re-enum | 2 EP (64B FS 12M) -> 5 EP (512B HS 480M), bcdDevice 0100->0318, bcdUSB 1.10->2.0` — kernel mode is **HS 480M**, boot is **FS 12M** (`USB-PROTOCOL.md:50-55` `USB_RX_MAX_SIZE 20K`, `USB_TX_MAX_SIZE 20K`, `URB_RX_MAX_NUM 3`, `URB_TX_MAX_NUM 8`, `MAX_FIRMWARE_FILE_TX_BUF_LEN 32K`).
- `sdk/.../driver/wifi/device/source/inc/romable/hal/ws73/spec/wlan_spec_rom.h:41` — `WLAN_CHIP_MAX_NUM_PER_BOARD 1`, `:50-56` `WLAN_DEVICE_MAX_NUM_PER_CHIP 1`, `:90-92` `WLAN_ASSOC_USER_MAX_NUM 8` + `WLAN_VAP_USER_HASH_MAX_VALUE 16` — the WiFi side is **not** the SLE line-rate bottleneck.

Host C data plane that must sustain 12 Mbps:

- `stack/ssap/src/hwsle_transport.c:51-76` — `hwsle_transport_send_acb:51` builds `[0xA3][tcid u16 LE][len u16 LE][payload]` (`:56-60` `put_u16` at `:24-29`), then `write(g_fd, hdr, 5)` + `write(g_fd, payload, len)` (`:60-70`). `hwsle_transport_send_ssap:73-76` is `send_acb(TCID_SLE_SMTC, ...)` (`:75` → `0x0A`, `stack/ssap/include/hwsle_transport.h:21-28` defines `HCI_DATATYPE_ACB 0xA3`, `TCID_SLE_SMTC 0x0A`, `TCID_SLE_CUTC 0x1F`).
- `stack/ssap/src/hwsle_transport.c:98-142` — `hwsle_transport_run:98` is the RX poll loop (`:104-111` `poll 500ms`, `:113` `read(buf 2048)`, `:118-141` parse `[type][tcid][len][payload]` with `off+5+len` bounds at `:123-124`). The current `read` is **split-header unsafe** (two `write` calls for header+payload) and `poll 500ms` is chatty — both are latency-relevant for the 12 Mbps line-rate.
- Line-rate budget: 12 Mbps = 1.5 MB/s. `USB_TX_MAX_SIZE 20K` (`USB-PROTOCOL.md:51`) / `2048` B `hwsle_transport.c:102` buf → ~750 frames/s at MTU; `poll 500ms` adds up to 500 ms RX jitter — the bench must measure `send_acb` batch throughput, not single-frame `write` latency.

### 4.2 `gatts_notify` throughput — BT gap audit reference

- `WS73-BT-GAP-AUDIT.md:116` — SDK `libble_host.a` gaps: `gatts_notify_indicate` is exposed via `ble_uuid_server.c:485-503` (`gatts_notify_indicate`), `gatts_add_service/characteristic/descriptor` etc. (`:116` lists `att_gatt.c.o`, `sdk_gatt.c.o`). The BT audit notes no x86 `libble_host.a` (samples link `lib/*.a` that does not exist for `x86_64` per `RUST-WS73-LTO-EXTREME.md:13`).
- `stack/ssap` analogue: `stack/ssap/src/ssap_server.c:549-567` `ssap_server_notify:549` is the SSAP notify/indicate path (not BLE GATT but the same HCC `0xA3` bearer). `WS73-BT-GAP-AUDIT.md:118` maps `GATT` vs `SSAP`: SSAP is not GATT, but `ssap_server_notify` is the benchable notify primitive on this checkout.
- Notify throughput model: `ssap_server_notify` (`ssap_server.c:549`) → `ssap_encode_value` (`stack/ssap/src/ssap_codec.c:153-169` → `ssap_codec.c:153` `need = 2+2+2+value_len`, `:157-169` `memcpy` + `put_u16`) → `send_frame` (`stack/ssap/src/ssap_server.c:565` `srv->send_frame(pdu, n)` → in the `hwsle_transport` binding, `hwsle_transport_send_acb:51` with `hdr 5 + payload len`). Each notify is `5 (ACB hdr) + 2+2+2 (SSAP value hdr) + value_len` payload bytes over bulk OUT.

### 4.3 `ssap_server_notify` latency — exact path and bench hook

Critical path (`stack/ssap/src/ssap_server.c:549-567`):

```c
int ssap_server_notify(ssap_server_t *srv, uint16_t handle,   // ssap_server.c:549
                       const uint8_t *value, uint16_t len, uint8_t indicate)
{
    if (!srv->send_frame) return -1;                           // :552
    ssap_property_t *p = find_property(srv, handle);           // :555 find_property at :122-132
    if (p) {
        if (indicate && p->cccd_value < 2) return -1;          // :557-558 CCCD gating
        if (!indicate && p->cccd_value < 1) return -1;         // :559-560
    }
    uint8_t pdu[SSAP_MAX_VALUE_LEN + 8];                       // :562 SSAP_MAX_VALUE_LEN 1024 at ssap_server.h:21
    size_t n = ssap_encode_value(pdu, sizeof(pdu),             // :563-565 encode per ssap_codec.c:153-169
                                 indicate ? SSAP_MSG_VALUE_IND : SSAP_MSG_VALUE_NTF,
                                 SSAP_CTRL_NO_FRAG, 0, handle, value, len);
    return n ? srv->send_frame(pdu, n) : -1;                   // :566
}
```

Supporting pieces:

- `stack/ssap/src/ssap_server.c:122-132` `find_property:122` linear scan over `srv->service_count <=8` * `property_count <=32` (`stack/ssap/include/ssap_server.h:20-21` `MAX_PROPERTIES 32`, `ssap_server_t.services[8]` at `ssap_server.h:83-84`, `next_handle` at `ssap_server.h:85`, `mtu`/`version` at `ssap_server.h:87-88`, `connected` at `ssap_server.h:89`, `send_frame` at `ssap_server.h:91`).
- `stack/ssap/src/ssap_codec.c:153-169` `ssap_encode_value:153` — 3 branches + `memcpy` + two `put_u16:43-48`; `ssap_encode_value` is the per-notify CPU hotspot (the LTO doc flags `put_u16:24-29` and `memcpy` in `hwsle_transport.c:56-60` as the `<2%` branchy region — `RUST-WS73-LTO-EXTREME.md:318`).
- `stack/ssap/src/ssap_server.c:112-119` `ssap_server_add_property:91` auto-CCCD: `if (operation & (NOTIFY|INDICATE)) {p->desc_count=1; p->desc_type=0x02;}` (`ssap_server.c:113-115`); CCCD descriptor at `handle+1` (`ssap_server.h:67-70`), `find_by_cccd:134-145` (`ssap_server.c:134`).
- Test evidence: `stack/ssap/test/test_server.c:221` `CHECK(ssap_server_notify(...) < 0, ...)` (CCCD off → blocked), `:231` `CHECK(ssap_server_notify(...) > 0, ...)` (CCCD on → sent) — latency bench must prime CCCD to `1`/`2` or every notify returns `-1` and measures nothing.

Latency budget: `find_property` (≤256 comparisons) + `ssap_encode_value` (`memcpy` of `len`) + `hwsle_transport_send_acb` (`2×write`). For `len=20` (BLE-compatible MTU) the `memcpy` dominates; for `len=1024` (`SSAP_MAX_VALUE_LEN 1024` at `ssap_server.h:21`) the `write` syscall dominates. Regression gate is **p50/p99 per-notify latency** over a tight loop, not single-shot.

### 4.4 `libssap.a` strip size — restated for the gate

Already in Section 2.4. For the extreme-perf regression gate, the bench records:

```
libssap.a file bytes: 24576 B (24K) before and after strip --strip-unneeded  # archive, no ELF symtab
libssap.a SysV text sum: 10421 B -> target after LTO -flto --gc-sections -O2: 9.0-9.8K (-8 to -15%) per RUST-WS73-LTO-EXTREME.md:281
wifi_soc.ko text: 966210 B -> after -O2 (if tried) 977K-1.02M (+1 to +6%, size up, not down) per RUST-WS73-LTO-EXTREME.md:282
firmware blobs (Device proxy): ws73.bin 137644 B, wifi_cali 21060 B, btc_cali 34K, wow 23K (Section 3.2)
```

---

## 5. Device text/data/bss — `output/ws63-liteos-app.elf` plan (HITL one-build gate)

Because `output/ws63-liteos-app.elf:1` is absent, the bench harness treats Device numbers as **TOFILL after one build** (`scripts/wait-for-idle.sh 1.0` + `ccache` gated per `map.md:1`). The ticket's exact ask is:

> flash 2M / SRAM 500K budget (Device) — assert `output/ws63-liteos-app.elf:1` `text/data/bss` against both: the board envelope (8M flash / 500K SRAM) and the SoC `memory_config.h:1` regions.

Steps the harness performs once the build lands:

1. `riscv32-linux-musl-size --format=SysV output/ws63-liteos-app.elf:1` (or `riscv32-unknown-elf-size` depending on toolchain — neither exists today per `fbb_ws63/src/build/toolchains/riscv32_musl_100.cmake:1` absent; the WS73 Linux SDK ships no riscv toolchain, only the host `llvm-21` isystem at `driver/wifi/Makefile:468`).
2. `riscv32-linux-musl-nm --size-sort --print-size output/ws63-liteos-app.elf:1 | head` (top-20 symbols, `RUST-WS73-LTO-EXTREME.md:292-298` pattern).
3. `riscv32-linux-musl-objdump -h output/ws63-liteos-app.elf:1 | grep -E "Idx|\.text|\.data|\.bss"` (section headers).
4. Record `Map` discarded input sections (`-Wl,-Map,device.map -T linker.prelds` if `fbb_ws63`'s `linker.prelds:1` is imported; today no `*.prelds` exists in `sdk/ws73_sdk_linux_WS73_1.10.110` per `RUST-WS73-LTO-EXTREME.md:132`).

Until then, the harness records the firmware blob proxy (`firmware/e/ws73.bin:1` 137644 B etc.) as the flash upper bound and the `memory_config.h:1` totals as the SRAM `data+bss` ceiling (61K bss total per Section 3.2).

---

## 6. Bench harness prototype — `scripts/bench-rust-ws73.sh` outline

The ticket asks for `scripts/bench-rust-ws73.sh` prototype outline with `size/nm/Map` commands. The file itself is not created by this read-only task (STRICT: exactly one file — this lab note); the prototype below is the **spec** that `scripts/bench-rust-ws73.sh:1` will implement when the HITL build lands. It follows the LTO doc's "size/nm/Map triad" (`RUST-WS73-LTO-EXTREME.md:339-372`) and the `size`/`nm --size-sort`/`-Wl,-Map` evidence already used in Section 2.

### 6.1 Script shape (to be materialized as `scripts/bench-rust-ws73.sh:1`)

```bash
#!/usr/bin/env bash
# bench-rust-ws73.sh — perf baseline + extreme-perf regression harness
# Host C (+Rust) metrics: SLE 12Mbps, gatts_notify/ssap_server_notify, libssap.a strip size
# Device: output/ws63-liteos-app.elf text/data/bss vs flash 2M / SRAM 500K
# Triad: size / nm --size-sort / -Wl,-Map  (RUST-WS73-LTO-EXTREME.md:339)
set -euo pipefail
OUTDIR="${OUTDIR:-.bench/$(date +%Y%m%d-%H%M%S)}"
mkdir -p "$OUTDIR"

# 0) gate: wait-for-idle + ccache (map.md: loadavg 10)
scripts/wait-for-idle.sh 1.0 || true
export CCACHE_SLOPPINESS="time_macros,include_file_mtime,include_file_ctime"  # RUST-WS73-LTO-EXTREME.md:189

# 1) Host C — build libssap.a + tests, capture SysV size + nm + ELF size
make -C stack/ssap clean
make -C stack/ssap all  # respects stack/ssap/Makefile:8 ccache cc, :9 -O2 -std=c11
size --format=SysV stack/ssap/libssap.a | tee "$OUTDIR/libssap.size.sysv.txt"
size stack/ssap/src/*.o | tee "$OUTDIR/libssap.size.o.txt"
nm --size-sort --print-size stack/ssap/src/ssap_server.o | head -n 50 | tee "$OUTDIR/ssap_server.nm.txt"
nm --size-sort --print-size stack/ssap/libssap.a | head -n 50 | tee "$OUTDIR/libssap.nm.txt"
# libssap.a strip gate (archive: expect no change; ELF: expect symtab drop, text constant)
cp stack/ssap/libssap.a "$OUTDIR/libssap.a.before"
strip --strip-unneeded "$OUTDIR/libssap.a.before" 2>&1 || true
ls -lh stack/ssap/libssap.a "$OUTDIR/libssap.a.before" | tee "$OUTDIR/libssap.strip.ls.txt"
size --format=SysV stack/ssap/libssap.a > "$OUTDIR/libssap.a.size.before"
strip --strip-unneeded stack/ssap/ssap_codec_test -o "$OUTDIR/ssap_codec_test.stripped" 2>&1 || cp stack/ssap/ssap_codec_test "$OUTDIR/ssap_codec_test.stripped"
size --format=SysV stack/ssap/ssap_codec_test | tee "$OUTDIR/ssap_codec_test.size.txt"
size --format=SysV "$OUTDIR/ssap_codec_test.stripped" | tee "$OUTDIR/ssap_codec_test.stripped.size.txt"

# 2) Host C — link with Map (triad third leg; RUST-WS73-LTO-EXTREME.md:344-345)
# Re-link one ELF with -Wl,-Map for Discarded input sections
cc -O2 -Wall -Wextra -Istack/ssap/include -Wl,-Map,"$OUTDIR/host.map" \
   -o "$OUTDIR/ssap_codec_test.map.elf" stack/ssap/test/test_codec.c stack/ssap/libssap.a
grep -E "^\.text|\.data|\.bss|Discarded input sections|Memory Configuration|Linker script" "$OUTDIR/host.map" | head -n 80 | tee "$OUTDIR/host.map.summary.txt"

# 3) Host C — extreme-perf trial (gated LTO=1 per RUST-WS73-LTO-EXTREME.md:195-200)
# Requires gcc-ar/llvm-ar when -flto active (RUST-WS73-LTO-EXTREME.md:190)
make -C stack/ssap clean
make -C stack/ssap LTO=1 all  # expected: CFLAGS+=-flto -ffunction-sections -fdata-sections -O2; LDFLAGS+=-flto -Wl,--gc-sections -Wl,-Map=*.map; AR=gcc-ar
size --format=SysV stack/ssap/libssap.a | tee "$OUTDIR/libssap.lto.size.sysv.txt"
nm --size-sort --print-size stack/ssap/libssap.a | head -n 50 | tee "$OUTDIR/libssap.lto.nm.txt"
diff -u "$OUTDIR/libssap.size.sysv.txt" "$OUTDIR/libssap.lto.size.sysv.txt" | tee "$OUTDIR/libssap.lto.diff.txt" || true
# assert: text down 8-15% or revert (RUST-WS73-LTO-EXTREME.md:379)

# 4) Host C — notify latency + 12Mbps throughput (requires /dev/hwsle or mock)
# ssap_server_notify path: find_property(:122) + ssap_encode_value(:153) + send_frame(:565) via hwsle_transport_send_acb(:51)
# Without hardware, loopback via mock send_frame that memcpy's to /dev/null and timestamps
./stack/ssap/ssap_server_test 2>&1 | tee "$OUTDIR/ssap_server_test.log" || true  # exercises CCCD-gated notify at test_server.c:221,231
# Synthetic throughput: 12 Mbps = 1.5 MB/s; frame = 5 + 6 + value_len; bench with value_len 20 and 1024
# (HITL with dongle: replace mock with real /dev/hwsle + hwsle_transport_run:98 poll 500ms vs epoll)
cat > "$OUTDIR/bench_notify.c" <<'C'
#include "ssap_server.h"
#include "ssap_codec.h"
#include <time.h>
// ... mock send_frame, loop ssap_server_notify(&srv, h, buf, 20/1024, 0) 10000 times, clock_gettime p50/p99
C
# Host Rust — cargo bench (once rust-ws73 workspace exists; RUST-WS73-LTO-EXTREME.md:215-242)
# cargo bench --manifest-path rust-ws73/Cargo.toml -- --output-format bencher | tee "$OUTDIR/cargo.bench.txt"
# cargo bloat --release --manifest-path rust-ws73/Cargo.toml | tee "$OUTDIR/cargo.bloat.txt"

# 5) Device — output/ws63-liteos-app.elf text/data/bss + flash 2M / SRAM 500K (HITL one-build gate)
if [ -f output/ws63-liteos-app.elf ]; then
  riscv32-linux-musl-size --format=SysV output/ws63-liteos-app.elf | tee "$OUTDIR/device.size.sysv.txt" 2>&1 || \
  riscv32-unknown-elf-size --format=SysV output/ws63-liteos-app.elf | tee "$OUTDIR/device.size.sysv.txt"
  riscv32-linux-musl-nm --size-sort --print-size output/ws63-liteos-app.elf | head -n 100 | tee "$OUTDIR/device.nm.txt" 2>&1 || true
  riscv32-linux-musl-objdump -h output/ws63-liteos-app.elf | grep -E "Idx|\.text|\.data|\.bss" | tee "$OUTDIR/device.objdump.txt" 2>&1 || true
  # Map: -Wl,-Map,device.map -T linker.prelds (if fbb_ws63 imported; RUST-WS73-LTO-EXTREME.md:264-268)
  grep -E "^\.text|\.data|\.bss|Discarded input sections|Memory Configuration|Linker script" device.map 2>&1 | head -n 80 | tee "$OUTDIR/device.map.summary.txt" || true
  # flash 2M / SRAM 500K gates (ticket) + memory_config.h:1 TCM/RAM ceilings
  python3 -c "import re,sys; txt=open('$OUTDIR/device.size.sysv.txt').read(); m=re.search(r'text\s+(\d+)',txt); print('text',m.group(1) if m else 'TOFILL')"
else
  echo "TOFILL: output/ws63-liteos-app.elf not built yet — build once (HITL) then re-run this script" | tee "$OUTDIR/device.TOFILL.txt"
  ls -lh sdk/ws73_sdk_linux_WS73_1.10.110/firmware/e/ws73.bin sdk/ws73_sdk_linux_WS73_1.10.110/firmware/e/wifi_cali.bin | tee "$OUTDIR/firmware.proxy.txt"
  size --format=SysV stack/ssap/libssap.a | tee "$OUTDIR/device.proxy.note.txt"
fi

# 6) Emit bench.json for CI regression (extreme-perf gate is PGO regression gate per ticket)
cat > "$OUTDIR/bench.json" <<JSON
{"host":{"libssap_text": "from $OUTDIR/libssap.size.sysv.txt", "wifi_soc_text": 966210, "notify_p50_ns": "from bench_notify", "throughput_mbps": "from bench_notify"},
 "device":{"elf_text": "from $OUTDIR/device.size.sysv.txt or TOFILL", "flash_budget": 2097152, "sram_budget": 512000}}
JSON
echo "bench done -> $OUTDIR"
```

### 6.2 `size`/`nm`/`Map` triad — exact commands the script runs (copy-paste)

| Tool | Invocation (Host C) | Invocation (Device, once `output/ws63-liteos-app.elf:1` exists) | What to assert |
|---|---|---|---|
| `size` | `size --format=SysV stack/ssap/libssap.a:1` / `size --format=SysV stack/ssap/ssap_codec_test:1` / `size stack/ssap/src/*.o:1` | `riscv32-linux-musl-size --format=SysV output/ws63-liteos-app.elf:1` (fallback `riscv32-unknown-elf-size`) | `text` down 8-15% for `libssap.a` after `LTO=1`; `wifi_soc.ko:1` text flat/up under `-Os:472` vs `-O2` |
| `nm` | `nm --print-size --size-sort stack/ssap/libssap.a:1 \| head -n 50` / `nm --size-sort stack/ssap/src/ssap_server.o:1 \| head` | `riscv32-linux-musl-nm --print-size --size-sort output/ws63-liteos-app.elf:1 \| head` | dead symbols gone (`fm_update:0x20`, `ssap_link_mark_activity:0x200` etc. when unused); `ssap_server_notify 0x112` retained |
| `Map` | `cc -Wl,-Map,host.map:1 -o host.elf ...` then `grep -E "^\.text\|\.data\|\.bss\|Discarded input sections" host.map:1` | `riscv32-linux-musl-gcc -Wl,-Map,device.map:1 -T linker.prelds:1 -o output/ws63-liteos-app.elf:1 ...` then same `grep` | `Discarded input sections` lists gc'd `.text.*`/`.data.*`; `Memory Configuration` confirms ROM/RAM addresses post-relax vs `memory_config.h:10-11,66-71` |

Baseline numbers already captured (Sections 2-3) are the **before** for any A/B; after an `LTO=1` trial diff with `diff -u <(size --format=SysV libssap.a.before) <(size --format=SysV libssap.a.after)` + `nm --size-sort` + `Map` discarded count (`RUST-WS73-LTO-EXTREME.md:365-368`).

---

## 7. PGO rejection rationale — from `RUST-WS73-LTO-EXTREME.md:307-333`

Verdict: **do NOT adopt PGO (`--pgo-generate` / `--pgo-use`) for this stack** — host C, host Rust, or Device. The LTO extreme-perf doc (`RUST-WS73-LTO-EXTREME.md:308-319`) gives four blockers that outweigh the typical 5-15% branch-prediction / block-layout win:

1. **No representative workload on the host.** PGO needs a `--pgo-generate` instrumented run over a realistic trace, then `--pgo-use` rebuild. The only "workload" for `libssap.a` is `hwsle_transport_run:98-142` ACB/ICB framing (`hwsle_transport.c:98`) and `ssap_link tick:250-295` / `on_event:137-248` (`stack/ssap/src/ssap_link.c:137,250`) — both are exercised by the 4 unit tests (`stack/ssap/test/test_codec.c:1` etc. inject events) but that is not a real USB bulk trace. For Device FW there is no host-side emulator (`RUST-WS73-LTO-EXTREME.md:312`); profiling would require QEMU `rv32imc` or on-device `gcov` over the air, neither of which exists in this repo (`WS73-WIFI-GAP-AUDIT.md:144` notes "`fbb_ws63` absent — do not spec a dependency").

2. **Kernel modules cannot PGO-profile.** `wifi_soc` / `plat_soc` / `ble_soc` / `sle_soc` are KOs (`obj-m` at `driver/wifi/Makefile:351`, `driver/platform/Makefile:253` etc.). Kbuild has no `KBUILD_CFLAGS += -fprofile-generate` path that survives `modpost`; even if forced, profile data (`*.gcda`) would be written to `debugfs` not to a file, and `llvm-profdata` for `WSCFG_USING_LLVM_CLANG:489` (`driver/wifi/Makefile:489`) uses a different format than gcc PGO. The `wifi_soc.o` combined object is 119 MB already — instrumentation would double it (`RUST-WS73-LTO-EXTREME.md:314`).

3. **Rust PGO is toolchain-orthogonal and CI-heavy.** `rustc -C profile-generate/-C profile-use` (Cargo `build.rustflags = ["-C", "profile-generate"]`) requires `llvm-profdata merge` and a two-pass CI. For the small `ssap` crate the PGO win is <2% (hot path is `memcpy` in `hwsle_transport.c:56-60` and `put_u16:24-29` at `stack/ssap/src/hwsle_transport.c:24`, not branchy control flow). The `panic=abort` + `codegen-units=1` + `lto=thin` combo in `RUST-WS73-LTO-EXTREME.md:215-242` already captures most of the layout win without the instrumentation tax (`RUST-WS73-LTO-EXTREME.md:316`).

4. **Device PGO is architecturally blocked by fixed ROM.** Even if `riscv32-linux-musl-gcc -fprofile-generate` were produced, the `interim_binary/ws63-liteos_rom.bin` fixed-ROM portion cannot be PGO-reordered (address frozen by `linker.prelds` / `rom_ram_check`). Only the RAM overlay could use PGO, so the end-to-end win is halved (`RUST-WS73-LTO-EXTREME.md:318`).

The ticket's "extreme档以此为 PGO 回归门" (extreme tier is the PGO regression gate) is therefore implemented as: extreme-perf **is** `LTO=thin->fat` + `codegen-units=1` + `strip=true` (Rust) and `-flto --gc-sections -O2` (Host C `stack/ssap`), **not** PGO. If PGO is still demanded for the host userspace daemon (`sparklinkd`), the minimal safe shape behind `PGO=1` two-pass (host x86_64 only, never KOs/Device) is given at `RUST-WS73-LTO-EXTREME.md:321-333` — gated off by default, CI must not require `.profraw` to build.

---

## 8. First-round baseline table (what to land after one `output/` build)

| Artifact | Today (no `output/`, no LTO) | How to fill | Extreme-perf target (regression gate) | Cite |
|---|---|---|---|---|
| `libssap.a` SysV text sum | 10 421 B (1117+865+6145+1785+509) (`RUST-WS73-LTO-EXTREME.md:102`) / 12 170 B via `size` (Section 2.3) | `size --format=SysV stack/ssap/libssap.a:1` | **9.0-9.8K** (-8 to -15%, ~1K) + `feature_mgr.o` droppable | `stack/ssap/Makefile:9` `-O2`, `:8` `ccache cc` |
| `libssap.a` file bytes | 24 576 B (24K) before and after `strip` (Section 2.4) | `ls -lh stack/ssap/libssap.a:1` + `strip` | unchanged (archive) — gate is SysV text, not file bytes | `stack/ssap/Makefile:17` `ar rcs` |
| `ssap_codec_test` text | 4 765 B (ELF, dynamically linked) (`RUST-WS73-LTO-EXTREME.md:108`) | `size stack/ssap/ssap_codec_test:1` | **4.3-4.6K** — inlines `ssap_trans_type_of:11` (`stack/ssap/src/ssap_codec.c:11`) | `stack/ssap/Makefile:20-21` |
| `wifi_soc.ko` text | 966 210 B (stripped KO, `size` dec 1 082 554) (`RUST-WS73-LTO-EXTREME.md:115`) | `size --format=SysV wifi_soc.ko:1` (once built; today `output/build` empty) | **977K-1.02M** if `-O2` replaces `-Os:472` (+1 to +6%) — size **up**, not down | `driver/wifi/Makefile:472` `-Os` |
| `plat_soc.ko` text | 259 295 B (dec 271 611) | `size plat_soc.ko:1` / `output/bin/plat_soc.ko:1` | flat | `driver/platform/Makefile:308` `-Os` |
| `ws73.bin` (EU) Device proxy | 137 644 B (135K) (`firmware/e/ws73.bin:1`) | `ls -lh firmware/e/ws73.bin:1` / `wc -c` | no change in this repo (opaque blob); if FBB RAM LTO: 124-131K (-5 to -10% RAM only) | `firmware/e/ws73.bin:1` |
| `output/ws63-liteos-app.elf` text / data / bss | **TOFILL** (no `output/ws63-liteos-app.elf:1` today) | `riscv32-*-size --format=SysV output/ws63-liteos-app.elf:1` after one HITL build | text+data < 2M flash, data+bss < 500K SRAM (ticket) + `memory_config.h:54-89` bss 61K ceiling | `memory_config.h:10-11,22,44-48,54-59,66-89,96-97` |
| SLE line-rate | 12 Mbps FS (boot) → 480 Mbps HS (kernel) — `docs/DEVICE-INTEL.md:12`, `docs/USB-PROTOCOL.md:105` | `hwsle_transport_send_acb:51` + `hwsle_transport_run:98` loopback bench | 12 Mbps sustained over `hwsle_transport_send_acb:51` (750 frames/s @ 2K buf) | `stack/ssap/src/hwsle_transport.c:51,98,102` |
| `ssap_server_notify` latency | CCCD-gated (`ssap_server.c:557-560`), p50/p99 via `clock_gettime` loop over `ssap_server_notify:549` → `ssap_encode_value:153` → `send_acb:51` | bench harness Section 6.1 `bench_notify.c` + `test_server.c:221,231` | p50 < 50 us (mock), p99 < 200 us (mock); HITL with `/dev/hwsle:1` adds `write` syscall jitter | `stack/ssap/src/ssap_server.c:549`, `stack/ssap/src/ssap_codec.c:153` |
| `fwpkg` | 1 391 436 B (`HHD01-firmware-violin-1.10.102.fwpkg:1` per `RUST-WS73-LTO-EXTREME.md:147`) | `ls -lh *.fwpkg:1` | unchanged (container format) | `RUST-WS73-LTO-EXTREME.md:288` |

---

## 9. Recommended build matrix (Host C / Host Rust / Device) — carries LTO decision forward

| Artifact | Default (`make` / `cargo build`) | Extreme-perf (`LTO=1` / `cargo build --release`) | CI gate |
|---|---|---|---|
| `stack/ssap` libssap.a + tests (Host C) | `CFLAGS=-O2 -Wall -Wextra -Iinclude -std=c11` (`stack/ssap/Makefile:9-10`), `CC?=ccache cc` (`:8`), no LTO | `CFLAGS+=-flto -ffunction-sections -fdata-sections -O2` + `LDFLAGS+=-flto -Wl,--gc-sections -Wl,-Map=*.map` + `AR=gcc-ar` — gated `LTO=1` (`RUST-WS73-LTO-EXTREME.md:195-200`) | `size` + `nm --size-sort` + `Map` triad; assert SysV text down >=8% or revert |
| `driver/wifi` / `platform` / `bsle/*` KOs | `ccflags-y=-fno-pic -Os -DDMAC_ON_HOST` (`driver/wifi/Makefile:472-473`) via Kbuild, no LTO | **no change** (keep `-Os`); if trial, add only `-ffunction-sections -fdata-sections` + `ldflags-y += --gc-sections` without `-flto` (`RUST-WS73-LTO-EXTREME.md:209`) | KO `size` unchanged ±2%; `insmod` smoke + `wifi_soc.ko` 2.5 MB ceiling |
| Host Rust (`hwsle-transport-sys` / `sm-fsm` / `ssap` / `sparklinkd`) | `lto="thin"` `codegen-units=1` `panic=abort` `strip=true` in `[profile.release]` | promote binary crate to `lto="fat"` only if `cargo bloat` proves >5% win (`RUST-WS73-LTO-EXTREME.md:215-242`) | `cargo bloat --release` + `nm --size-sort` + `strip` parity |
| Device `rv32imc` (if `fbb_ws63` imported) | RAM `-O2 -march=rv32imc -ffunction-sections -fdata-sections`, ROM `-fno-lto -Os`, link `-Wl,--gc-sections -Wl,--relax -T linker.prelds` | add `-flto` to **RAM only**; keep ROM `-fno-lto`; `rom_ram_check` must pass (`RUST-WS73-LTO-EXTREME.md:260-268`) | `riscv32-size` + `nm --size-sort` + `device.map` + `rom_ram_check` pass |
| PGO (`--pgo-generate/use`) | **off** everywhere | Host userspace only, `PGO=1` two-pass, never KOs/Device (Section 7) | `.profraw` not required to build; CI skips PGO unless `PGO=1` |

---

## 10. Repro commands (read-only, AFK-safe — no Device toolchain required today)

```bash
# Host C baseline (today)
wc -l sdk/ws73_sdk_linux_WS73_1.10.110/driver/wifi/Makefile   # 528 -> driver/wifi/Makefile:1
sed -n '467,474p' sdk/.../driver/wifi/Makefile                 # ccflags-y quote -> driver/wifi/Makefile:467
cat stack/ssap/Makefile                                          # 44 lines, CFLAGS -O2 -std=c11 -> stack/ssap/Makefile:1
size --format=SysV stack/ssap/libssap.a                        # libssap.a triad -> stack/ssap/Makefile:17, RUST-WS73-LTO-EXTREME.md:101
size stack/ssap/src/*.o                                        # per-.o text/data/bss -> stack/ssap/src/*.c:1
nm --size-sort --print-size stack/ssap/src/ssap_server.o | head  # ssap_server_notify 0x112 etc. -> stack/ssap/src/ssap_server.c:549
ls -lh stack/ssap/libssap.a && cp stack/ssap/libssap.a /tmp/a && strip --strip-unneeded /tmp/a && ls -lh /tmp/a  # strip gate -> stack/ssap/libssap.a:1
ls -l sdk/.../firmware/e/ws73.bin sdk/.../firmware/e/wifi_cali.bin  # Device blob proxy -> firmware/e/ws73.bin:1 137644
wc -c sdk/.../firmware/e/ws73.bin                              # 137644 -> firmware/e/ws73.bin:1
hexdump -C sdk/.../firmware/e/ws73.bin | head -n 2            # 64B sha256 header -> firmware/e/ws73.bin:0
grep -rn "gc-sections" sdk/.../application/sample --include="Makefile"  # userspace precedent -> application/sample/ble/ble_gatt_client/Makefile:24
find . -name "*.cmake" -o -name "*musl*" -o -name "*riscv*" | head  # fbb absent -> fbb_ws63/src/build/toolchains/riscv32_musl_100.cmake:1 (0 hits)
ls fbb_ws63 2>&1 | head                                         # No such file -> fbb_ws63/src/drivers/boards/ws63/evb/memory_config:1 (absent)
cat sdk/.../driver/platform/drv/device/romable/include/memory_config.h  # ITCM 432K etc. -> memory_config.h:10
ls -R sdk/ws73_sdk_linux_WS73_1.10.110/output 2>&1 | head      # output/ws63-liteos-app.elf missing -> output/ws63-liteos-app.elf:1 (TOFILL)
```

For `output/ws63-liteos-app.elf:1` after the HITL build, add `RUST-WS73-LTO-EXTREME.md:291-301`:

```bash
riscv32-linux-musl-size    --format=SysV output/ws63-liteos-app.elf # or riscv32-unknown-elf-size
riscv32-linux-musl-nm      --size-sort --print-size output/ws63-liteos-app.elf | head -n 100
riscv32-linux-musl-objdump -h output/ws63-liteos-app.elf | grep -E "Idx|\.text|\.data|\.bss"
# Map
riscv32-linux-musl-gcc -Wl,-Map,device.map -T linker.prelds -o output/ws63-liteos-app.elf ...
grep -E "^\.text|\.data|\.bss|Discarded input sections|Memory Configuration" device.map | head
```

---

## 11. Open questions for downstream tickets (no code, just edges)

1. Does the TV-box target actually import the `fbb_ws63` LiteOS ROM flow, or does `ws73.bin:1` remain an opaque blob? If the latter, close the Device `-flto` sub-question as "not applicable" and track it under a separate `fbb-ws63-rom` ticket (`RUST-WS73-LTO-EXTREME.md:407`).
2. If Device RAM LTO is trialled, who owns the `linker.prelds:1` `KEEP()` audit — Device firmware team or host driver team? Every `rom_ram_check` region must be audited before `--gc-sections` is enabled (`RUST-WS73-LTO-EXTREME.md:408`).
3. Rust workspace scaffold `rust-ws73/`: which crate is the `lto="fat"` candidate — `sparklinkd` binary or a `cdylib`? Library crates must not set `lto="fat"` independently (`RUST-WS73-LTO-EXTREME.md:409`).
4. PGO workload: can a realistic DLI/ACB/ICB bulk trace be captured from a running `/dev/hwsle:1` (`hwsle_transport.h:7` `HWSLE_DEV`) session for `--pgo-generate`, or is unit-test-only profiling acceptable? (Answer: need hardware trace — Section 7.1.)
5. KO size ceiling: is the `wifi_soc.ko:1` 2.5 MB file size a hard OTA ceiling? If so, `-Os` (`driver/wifi/Makefile:472`) must stay and any `-O2` experiment needs OTA sign-off (`RUST-WS73-LTO-EXTREME.md:411`).
6. `output/ws63-liteos-app.elf:1` HITL build: which `riscv32` toolchain (musl `riscv32_musl_100.cmake:1` vs `fp` variant) and which `linker.prelds:1` will the WS63 evb build use? Neither exists on this checkout today.

---

## 12. File:line index (every file touched, ticket-mandated)

- `sdk/ws73_sdk_linux_WS73_1.10.110/driver/wifi/Makefile:1` CUR_MK_PATH, `:28` KO_NAME wifi_soc, `:31` alg include, `:39-40` defconfigs, `:42` WIFI_ALG_MACRO_DEFINES, `:45-55` always HMAC core, `:62-66` WOW gate, `:106-218` feature gates, `:258` alg-objs, `:271-273` hal-rom-objs, `:312` TRNG_TRIG_RPT, `:351` obj-m, `:467-474` ccflags-y (`-fno-pic -Os -DDMAC_ON_HOST` + duplicate COPTS), `:487-493` GCC vs LLVM_CLANG branch, `:501-504` gcc KO build + strip, `:515-520` clang/lld KO build + llvm-strip, 528 lines total
- `sdk/ws73_sdk_linux_WS73_1.10.110/driver/platform/Makefile:299-346` ccflags-y (`:308 -Os`, `:345 -fno-pic`), `:26` guard, `:360-387` STRIP variants, `:252` sort, `:253` obj-m plat_soc
- `sdk/ws73_sdk_linux_WS73_1.10.110/driver/platform/drv/device/romable/include/memory_config.h:1` 99 lines: `:10-11` ITCM 432K, `:13-14` WOW 32K, `:18-19` BOOTROM 25K, `:21-22` FW_ROM_CODE 412K, `:24-29` FW_PLAT/WIFI/BGLE ROM split, `:37-41` PATCH_TABLE/CB 2K each, `:43-48` ROM data 1K/8K/8K, `:54-59` ROM bss 12K/32K/6K, `:61-62` DFR 1K, `:66-71` FW PLAT/WIFI/BGLE CODE 4K/12K/4K, `:77-82` DATA 1K/1K/1K, `:84-89` BSS 4K/6K/1K, `:91-97` PATCH_DATA 1K + WOW 32K
- `sdk/ws73_sdk_linux_WS73_1.10.110/driver/wifi/device/source/inc/romable/hal/ws73/spec/wlan_spec_rom.h:1` 248 lines: `:41` CHIP_MAX_NUM 1, `:90-92` ASSOC_USER_MAX 8
- `sdk/ws73_sdk_linux_WS73_1.10.110/driver/bsle/ble_driver/linux/Makefile:1-41` ble_soc `ccflags-y` lean, `:10` obj-m ble_soc
- `sdk/ws73_sdk_linux_WS73_1.10.110/driver/bsle/sle_driver/Makefile:1-74` sle_soc `ccflags-y` lean, `:63-74` dual GCC/Clang paths
- `sdk/ws73_sdk_linux_WS73_1.10.110/application/sample/ble/ble_gatt_client/Makefile:24` LDFLAGS gc-sections precedent; `ble_uuid_server/Makefile:23` idem; `sle_uuid/*:1` idem
- `sdk/ws73_sdk_linux_WS73_1.10.110/firmware/e/ws73.bin:1` 137644 B (135K), `firmware/us/ws73.bin:1` 143956 B, `firmware/e/wifi_cali.bin:1` 21060 B, `firmware/e/btc_cali.bin:1` 34K, `firmware/e/wow.bin:1` 23K
- `sdk/ws73_sdk_linux_WS73_1.10.110/output:1` `bin/autoconfig.h`, `bin/plat_soc.ko`, `bin/ws73_cfg.ini`, `build/` empty — no `output/ws63-liteos-app.elf:1` (TOFILL HITL)
- `sdk/ws73_sdk_linux_WS73_1.10.110/driver/platform/drv/device/romable/include/riscv_common.h:1` — only riscv-named file in tree (header, not toolchain)
- `fbb_ws63/src/drivers/boards/ws63/evb/memory_config:1` — absent on this checkout (`ls fbb_ws63 2>&1` = "No such file"); `fbb_ws63/src/build/toolchains/riscv32_musl_100.cmake:1` + `riscv32_musl_100_fp.cmake:1` — 0 hits
- `stack/ssap/Makefile:8` CC ccache, `:9-10` CFLAGS -O2 -std=c11 -Wall -Wextra, `:12-13` SRCS/OBJS, `:17-18` ar rcs, `:20-31` test builds, `:32-33` pattern rule, 44 lines, no LTO today
- `stack/ssap/src/hwsle_transport.c:1` 143 lines: `:24-29` put_u16, `:51-76` send_acb (`[0xA3][tcid][len][payload]`), `:73-76` send_ssap hardwired 0x0A, `:98-142` run (poll 500ms `:105`, buf 2048 `:102`, only 0x0A dispatched `:125-126`), `:128-137` EVENT heuristic
- `stack/ssap/src/ssap_codec.c:1` 217 lines: `:11-41` trans_type_of, `:43-48` put_u16/get_u16, `:55-94` exchange_info encode/decode, `:153-169` encode_value (value hdr 6 + memcpy), `:185-206` encode_write_rsp
- `stack/ssap/src/ssap_server.c:1` 582 lines: `:12-19` init (mtu `SSAP_MTU_DEFAULT` 251, version 1.3), `:22-48` add_service (SERVICE_CHANGE 0x000E), `:91-119` add_property (auto-CCCD `:112-115`), `:122-132` find_property, `:134-145` find_by_cccd, `:163-547` dispatch (EXCHANGE_INFO `:172`, FIND `:188-281`, READ `:335-433`, WRITE `:435-510`, CALL_METHOD `:511-543`), `:549-567` ssap_server_notify (CCCD gating `:557-560`, encode `:563-565`, send_frame `:566`), `:569-581` apply_config
- `stack/ssap/include/ssap_server.h:1` 118 lines: `:16-21` caps (HANDLE_START 0x0001, MAX_PROPERTIES 32, MAX_VALUE_LEN 1024), `:67-70` CCCD at handle+1, `:83-91` ssap_server_t (services[8], service_count, next_handle, mtu, version, connected, send_frame), `:94-114` API, `:113-114` ssap_server_notify signature
- `stack/ssap/include/hwsle_transport.h:1` 50 lines: `:7` HWSLE_DEV `/dev/hwsle`, `:21-28` HCI_DATATYPE_ACB 0xA3 / ICB 0xA4 / TCID 0x0A/0x1F/0x02 / CMD 0xA1 / EVENT 0xA2
- `stack/ssap/src/ssap_link.c:1` 295 lines: `:49-67` 23 B connect blob, `:137-248` on_event, `:250-295` tick
- `stack/ssap/src/feature_mgr.c:1` 121 lines: `:6-17` g_feat_cost, `:20-89` fm_update trims
- `stack/ssap/test/test_server.c:1` `:221` notify blocked when CCCD off, `:231` notify sent when CCCD on
- `docs/DEVICE-INTEL.md:12` Speed 12 Mbps Full Speed; `:11` idVendor/product `ffff:3733`
- `docs/USB-PROTOCOL.md:1` 118 lines: `:11` DEVICE_BOOT_EP_NUM 2, `:33-39` EP layout 2->5, `:44-46` vendor/product 0xFFFF/0x3733, `:50-55` USB_RX_MAX_SIZE 20K / TX 20K / URB counts, `:58-60` MAX_FW 32K/128K/200K, `:104-105` boot 12M -> kernel 480M re-enum
- `README.md:33` SLE 12Mbps/250us/256 users; `README.en.md:33` English mirror
- `.scratch/nearlink-driver/lab-notes/WS73-WIFI-GAP-AUDIT.md:50` always-on COPTS, `:54` commented COPTS, `:69-73` WOW drop for TV box, `:90` btc_cali 34K, `:144` fbb_ws63 absent
- `.scratch/nearlink-driver/lab-notes/WS73-BT-GAP-AUDIT.md:116` gatts_notify_indicate via ble_uuid_server, `:118` SSAP vs GATT map
- `.scratch/nearlink-driver/lab-notes/RUST-WS73-LTO-EXTREME.md:1` 442 lines: `:49` negatives (no flto/gc), `:68-73` gc-sections precedent, `:97` no LTO in stack/ssap, `:101-108` libssap SysV sizes, `:111` 26 T symbols, `:115-118` KO baselines, `:122` llvm_addrsig bloat, `:132` no prelds/liteos_rom on WS73, `:138-147` firmware blob table, `:187-200` Host C extreme-perf patch, `:215-242` Rust profile, `:281-286` size projections, `:291-301` size/nm/Map commands, `:307-333` PGO rejection 4 blockers, `:339-372` metrics triad, `:407-411` open questions
- Tickets: `.scratch/rust-ws73-tri-mode/issues/06-perf-baseline-extreme.md:1` (this task), `:01-05` tri-mode wayfinder

---

*English-only per `AGENTS.md` / `docs/agents/domain.md`. Whitelist `.gitignore` respected — no binaries committed. Read-only per task STRICTION — no network/build/hardware was used beyond `size`/`nm`/`ls`/`find`/`strip` on existing on-disk artifacts; `output/ws63-liteos-app.elf:1` numbers remain TOFILL until one HITL build lands.*
