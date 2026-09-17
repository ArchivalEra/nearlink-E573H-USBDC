---
type: harvest
title: "hisi-fwpkg container-to-image planning: partition selection, hash ownership and range contracts"
language: en
created: 2026-09-17
tags: [harvest, hispark-rs, rust, hisi-fwpkg, image-planning, containers, source-contracts]
sources:
  - "https://github.com/hispark-rs/hisi-fwpkg/blob/366c2cfde859d20efe7278c79f8b6de347a27ef1/crates/hisi-fwpkg/src/plan.rs"
  - "https://github.com/hispark-rs/hisi-fwpkg/blob/366c2cfde859d20efe7278c79f8b6de347a27ef1/crates/hisi-fwpkg/src/fwpkg.rs"
  - "https://github.com/hispark-rs/hisi-fwpkg/blob/366c2cfde859d20efe7278c79f8b6de347a27ef1/crates/hisi-fwpkg/src/lib.rs"
  - "https://github.com/hispark-rs/hisi-fwpkg/blob/366c2cfde859d20efe7278c79f8b6de347a27ef1/crates/hisi-fwpkg/src/image.rs"
  - "https://github.com/hispark-rs/hisi-fwpkg/blob/366c2cfde859d20efe7278c79f8b6de347a27ef1/crates/hisi-fwpkg/src/patch.rs"
  - "https://github.com/hispark-rs/hisi-fwpkg/blob/366c2cfde859d20efe7278c79f8b6de347a27ef1/crates/hisi-fwpkg/src/elf.rs"
  - "https://github.com/hispark-rs/hisi-fwpkg/blob/366c2cfde859d20efe7278c79f8b6de347a27ef1/crates/hisi-fwpkg/src/error.rs"
  - "https://github.com/hispark-rs/hisi-fwpkg/blob/366c2cfde859d20efe7278c79f8b6de347a27ef1/crates/hisi-fwpkg/tests/vendor_parity.rs"
  - "https://github.com/hispark-rs/hisi-fwpkg/blob/366c2cfde859d20efe7278c79f8b6de347a27ef1/crates/hisi-fwpkg-cli/src/main.rs"
  - "https://github.com/hispark-rs/hisi-fwpkg/blob/366c2cfde859d20efe7278c79f8b6de347a27ef1/Cargo.toml"
  - "https://github.com/hispark-rs/hisi-fwpkg/blob/366c2cfde859d20efe7278c79f8b6de347a27ef1/crates/hisi-fwpkg/Cargo.toml"
  - "https://github.com/hispark-rs/hisi-fwpkg/blob/366c2cfde859d20efe7278c79f8b6de347a27ef1/README.md"
  - "https://github.com/hispark-rs/hisi-fwpkg/blob/366c2cfde859d20efe7278c79f8b6de347a27ef1/CHANGELOG.md"
trust: B
stale_after: 2027-03-17
---

# hisi-fwpkg container-to-image planning

## Scope and provenance

One implementation topic: how a package or image becomes the library's canonical, single-application `FlashPlan`, and what that conversion does and does not promise. Container parsing, image normalization and app-only repackaging are examined only as parts of that boundary. This is not a release survey, flashing protocol investigation, signing implementation or hardware procedure.

The inspected checkout's configured origin is `https://github.com/hispark-rs/hisi-fwpkg.git`. Its HEAD is `366c2cfde859d20efe7278c79f8b6de347a27ef1`, dated 2026-09-07, subject `release: publish load-address image semantics as 0.3.3`, tree `8008d2310235aaf38974bad51be6aff47ea5ae73`. The checkout is shallow. Origin and object identities were verified from local Git metadata only; remote availability, current upstream freshness and registry artifacts were not checked.

Every file below was read fully. Hashing the read working-copy bytes with Git's blob algorithm, without filters or object writes, matched its entry in the pinned HEAD tree. Paths and subsequent line references are repository-relative to this upstream revision.

| Fully read file | Git blob SHA-1 |
|---|---|
| `crates/hisi-fwpkg/src/plan.rs` | `3568e079e5a6992dbb64288afcb208ca270a9b41` |
| `crates/hisi-fwpkg/src/fwpkg.rs` | `9865d637b91f5012e5b221b3c8ea54b7e2f9c1db` |
| `crates/hisi-fwpkg/src/lib.rs` | `7709dfe2adee948514c8276c9ae08da053039f75` |
| `crates/hisi-fwpkg/src/image.rs` | `91df9e75f3f290a73fe8acf8d6b47c53f496bccd` |
| `crates/hisi-fwpkg/src/patch.rs` | `632c60e51db0ecd15a949dd58260fbdfaf36e3f3` |
| `crates/hisi-fwpkg/src/elf.rs` | `dda9996095486d67acb48fc155e1550cf4afacc5` |
| `crates/hisi-fwpkg/src/error.rs` | `e63032a365e28719c0bb6e2f56e5c63152bdfd34` |
| `crates/hisi-fwpkg/tests/vendor_parity.rs` | `ad5e3dde254cc567affa52a960630824d115dcab` |
| `crates/hisi-fwpkg-cli/src/main.rs` | `70f7d297c66e9ef03ae8f7f245b6465e56bccecb` |
| `Cargo.toml` | `895ca5f0430efcc9da86afb99348a468dcb72d2d` |
| `crates/hisi-fwpkg/Cargo.toml` | `42c3a9642f044a1b2155cc6642519c9221ca137e` |
| `README.md` | `4268c9e0ba51801ae0ae04625799a048067df986` |
| `CHANGELOG.md` | `3e024dc72590e8ef6e8be82399c34454b9901437` |

Trust B means source-derived interpretation, not vendor specification authority or observed runtime success. No network, build, project execution, binary-fixture inspection, hardware access or device-memory access was performed. Test definitions were read, not run.

## Deduplication and exact novelty

The [harvest index](index.md) and the complete knowledge bundle were searched for `hisi-fwpkg`, its underscore spelling, `plan_app_flash`, `FwpkgSourceInfo`, `build_plan_from_image`, `headered_elf_to_app_image`, and package/container/CRC planning terminology. Relevant prior report sections were read rather than relying on search titles alone.

- [The ecosystem report](NEW-HISPARK-RS-ECOSYSTEM.md), section 9, already establishes the producer role, fixed application header, chip presets and companion flash tools. These are context, not new findings.
- [The September increment](NEW-HISPARK-RS-SEPT-INCREMENT.md), section 3, already archives release 0.3.3 and load-address semantics. This report does not repackage that release delta as a new topic; ELF behavior appears below only to delimit the planner's address contract.
- [The C flasher report](NEW-WS63FLASH-GHIDRA.md), sections 1.7-1.8, and [the Python packager report](NEW-XF-BURN.md), section 5, already establish V1 descriptors, CRC coverage, image-header construction and a historical package partition map.
- [The browser packager report](NEW-WEB-FLASHER-FWPKG.md) and [the quadruple synthesis](NEW-WEB-FLASHER-QUADRUPLE.md) cover the parser and transport family, not this Rust canonical plan.
- [The entropy report](NEW-HISI-CRYPTO-ENTROPY-DRBG.md), its available-software table, explicitly lists image planning as an unselected implementation surface. It does not archive these contracts.

The new coherent contribution is the **container-to-single-app planning contract**: selection fallback ordering, diagnostic versus enforcing validation, input-dependent hash ownership, distinction between preserved source metadata and effective output metadata, and erase/write range asymmetry. No matching implementation treatment was found in the bundle. Persistence, allocation, entropy, scheduler and radio-composition topics remain closed and are not subdivided here.

## Executive findings

1. **App selection is a heuristic, not an exact-name requirement.** An exact name wins; otherwise the first non-loader descriptor whose name contains `app` or whose type is app-related wins; otherwise a sole non-loader descriptor wins. An explicit missing name still falls back, and `normal_bins()` means every type except Loader, not only `PartitionType::Normal`. Evidence: `crates/hisi-fwpkg/src/plan.rs:259-295`; `crates/hisi-fwpkg/src/fwpkg.rs:428-458`.
2. **Parsing and planning do not imply integrity verification.** The package reader checks structural availability and payload bounds; CRC verification is a separate method. Planning records `crc_valid` but does not reject a false result. Package-extracted image hashes are copied from the header, not independently verified. Evidence: `crates/hisi-fwpkg/src/fwpkg.rs:312-398,415-426`; `crates/hisi-fwpkg/src/plan.rs:181-220,293-306`.
3. **Hash ownership depends on input kind.** Standalone headered bytes are rehashed on the default ELF-enabled path; a selected package payload is not. Both reach padding that materializes a declared missing tail as `0xFF`. Thus a populated `code_area_hash` field is not a universal computed-digest receipt. Evidence: `crates/hisi-fwpkg/src/plan.rs:135-181,293-294`; `crates/hisi-fwpkg/src/lib.rs:137-164`; `crates/hisi-fwpkg/src/patch.rs:168-228`.
4. **Erase extent and write extent deliberately differ.** The plan emits one complete-image write chunk, while package `burn_size` can enlarge its logical erase range. Explicit address override wins over package address, which wins over the chip preset. Source descriptor addresses remain unchanged in the diagnostic metadata. Evidence: `crates/hisi-fwpkg/src/plan.rs:168-173,195-244,297-319`.
5. **Repacking is a projection, not package preservation.** `pack_app_fwpkg` selects one image through the planner, emits a new V1 `Normal` partition and discards the source package's other partitions and larger reserved erase size. The return value is not a lossless round trip of a V1 or V2 input package. Evidence: `crates/hisi-fwpkg/src/lib.rs:225-240`; `crates/hisi-fwpkg/src/fwpkg.rs:486-508,531-588`.
6. **Current source contains important evidence limits.** The headered-ELF body anchor is WS63-specific despite the public chip choice; lower-level helpers do not all share identical normalization; vendor parity tests cover selected fields rather than full-file parity. These are integration boundaries, not proof of runtime failure. Evidence: `crates/hisi-fwpkg/src/patch.rs:35-36,73-156`; `crates/hisi-fwpkg/src/lib.rs:166-223`; `crates/hisi-fwpkg/tests/vendor_parity.rs:20-63`.

## 1. Container admission and selection

### Reader contract

`Fwpkg::from_bytes` owns the original byte vector and parses a header plus descriptor vector. V1 uses a 12-byte header and 52-byte descriptors; V2 uses a 272-byte header and 284-byte descriptors with 260-byte names. The implementation accepts the declared V2 magic range and preserves unknown numeric partition types through `Unknown(u32)`. Names stop at the first zero and use lossy UTF-8 decoding. Evidence: `crates/hisi-fwpkg/src/fwpkg.rs:33-54,176-225,294-398,605-611`.

The actual admission checks are narrower than a complete package validator:

- Magic, a maximum of 255 descriptors, descriptor-table availability and each payload's checked in-file end are enforced.
- The recorded total file size is retained as metadata, not compared with the byte-vector length.
- CRC is not automatically checked by `from_bytes`.
- Descriptor overlap, payload placement after the table and erase-region suitability are not established by these checks.

Evidence: `crates/hisi-fwpkg/src/fwpkg.rs:245-250,312-398,415-426`. These are API boundary observations only; no malformed-input reproduction or exploitation workflow was attempted.

### Selector precedence

For the requested name, defaulting to `app`, `fwpkg_to_app_image` applies this order:

| Priority | Actual selection rule |
|---|---|
| 1 | First exact, case-sensitive name match across all descriptors |
| 2 | First non-loader entry with case-insensitive name substring `app`, or type `AppsA`, `App`, or `AppSign` |
| 3 | The sole non-loader entry, if exactly one exists |
| Otherwise | `InvalidFwpkg` selection error |

An exact match is not filtered by partition type. Substring/type selection is ordered by descriptor order, without rejecting multiple candidates. A requested name therefore expresses preference rather than strict identity. The final image still must satisfy the planner's header checks. Evidence: `crates/hisi-fwpkg/src/plan.rs:259-295`; `crates/hisi-fwpkg/src/fwpkg.rs:433-458`.

Source metadata includes the entire descriptor table, package version/name, recorded total size and CRC status. It does not contain an explicit selected-descriptor index. When a caller overrides the effective address, the preserved descriptors still report their original addresses. Evidence: `crates/hisi-fwpkg/src/plan.rs:56-94,168-173,297-319`.

## 2. Normalization and hash ownership

The planner's input dispatch is ordered: recognizable package magic, ELF magic, standalone headered image, otherwise raw body. Headered-image detection in the planner requires the fixed header length and key-area ID; it does not require the code-info ID. Evidence: `crates/hisi-fwpkg/src/plan.rs:135-166,247-257`.

| Input class | Materialization path | Hash behavior |
|---|---|---|
| Raw body | Construct header, append body | Compute body SHA-256 and both length fields |
| ELF without `.boot_header` | Flatten file-backed loads, then construct header | Compute hash over flattened body |
| ELF with `.boot_header` | Patch header in an ELF copy, extract header and body, pad declared tail | Compute hash over declared body length, treating missing tail as `0xFF` |
| Standalone headered image, default feature path | Patch existing header, then build plan | Recompute hash; if declared length is zero, fill both length fields |
| Selected package payload | Copy payload directly, then build plan | Preserve stored hash and length fields; no rehash or digest comparison |

Evidence: `crates/hisi-fwpkg/src/image.rs:142-213`; `crates/hisi-fwpkg/src/lib.rs:117-188`; `crates/hisi-fwpkg/src/plan.rs:135-181,293-294`; `crates/hisi-fwpkg/src/patch.rs:159-228`.

Padding only extends the image when its header declares a nonzero body length longer than present bytes. It does not truncate a longer payload or recompute a package payload's digest. Conversely, a shorter declared body can leave additional image bytes outside the reported hashed body, while the write chunk still covers the complete image. For a zero declared length, the plan reports the available body length even when the package header itself still contains zero. The `FlashPlan.code_area_len` field is therefore an effective length on this path, despite its doc comment describing a header value. Evidence: `crates/hisi-fwpkg/src/lib.rs:143-164`; `crates/hisi-fwpkg/src/plan.rs:195-241`.

The separate public helpers must not be assumed interchangeable:

- `build_app_image_from_input` treats flattened or raw input as a body to wrap; it is not the canonical existing-header/package detector.
- `input_to_app_image` recognizes raw headered images by both key and code IDs, unlike the planner's key-only recognition.
- `plan_app_flash` adds package selection and source metadata beyond those helpers.

Evidence: `crates/hisi-fwpkg/src/lib.rs:166-223`; `crates/hisi-fwpkg/src/plan.rs:135-173,247-256`.

## 3. Address and range contracts

`FlashPlan` owns complete image bytes, effective base address, body range, image length, hash field, logical erase range, one or more write descriptors and optional source package metadata. With serde enabled, raw image bytes are deliberately excluded from serialization. A JSON plan is therefore metadata, not a self-contained payload. Evidence: `crates/hisi-fwpkg/src/plan.rs:22-54,103-131`; CLI materialization and JSON output: `crates/hisi-fwpkg-cli/src/main.rs:182-205`.

The current builder always emits a single chunk covering `image_bytes[0..image_len]`. Its erase length starts at `image_len` and becomes `max(image_len, source burn_size)` for package input. The larger erase-only suffix is not emitted as a second write chunk. Alignment to sector geometry is explicitly left to the transport, which may enlarge but must not shrink the logical erase range. Evidence: `crates/hisi-fwpkg/src/plan.rs:122-127,168-171,222-244`.

Address precedence is `PackOptions.app_addr`, then selected package `burn_addr`, then `Chip::app_partition_addr()`. This chooses output placement metadata; it does not relocate executable contents. Raw-ELF conversion discards the base address returned by `flatten_elf`, and headered-ELF extraction uses the fixed WS63 body anchor in `patch.rs` rather than the selected chip or override. Consequently, chip/address choices alone do not prove a binary's linkage is compatible with that destination. Evidence: `crates/hisi-fwpkg/src/lib.rs:85-101,168-174`; `crates/hisi-fwpkg/src/plan.rs:168,198,226-229`; `crates/hisi-fwpkg/src/patch.rs:35-36,121-154`.

The already archived load-address rule remains supporting context: ELF32 file-backed `PT_LOAD` segments are sorted by physical load address, gaps become `0xFF`, and zero-file-size segments do not extend the output. There is no new release claim here. Evidence: `crates/hisi-fwpkg/src/elf.rs:24-66`; comparison: [September increment](NEW-HISPARK-RS-SEPT-INCREMENT.md).

## 4. App-only repackaging and metadata loss

`pack_app_fwpkg` invokes the planner, takes its image and effective base, selects an output name, creates a `Normal` partition and calls the V1 writer. `Partition::new` resets `burn_size` to payload length. Thus a source package with a larger reserved erase extent can produce an app-only package that no longer carries that reservation. V2 package naming, other partitions and source CRC metadata are likewise absent from the output. Evidence: `crates/hisi-fwpkg/src/lib.rs:232-240`; `crates/hisi-fwpkg/src/fwpkg.rs:486-508,531-588`.

One option is overloaded: `PackOptions.app_name` is both the preferred source selection name and the output partition name. The CLI `Pack` command passes its `name` into that option. It is not a pure rename operation when its input is a package. Evidence: `crates/hisi-fwpkg/src/plan.rs:265-268`; `crates/hisi-fwpkg/src/lib.rs:237-238`; `crates/hisi-fwpkg-cli/src/main.rs:146-161`.

The writer rejects empty inputs and names of 32 bytes or more, appends 16 zero separator bytes after each payload, and computes table-only CRC16/XMODEM. These layout constants corroborate existing reports, rather than constituting novelty. A narrower round-trip limitation is new here: the writer does not apply the parser's 255-descriptor admission ceiling, so writer and reader acceptance domains are not identical. Evidence: `crates/hisi-fwpkg/src/fwpkg.rs:43-44,245-250,531-588`.

The CLI's `Pack` status line derives its displayed address from the override or chip preset rather than the selected package's effective address. For package input that uses a different descriptor address without override, the emitted package and display calculation can therefore disagree. This is a static control-flow observation, not an executed CLI result. Evidence: `crates/hisi-fwpkg-cli/src/main.rs:160-169`; `crates/hisi-fwpkg/src/lib.rs:234-239`; `crates/hisi-fwpkg/src/plan.rs:168`.

## 5. Test evidence and runtime limits

### Source-defined assertions, not executed outcomes

| Test source | What its assertions actually establish as intent |
|---|---|
| `crates/hisi-fwpkg/src/plan.rs:334-375` | Raw-body full-image chunking; standalone-header rehashing; declared tail materialization with matching hash and lengths |
| `crates/hisi-fwpkg/src/plan.rs:377-407` | App selection from a multi-entry package; descriptor address use; larger reserved erase extent; source metadata retention |
| `crates/hisi-fwpkg/src/lib.rs:273-283` | App-only packing avoids adding a second header to an already constructed image |
| `crates/hisi-fwpkg/src/fwpkg.rs:617-688` | Known CRC vector, V1 layout and parse round trip, long-name and empty-input errors |
| `crates/hisi-fwpkg/src/patch.rs:238-301,408-447` | Tail hashing, unset-length repair and a synthetic initialized-data ELF hash layout |
| `crates/hisi-fwpkg/src/elf.rs:98-128` | Synthetic load-address ordering, erased gaps and omission of zero-file-size loads |
| `crates/hisi-fwpkg/tests/vendor_parity.rs:20-63` | Selected structural prefixes and encryption flag match fixture fields; constructed body hash matches SHA-256 |

The parity test does not assert whole-file equality excluding signatures, despite broader README/changelog wording. Its checked structural windows are `[0x00,0x3C)`, `[0x100,0x120)` and `[0x148,0x14C)`, plus a hash comparison against the constructed body. Fixture binaries were not opened or executed. Evidence: `crates/hisi-fwpkg/tests/vendor_parity.rs:17-63`; comparison claims: `README.md:119-130`, `CHANGELOG.md:86-87`.

The read test modules do not exercise the full selector ambiguity matrix, CRC-status rejection policy, V2 package planning, preservation of package hash fields or a larger-reservation repack round trip. Their absence from these files is not a claim about every downstream test suite.

### Explicit limits

- This is an allocating host-side library using owned vectors, strings and file I/O, not a transport or an embedded `no_std` streaming planner. No memory-use benchmark or input-size stress test was performed. Evidence: `crates/hisi-fwpkg/src/fwpkg.rs:28-31,294-310`; `crates/hisi-fwpkg/src/plan.rs:103-130`; `crates/hisi-fwpkg/src/elf.rs:54-65`.
- Default features include `elf`. Although planner branches describe behavior without it, `input_to_app_image` contains an unguarded call to `patch_hash`, whose re-export is feature-gated. No-default-feature build support is therefore not established by this inspection; no build was attempted. Evidence: `crates/hisi-fwpkg/Cargo.toml:26-30`; `crates/hisi-fwpkg/src/lib.rs:65-66,209-223`; `crates/hisi-fwpkg/src/plan.rs:150-158`.
- The builder checks image-length representability but does not establish complete device address-space validity or sector geometry. Placement, compatible linkage, storage capacity and transport completion remain caller responsibilities. Evidence: `crates/hisi-fwpkg/src/plan.rs:122-127,195-244`.
- Header construction fills structural fields and computes a digest; it is not cryptographic signing or authentication. Existing comments about device verification are inconsistent: `README.md:100-117` and `image.rs:16-30` say the body hash is skipped in one configuration, while `patch.rs:3-11` and `CHANGELOG.md:66-72` say it is checked. This source-only report does not resolve device behavior and does not infer boot success from either assertion.
- No WS73 USB behavior, SLE runtime compatibility, actual erase/write success, restart outcome, power-loss behavior or physical-device compatibility follows from a `FlashPlan`. No device actions are proposed here.

## Comparison and reusable conclusion

Compared with [the Python report](NEW-XF-BURN.md), which distinguishes payload length from reserved size but describes a consumer using length for erase computation, this library exposes a separate logical erase range and preserves a larger package reservation during planning. That distinction does not survive its app-only repackaging helper.

Compared with [the C image/header report](NEW-WS63FLASH-GHIDRA.md) and [the browser parser report](NEW-WEB-FLASHER-FWPKG.md), the reusable addition is not another constant table: it is an explicit host-side boundary between package metadata, selected image bytes and downstream transport work.

The useful design lesson is to keep four claims distinct: **a descriptor was selected, bytes were materialized, metadata was reported, and integrity was verified**. At this revision, the planner performs the first three with input-dependent normalization; it is not a universal verifier. Consumers can reuse the complete-image and logical-range representation while treating source selection, digest provenance and transport success as separate responsibilities.
