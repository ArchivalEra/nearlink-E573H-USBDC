---
type: harvest
title: hispark-rs/hisi-rf-ws63 — composition root for a Cargo-only WS63 WiFi build: normalized archives + rust-lld, no vendor SDK, wpa2-personal + smoltcp profile
language: en
created: 2026-09-13
tags: [rust, ws63, wifi, wpa2, smoltcp, composition-root, cargo-only, blob, harvest]
sources:
  - url: https://github.com/hispark-rs/hisi-rf-ws63
    note: local clone (pushed 2026-09-10); read-only inspection of README and src layout
trust: verified
stale_after: 2026-12-13
---

# hispark-rs/hisi-rf-ws63 — composition root for a Cargo-only WS63 WiFi build: normalized archives + rust-lld, no vendor SDK, wpa2-personal + smoltcp profile

- Inspection date: 2026-09-13 (local hispark-rs subrepo, current with upstream 2026-09-10)
- Mode: read-only local program inspection; no build, network, hardware, or PCB access
- Scope: the WS63 radio backend and composition root — layer ownership, build independence, feature profiles

## Executive findings

1. The Rust RF stack is a **five-layer ownership chain**: `hisi-rf` (facade, what apps select) → `hisi-rf-core` (portable controller/runner/config/event/L2-device contracts) → **`hisi-rf-ws63`** (this crate: WS63 radio ABI adapter, L2 bridge, hardware-crypto resource wiring, safe assembly of HAL peripheral tokens into one radio controller) → `ws63-radio-sys` (raw ABI declarations, normalized target archives, ROM patch objects, native link metadata) → `hisi-rtos` + `hisi-rf-rtos-driver` (runtime backend and scheduler/IPC contract). Each layer independently versioned. [README.md:1-16]
2. **The consumer build is Cargo-only**: "Cargo-delivered normalized archives and stock `rust-lld`; it does not require a vendor SDK checkout, Python, Bash, RISC-V GCC, or post-link relocation scripts" — the entire vendor WiFi blob complexity is pre-normalized into crates. This is the cleanest vendor-blob consumption model we have archived. [README.md:18-20]
3. Feature **profiles** compose vendor capability + userspace stack: the named profile `wpa2-personal,smoltcp` pins an upstream hostap WPA2-Personal backend plus the smoltcp TCP/IP stack — Rust bare-metal WiFi with real security and networking, selected by profile rather than config surgery. [README.md:22-23]
4. The src tree confirms the breadth: `ble.rs`/`ble_sc.rs`/`ble_compat.rs`/`ble_init_diag.rs` (BLE surface), `composition.rs` (the assembly root), `alloc.rs`, `compiler_rt.rs`, `blocking_diagnostics.rs`. [src/ listing]

## Boundaries and gaps

- README-level analysis; the ABI adapter and L2 bridge internals were not line-read.
- Profile list beyond `wpa2-personal,smoltcp` not enumerated.
- Hardware-crypto wiring described but not verified against the SDK's cipher APIs.

## Reusable for our stack

- **The normalized-archive pattern is the answer to the vendor-blob problem for rust-ws73**: pre-normalize vendor archives into Cargo-delivered crates with native link metadata, so our host/device builds never touch the vendor SDK — the same philosophy our knowledge plane applies to SDK knowledge.
- The five-layer ownership chain (facade → contracts → backend → ABI-sys → rtos) is a build-ready architecture for rust-ws73 that avoids the layering ambiguity our deep-modules report flagged.
- The wpa2+smoltcp profile proves Rust WiFi on WS63 is working code, not aspiration — the rust-ws73 tri-mode goal has its WiFi leg demonstrated upstream.

## Comparison anchors (vs existing reports)

- `NEW-WS63-EXAMPLES-RUST.md`: the blob-linking build.rs is this crate's mechanism observed from the consumer side.
- `NEW-HISI-RTOS-SCHEDULER.md`: the runtime layer of the same chain.
- `NEW-FBB-WS63-QEMU-FORK.md`: QEMU runs the C SDK; a Cargo-only Rust stack should be even more emulator-friendly (no vendor toolchain).
