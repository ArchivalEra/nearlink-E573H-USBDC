---
type: harvest
title: "hispark-rs/ws63-examples — 30+ example curriculum with substantial WiFi implementations; wifi_blob_link links the vendor ROM blob into packet-RAM from Rust"
language: en
created: 2026-09-13
tags: [rust, ws63, bare-metal, wifi-blob, packet-ram, embassy, rtos-coexist, no_std, harvest]
sources:
  - "ArchivalEra/nearlink-E573H-USBDC workspace"
trust: A
stale_after: 2026-12-13
---

# hispark-rs/ws63-examples — 30+ example curriculum with substantial WiFi implementations; wifi_blob_link links the vendor ROM blob into packet-RAM from Rust

- Inspection date: 2026-09-13 (local hispark-rs subrepo, current with upstream 2026-09-09)
- Mode: read-only local program inspection; no build, network, hardware, or PCB access
- Scope: the Rust bare-metal example curriculum for WS63 — taxonomy, maturity, and the WiFi blob-linking mechanism

## Executive findings

1. The example tree plans (and largely populates) a **30+-example curriculum** spanning: async (`async_bus`, `async_delay`, `embassy_async_io`, `embassy_multitask`), RTOS interop (`rtos_embassy_coexist`, `rtos_preemption`, `rtos_priority_inheritance`, `rtos_scheduler_stress`, `rtos_budget_enforcement`), connectivity (`wifi_blob_link`, `wifi_connectivity`, `wifi_init_smoke`, `wifi_softap`, `net_ping`, `rf_port_demo`), peripherals (gpio_irq, i2c_scan, spi_loopback, dma_loopback, timer_irq, software_irq, uart_*), and platform hazards (`xip_flash_clk_hazard`, `custom_memory`, `semihost_selftest`, `reset_demo`). [repo tree]
2. **The ARCHITECTURE.md self-report is stale**: it claims "currently only blinky" with a linker-script propagation bug, but `wifi_connectivity` holds **2,180 lines of Rust across 5 files** and `wifi_softap` 1,445 lines — the WiFi implementations are real and substantial. [`ARCHITECTURE.md` vs `wc -l` per tree]
3. **`wifi_blob_link/build.rs` is the Rust↔vendor-blob bridge**: it links `libwifi_rom_data.a` (supplied by the `ws63-radio-sys` blob profile, compiled `rv32imfc/ilp32f` to match the `riscv32imfc-unknown-none-elf` target ABI) with `+whole-archive` ("pull in EVERY object/section of the config archive") into the `.wifi_pkt_ram` NOLOAD section — base `0xA00000`, size `0xC000`, with the blob's `g_mem_start_addr_cfg` words stored against `__wifi_pkt_ram_begin__`. Rust bare-metal code runs the vendor WiFi stack from packet-RAM. [`wifi_blob_link/build.rs:1-25`]
4. The repo is a submodule of the **`ws63-rs` monorepo** (renamed from earlier naming), with docs and ROADMAP centralized upstream. [ARCHITECTURE.md:3-8]

## Boundaries and gaps

- Which examples build green today was not verified (no build per standing rules); ARCHITECTURE.md's blinky link failure may or may not still apply to the newer examples.
- `rtos_embassy_coexist` (215 lines) is a skeleton by comparison to the WiFi trees.
- The blob profile's provenance (what exactly ships inside `libwifi_rom_data.a`) is documented in `ws63-radio-sys`, not here.

## Reusable for our stack

- The **packet-RAM NOLOAD blob-linking recipe** (whole-archive ROM data + linker-symbol base + ABI-matched toolchain) is the reusable mechanism for any Rust-on-WS73 firmware that must host vendor radio blobs — directly applicable to rust-ws73 device-side plans.
- The example taxonomy (async/rtos-interop/connectivity/hazards) is a completeness checklist for our own rust-ws73 crate validation matrix.
- Lesson: vendor-blob config archives need whole-archive linking — partial symbol pull silently drops config objects.

## Comparison anchors (vs existing reports)

- `NEW-HISPARK-RS-ECOSYSTEM.md` / `NEW-HISPARK-RS-SEPT-INCREMENT.md`: the ecosystem parent reports; this opens the example curriculum that those summaries only named.
- `NEW-FBB-WS63-QEMU-FORK.md`: examples runnable on ws63-qemu (blinky/uart_hello/timer_irq/gpio_irq verified there); the WiFi trees are the next emulator frontier pending ROM/efuse gaps.
- `hisi-rf` ecosystem reports: ws63-radio-sys is the blob profile feeding this build script.
