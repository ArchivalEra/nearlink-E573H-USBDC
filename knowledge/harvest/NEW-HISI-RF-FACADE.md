---
type: harvest
title: "hispark-rs/hisi-rf — the thin facade completing the five-layer chain; named profiles include wpa3-smoltcp"
language: en
created: 2026-09-13
tags: [rust, facade, wpa3, profile, feature-flags, harvest]
sources:
  - "ArchivalEra/nearlink-E573H-USBDC workspace"
trust: verified
stale_after: 2026-12-13
---

# hispark-rs/hisi-rf — the thin facade completing the five-layer chain; named profiles include wpa3-smoltcp

- Inspection date: 2026-09-13 (local hispark-rs subrepo, current)
- Mode: read-only local program inspection; no build, network, hardware, or PCB access
- Scope: the application-facing selection layer of the Rust radio stack

## Executive findings

1. The facade is deliberately thin: `lib.rs` + `ws63_diagnostics.rs`, re-exporting the chip-neutral contracts from `hisi-rf-core` and selecting a chip composition root through an explicit `chip-*` Cargo feature — applications never name backend crates. [README.md:1-8]
2. Selection is by **named composition profiles**, not ad-hoc config: `features = ["chip-ws63", "profile-wifi-wpa2-smoltcp"]`, and a **`profile-wifi-wpa3-smoltcp` profile also exists** — WPA3 is already wired into the Rust WS63 WiFi story. [README.md:6-9, 17]
3. The dependency boundary holds at the top: vendor archives, ROM symbols, schedulers, TLS, NVS formats, and image packaging "remain outside the facade API"; resource construction is namespaced (`hisi_rf::ws63`). [README.md:10-16]

## Boundaries and gaps

- Two-file facade; the diagnostics re-export was not read in depth.
- The full profile list (beyond wpa2/wpa3-smoltcp) and the A5B incremental experiment's facade exposure were not enumerated.

## Reusable for our stack

- Feature-named composition profiles (`profile-<capability>-<stack>`) are the cleanest Cargo selection idiom we have seen for multi-capability firmware — adopt for rust-ws73.
- WPA3 availability upstream means our tri-mode WiFi leg should plan for WPA3 from the start, not WPA2-only.

## Comparison anchors (vs existing reports)

- `NEW-HISI-RF-CORE-CONTRACTS.md` / `NEW-HISI-RF-WS63-COMPOSITION.md` / `NEW-RADIO-SYS-NORMALIZATION.md`: completes the five-layer chain documentation (facade → contracts → backend → blob-sys → rtos).
- `NEW-WS63-EXAMPLES-RUST.md`: consumer-side examples select through this facade.
