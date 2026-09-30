---
type: harvest
title: "hispark-rs/hisi-rom-sys — the chip-selection facade over HiSilicon mask-ROM facts: one-chip gate, Cargo links forwarding chain, exact-pinned provenance backend, and publish discipline (v0.1.0-alpha.4)"
language: en
created: 2026-09-17
tags: [hispark-rs, rust, no-std, rom, cargo, build-system, ci, provenance, hisilicon, harvest]
sources:
  - "https://github.com/hispark-rs/hisi-rom-sys/tree/fcce718f4f15b042c5cfafe4e88466fc9b12b245"
  - "https://github.com/hispark-rs/hisi-rom-sys/blob/fcce718f4f15b042c5cfafe4e88466fc9b12b245/src/lib.rs"
  - "https://github.com/hispark-rs/hisi-rom-sys/blob/fcce718f4f15b042c5cfafe4e88466fc9b12b245/build.rs"
  - "https://github.com/hispark-rs/hisi-rom-sys/blob/fcce718f4f15b042c5cfafe4e88466fc9b12b245/Cargo.toml"
  - "https://github.com/hispark-rs/hisi-rom-sys/blob/fcce718f4f15b042c5cfafe4e88466fc9b12b245/README.md"
  - "https://github.com/hispark-rs/hisi-rom-sys/blob/fcce718f4f15b042c5cfafe4e88466fc9b12b245/CHANGELOG.md"
  - "https://github.com/hispark-rs/hisi-rom-sys/blob/fcce718f4f15b042c5cfafe4e88466fc9b12b245/.github/workflows/ci.yml"
  - "https://github.com/hispark-rs/hisi-rom-sys/blob/fcce718f4f15b042c5cfafe4e88466fc9b12b245/.github/workflows/publish.yml"
  - "https://github.com/hispark-rs/hisi-rom-sys/blob/fcce718f4f15b042c5cfafe4e88466fc9b12b245/.github/workflows/yank.yml"
  - "https://github.com/hispark-rs/hisi-rom-sys/blob/fcce718f4f15b042c5cfafe4e88466fc9b12b245/Cargo.lock"
  - "https://crates.io/crates/hisi-rom-sys-ws63/0.1.0-alpha.2"
trust: A
stale_after: 2027-03-17
---

# hispark-rs/hisi-rom-sys — the chip-selection facade over HiSilicon mask-ROM facts

- Inspection date: 2026-09-17 (v0.1.0-alpha.4, commit `fcce718f4f15b042c5cfafe4e88466fc9b12b245`, tagged `v0.1.0-alpha.4`, commit dated 2026-07-20)
- Mode: source-only read; no build, network fetch, hardware, or PCB access
- Scope: the facade crate only. The backend crate `hisi-rom-sys-ws63` is a checksum-pinned crates.io dependency and was NOT inspected; backend facts below are changelog-declared, marked as such.

## Executive findings

1. **The facade holds zero ROM facts.** `src/lib.rs` is 9 lines: `#![no_std]`, one cfg-gated re-export (`pub use hisi_rom_sys_ws63 as ws63;` under `chip-ws63`), and a `compile_error!` when no chip feature is enabled (`src/lib.rs:1-9`). `Cargo.toml` confirms the split: `links = "hisi_rom_sys"` with the chip-coupled data living in the optional backend dependency (`Cargo.toml:12,17,20`). README states it outright: "This facade contains no chip address tables or generated payloads" (`README.md:19-23`).
2. **Exactly-one-chip selection is enforced twice, at two stages.** Compile time: the `#[cfg(not(feature = "chip-ws63"))] compile_error!` arm (`src/lib.rs:8-9`). Build time: `build.rs` panics when `CARGO_FEATURE_CHIP_WS63` is unset — "hisi-rom-sys requires one chip feature; enable `chip-ws63`" (`build.rs:8-10`). Today only `chip-ws63` exists, so "exactly one" degenerates to "the one", but the gate shape is written for N backends.
3. **The Cargo `links` forwarding chain is the real API.** The backend exports four paths under its own links key: `DEP_HISI_ROM_SYS_WS63_ROM_SYMBOLS`, `_ROM_CALLBACKS`, `_WIFI_PATCHES`, `_MANIFEST` (`README.md:7-12`). The facade build script re-publishes each one verbatim through its `forward()` helper (`println!("cargo::metadata={destination}={value}")`, `build.rs:1-14`), so downstream crates of `hisi-rom-sys` see the same four `DEP_HISI_ROM_SYS_WS63_*` variables with the facade's links key — a version-stable re-export surface. Any missing variable is a hard panic: "selected ROM backend did not export {source}" (`build.rs:1-5`) — contract drift between facade and backend fails the build, not the link stage.
4. **The backend is an exact-pin with a recorded checksum.** `hisi-rom-sys-ws63 = { version = "=0.1.0-alpha.2", optional = true }` (`Cargo.toml:20`); `Cargo.lock` records `registry+https://github.com/rust-lang/crates.io-index` with checksum `5fc2f9dded6797c89610a734aab74bdb96c9a44399fc38e40bb61ee337a8559d` (`Cargo.lock:15-16`). For ROM-symbol facts, provenance is the feature: the exact backend release is pinned, and CI's build/test/check/package steps all run `--locked` (formatting is checked without it).
5. **Responsibility split at the bottom of the stack is explicit** (`README.md:14-17`): the facade only forwards paths; the *final binary* decides whether symbol assignments become `PROVIDE` linker fallbacks; `hisi-rf-link` remains responsible for generating the patch table against the final ELF. Selection, fallback policy, and patch-table generation are three different owners.
6. **Changelog-declared backend content** (backend not inspected): alpha.4 (2026-07-20) exposes "public security-ROM and PKE instruction metadata needed by the fallible crypto backend while keeping entries that depend on private ROM RAM hidden" (`CHANGELOG.md:5-11`); alpha.3 moved fixed-address/generated metadata into the backend and removed the default chip feature (`CHANGELOG.md:13-19`); alpha.2 normalized artifacts to "symbol assignments and names only" with language-neutral comments and recorded both canonical-source and generated-artifact hashes in the manifest (`CHANGELOG.md:21-27`); alpha.1 added WS63 application-core ROM linker symbols, callback ABI, and Wi-Fi patch metadata with source hashes (`CHANGELOG.md:29-34`).

## Substantive contracts

- **Selection contract**: a consumer must enable exactly one chip feature; violating it is a compile error and, independently, a build-script panic. Two independent tripwires for one invariant.
- **Forwarding contract**: for each of the four metadata variables, the facade forwards the backend value 1:1 under the same `links` family; absence anywhere in the chain aborts the consumer's build with a named-variable panic. The chain is: backend crate → (backend's own links key) → facade build.rs → facade links key `hisi_rom_sys` → downstream `DEP_HISI_ROM_SYS_*`.
- **Provenance contract**: manifest carries dual hashes (canonical source + generated artifact) per alpha.2; artifacts are normalized to symbol assignments and names only (no comments, no addresses of private ROM RAM in the public surface per alpha.4); the canonical upstream input is the `ws63-RF/rom` delivery, with normalization and provenance owned by the backend (`README.md:19-23`).
- **Fallback/patch policy contract**: `PROVIDE` fallback materialization is a binary-level decision; ROM patch-table generation is `hisi-rf-link`'s job against the final ELF — neither belongs to this crate.
- **Hygiene contract**: `unsafe_op_in_unsafe_fn = "deny"` and `undocumented_unsafe_blocks = "deny"` (`Cargo.toml:22-26`); `src/lib.rs` contains zero `unsafe`. `rust-version = "1.85"`, edition 2024 (`Cargo.toml:4-5`).
- **CI contract** (`.github/workflows/ci.yml:15-21`): pinned toolchain `nightly-2026-07-09` with clippy/rust-src/rustfmt; `cargo fmt --all -- --check`; `cargo test --locked --features chip-ws63`; `cargo clippy --locked --all-targets --features chip-ws63 -- -D warnings`; `cargo check -Zbuild-std=core --locked --target riscv32imfc-unknown-none-elf --features chip-ws63`; `cargo package --locked --features chip-ws63`.
- **Publish contract** (`.github/workflows/publish.yml:24-39`): lockfile preflight (`cargo generate-lockfile --locked`, `git ls-files --error-unmatch Cargo.lock`, `git diff --exit-code -- Cargo.lock`), then `cargo publish --locked --no-verify` behind a dry-run input, tolerating an "already uploaded" registry response as success (idempotent republish); registry credentials supplied only via a repository secret expression.
- **Yank contract** (`.github/workflows/yank.yml`): a separate manual `workflow_dispatch` that yanks one exact version of `hisi-rom-sys` — rollback is a deliberate, per-version operation, not a re-publish.

## Dedup novelty

- Not present in `knowledge/harvest/index.md` (154 reports scanned): no entry covers `hisi-rom-sys`. Knowledge-wide grep finds only passing mentions: `NEW-HISPARK-RS-ECOSYSTEM.md:38,46,85,360-362` (the bare repo name in the org list, plus three lines: "chip-neutral facade over mask-ROM facts; Cargo `links` exports ... `hisi-rf-link` generates patch tables against the final ELF") and `NEW-HISI-RF-FACADE.md:23` (ROM symbols named as an exclusion of the RF facade). This report supersedes those three lines with a full-file digest.
- Checked against the closed list — no overlap: hisi-nvs, hisi-alloc, hisi-crypto entropy/DRBG, hisi-fwpkg, hisiflash monitor, hisi-registers conversion, hisi-rtos scheduler, hisi-rf WS63 composition, hispark-rs ecosystem/September increment, DSoftBus monitors, Ai-BS21 OSAL/NV, RSSI pipelines, toolbox docs, NLChat web — none treat the ROM-facts facade.
- Alternatives considered and rejected this round: hisi-rtos non-scheduler modules (scheduling-adjacent overlap risk with the closed scheduler report; source rendering in the local environment proved unreliable for line-exact citation), ws63-pac/bs2x-pac generation contract (substantively pre-covered by the September increment's SVD-to-PAC regeneration note), fbb_ws63 SDK slice (heavily harvested), communication_nearlink_service slice (covered by the OHOS September increment report).

## Runtime limits

- No runtime: the crate is metadata-only (`no_std`, zero `unsafe`, no code beyond a re-export). There is nothing to execute, benchmark, or fuzz here; the artifact under contract is a set of paths and a manifest.
- The ROM facts themselves (symbols, callback slots, Wi-Fi patch offsets) are out of scope of this crate and were not inspected — the backend exists only as a checksum-pinned registry dependency here. Claims about backend content are therefore changelog-declared, not line-read.
- Clone basis: shallow depth-1 at the alpha.4 tag; per-file history before `fcce718` was not available, so the 2026-07-13 alpha.1-alpha.3 dates rest on the changelog text alone.

## Comparison anchors (vs existing reports)

- `NEW-HISI-RF-FACADE.md`: same facade pattern one level up — the RF facade excludes ROM symbols (`:23`), and this crate is exactly where those symbols are selected and forwarded instead.
- `NEW-HISI-CRYPTO-ENTROPY-DRBG.md`: alpha.4's public security-ROM/PKE instruction metadata exists to serve a fallible crypto backend — the entropy/DRBG report is the consumer-side counterpart.
- `NEW-HISPARK-RS-SEPT-INCREMENT.md`: the FRW slot-261 ROM callback ABI in `ws63-radio-sys` is the runtime consumer of exactly the ROM-callback facts this facade forwards; the two reports bracket the ROM-callback plane (selection vs use).
- `NEW-FBB-WS63-QEMU-FORK.md`: the emulator intercepts mask-ROM calls to run vendor firmware — an emulator-side consumer of the same facts, useful when modeling what the backend's symbol table must describe.
- `NEW-HISPARK-RS-ECOSYSTEM.md`: dependency graph row (`:85`) and the three-line facade mention (`:360-362`) are now superseded by this digest.

---

*Method note: every quoted string and line reference was verified against the committed git objects of the pinned revision (full-object comparison of all 12 tracked files, plus exact-match presence checks for each quoted pattern); the shallow clone origin is `https://github.com/hispark-rs/hisi-rom-sys.git`. The crates.io backend was not fetched.*
