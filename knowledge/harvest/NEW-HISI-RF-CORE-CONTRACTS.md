---
type: harvest
title: hispark-rs/hisi-rf-core — chip-neutral radio contracts with a credential-excluding allocation-free diagnostic schema (v2)
language: en
created: 2026-09-13
tags: [rust, contracts, traits, diagnostics, wifi-backend, smoltcp, embassy-net, harvest]
sources:
  - url: https://github.com/hispark-rs/hisi-rf-core
    note: local clone (current); read-only inspection of README and src trait listing
trust: verified
stale_after: 2026-12-13
---

# hispark-rs/hisi-rf-core — chip-neutral radio contracts with a credential-excluding allocation-free diagnostic schema (v2)

- Inspection date: 2026-09-13 (local hispark-rs subrepo, current)
- Mode: read-only local program inspection; no build, network, hardware, or PCB access
- Scope: the portable radio contracts all backends implement

## Executive findings

1. The contract layer owns **Wi-Fi control, L2 device ownership, a mandatory background runner, and bounded events — explicitly without owning an IP stack**: applications drive TCP/IP through `embassy-net` or the optional `smoltcp::phy::Device` adapter. Backend crates (`hisi-rf-ws63`) implement `WifiBackend`; the facade (`hisi-rf`) is the only selection point. [README.md:1-8]
2. All three radios carry contracts: `wifi.rs` (`WifiBackend` at line 508), `ble.rs`, `sle.rs`, plus `control.rs`, `state.rs`, `diagnostics.rs`, and an `incremental/` experiment module (`IncrementalWifiBackend`, `IncrementalWaitPlatform` traits) behind a non-default feature. [src/ trait listing]
3. The **diagnostic schema v2 is the design standout**: errors expose an allocation-free, versioned diagnostic view with stable machine codes and recovery actions; the schema **deliberately excludes SSID, passphrase, key material, and arbitrary backend text** while preserving a lossless numeric backend code; v2 adds an immutable backend profile revision and a fixed four-entry numeric trace so protocol stages and raw statuses stay observable without allocation or text logs. [README.md:10-14]
4. Dependency hygiene is contractual: "no PAC, radio blob, scheduler, allocator, ROM, NVS, TLS, or image format dependency" — the contract crate cannot accrete vendor baggage. [README.md:16]
5. Status honesty is in the README: early alpha, public surface may change. [README.md:17-18]

## Boundaries and gaps

- Trait method signatures were not read; the L2-device ownership model (how the runner and IP stack share the radio) is the deepest unexamined part.
- The incremental-backend A5B experiment is non-default and unstable.

## Reusable for our stack

- The **credential-excluding diagnostic schema** (numeric codes + recovery actions + bounded trace, no text, no secrets) is the best error-reporting design we have archived — directly applicable to our SSAP stack's error surface and our USB diagnostics.
- Contracts-without-IP-stack (letting the app choose embassy-net vs smoltcp) keeps our rust-ws73 layering honest.
- The exclusion list as a README contract ("this crate must never depend on...") is a dependency-hygiene pattern worth adopting in our own crate docs.

## Comparison anchors (vs existing reports)

- `NEW-HISI-RF-WS63-COMPOSITION.md`: the backend implementing these contracts.
- `NEW-SSAP-LINK-PLANE.md`: comparable contract-first thinking on the SSAP side.
- `RUST-WS73-DEEP-MODULES.md`: this is a living example of the seam discipline that plan prescribes.
