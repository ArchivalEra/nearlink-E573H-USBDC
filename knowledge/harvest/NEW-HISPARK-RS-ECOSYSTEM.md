---
type: harvest
title: "Lab note: the hispark-rs Rust ecosystem for HiSilicon WS63/BS2X (hisi-rf, SLE, QEMU)"
language: zh
created: 2026-09-05
tags: [harvest, note, hispark, rust]
sources:
  - "the local harvest material tree (see guide.md)/hispark-rs"
trust: B
stale_after: 2027-03-05
---

# Lab note: the hispark-rs Rust ecosystem for HiSilicon WS63/BS2X (hisi-rf, SLE, QEMU)

- Date: 2026-09-03
- Source snapshot: `the local harvest material tree (see guide.md)/hispark-rs/` (25 repos, ~771 MB, read-only)
- Scope of this note: map the ENTIRE ecosystem — crate dependency graph, implemented
  vs stub, exactly how `hisi-rf` handles SLE (vendor-lib FFI, NOT raw DLI), the QEMU
  SLE verdict, and what rust-ws73 (ticket 04 option B) can concretely adopt.
- All file paths below are relative to `the local harvest material tree (see guide.md)/hispark-rs/` unless
  written as absolute; citations are `repo/file:line`.
- Cross-references: `BS21-WS63-SDK-COMPARISON.md`, `FBB-WS63-GLE.md`,
  `FBB-WS63-BGTP.md`, `COMMUNITY-PROJECTS.md` (existing lab notes).

---

## 1. What this ecosystem is

`hispark-rs` (GitHub org `hispark-rs`, umbrella repo `hisi-riscv-rs`) is a
from-scratch Rust ecosystem for HiSilicon's RISC-V SoCs: WS63 (Hi3863, Wi-Fi 6 +
BLE + SLE — the same chip family as our WS73 dongle's SoC) and BS2X (BS20/BS21E/BS22,
BLE 5.4 + SLE, no Wi-Fi). The local mirror holds 25 repos:

```
bs2x-pac  bs2x-svd  fbb_bs2x-qemu  hisi-alloc  hisi-crypto  hisi-crypto-ws63
hisiflash  hisi-flash-algorithm  hisi-fwpkg  hisi-nvs  hisi-registers  hisi-rf
hisi-rf-core  hisi-rf-ws63  hisi-riscv-qemu  hisi-riscv-rt  hisi-riscv-rust-toolchain
hisi-rom-sys  hisi-rs-template  hisi-rtos  ws63-examples  ws63-pac  ws63-radio-sys
ws63-svd  (+ hisi-rf-ws63 docs/)
```

Key mental model — five layers, each a separately versioned crates.io crate:

1. **Silicon contracts**: PACs + SVDs + `hisi-registers` (register truth).
2. **Runtime**: `hisi-riscv-rt` (startup/link), `hisi-rtos` (scheduler), `hisi-alloc`,
   `hisi-rom-sys` (mask-ROM facts), toolchain radar.
3. **Radio stack**: `ws63-radio-sys` (raw vendor-blob ABI + Cargo-delivered archives)
   ← `hisi-rf-ws63` (WS63 backend) ← `hisi-rf-core` (chip-neutral contracts)
   ← `hisi-rf` (application facade with compile-time profiles).
4. **Services**: `hisi-crypto` (+ `-ws63` backend), `hisi-nvs`, `hisi-storage`
   (on crates.io, not mirrored).
5. **Tooling & apps**: `hisi-fwpkg`, `hisiflash`, `hisi-flash-algorithm` (probe-rs),
   `hisi-riscv-qemu`, `ws63-examples`, `hisi-rs-template`.

The ecosystem targets bare-metal firmware ON the chip. It does NOT run on the Linux
host and does not speak our dongle's USB protocol — but its vendor-ABI knowledge and
tooling are directly reusable (Section 12).

---

## 2. Dependency graph (verified from Cargo.tomls)

```
                          hisi-rf 0.1.0-alpha.113          (facade, app-facing)
                             │  =hisi-rf-core 0.1.0-alpha.24
                             │  =hisi-rf-ws63 0.1.0-alpha.100 (optional, chip-ws63)
                             │  hisi-hal 0.7.0-alpha.9, smoltcp 0.13
                             ▼
   hisi-rf-core (0 deps of substance: embassy-sync 0.7, portable-atomic 1)
      ├─ wifi.rs 1596 L, ble.rs 885 L, sle.rs 585 L, control.rs 1029 L
      └─ incremental/ (A5B generation-tagged backend experiment)
                             ▼
   hisi-rf-ws63 (WS63 composition root)
      ├─ ws63-radio-sys =0.1.0-alpha.25   ← vendor .a blobs + raw extern fns
      ├─ hisi-rf-core, hisi-rf-rtos-driver =0.1.0-alpha.20
      ├─ hisi-alloc 0.1.0-alpha.3, hisi-nvs =0.1.0-alpha.3, hisi-storage =0.1.0-alpha.3
      ├─ hisi-crypto 0.1.0-alpha.5 + hisi-crypto-ws63 =0.1.0-alpha.5 (optional, BLE/SLE init)
      └─ hisi-hal =0.7.0-alpha.9 (chip-ws63)
                             ▼
   ws63-radio-sys release unit (3 packages, one version):
      ws63-radio-sys (links="ws63_radio_sys") ← ws63-radio-blob (9 MB .a payload)
                                              ← hisi-rf-link (maintainer normalizer)
                             ▼
   platform: hisi-rtos =0.1.0-alpha.25 ← hisi-riscv-rt 0.5.10 ← ws63-pac / bs2x-pac
             hisi-rom-sys (ROM symbols/patches) ← hisi-alloc
```

- Facade pins, exact-version (`=`) between core/ws63 layers:
  `hisi-rf/Cargo.toml:74-76,90`.
- Backend crate pulls in the whole platform: `hisi-rf-ws63/Cargo.toml`
  `[dependencies]` block (hisi-alloc, hisi-storage, hisi-nvs, hisi-rf-rtos-driver,
  hisi-crypto*, ws63-radio-sys, hisi-hal).
- `hisi-rf-core` is deliberately dependency-free (no PAC/blob/scheduler):
  `hisi-rf-core/README.md:5-20`; confirmed by its tiny Cargo deps
  (`hisi-rf-core/Cargo.toml`, only embassy-sync + portable-atomic).

---

## 3. hisi-rf / hisi-rf-core / hisi-rf-ws63 — the radio facade

### 3.1 Architecture

- `hisi-rf` re-exports `hisi-rf-core` contracts and selects a chip composition root
  via `chip-*` features; applications pick a *profile* feature that removes
  role-inapplicable methods at compile time (`hisi-rf/README.md:1-21`).
- Wi-Fi profiles: `profile-wifi-wpa2-smoltcp`, `profile-wifi-wpa3-smoltcp`
  (`hisi-rf/README.md:21-22`).
- BLE + SLE profiles: `profile-ble-peripheral|central|dual-role`,
  `profile-sle-announce`, `profile-sle-seek`, `profile-sle-ssap`
  (`hisi-rf/README.md:25-27`, feature defs at `hisi-rf/Cargo.toml:164-169`).
- Ownership boundary is explicit: core = portable contracts; ws63 = WS63 resources;
  `ws63-radio-sys` = raw ABI + normalized archives; `hisi-rf-rtos-driver` =
  scheduler contract; `hisi-rtos` = runtime started by the app, never hidden
  (`hisi-rf-ws63/README.md:12-20`).
- Caller-owned storage: `declare_radio_storage!` places bounded state in BSS and the
  big arena in a NOLOAD section; `RADIO_STORAGE.install()` is the single pre-RTOS
  admission point; the profile atomically reserves its 7 dynamic RTOS task slots
  (1 runner + 6 vendor workers) (`hisi-rf-ws63/README.md:31-45`).
- Allocation-free versioned diagnostics: `hisi-rf-error/v3` schema with machine code,
  stage, recovery action, raw backend code, 4-entry numeric trace — never SSIDs or
  key material (`hisi-rf/README.md:117-125`, `hisi-rf-ws63/README.md:47-52`).
- Resource truth: `RadioStorage::report()` → `hisi-rf-resource-report/v10` JSON with
  task stacks, arena, 4,384-byte crypto DMA scratch, 48 KiB linker-owned packet RAM
  (`hisi-rf-ws63/README.md:47-52`).

### 3.2 THE ANSWER: how SLE is abstracted — vendor archive FFI, not raw DLI

SLE is **not** implemented against registers or a raw DLI. It wraps the vendor
`libbth_gle.a` (GLE = HiSilicon's unified BLE+SLE host) and `libbgtp.a` (controller)
archives through hand-written `unsafe extern "C"` bindings:

- The GLE/BGLE archives ARE the SLE host: "All six [BLE/SLE profiles] currently use
  the same pinned WS63 BGLE archive" (`hisi-rf/README.md:29-31`).
- `ws63-radio-sys` binds the vendor API in two modules:
  - `crates/ws63-radio-sys/src/sle.rs:2-6` — "Raw WS63 SLE discovery and connection
    ABI. The layout is derived from the pinned WS63 SDK's `sle_device_discovery.h`
    and `sle_connection_manager.h`" — bounded S1 (discovery/announce/seek) and S2
    (connect) slices; SSAP in `crates/ws63-radio-sys/src/ssap.rs:1-5` ("bounded S3
    slice... Write and indication APIs remain outside this evidence slice").
  - Extern surface (16 functions): `enable_sle/disable_sle`,
    `sle_announce_seek_register_callbacks`, `sle_set_announce_param/data`,
    `sle_start/stop_announce`, `sle_set_seek_param`, `sle_start/stop_seek`,
    `sle_set_local_addr`, `sle_default_connection_param_set`,
    `sle_connection_register_callbacks`, `sle_connect/disconnect/pair_remote_device`
    (`crates/ws63-radio-sys/src/sle.rs:172-198`).
  - Raw struct layouts with compile-time `offset_of!`/`size_of` assertions pinned to
    the vendor ABI: `Address` = 7 B, `AnnounceParameters` = 40 B, `SeekParameters`
    = 20 B (3 PHYs), `SeekResult` = 24 B, `DefaultConnectionParameters` = 14 B
    (`crates/ws63-radio-sys/src/sle.rs:200-227`).
  - Callback tables: `AnnounceSeekCallbacks` (10 callbacks) and
    `ConnectionCallbacks` (9 callbacks incl. pair/auth, RSSI, low-latency, PHY)
    (`crates/ws63-radio-sys/src/sle.rs:140-170`).
- The safe chip backend wraps them with bounded, generation-tagged state:
  `hisi-rf-ws63/src/sle.rs` (1826 L) — internal stage type `SleS1Controller`:
  - init: register callback tables then `enable_sle()`
    (`hisi-rf-ws63/src/sle.rs:1570,1627-1632,1675`).
  - ops: `sle_set_local_addr` (:582), `sle_set_announce_param/data/start/stop`
    (:637-665), seek (:684-727), connect/disconnect/pair (:736-760), SSAP server
    config + value storage (:762-822), SSAP exchange/discover/read/write/notification
    (:954-1048), C callbacks `ssap_*` (:1289-1490).
  - Bounded storage: 32-event queue, 64-byte event/announce payloads, SSAP values ≤
    64 B, shared 296 KiB radio arena, 4 vendor tasks × stacks totaling 10,240 B min
    512 B (`hisi-rf-ws63/src/sle.rs:36-49`; arena at
    `hisi-rf-ws63/src/lib.rs:703`).
- Chip-neutral typed config lives in core: `SleAddress`, `AnnounceInterval` (125 µs
  units, 0x20..0x00ff_ffff), `AnnounceChannels` (3-bit), 64-byte `AnnouncePayload`,
  `SeekInterval/SeekTiming`, and a full static SSAP server DB model (UUID16/128,
  permissions/operations bitmaps, property/descriptor/service/server definitions)
  (`hisi-rf-core/src/sle.rs:1-518`).
- Stage slicing: S0 = archive normalization evidence, S1 = announce/seek, S2 =
  connect/pair, S3 = SSAP. Migration contract freezes surface snapshots
  `ble-b3-stage.txt` / `sle-s3-stage.txt` and maps `SleS1Controller` → future
  `hisi_rf::sle::SleController`; raw DLI/HCI is explicitly a NON-goal
  (`hisi-rf-ws63/docs/radio-stage-api-migration.md:5-33`).

### 3.3 Silicon evidence status (implemented vs stub)

| Area | Status | Evidence |
|---|---|---|
| Wi-Fi WPA2 STA | working, on-silicon parity | scan/connect/DHCP/ICMP example (`ws63-examples/README.md:12-15`); "W2C and W2D are closed... on-silicon parity evidence" (`ws63-radio-sys/README.md` upstream-port section) |
| Wi-Fi WPA3/SAE | transition-mode proven; pure-WPA3 20-reset gate blocked on AP | `hisi-rf-ws63/README.md:26-28` |
| Wi-Fi SoftAP | two-board HIL with DHCP+UDP echo | `ws63-examples/README.md:14,45-63` |
| BLE GAP/GATT | B3 stage, two-board 20-reset matrix | `hisi-rf-ws63/docs/radio-stage-api-migration.md:8-11` |
| SLE announce/seek/connect/SSAP | S3 stage, two-board 20-reset matrix | same table, `sle-init` row |
| SLE coexistence with Wi-Fi | maintainer-only features `coexistence-wifi-sle`, connected smoke examples exist | `hisi-rf-ws63/Cargo.toml:197-198`; examples `wifi_sle_coexistence_smoke.rs`, `wifi_sle_connected_client_smoke.rs` |
| SLE write/indication, pairing/auth SSAP | NOT implemented (outside evidence slice) | `crates/ws63-radio-sys/src/ssap.rs:3-5`; "Pairing, authenticated SSAP, coexistence, raw DLI/HCI" listed as separate gates (`radio-stage-api-migration.md:28-31`) |
| Supplicant | replaced by pinned upstream hostap 2.11 + 3 backported 2026 CVE fixes; vendor supplicant kept only as migration oracle | `ws63-radio-sys/README.md` (W2C/W2D + port description) |
| Cargo consumer build | no SDK checkout, no Python/Bash/RISC-V GCC at build time; stock rust-lld | `hisi-rf-ws63/README.md:23-25` |

Crate versions are early-alpha but actively released (hisi-rf 0.1.0-alpha.113,
2026-09-01: `hisi-rf/CHANGELOG.md:14-15`); the changelog tracks BLE/SLE lifecycle
HIL fixtures and bounded event queues (`hisi-rf/CHANGELOG.md:283-320`).

---

## 4. ws63-radio-sys — the blob-integration pattern (the crown jewel for us)

One versioned release unit of three Cargo packages (`ws63-radio-sys/README.md:5-13`):

1. `ws63-radio-sys` — `no_std`, `links = "ws63_radio_sys"`; build script exports
   normalized-archive paths via Cargo metadata; feature-gated modules
   `authenticator/ble/sle/ssap/supplicant` with mutually exclusive feature
   `compile_error!` guards (`crates/ws63-radio-sys/src/lib.rs:14-47`).
2. `ws63-radio-blob` — 9 MB of redistributable, *normalized* vendor archives
   delivered via Cargo (no build-time downloads): 21 artifacts
   (`crates/ws63-radio-blob/artifacts/manifest.json`, `artifacts` = 21), incl.
   `libwifi_driver_hmac/dmac/tcm.a`, `libbgtp.a` + `libbgtp_rom_data.a` (BLE/SLE
   controller), `libbth_gle.a` (GLE host), `libbt_host/app/sdk.a`, `libbg_common.a`,
   `librom_callback.a`, and the four `libhisi_wpa_native_{supplicant,authenticator}_
   {wpa2,wpa3}.a` upstream-hostap port archives.
3. `hisi-rf-link` — host-side maintainer tool + library: relocation inventory,
   archive normalization, verification, compatibility profiles
   (`ws63-radio-sys/README.md:8-12`).

Machine-readable profiles in `crates/hisi-rf-link/profiles/`:

- `ws63.toml` — Wi-Fi base archives.
- `ws63-sle-s0.toml` — SLE ABI profile, `revision = "ws63-sle-s0-archive-abi-v1"`,
  five host archives with sha256 + role comments ("SLE host: GAP/SSAP application
  services and host state" for `libbth_gle.a`; controller = `libbgtp.a` +
  `libbgtp_rom_data.a` + `librom_callback.a`) (`ws63-sle-s0.toml:1-27`; manifest
  `sle_profile` block: `init_revision = "ws63-sle-s3-ssap-v1"`).
- `ws63-ble-b0.toml` / `ws63-ble-b1.toml` (+ JSON reports) — BLE archive ABI +
  per-symbol integration ownership.
- `ws63-scheduling.toml` — observed RF task entry symbols/vendor priorities bound to
  archive hashes (classification evidence, not policy).
- `ws63-runtime-compat.toml` — distinguishes 7 LiteOS-namespace symbols supplied by
  the Rust native-runtime adapter from 8 archive-only unreachable symbols.
- `ws63-supplicant-boundary.toml` — prevents legacy vendor supplicant from silently
  re-entering an upstream image.

Technique highlights:

- Normalization happens at RELEASE time, not consumer build time; consumer links
  with stock `rust-lld` and zero post-link ELF mutation (`ws63-radio-sys/README.md:23-26`,
  "The normal Cargo path contains no vendor relocation...").
- Mask-ROM replacement uses a relocatable patch object with standard
  `R_RISCV_CALL_PLT` relocations; lld resolves final addresses in one link
  (`ws63-radio-sys/README.md:100-101`).
- The pinned `ws63-RF` SDK submodule is provenance/oracle only; consumers get
  hash-bound payloads (`ws63-radio-sys/README.md`, "The language-neutral `ws63-RF`
  submodule..."). CI byte-compares normalized output against the Cargo payload,
  including a canonical macOS builder for the hostap archives.
- Upstream hostap 2.11 port: `os_hisi_rtos.c`/`eloop_hisi_rtos.c`/
  `hisi_wpa_port.c`/`l2_packet_ws63.c`/`driver_ws63.c`/`supplicant_ws63.c` behind a
  versioned OS hook table + ABI version prefix checks; W2C/W2D closed with
  silicon parity (`ws63-radio-sys/README.md`, upstream-port bullet list).
- Weekly "Hostap security radar" GitHub workflow diffs `w1.fi/security/` against the
  source manifest (`ws63-radio-sys/README.md:25-29`).

This is the most mature open implementation of the exact problem ticket 04 option B
describes: linking closed HiSilicon `.a` blobs into Rust firmware with reproducible,
verifiable, Cargo-native delivery.

---

## 5. QEMU: can we run SLE firmware without hardware? — NO (measured dead end)

`hisi-riscv-qemu` is a fork-based QEMU (esp-qemu style) adding `-M ws63` /
`-M bs21` machines. Delivered capability (README status, Chinese):

- Single RV32IMFC hart `-cpu ws63` + HiSilicon `xlinx` custom ISA (13/13 self-test;
  required for vendor-gcc firmware) (`hisi-riscv-qemu/README.md:3-13`, xlinx table at
  ROADMAP "里程碑" section).
- All 35 SVD peripherals behaviorally modeled (DMA/RTC/WDT/I2C/SPI/I2S/LSADC/UART-RX/
  SFC/TSENSOR/EFUSE/TRNG + GPIO pin net/pinmux + CLDO_CRG clock gating), IRQ ≥32
  custom local interrupts with LOCIPRI/PRITHD priorities, ROM-call interception
  (`ws63_rom_call`), NV/partition flash overlay, qtest register-level regression,
  semihosting exit codes, GDB, `-icount` (`hisi-riscv-qemu/README.md:10-13`;
  ROADMAP stages 0-4 all "✅").
- Runs fbb_ws63 C SDK vendor firmware: flashboot boots, `ws63-liteos-app` reaches
  "cpu 0 entering scheduler" (`hisi-riscv-qemu/ROADMAP.md`, 里程碑 section).
- Connectivity base is Wi-Fi-only and research-grade: synthetic MAC + SLIRP, no RF
  (ROADMAP stage 5 "不仿 RF").

The BLE/SLE verdict is a dedicated doc that records the dead end so it is not
re-explored (`hisi-riscv-qemu/docs/explanation/bs21-connectivity-feasibility.md`):

- Architecture fact: BLE/SLE host AND controller both run as LiteOS tasks on the one
  app core; boundaries are (a) in-memory `api_h2c_write()` HCI host↔controller and
  (b) B_CTL radio MMIO @0x59000000 + closed analog PHY (`:13-27`).
- Boundary 1 (radio MMIO): 56 write-only PHY constants, ~16.5k accesses to BT_EM
  window are RAM, then the controller reboots the core waiting for a PHY IRQ-26
  event no analog model emits — "You can make the controller *start* but never
  *work*" (`:29-48`).
- Boundary 2 (HCI): both `libbth_gle.a` and `libbgtp.a` are closed; no DTM/HCI-UART
  controller firmware exists, so BlueZ/H4 tooling cannot attach; SLE is "effectively
  infeasible. Proprietary command/event set, no public reference, fused into the GLE
  blob" (`:50-72`).
- Probe result: `api_h2c_write` is NEVER executed before the radio wall kills the
  boot (0 hits in instruction trace), and the framing is a HiSilicon message ABI
  (type in `a0>>16`), not raw H4 (`:74-99`).
- Recommendation: do not pursue radio-MMIO emulation; SLE stays out of scope; the
  B_CTL absorber probe lives only on the `sle-radio-probe` branch, deliberately not
  master (`:101-139`).

Consequence for us: **no QEMU-based SLE bring-up**. QEMU is usable for peripheral
driver correctness (which is what its ROADMAP north star says: "在没有 EVB 的情况下,
把 WS63 固件「跑得足够真」" — run firmware real-enough without EVB, ROADMAP 北极星
section). The template confirms: the wifi/radio starter "QEMU can compile this
starter but cannot execute its radio path" (`hisi-rs-template/README.md`,
wifi starter section).

### 5.1 `fbb_bs2x-qemu` is NOT a QEMU fork

Despite the directory name, this is HiSilicon's official **fbb_bs2x SDK** (BS20/BS21E/
BS22, LiteOS, FBB framework) — vendor spec table and board list in
`fbb_bs2x-qemu/README.md:9-21` (BS2X: 64 MHz, 1 MB flash, 128-160 KB RAM, SLE 1K/2K/4K
report rate, 4-12 Mbps air rate). Value for us:

- Vendor SLE headers: `src/include/middleware/services/bts/sle/sle_device_discovery.h`
  and `sle_connection_manager.h` — the exact files `ws63-radio-sys`'s SLE bindings
  were derived from (`crates/ws63-radio-sys/src/sle.rs:2-6`).
- SLE application samples: `sle_uart`, `sle_measure_dis` (SLE ranging), `ble_sle_tag`,
  `sle_multi_conn`, `rcu`, `air_mouse` under
  `src/application/samples/products/` (README example table; samples dir listing).
- `hisi-riscv-qemu` can boot its signed loaderboot on `-M bs21` with xlinx decoding
  (`docs/explanation/bs21-vendor-firmware.md:1-14`).

---

## 6. Register truth: ws63-pac / bs2x-pac / svd / hisi-registers

- `ws63-pac/src/lib.rs` = 37,251 generated lines; regenerated from
  `ws63-svd/WS63.svd` (31 `<peripheral>` entries) via svd2rust; no README, truth is
  `ws63-pac/ARCHITECTURE.md:1-9` ("为 35 个片上外设提供 RegisterBlock 与 Peripherals
  单例" — 35 peripherals via RegisterBlock + Peripherals singletons).
- `bs2x-pac/src/lib.rs` = 37,705 lines from `bs2x-svd/BS2X.svd` (19 peripherals).
  BS21 shares WS63's HimiDeer riscv31 core + versioned IP (UART v151, TIMER v150,
  GPIO v150) so the SVD reuses WS63 register blocks, but the crate is standalone;
  BS2X-specific blocks: GADC, KEYSCAN, PDM, QDEC, USB DWC OTG @0x5800_0000
  (`bs2x-pac/README.md:1-25`).
- `hisi-registers` is an experimental SystemRDL-first flow: manuals/SDK headers/
  silicon observations → reviewed RDL blocks → per-chip maps → SVD → svd2rust PACs
  (`hisi-registers/README.md:5-17`). Cross-chip IP sharing is evidence-gated: SPI
  shared only because v151 definitions are byte-identical across SDKs; WDT v151,
  SFC v150, PWM v151, Timer v150, TCXO v150, GPIO v150, UART v151, SIO v151
  confirmed shared across WS63/BS2X/WS53 (`hisi-registers/README.md:24-32`).

---

## 7. Runtime stack

- `hisi-riscv-rt`: chip-neutral `riscv-rt` facade; per-chip adapters own reset asm,
  linker scripts, image headers; `chip-ws63` stable, `chip-bs21` behind `unstable`;
  Hi3322 doc-only. Linker contract exports `hisi-riscv-link.x` =
  `memory.x → layout.ld → device.x → riscv-rt-symbols.x` (+ `boot-header.x`)
  (`hisi-riscv-rt/ARCHITECTURE.md:5-27`).
- `hisi-rtos`: `no_std` single-hart scheduler, three per-task policies
  (Cooperative / Budgeted / Preemptive), 272-byte unified task/trap frame, TIMER +
  software-IRQ deferred preemption, priority-ordered recursive mutexes with
  transitive priority inheritance, optional Embassy time driver on the same
  SchedulerPort timer (`hisi-rtos/README.md:5-19`). WS63 startup binds
  `TIMER_INT0`/`SOFT_INT0`, caller-owned `SchedulerStorage<15>` + arena, starts via
  `hisi_rtos::ws63::start` (`hisi-rtos/README.md:22-45`). Formal specs: nine TLA+
  models (SwitchIntent, PriorityInheritance, SchedulerBudget, WaitLinearization,
  ResourceLifecycle, ReadyOwnership, TimerRearm...) in `hisi-rtos/spec/`.
- `hisi-alloc`: runtime-neutral arena allocator `CHeap`; deliberately defines NO
  `malloc`/`osal_kmalloc`/global allocator — adapters own that (`hisi-alloc/README.md:5-24`).
- `hisi-rom-sys`: chip-neutral facade over mask-ROM facts; Cargo `links` exports
  ROM symbols/callbacks/Wi-Fi patch paths; `hisi-rf-link` generates patch tables
  against the final ELF (`hisi-rom-sys/README.md:5-22`).
- `hisi-riscv-rust-toolchain`: no longer builds a custom rustc — the ecosystem builds
  on **official Rust nightly** with `riscv32imfc-unknown-none-elf` (hard-float ilp32f,
  no atomics) + `-Zbuild-std=core,alloc`; repo is now a CI "radar" watching upstream
  for prebuilt rust-std and Tier-2 readiness (`hisi-riscv-rust-toolchain/README.md:3-20`).

---

## 8. Platform services

- `hisi-crypto`: chip-neutral `no_std` capability traits; RustCrypto default oracle;
  `SecretBytes` zeroizing + opaque `KeyHandle`; explicit `EntropySource` vs
  `CryptoEntropySource` marker (TRNG bytes can't silently seed a DRBG);
  `sae` module = narrow hostap-2.11 WPA3-SAE contract with typed P-256 elements,
  group-19-only (`hisi-crypto/README.md:5-31`).
- `hisi-crypto-ws63`: WS63 hardware/ROM backend — PBKDF2-HMAC-SHA1 on the RKP engine,
  SHA-1/256/SM3 + HMAC on SPACC hash channel with D-cache maintenance, AES single
  block + AES-128-CMAC via SPACC + KM/KLAD key slots, P-256 point multiply on the PKE
  (Montgomery R^2 reproduced explicitly), TRNG FIFO; HIL-proven KATs on real silicon;
  never falls back to software after hardware failure (`hisi-crypto-ws63/README.md:5-27`).
- `hisi-nvs`: read-only `no_std` parser of the ACPU KV store (page complements,
  duplicate-page sequence selection, CRC, encryption flags); write/GC/keystore are
  future crates (`hisi-nvs/README.md:5-9`).

---

## 9. Tooling (vs our ws63flash findings)

- `hisi-fwpkg`: producer for the HiSilicon app image + fwpkg V1 container. Format
  truth (matches our earlier ws63flash reverse-engineering): flashboot does NOT boot
  a bare ELF; app partition at `0x230000` needs the `0x300`-byte header =
  `image_key_area_t` (0x100, image_id 0x4B0F2D1E) + `image_code_info_t` (0x200,
  image_id 0x4B0F2D2D) (`hisi-fwpkg/README.md:5-16,66-76`). CLI: `hisi-fwpkg pack
  blinky -o blinky.fwpkg --chip ws63|bs21`; works for both WS63 and BS2X.
- `hisiflash`: espflash-inspired cross-platform flasher (Rust workspace: lib + CLI).
  SEBOOT protocol compatible with official fbb_burntool, YMODEM; USB VID/PID
  autodetect (CH340/CP210x/FTDI/PL2303/HiSilicon native), TOML config, env vars,
  shell completion, monitor mode (`hisiflash/README.md:13-35`). Chip support: WS63
  full; BS2X/BS25 experimental, all sharing the same SEBOOT/YMODEM serial flow
  (`hisiflash/README.md:44-50`); protocol spec in
  `hisiflash/docs/.../PROTOCOL.md` (tree listing `hisiflash/README.md:390,404`).
- `hisi-flash-algorithm`: probe-rs flash loaders; `ws63` member (SFC v150 NOR) is
  hardware-verified, builds on stable rust for `riscv32imc`; embeds into
  `probe-rs/targets/HiSilicon_WS63.yaml` via `target-gen`; BS2X planned
  (`hisi-flash-algorithm/README.md:5-40`).

Comparison to our `ws63flash` work: hisi-fwpkg is the packer twin (same 0x300 header
IDs), hisiflash covers the SEBOOT serial path, and the probe-rs algorithm adds a
SWD path we have not built. Any flashing UI for rust-ws73 could reuse all three
instead of re-deriving formats.

---

## 10. Application patterns

- `ws63-examples`: 30+ examples from blinky to `wifi_connectivity` (scan→connect→DHCP
  →ICMP→lease renewal via the public `hisi-rf` facade), `wifi_softap` two-board HIL,
  `net_ping`, `rf_port_demo`, `wifi_blob_link`, RTOS conformance fixtures
  (`ws63-examples/README.md:9-15`; full dir listing incl. `embassy_async_io`,
  `rtos_preemption`, `xip_flash_clk_hazard`). Credentials are build-time env only,
  never committed (`ws63-examples/README.md:26-42`).
- `hisi-rs-template`: cargo-generate template; pins nightly + `-Zbuild-std`,
  optional QEMU for non-radio starters, `just image` also emits a
  `<crate>.resource.json` resource report (`hisi-rs-template/README.md` prerequisites
  + wifi starter sections).
- Canonical app shape (facade): declare storage → `install()` before RTOS → start
  runtime with the installed allocator → `hisi_rf::ws63::init` → `controller.split()`
  → run `RadioRunner` loop alongside embassy tasks (`hisi-rf/README.md:33-58`,
  `:63-100`).

---

## 11. Implemented vs stub — summary matrix

| Layer | Implemented | Stub / missing |
|---|---|---|
| PACs/SVD | WS63 35 periphs, BS2X 19 periphs | NFC not modeled (`bs2x-pac/README.md` last paragraph) |
| Runtime/RTOS | full scheduler + TLA+ specs, WS63+BS2X adapters | BS2X adapter `unstable`, no board HIL (`hisi-riscv-rt/ARCHITECTURE.md:15-21`) |
| Wi-Fi | WPA2/WPA3 STA + SoftAP, silicon parity | pure-WPA3 gate blocked on AP hardware |
| BLE | GAP/GATT client+server (B3) | pairing/authenticated paths not yet |
| SLE | announce/seek/connect/SSAP read+notify (S3, 20-reset two-board HIL) | write/indication, pairing, raw DLI/HCI; coexistence maintainer-only |
| QEMU | 35 peripherals, boots vendor C SDK | no radio/BT/SLE, documented dead end |
| Crypto | HW SPACC/RKP/PKE/TRNG + RustCrypto oracle | Dragonfly remaining pieces software (`hisi-crypto-ws63/README.md:23-25`) |
| NVS | read-only parse | write/GC/keystore |
| Flash tooling | fwpkg pack, SEBOOT flash, probe-rs algo (WS63) | BS2X flash-algo planned |
| HAL | `hisi-hal 0.7.0-alpha.9` on crates.io | not mirrored locally (referenced by bs2x-pac README) |

---

## 12. Adoption path for rust-ws73 (ticket 04 option B — now concrete)

Our target: the E573H USB dongle (`ffff:3733`), a WS73-family device; the Linux
driver talks USB while vendor firmware runs on the chip. hispark-rs is firmware-side
Rust, so adoption is pattern/ABI/tooling-level, not a drop-in library. Concrete
items:

1. **Blob-integration blueprint (highest value).** `ws63-radio-sys`'s three-package
   release unit is the template for any rust-ws73 component that must link vendor
   `.a` blobs (e.g. a userspace daemon, or future on-chip co-processor firmware):
   Cargo `links` key + build-script metadata export (`crates/ws63-radio-sys/src/lib.rs:14-42`),
   release-time archive normalization with sha256-pinned manifests
   (`crates/ws63-radio-blob/artifacts/manifest.json`), byte-for-byte CI rebuild gates,
   and `R_RISCV_CALL_PLT` ROM-patch objects instead of post-link ELF surgery
   (`ws63-radio-sys/README.md:96-101`).
2. **SLE vendor ABI reference.** The 16-function SLE extern surface + struct offsets +
   callback tables (`crates/ws63-radio-sys/src/sle.rs:140-227`,
   `crates/ws63-radio-sys/src/ssap.rs`) are a de-facto annotated binding of
   `sle_device_discovery.h` / `sle_connection_manager.h`. Cross-check these against
   our SDK tree (`sdk/ws73_sdk_linux_WS73_1.10.110/`) — if WS73's GLE archives expose
   the same symbols, we inherit validated `repr(C)` layouts (Address=7 B,
   AnnounceParameters=40 B, SeekParameters 3-PHY layout, SSAP Uuid/ServerPropertyInfo)
   instead of re-deriving them. This also complements our existing
   `FBB-WS63-GLE.md` notes with the Rust-side view.
3. **Expectation-setting for SLE scope.** Even in this mature ecosystem, SLE is only
   at announce/seek/connect/SSAP-read+notify; write/indication/pairing/authenticated
   SSAP and raw DLI/HCI are explicitly future gates
   (`hisi-rf-ws63/docs/radio-stage-api-migration.md:28-31`). If rust-ws73 needs full
   SLE data-path features, nobody has them yet — plan for our own SSAP write/indicate
   bindings as novel work.
4. **QEMU: use for host-protocol mocking, not SLE.** The measured verdict kills any
   plan to run SLE firmware in QEMU (`bs21-connectivity-feasibility.md:103-115`);
   B_CTL @0x59000000 faults loudly by design (`:131-139`). But the QEMU fork's
   peripheral models + qtest harness are reusable if we ever emulate the dongle's
   non-radio side, and the `-M bs21` signed-image work
   (`docs/explanation/bs21-vendor-firmware.md`) helps us parse vendor images.
5. **Flashing/packaging reuse.** `hisi-fwpkg` (same `0x300` header + `image_id`
   constants we found) + `hisiflash` SEBOOT/YMODEM + `hisi-flash-algorithm`
   (probe-rs WS63 YAML) replace any remaining bespoke ws63flash functionality with
   maintained crates; BS2X/BS25 experimentality only matters if E573H's bootloader
   differs.
6. **Diagnostics/report schemas.** `hisi-rf-error/v3` (machine code + stage + 4-entry
   numeric trace, no secrets) and `hisi-rf-resource-report/v10` are good models for
   rust-ws73's kernel-side error reporting toward userspace.
7. **Upstream hostap port.** The `ws63-radio-sys` upstream hostap 2.11 port with
   CVE radar is a reference if rust-ws73 ever needs supplicant-grade WPA logic
   in-tree rather than delegating to wpa_supplicant on the host.
8. **Version pins to cite in tickets**: hisi-rf 0.1.0-alpha.113,
   hisi-rf-core 0.1.0-alpha.24, hisi-rf-ws63 0.1.0-alpha.100, ws63-radio-sys
   0.1.0-alpha.25, hisi-rtos 0.1.0-alpha.25, hisi-hal 0.7.0-alpha.9, nightly
   `riscv32imfc-unknown-none-elf` via `-Zbuild-std` (`hisi-rf/Cargo.toml:3,74-76,90`;
   `hisi-riscv-rust-toolchain/README.md:5-11`).

Risks / caveats: everything is pre-1.0 alpha with "may change" disclaimers
(`hisi-rf/README.md` closing lines); the facade forbids bypassing it to name `BleB*`/
`SleS*` stage APIs (`hisi-rf/README.md:35-37`), so reusing stage internals means
fork-level coupling; and blob redistribution licensing is confined to
`crates/ws63-radio-blob/LICENSE-BLOB.md` — check terms before re-shipping any
archive in rust-ws73.

Open follow-ups:
- Diff `crates/ws63-radio-sys/src/{sle,ssap,ble}.rs` against
  `sdk/ws73_sdk_linux_WS73_1.10.110/` GLE headers (ticket-04 input).
- Read `hisi-rf-ws63/src/hcc.rs` / `wal.rs` / `frw.rs` / `uapi.rs` to see the
  vendor firmware-internal transport names — potentially matching our USB trace of
  the dongle firmware's command/event framing.
- Inventory `fbb_bs2x-qemu` SLE samples (`sle_uart`, `sle_multi_conn`,
  `sle_measure_dis`) as behavioral reference for SLE data-path semantics.

---

## 13. Ten-line summary

1. hispark-rs = 25-repo Rust ecosystem for WS63/BS2X RISC-V SoCs; local mirror at
   `the local harvest material tree (see guide.md)/hispark-rs/`; note saved to
   `.scratch/nearlink-driver/lab-notes/NEW-HISPARK-RS-ECOSYSTEM.md`.
2. Layered crates.io stack: PAC/SVD → rt/RTOS/alloc → ws63-radio-sys → hisi-rf-ws63
   → hisi-rf-core → hisi-rf facade (pinned alphas, verified in `hisi-rf/Cargo.toml:74-76`).
3. SLE handling = vendor-archive FFI, NOT raw DLI: `libbth_gle.a`/`libbgtp.a` wrapped
   by 16 hand-bound externs in `ws63-radio-sys/crates/ws63-radio-sys/src/sle.rs:172-198`
   (+ SSAP in `ssap.rs`), safe-wrapped in `hisi-rf-ws63/src/sle.rs` (1826 L).
4. SLE maturity: announce/seek/connect/SSAP-read+notify pass two-board 20-reset HIL
   (`hisi-rf-ws63/docs/radio-stage-api-migration.md:8-11`); write/indication/pairing/
   raw DLI are explicit non-goals — adopting this saves ABI work, not feature work.
5. QEMU SLE: measured dead end — radio MMIO wall + private non-H4 HCI ABI
   (`hisi-riscv-qemu/docs/explanation/bs21-connectivity-feasibility.md:103-115`);
   QEMU is for peripheral/driver correctness only (35 peripherals, boots vendor SDK).
6. `fbb_bs2x-qemu` is actually the official BS2X SDK with SLE headers + samples
   (`sle_uart`, `sle_measure_dis`) — the behavioral reference for SLE apps.
7. The blob-integration pattern (Cargo links + normalized .a payload + sha256
   profiles + `R_RISCV_CALL_PLT` ROM patches + upstream hostap 2.11 port) in
   `ws63-radio-sys` is the concrete blueprint for ticket 04 option B.
8. `hisi-rf-core/src/sle.rs` provides chip-neutral typed SLE config (SSAP static DB
   model) worth porting for any Rust SLE surface we build.
9. Tooling duplicates our ws63flash findings (0x300 app header, fwpkg V1) as
   maintained crates: `hisi-fwpkg`, `hisiflash` (SEBOOT/YMODEM),
   `hisi-flash-algorithm` (probe-rs).
10. Everything is pre-1.0 alpha; facade forbids stage-API bypass; blob licensing in
    `crates/ws63-radio-blob/LICENSE-BLOB.md` must be checked before reuse.
