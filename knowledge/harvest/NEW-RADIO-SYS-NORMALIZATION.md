---
type: harvest
title: "hispark-rs/ws63-radio-sys — three-package release unit with a byte-reproducible blob-normalization pipeline (relocation inventory, cross-compile ABI verification, canonical builder)"
language: en
created: 2026-09-13
tags: [rust, blob-normalization, reproducible-builds, relocation, abi, hostap, ci, harvest]
sources:
  - "ArchivalEra/nearlink-E573H-USBDC workspace"
trust: verified
stale_after: 2026-12-13
---

# hispark-rs/ws63-radio-sys — three-package release unit with a byte-reproducible blob-normalization pipeline (relocation inventory, cross-compile ABI verification, canonical builder)

- Inspection date: 2026-09-13 (local hispark-rs subrepo, current with upstream 2026-09-09)
- Mode: read-only local program inspection; no build, network, hardware, or PCB access
- Scope: how a vendor radio blob becomes a Cargo-delivered, byte-reproducible package

## Executive findings

1. The release unit is **three Cargo packages versioned and tagged together**: `ws63-radio-sys` (a `no_std` crate that identifies the vendor archive ABI and exports link contracts to dependent build scripts through Cargo `links` metadata), `ws63-radio-blob` (redistributable **normalized** WS63 target archives plus pinned upstream hostap target archives, delivered through Cargo **without build-time downloads**), and `hisi-rf-link` (the maintainer tool and pure-Rust library for **relocation inventory, normalization, verification, and compatibility profiles**). [README.md:3-11]
2. The CI pipeline is **byte-reproducibility-enforced**: it rebuilds the normalized vendor archives from the pinned `ws63-RF` submodule, **compares their bytes and manifest against the Cargo payload**, cross-compiles the complete pinned hostap source profiles for ABI verification, and a **separate canonical macOS builder rebuilds both hostap archives with a pinned GCC/binutils/cc-rs contract requiring a byte-for-byte match**. [README.md:13-18]
3. Publishing is dependency-ordered with registry-visibility waits (`hisi-rf-link` → `ws63-radio-blob` → `ws63-radio-sys`), idempotent for already-published versions; manual runs are dry-run only, uploads happen only on matching tags — Cargo cannot package the final crate until its exact blob dependency is indexed, so CI sequences that explicitly. [README.md:19-24]
4. The repo carries `port/`, `linker/`, `upstream/`, `third-party/`, `ws63-RF/` (the pinned submodule), `scripts/`, `tests/` — the full normalization workbench, not just metadata. [repo tree]

## Boundaries and gaps

- The relocation-inventory algorithm inside `hisi-rf-link` was not read; only its role is documented.
- Which blob objects require relocation entries (and how normalization rewrites them) is the deep detail behind this pipeline — future dig target in `port/` and `hisi-rf-link` sources.

## Reusable for our stack

- **This pipeline is the reference for vendoring ANY vendor binary blob into a reproducible build system**: normalize once with an inventory tool, deliver via the package manager without build-time downloads, verify byte-identity with a canonical builder on a second platform, and gate publishing on the full chain.
- The `links`-metadata contract pattern (a -sys crate exporting link contracts to downstream build scripts) is the Cargo-idiomatic way to couple blob versioning to consumers.
- Byte-for-byte cross-platform rebuild contracts (pinned toolchains, canonical builder) are a supply-chain trust model our own releases could adopt.

## Comparison anchors (vs existing reports)

- `NEW-HISI-RF-WS63-COMPOSITION.md`: the consumer of these crates; the composition root's "Cargo-delivered normalized archives" is implemented here.
- `NEW-WS63-EXAMPLES-RUST.md`: the `DEP_WS63_RADIO_SYS_LIB_DIR` environment variable that build.rs consumes is this crate's `links` metadata in action.
- `NEW-HISPARK-RS-ECOSYSTEM.md`: completes the story of the ecosystem's most infrastructure-grade subrepo.
