# NearLink / SparkLink / SLE — Assembly, Compiler, Linker Optimization Resources

> Ticket: new harvest — public network research on NearLink / SparkLink / SLE / WS63 / WS73 assembly, compiler, linker, and runtime optimization resources. Ask: can these be organized into a reusable general optimization instruction set.
> Date: 2026-09-12. Scope: read-only public network research + local program/source evidence only. No builds, no hardware, no PCB. English-only per `AGENTS.md`. Every factual claim cites `path:line` (local) or a source repository/URL (network).
> Strict: exactly one new report file. This file only. Local reports cited are linked, not duplicated.

---

## 0. Sources checked

| Source | Type | Found | Verdict |
|---|---|---|---|
| Local SDK + stack sources (`sdk/ws73_sdk_linux_WS73_1.10.110/`, `stack/ssap/`, `driver/...`) | local on-disk | yes | load-bearing assembly/inline/build evidence |
| Local lab notes: `RUST-WS73-LTO-EXTREME.md`, `PERF-RUST-PATTERNS.md`, `PERF-WIFI-THROUGHPUT.md`, `SLE-MEASURE-QOS.md` | local on-disk | yes | reuse as cross-reference, do not duplicate |
| `hispark-rs/bs2x-svd` | GitHub repo | yes (API metadata 2026-07-08) | BS21/BS2X CMSIS-SVD + svd2rust; primary toolchain source reference for Rust-rv32m/svpbind wiring |
| `hispark-rs/riscv32-ws63-gcc730-toolchain` | GitHub repo | yes (API metadata 2026-06-11) | riscv32 ws63 GCC 7.3.0 toolchain script; target-ABI / -march / -mabi evidence |
| `hispark-rs/hisi-rf` | GitHub repo | search-hit; API connection reset, not fetched fully | chip-neutral Wi-Fi/BLE/SLE Rust facade for HiSilicon SoC; treat as live reference but treat metadata fetch as partial |
| `OpenSparklink/nearlink_sdr_sim` | GitHub repo | yes (API metadata 2026-09-10, most recent) | SDR simulator for NearLink devices; external test/bench harness for line-rate measurement |
| `Hny0305Lin/NLChat` | GitHub repo | yes (search hit + local mirror) | NLChat SLE_UART Android app for Hi2821/Hi3863/WS63; program-side SLE framing/transport evidence |
| `Hny0305Lin/Hihope_WS63_NearLink_SDK` | GitHub repo | yes (API metadata 2026-08-28) | HiHope WS63 code pointing at canonical gitee.com/HiSpark/fbb_ws63; external fbb_ws63 source |
| `NearLink-ePaper/NearLink-Mesh-ePaper` | GitHub repo | yes (API metadata 2026-09-04) | H3863 SLE Mesh PoC (C), AODV routing + AIMD flow control; mesh runtime performance/overhead evidence |
| `Sgguo-Development-Team/nearlink-toolbox-docs` | GitHub repo | search-hit | NearLink Toolbox docs; marketing-to-tooling boundary reference |
| `hispark-rs/fbb_bs2x-qemu` | GitHub repo | yes (API metadata 2026-06-11) | QEMU-runnable BS2x BT/SLE trimmed SDK; bare-metal RISC-V runtime optimization context |
| `hispark-rs/bs2x-guide` | GitHub repo | yes (API metadata 2026-07-13) | BS21/BS2X (BLE 5.4 + SLE/NearLink) SoC user guide; SLE/SPARK protocol behavior guidance |
| `iMiracle/sparklink-docs` / `iMiracle/sparklink-web` | GitHub repos | yes (API metadata) | sparklink docs + TypeScript web; external SparkLink reference material |
| `Yarchmage/SparkLink-mmW`, `kkoblog/sparklink_` | GitHub repos | yes (API metadata) | SparkLink mmW / web; adjacent naming collision and marketing material (not core stack sources) |

Network access note: GitHub REST API was used for metadata verification with per-request delay; full repository file dumps were not pulled except as already-cited local lab notes. Google/DuckDuckGo scraping returned JS-wrapped pages not usable for content extraction, so the candidate list above was formed from GitHub search API and repository metadata only.

---

## 1. Assembler and inline control patterns (direct NearLink evidence + generic GCC/Clang/LLVM)

### 1.1 Direct NearLink inline/attribute evidence

NearLink device/SoC headers establish inline and register-access idioms that must not be removed for "simplification" (this is what keeps the SLE 12 Mbps path and SLE PHY register pokes performant).

- `driver/platform/drv/device/romable/include/hi_types.h:143-144` — `HI_ALWAYS_INLINE __attribute__((always_inline)) inline`, `HI_ALWAYS_STAIC_INLINE __attribute__((always_inline)) static inline`. These are the SoC register-type accessor macros used by wifi/ble/sle KOs.
- `driver/platform/drv/device/romable/include/td_base.h:77-78` — `TD_ALWAYS_INLINE __attribute__((always_inline)) inline`, `TD_ALWAYS_STAIC_INLINE __attribute__((always_inline)) static inline`. Companion idiom for `td_u32`/`td_s32` register access.
- `driver/platform/osal/include/osal_common.h:129` — `__IRQ __attribute__((interrupt("IRQ")))` and `__INLINE __inline__ __attribute__((always_inline))`. `always_inline` is used for interrupt context helpers that the SLE/USB transaction path may call from IRQ.
- `driver/platform/osal/include/osal_common.h:172` — `volatile int counter;` global state exposed through `volatile` to avoid optimization that would discard the cross-thread counter. This is the kind of `volatile` gate that must survive even `-O3`.

Local stack wiring (no `always_inline` on hot pure helpers to avoid bloat, but the pattern exists):

- `stack/ssap/src/ssap_codec.c:43-53` — `put_u16` / `get_u16` are the per-PDU field primitives (two insns each). They are deliberately kept as direct shifts with no `transmute`/packed-struct cast, preserving byte-wise correctness over the wire (`stack/ssap/include/ssap_pkt.h:180-588` defines `__attribute__((packed))` PDU ground truth).
- `stack/ssap/include/ssap_pkt.h:186-588` — authoritative packed PDU ground truth. Inline control on these structs is **not** the optimization lever; the levers are the C toolchain and linker section handling.

### 1.2 Generic GCC / Clang / LLVM inline and attribute guidance (transferable)

For any compiler stack shared by NearLink/SLE/SparkLink code (host x86 userspace, riscv32/aarch64 device, and Rust), the following are generic but apply:

- `inline` vs `inline(always)`: use `always` only for tiny pure helpers (e.g., `put_u16`/`get_u16`), because the linker cannot guess the call-site count. Over-annotating `always` on larger encode/decode helpers (10 `encode_*` path in `stack/ssap/src/ssap_codec.c:55-217`) bloats `.text` and defeats ThinLTO size heuristics (`stack/ssap/src/ssap_server.c:162-547` dispatch path).
- `packed` structs: treat them as wire-ground-truth, never as an optimization target. Casting packed `SSAP_Pdu*` fields to unaligned Rust pointers should be avoided; safe byte-wise codecs (shifts, `&[u8]` borrows) are preferred.
- `volatile` / register access macros: keep explicit `volatile` on globals and register accessors (e.g., `hi_reg_read*`, `osal_reg_read*`) because optimization can otherwise discard the cross-thread/intrusive state.
- `restrict`: where the current code does not already use it, add it only on loop-carried pointer/array accesses with the alignment invariant verified, to shrink ABI metadata and improve downstream inlining.

---

## 2. Compiler optimization levels and flags (NearLink evidence + transferable)

### 2.1 Current NearLink build level (baseline)

**Host userspace (`stack/ssap`):**

`stack/ssap/Makefile:8-10` verbatim:

```make
CC      ?= $(shell which ccache >/dev/null 2>&1 && echo "ccache cc" || echo cc)
CFLAGS  ?= -O2 -Wall -Wextra -Iinclude
CFLAGS  += -std=c11
```

No LTO, no `-ffunction-sections`, no `-fdata-sections`, no `-Wl,--gc-sections`, no `-fvisibility=hidden` at this level (`RUST-WS73-LTO-EXTREME.md:97`).

**Device/SoC KOs:**

`sdk/ws73_sdk_linux_WS73_1.10.110/driver/wifi/Makefile:467-474` verbatim:

```make
ccflags-y = $(_INCLUDES) $(COPTS)
ccflags-y += -isystem /usr/lib/llvm-21/lib/clang/21/include
ccflags-y += $(COPTS)
ccflags-y += $(COMM_COPTS)
ccflags-y += -fno-pic -Os
ccflags-y += -DDMAC_ON_HOST
MODFLAGS = -fno-pic
```

Same pattern in `sdk/ws73_sdk_linux_WS73_1.10.110/driver/platform/Makefile:299-346` (`-Os` at `:308`, `-fno-pic` at `:345`). The only optimization lever on KOs today is `-Os` plus `-DDMAC_ON_HOST`; no `-flto`, no gc-sections for KOs (`RUST-WS73-LTO-EXTREME.md:205-211`).

**Sample/runtime apps:** `application/sample/sle/sle_uuid/...` and `application/sample/ble/...` use `CFLAGS += -Wall -Os -std=gnu99` (e.g., `application/sample/sle/sle_uuid/sle_uuid_server/Makefile:21`); these are the user-space precedent that makes `--gc-sections` and stripping safe before trial on KOs.

### 2.2 Recommended flags with verified NearLink gates

**Host userspace (safely exercisable):**

```make
# Host userspace LTO gate (RUST-WS73-LTO-EXTREME.md:195-203)
ifeq ($(LTO),1)
CFLAGS += -flto -ffunction-sections -fdata-sections -O2
LDFLAGS += -flto -Wl,--gc-sections -Wl,-Map=$(@:.a=.map)
AR      := $(shell $(CC) -print-prog-name=gcc-ar 2>/dev/null || echo ar)
endif
```

- `-flto` + `-ffunction-sections` + `-fdata-sections` + `-O2` for host userspace.
- `-Wl,--gc-sections` + `-Wl,-Map=$(@:.a=.map)` at link time.
- Use `gcc-ar` (`AR := $(CC) -print-prog-name=gcc-ar`) when LTO archives must be produced for `-flto`.

**Device/SoC KOs (caution-gated):**

Do **not** blanket `-flto` on KOs. Kbuild partial linking, `.modinfo`, `KEEP()` linker anchors, and OOM risk make LTO on `driver/wifi`/`driver/platform`/`driver/bsle` KOs unsafe without measured validation. The safe subset if a KO size trial is requested is only `-ffunction-sections -fdata-sections` plus `ldflags-y += --gc-sections` without `-flto`, gated behind `WSCFG_EXTRA_CFLAGS` (`sdk/ws73_sdk_linux_WS73_1.10.110/driver/wifi/Makefile:484`), and validated with `nm --size-sort` + `grep -E "Discarded|KEEP"` to ensure `WOW`/`TWT`/`BTCOEX` weakrefs are not dropped.

**Rust release profile (for `rust-ws73` daemon/workspace):**

```toml
[profile.release]
lto = "thin"            # start thin; promote "fat" only for the final binary
codegen-units = 1
panic = "abort"
strip = true
overflow-checks = false
```

- `lto="thin"` → `"fat"` only for the final binary crate (`sparklinkd`), not per library crate (`RUST-WS73-LTO-EXTREME.md:224,242`).
- `codegen-units=1` for release, `codegen-units=16` only for `dev`/`test` so `cargo test` stays fast.
- `panic="abort"` for daemons with no unwind recovery; audit `grep -r catch_unwind` before flipping.
- `strip=true`; for debuggability use `strip="debuginfo"` + `split-debuginfo="packed"` and ship `*.dwp`.

**Target-specific ABI flags:**

- RISC-V device builds must set `-march` and `-mabi` to match the SoC (Hi3863/Hi2821). `hispark-rs/riscv32-ws63-gcc730-toolchain` is the referenced GCC 7.3.0 toolchain source for this class of target (metadata 2026-06-11).
- Host builds using the clang/LLVM isystem (`-isystem /usr/lib/llvm-21/lib/clang/21/include`) must keep toolchain parity with the one already present in the checkout (`sdk/ws73_sdk_linux_WS73_1.10.110/driver/wifi/Makefile:468`, `driver/platform/Makefile:300`).

---

## 3. Linker behavior, section handling, and KBuild/LTO boundaries

### 3.1 Section handling and `KEEP()` anchors

- `stack/ssap/include/ssap_pkt.h:186-588` defines `__attribute__((packed))` PDUs; CKEK that the linker section model respects the packed layout and never reorders these into a non-packed section that would change wire bytes.
- Device KOs keep `KEEP()` anchors for metadata that LTO cannot drop. `RUST-WS73-LTO-EXTREME.md:302-303` documents `KEEP()` on `WOW`/`TWT`/`BTCOEX` weakrefs that must not be GC'd by `--gc-sections`.
- The current checkout has **no** `*.gcno` files, so `DBG_COVERAGE=y` PGO data is absent; this is a structural blocker, not a toolchain defect (`RUST-WS73-LTO-EXTREME.md:309`).

### 3.2 LTO vs Kbuild `ld -r` boundaries

- KOs are linked by Kbuild (`Makefile:501-503` gcc path / `:517-519` clang/lld path), not a hand-written `ld`. Injecting `-flto` / `--gc-sections` into KOs must flow through `ccflags-y` / `LOCAL_WSCFG_EXTRA_CFLAGS` / `KBUILD_CFLAGS`.
- Host userspace uses standard make/`gcc-ar` LTO. For NearLink FW flows that import a fixed ROM, `RUST-WS73-LTO-EXTREME.md:132,247` states `-fno-lto` on fixed ROM objects with linker-script `KEEP()` anchors preserved, and `rom_ram_check` retained.

### 3.3 Metrics triad — prove any optimization win

Any candidate flag set must be validated with the three-command triad (from `RUST-WS73-LTO-EXTREME.md:339-372` and `RUST-WS73-PERF-BASELINE.md:423-431`):

| Tool | Command | What to assert |
|---|---|---|
| `size` | `size --format=SysV target/release/sparklinkd` / `size stack/ssap/libssap.a` | `text` down 8-15% for codec-heavy binary after host LTO; `text` flat for `no_std` leaves |
| `nm` | `nm --print-size --size-sort target/release/sparklinkd \| head -n 50` | dead symbols removed; live symbols retained (`ssap_encode_value`) |
| `Map` | `cargo rustc --release -- -Wl,-Map,device.map`, then `grep -E "^\.text\|\.data\|\.bss\|Discarded input sections" device.map` | `Discarded input sections` lists GC'd `.text.*`/`.data.*`; Memory Configuration confirms layout |

Host C parity: `size --format=SysV stack/ssap/libssap.a`, `nm --size-sort stack/ssap/libssap.a | head`, `cc -Wl,-Map,host.map` then `grep "^\\.text"`.

For NearLink device side (once `output/ws63-liteos-app.elf` exists): `riscv32-linux-musl-size --format=SysV output/ws63-liteos-app.elf` + `riscv32-linux-musl-nm --size-sort` + `riscv32-linux-musl-objdump -h | grep -E "Idx|\.text|\.data|\.bss"`.

---

## 4. Target ABI, inline conventions, and register/asm hygiene

### 4.1 NearLink SoC register idioms (must stay when optimizing)

- `hi_types.h:143-144` `HI_ALWAYS_INLINE` / `HI_ALWAYS_STAIC_INLINE` — always-inline register accessors used by SLE/WiFi KOs; these are the SLE PHY and HCI register pokes that determine 12 Mbps line-rate behavior.
- `td_base.h:77-78` `TD_ALWAYS_INLINE` — same class for `td_u32`/`td_s32` register access.
- `osal_common.h:129` `__IRQ` `interrupt("IRQ")` + `__INLINE always_inline` — interrupt-context helpers; optimizing away `always_inline` on IRQ helpers can break SLE/USB transaction scheduling.
- `osal_common.h:172` `volatile int counter;` — cross-thread/interrupt state; `volatile` is a load-bearing gate for SLE credit/connection counters.

### 4.2 Register access pattern hygiene

- Use `restrict` on loop-carried array/pointer access only after alignment invariant verification.
- Keep explicit `volatile` on register accessors and cross-thread counters.
- Do not cast packed `SSAP_Pdu*` wire structs to unaligned Rust pointers; keep byte-wise C codecs.
- For device asm (`asm volatile` CSRs, `csrr`/`csrw`/`csrs`), keep `fence`/`memory` barriers (`sdk/ws73_sdk_linux_WS73_1.10.110/driver/platform/drv/device/romable/include/soc_riscv_regs.h:21-81`) so optimization does not discard the ordering that SLE PHY/DMAC depend on.

### 4.3 Codegen units and inline distribution (Rust/host)

- `codegen-units=1` release is required for cross-crate inline with LTO; `codegen-units=16` only for dev/test (`RUST-WS73-LTO-EXTREME.md:236`).
- Hot pure helpers (`put_u16`/`get_u16`, opcode/trans_type classify) get `#[inline(always)]`; larger encode/decode helpers get `#[inline]`, not `always`, to avoid bloating `.text` (`PERF-RUST-PATTERNS.md:81-102`).
- `no_std` leaves (`ssap-codec`, `feature-mgr`) are constructed to be alloc-free and pure byte transforms (`ssap_codec.c:43-53`), enabling them as the miri coverage pilot and the first target of safe Rust LTO (`PERF-RUST-PATTERNS.md:231-269`).

---

## 5. SLE / NearLink runtime performance context (for flag selection)

### 5.1 SLE line-rate and framing

- SLE 12 Mbps / 250 µs / 256 users is standard (`README.md:33`, `docs/DEVICE-INTEL.md:12`, `SLE-MEASURE-QOS.md:1`). The host C data plane must sustain 1.5 MB/s, so compiler-level optimization is a real (if secondary) lever next to link/frame budget.
- `hwsle_transport.c:51-76` `send_acb` builds `[0xA3][tcid u16 LE][len u16 LE][payload]` then two `write()` calls. The split header+payload `write` is a latency-relevant SLE host path; Rust/host C must fence it with `write_all`/`writev`, not bare `write` (risk noted in `RUST-WS73-UNSAFE-FFI.md:251`).
- `hwsle_transport.c:98-142` `run` poll loop is `poll 500ms` with `read(buf 2048)`; 500 ms RX jitter is measurable for SLE line-rate and must be part of any throughput measurement gate.

### 5.2 ACB credit gating

- The SLE flow-control pattern is `sle_flow_ctrl_flag() > 0` before send (`SLE-MEASURE-QOS.md:1-40`). The current `hwsle_transport.c` is fire-and-forget with no credit check; adding a pending-frame counter + `MAX_INFLIGHT` cap in `hwsle_transport_send_acb()` is a minimal host-side gate that optimization and correctness gates together.

### 5.3 USB throughput context

- USB boot is FS (64 B bulk, 12 Mbps) while kernel re-enumerates to HS (512 B bulk, 480 M); FS is a throughput killer (`docs/USB-PROTOCOL.md:33,105`, `docs/DEVICE-INTEL.md:12`). HCC USB framing adds 92 bytes per 24-SDU scatter header (`sdk/ws73_sdk_linux_WS73_1.10.110/driver/platform/hcc/comm/hcc_bus_usb_comm.h:17-19`). `DMAC_ON_HOST` (`-DDMAC_ON_HOST`) moves descriptor DMA to host and is the throughput lever; pinning the HCC thread + stripping debug locks is part of the same optimization set (`PERF-WIFI-THROUGHPUT.md:2-8`).

### 5.4 Coex / single-RF scheduling

- Tri-mode coex requires BTCOEX-on and single-NSS 40 MHz WiFi (`PERF-WIFI-THROUGHPUT.md:5-8`). `feature_mgr` FEAT bits and `mdc_twt`/`mdc_csa`/`mdc_wow` feature gates exist for compile-time tuning; the interaction of TWT/CSA/BTCOEX pauses in `hal_device_fsm.c` must be measured, not assumed offline.

---

## 6. Reusable general optimization instruction set (通用优化指令集)

The above source material, filtered for transferability, becomes the reusable instruction set. It is written as a general rule set because it applies to NearLink/SparkLink/SLE host stacks and shared RISC-V/aarch64 device toolchains, not to NearLink-specific internals only.

### 6.1 Compiler baseline and tiers

1. **Current baseline is accepted as the default** — host userspace uses `-O2 -Wall -Wextra -std=c11` with `ccache`; device KOs use `-Os` + `-DDMAC_ON_HOST` on riscv32/aarch64. Do not silently raise KOs above `-Os` without measured impact.
2. **Host userspace LTO tier** is the first safe gain: `-flto -ffunction-sections -fdata-sections -O2`, `-Wl,--gc-sections -Wl,-Map=...`, `gcc-ar` for archives. Gate behind a local `LTO=1` variable.
3. **Kernel/module KOs stay non-LTO** by default; only `-ffunction-sections/-fdata-sections` + `--gc-sections` are trial candidates behind a config flag, validated by `nm --size-sort` and KEEP() consistency.
4. **Rust release tier** uses `lto="thin"` (promote `"fat"` only for final binary), `codegen-units=1`, `panic="abort"`, `strip=true`. `dev`/`test` keep `codegen-units=16`.
5. **Do not use PGO** until representative traces and kernel-module profiling are available; current checkout has no `.gcno` data.

### 6.2 Inline and section control

6. **`always` only for tiny pure helpers** (e.g., 2-insn LE store/load); larger encode/decode helpers use `inline`.
7. **Pack/transmute hygiene** — packed wire structs are ground truth; avoid packed-struct transmutations to unaligned Rust pointers; use byte-wise `&[u8]` borrows and safe LE codecs.
8. **`volatile` and register macros** must not be stripped by optimization; keep `volatile` on cross-thread/interrupt counters and register accessors.
9. **`no_std` leaves** (`codec`, `feature-mgr`) are alloc-free pure byte transforms — make them the first target of safe Rust LTO and miri coverage.
10. **Section GC must not drop SLE/WiFi metadata** — `KEEP()` anchors on `WOW`/`TWT`/`BTCOEX` weakrefs must survive `--gc-sections`.
11. **Metrics gate before any gain** — use `size`, `nm --print-size --size-sort`, and `-Wl,-Map` triad; require text saving / dead-symbol elimination, not bytes alone.

### 6.3 Target and boundary discipline

12. **Set `-march`/`-mabi` for target SoC** (Hi3863/Hi2821 class) and keep host clang/LLVM isystem parity with checkout.
13. **Device ROM vs RAM separation** — fixed ROM objects get `-fno-lto`; RAM objects can use LTO only if `rom_ram_check` is retained and validated.
14. **Kernel-module partial linking** — LTO/gc-sections must flow through Kbuild variables (`ccflags-y`/`LOCAL_WSCFG_EXTRA_CFLAGS`/`KBUILD_CFLAGS`) and never be hand-injected.
15. **Fire-and-forget SLE/USB paths must be gated** — `hwsle_transport_send_acb` split `write` must become `write_all`/`writev`; add pending-frame + `MAX_INFLIGHT` credit gate; keep 500 ms poll jitter measured in throughput tests.
16. **Compilation discipline** — always `-j1` for nearLink builds; run `free` check before each compile; use ccache; wait for load ≤ 10%.

### 6.4 Measurement and acceptance

17. **Acceptance metrics**: host userspace `libssap.a` text target 9.0-9.8K after LTO (`10421 B` baseline → `RUST-WS73-PERF-BASELINE.md:135`); device device-FW `text+data` against flash 2M / SRAM 500K envelope once `output/ws63-liteos-app.elf` exists.
18. **Online test harnesses** (external, not required to modify): `OpenSparklink/nearlink_sdr_sim` and `Hny0305Lin/NLChat` repos provide SLE line-rate and SLE_UART transport harnesses useful for validating optimization-gated throughput, but their use is read-only reference, not a build dependency.

---

## 7. Unverified projections and open assumptions

- **Absolute throughput claims** for WiFi/coex in NearLink device firmware are projections, not measured values in this checkout. The only measured device number available is `wifi_soc.ko` text 966210 B under `-Os` (`RUST-WS73-PERF-BASELINE.md:63`), and the subsequent `-O2` delta is documented as projected up, not measured.
- **PGO benefit is unverified** and currently rejected for this stack; any PGO experiment must first produce `.gcno` and kernel-module profiling data.
- **Network source metadata fetch** (`hispark-rs/hisi-rf`) was incomplete due to a connection reset; treat its references as search-hit citations, not fully verified API metadata.
- **Bare-metal Device FW rebuild** (fixed ROM with `-fno-lto` vs RAM LTO split) is not applicable to the WS73 Linux checkout because the Device FW is an opaque blob in `firmware/e/ws73.bin` (`RUST-WS73-LTO-EXTREME.md:132`).

---

## 8. Source inventory

Local (all absolute under `/home/archivalera/plum/zcode-projects/nearlink`):

- `stack/ssap/Makefile:1-13` — host userspace `CFLAGS -O2`, `ar rcs`, no LTO.
- `stack/ssap/include/ssap_pkt.h:186-588` — packed PDU ground truth.
- `stack/ssap/src/ssap_codec.c:43-53` — `put_u16`/`get_u16` LE helpers; `:55-217` encode helpers.
- `stack/ssap/src/ssap_server.c:162-547` — hot `dispatch`; `:549-567` notify path.
- `stack/ssap/src/hwsle_transport.c:21-143` — `g_fd`/`g_recv_cb`, `send_acb:51-76`, `run:98-142`, split `write`.
- `stack/ssap/src/ssap_link.c:23-28`, `:250-295` — LE primitives, `tick`.
- `sdk/ws73_sdk_linux_WS73_1.10.110/driver/wifi/Makefile:467-474` — KOs `ccflags-y -fno-pic -Os -DDMAC_ON_HOST`; `:484` WSCFG_EXTRA_CFLAGS.
- `sdk/ws73_sdk_linux_WS73_1.10.110/driver/platform/Makefile:308,345` — `-Os`/`-fno-pic`.
- `sdk/ws73_sdk_linux_WS73_1.10.110/driver/platform/drv/device/romable/include/hi_types.h:143-144`, `:53,249-277`.
- `sdk/ws73_sdk_linux_WS73_1.10.110/driver/platform/drv/device/romable/include/td_base.h:77-78`, `:183-214`.
- `sdk/ws73_sdk_linux_WS73_1.10.110/driver/platform/osal/include/osal_common.h:129`, `:172`.
- `sdk/ws73_sdk_linux_WS73_1.10.110/driver/platform/drv/device/romable/include/soc_riscv_regs.h:21-81` — CSRs `asm volatile`.
- `sdk/ws73_sdk_linux_WS73_1.10.110/driver/platform/hcc/comm/hcc_bus_usb_comm.h:17-19` — 92-byte scatter header.
- `sdk/ws73_sdk_linux_WS73_1.10.110/driver/wifi/device/source/inc/romable/memory_config.h:1` — RAM/ROM budget (`RUST-WS73-PERF-BASELINE.md:192-208`).

External network sources (metadata-verified via GitHub API; full dumps not pulled in this read-only task):

- `hispark-rs/bs2x-svd` — CMSIS-SVD + svd2rust for BS21/BS2X.
- `hispark-rs/riscv32-ws63-gcc730-toolchain` — riscv32 ws63 GCC 7.3.0 toolchain.
- `hispark-rs/fbb_bs2x-qemu` — QEMU-runnable BS2x BT/SLE trimmed SDK fork.
- `hispark-rs/bs2x-guide` — BS21/BS2X user guide (BLE 5.4 + SLE/NearLink).
- `hispark-rs/hisi-rf` — chip-neutral HiSilicon radio facade (search-hit, metadata fetch partial).
- `OpenSparklink/nearlink_sdr_sim` — NearLink SDR simulator.
- `Hny0305Lin/NLChat` — NLChat SLE_UART Android app (Hi2821/Hi3863/WS63).
- `Hny0305Lin/Hihope_WS63_NearLink_SDK` — HiHope WS63 code → `gitee.com/HiSpark/fbb_ws63`.
- `NearLink-ePaper/NearLink-Mesh-ePaper` — H3863 SLE Mesh PoC (C), AODV + AIMD.
- `Sgguo-Development-Team/nearlink-toolbox-docs` — NearLink Toolbox docs.
- `iMiracle/sparklink-docs` / `iMiracle/sparklink-web` — SparkLink docs + TypeScript web.
- `Yarchmage/SparkLink-mmW`, `kkoblog/sparklink_` — adjacent SparkLink material (naming collision / marketing, not core stack sources).

Existing local reports this report links (not duplicated):

- `.scratch/nearlink-driver/lab-notes/PERF-RUST-PATTERNS.md` — inline/zero-copy codec patterns.
- `.scratch/nearlink-driver/lab-notes/RUST-WS73-LTO-EXTREME.md` — LTO decisions, metrics triad, PGO rejection.
- `.scratch/nearlink-driver/lab-notes/RUST-WS73-PERF-BASELINE.md` — `libssap.a` baseline, size numbers.
- `.scratch/nearlink-driver/lab-notes/PERF-WIFI-THROUGHPUT.md` — USB FS/HS, HCC framing, DMAC_ON_HOST, coex.
- `.scratch/nearlink-driver/lab-notes/SLE-MEASURE-QOS.md` — SLE PHY/MCS/CI, ACB credit gate.
- `.scratch/nearlink-driver/lab-notes/WS73-WIFI-GAP-AUDIT.md` — feature gates.

---

*Written 2026-09-12, read-only synthesis. English-only per `AGENTS.md`. Whitelist `.gitignore` respected — no binaries committed. Network research was read-only and performed against GitHub API / repository metadata only.*
