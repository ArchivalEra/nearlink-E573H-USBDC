---
type: harvest
title: "hisi-registers conversion contract: bootstrap normalization, SVD export and validation boundaries"
language: en
created: 2026-09-17
tags: [harvest, hisi-registers, systemrdl, cmsis-svd, pac, source-contract, validation]
sources:
  - "https://github.com/hispark-rs/hisi-registers/blob/4fb181dcada788f2288ec09e1df1f6f26b4996df/scripts/import_svd_baseline.py"
  - "https://github.com/hispark-rs/hisi-registers/blob/4fb181dcada788f2288ec09e1df1f6f26b4996df/scripts/export_svd.py"
  - "https://github.com/hispark-rs/hisi-registers/blob/4fb181dcada788f2288ec09e1df1f6f26b4996df/scripts/check.py"
trust: B
stale_after: 2027-03-17
---

# hisi-registers conversion contract: bootstrap normalization, SVD export and validation boundaries

## Scope and evidence identity

One implementation topic: the experimental SVD-to-SystemRDL bootstrap and SystemRDL-to-SVD validation boundary. This is a source contract, not a peripheral-behavior audit, PAC migration, SDK equivalence verification or runtime demonstration.

The inspected repository records origin `https://github.com/hispark-rs/hisi-registers.git` and HEAD `4fb181dcada788f2288ec09e1df1f6f26b4996df` (`feat: model shared register IP across chips`, commit date 2026-07-17). Each file below was read completely. Its working-file Git hash matched the corresponding committed blob at that revision; hashing did not write objects. Origin configuration and locally available objects establish the inspection identity, not current remote freshness or a signed authenticity claim.

| Fully read source, pinned to the inspected commit | Verified Git blob SHA |
|---|---|
| [scripts/import_svd_baseline.py](https://github.com/hispark-rs/hisi-registers/blob/4fb181dcada788f2288ec09e1df1f6f26b4996df/scripts/import_svd_baseline.py) | `708b6083c9be8b1c5b6749aaba98773b838abfc9` |
| [scripts/export_svd.py](https://github.com/hispark-rs/hisi-registers/blob/4fb181dcada788f2288ec09e1df1f6f26b4996df/scripts/export_svd.py) | `0e6cc020d5823b2977d13984123c2a3b51cddd28` |
| [scripts/check.py](https://github.com/hispark-rs/hisi-registers/blob/4fb181dcada788f2288ec09e1df1f6f26b4996df/scripts/check.py) | `05551131a9a4939ff51402e3aa7d857193b525c4` |
| [scripts/generate.sh](https://github.com/hispark-rs/hisi-registers/blob/4fb181dcada788f2288ec09e1df1f6f26b4996df/scripts/generate.sh) | `d28c89598629dbc084027b6e78d1b0afb7414e74` |
| [scripts/check.sh](https://github.com/hispark-rs/hisi-registers/blob/4fb181dcada788f2288ec09e1df1f6f26b4996df/scripts/check.sh) | `5faa1d475bd96fa003c066ee9a8db0f293b64b16` |
| [rdl/properties.rdl](https://github.com/hispark-rs/hisi-registers/blob/4fb181dcada788f2288ec09e1df1f6f26b4996df/rdl/properties.rdl) | `9f9cedc9699fa2675d5262dd13e4db101f9b33ac` |
| [rdl/chips/ws63/IMPORT-MANIFEST.txt](https://github.com/hispark-rs/hisi-registers/blob/4fb181dcada788f2288ec09e1df1f6f26b4996df/rdl/chips/ws63/IMPORT-MANIFEST.txt) | `0b2d2d2a4beb4f4bafbbd8f19e834b818ff0e309` |
| [rdl/chips/bs2x/IMPORT-MANIFEST.txt](https://github.com/hispark-rs/hisi-registers/blob/4fb181dcada788f2288ec09e1df1f6f26b4996df/rdl/chips/bs2x/IMPORT-MANIFEST.txt) | `52e1f962489d5d735368990d84371b7ef6c4d3c3` |
| [README.md](https://github.com/hispark-rs/hisi-registers/blob/4fb181dcada788f2288ec09e1df1f6f26b4996df/README.md) | `9edb96454487249dcd27730f5bcecf5bac2e888c` |
| [docs/architecture.md](https://github.com/hispark-rs/hisi-registers/blob/4fb181dcada788f2288ec09e1df1f6f26b4996df/docs/architecture.md) | `41f7757fa09569a2104bb3ada982b5bc94320ec0` |
| [evidence/README.md](https://github.com/hispark-rs/hisi-registers/blob/4fb181dcada788f2288ec09e1df1f6f26b4996df/evidence/README.md) | `876e192a4c71e488b1d398aefe344121eaf81643` |
| [.github/workflows/ci.yml](https://github.com/hispark-rs/hisi-registers/blob/4fb181dcada788f2288ec09e1df1f6f26b4996df/.github/workflows/ci.yml) | `e9bf4c00c5ffbcb1e8b02afe4b1d9720e6abcacc` |
| [pyproject.toml](https://github.com/hispark-rs/hisi-registers/blob/4fb181dcada788f2288ec09e1df1f6f26b4996df/pyproject.toml) | `44e40138f485e52dc6612510bfbf649a8f0a3b46` |

Evidence weight: direct implementation observations are strong source evidence; end-to-end behavior, imported baseline fidelity and hardware semantics remain unverified. Overall trust is B for those boundaries. Repository-relative `file:line` references below resolve through this pinned table.

## Executive findings

1. **Bootstrap is a one-time normalization tool, not a reversible synchronization pipeline.** It supplies full-width `VALUE` fields for undocumented registers, merges same-offset register fields into the first register, and records the removed alias. Shared-IP definitions are selected explicitly instead of re-imported. Both manifests record an `AON_SOFT_RST_CTL` merge into `CHIP_RESET`. Evidence: `scripts/import_svd_baseline.py:20-49,69-113,161-179`; both `IMPORT-MANIFEST.txt:8-9`.
2. **The exporter enforces a narrow structural contract.** Root children must be peripheral address maps or IRQ signals; IRQs need owner/number metadata and a matching peripheral; peripheral children must be a nonempty register-only list. Field access and side-effect mappings have explicit rejection branches. Evidence: `scripts/export_svd.py:90-101,120-143,175-189`.
3. **Fail-closed is not equivalent to semantic losslessness.** `rw1` and `w1` are accepted but mapped to ordinary read-write/write-only access; no write-once designation is emitted. Register reset values are assembled from known field resets without a reset mask. Range preservation in the bootstrap targets register-level ranges, whereas the exporter accepts register and field ranges. Evidence: `scripts/export_svd.py:22-28,53-74,163-189`; `scripts/import_svd_baseline.py:125-150`.
4. **The checker is a structural regression gate, not an exhaustive semantic comparator.** It asserts chip counts, selected addresses, nonempty register fields, range counts and absence of register arrays. Enumerations and read/write side effects are only summarized; their values and meanings are not compared against a golden source. Evidence: `scripts/check.py:9-55,114-190`.
5. **PAC adoption remains explicitly blocked by API-shape work.** Array compaction is not implemented here; the checker rejects any register with a `dim` element and reports hard-coded source-array counts. README says 10 BS2X arrays, but the checker says 11. Optional local `svd2rust` parsing can be skipped successfully, while CI installs it explicitly. Evidence: `README.md:68-74`; `scripts/check.py:14,27,50,160-185`; `scripts/check.sh:8-17`; `.github/workflows/ci.yml:11-15`.

## Dedup and novelty

The source-library inventory was examined before selecting this topic. The harvest index and bundle-wide searches for `hisi-registers`, `SystemRDL`, `export_svd`, `import_svd_baseline`, register aliases and array-compaction terminology were checked before implementation deep reading. A follow-up search covered `add_write_constraint`, `reset_value`, `known_source_arrays_not_yet_compacted` and `hisi_irq_owner`.

The relevant existing coverage is architecture-level register ownership and shared-IP promotion, not conversion implementation. The register section of [the ecosystem report](NEW-HISPARK-RS-ECOSYSTEM.md#6-register-truth-ws63-pac--bs2x-pac--svd--hisi-registers) was read, as was [the September increment](NEW-HISPARK-RS-SEPT-INCREMENT.md). The latter discusses KM register completion and PAC regeneration, not this bootstrap/export/check pipeline. No existing report was found covering these implementation symbols or the assertion-versus-summary distinction.

This report does not repeat the ecosystem survey or September delta as a new topic. It adds one coherent contract: what transformations this conversion boundary performs, what it rejects, and what its validation cannot prove. The scheduler report was also read during candidate selection; no scheduler or other closed topic is developed here.

## 1. Import contract: normalize once, then review

### Information retained and intentionally reshaped

`normalize_svd` iterates direct peripheral registers. When individual fields are missing, it constructs a `VALUE` field, inheriting size and access from register, peripheral, then device; defaults are 32 bits and read-write (`scripts/import_svd_baseline.py:58-93`). This is a representation fallback, not evidence that every bit is implemented or writable.

Offsets are keyed per peripheral. A later register at an already-seen offset has its fields appended to the first register and is removed; a note records the normalization (`:95-113`). The comment identifies a specific reset alias, but the implementation does not restrict merging to that name or check field overlap itself. Later third-party import/compiler behavior is outside this source inspection. Neither alias API preservation nor general alias correctness follows from this merge.

The importer delegates per-peripheral conversion to `peakrdl systemrdl`, checks the subprocess exit status, renames the top-level type, and injects omitted register write-range properties by matching generated text (`:184-207`). The range injector rejects a missing register marker/body, but only searches `register/writeConstraint/range`; it is not a general repair for every SVD constraint form (`:125-150`).

### Shared ownership and integration

A fixed table routes SPI, watchdog, SFC, PWM, SIO, timer, TCXO, GPIO and UART base definitions to shared RDL. A separate BS2X table selects its I2C/RTC family variants, preventing that selection from automatically applying to WS63 (`:20-43`). Non-derived, non-shared peripherals become private definitions; shared and derived instances reuse selected types (`:166-174,209-230`). The code resolves a direct `derivedFrom` name, rejects a missing base, and does not implement recursive chain traversal in this layer.

BS2X also receives explicit SFC and SIO extra instances (`:45-49,231-232`). Thus the checked output need not equal a raw input-peripheral count: its manifest records 30 imported peripherals, while the checker expects 32 (`rdl/chips/bs2x/IMPORT-MANIFEST.txt:4`; `scripts/check.py:19-21`). These are different stages, not by themselves a contradiction.

Interrupt metadata is generated from each peripheral's explicit interrupt entries, with the supplied evidence string attached (`scripts/import_svd_baseline.py:234-248`). There is no explicit inheritance of a base peripheral's interrupt list in this loop.

### Lifecycle and provenance limits

The importer removes every existing private `*.rdl` file in the selected output directory before generating replacements (`:176-179`). Individual files, the chip map and the manifest are written sequentially (`:203-207,250-264`); there is no transaction or rollback around the entire migration. That reinforces its documented one-time, review-required role rather than use as an unattended refresh operation.

The final manifest hashes the original SVD bytes using SHA-256 and records the evidence string, counts and normalization notes (`:252-264`). This identifies an input snapshot; it does not validate the evidence string, check source licensing, prove equivalence to an SDK, or hash the generated RDL set. The two committed manifest digests were read as historical records, not independently matched to original SVD inputs. Their `shared_definitions = 4` values are historical metadata, not the present shared-table size.

## 2. Export contract: explicit subset, incomplete equivalence

### Accepted topology and IRQ attachment

The exporter compiles one RDL file and elaborates its top map. It writes a CMSIS-SVD 1.3 device with an experimental version, 8-bit address units and 32-bit device width (`scripts/export_svd.py:104-118`). Each peripheral gets its absolute address and an address block sized from the RDL peripheral (`:139-160`).

Owner matching is case-normalized; unknown IRQ owners are rejected. IRQ entries are sorted by their numeric identifiers within each peripheral (`:120-137,149-156`). The wrapper does not separately validate unique IRQ numbers or normalized-name collisions. These checks should not be inferred from owner validation.

The exporter rejects empty peripherals and non-register peripheral children; it rejects unexpected register child types (`:141-143,175-180`). That is a flat map/register/field contract, not support for arbitrary nested register files or SVD clusters.

### Field semantics

| Dimension | Implemented behavior | Boundary |
|---|---|---|
| Access | `r`, `rw`, `w`, `rw1`, `w1` map through a fixed table | Write-once distinction is collapsed for the latter two (`:22-28,178-186`) |
| Reset | OR each available field reset shifted by its low bit; omit resetValue if none exist | No resetMask is emitted; partially specified resets do not retain an explicit unknown-bit mask (`:53-61,169-171`) |
| Read effects | `rclr` and `rset` emit clear/set actions | Other non-null `onread` values raise (`:30-33,90-95`) |
| Write effects | Eight fixed modified-write variants map to SVD values | Other non-null `onwrite` values raise (`:35-44,97-101`) |
| Numeric constraints | Both range endpoints must exist and minimum must not exceed maximum | No explicit endpoint-versus-bit-width validation in this helper (`:64-74`) |
| Enumeration | Emit member name, optional description and integer value | No source-equivalence comparison is performed by this exporter (`:77-87`) |

Register and field constraints use repository-defined unsigned properties (`rdl/properties.rdl:18-26`). IRQ owner and number are typed properties, while `hisi_evidence` is a string allowed on several component kinds (`:3-16`). The exporter uses IRQ evidence as its description, but does not validate an evidence class or require evidence on every register/field (`scripts/export_svd.py:149-189`). The evidence policy is therefore a review policy, not an automatically enforced proof system; its ranking is documented in `evidence/README.md:3-15`.

The XML tree is completed before destination output is written (`scripts/export_svd.py:191-192`). Most validation failures therefore occur before that final write, but the write is not an atomic replacement. There is no arbitrary-property allowlist scan that could justify a universal claim that every unsupported RDL property is rejected. The explicit branches support a narrower fail-closed claim.

## 3. What validation actually certifies

The checker hard-codes the following structural expectations (`scripts/check.py:9-55`). They are source assertions, not measurements obtained during this inspection.

| Output | Peripherals | IRQ entries | Expanded registers | Expanded fields | Write constraints | Reported un-compacted source arrays |
|---|---:|---:|---:|---:|---:|---:|
| WS63 | 38 | 46 | 1248 | 2104 | 6 | 39 |
| BS2X | 32 | 52 | 1017 | 2154 | 6 | 11 |
| WS53 | 13 | 0 | 349 | 664 | 2 | 0 |

Shared-IP reuse is guarded by chip-map substring counts, a canonical SPI type-name check and absence of selected private RDL filenames (`:59-112`). These detect specific drift patterns but are not AST-level type-identity proofs or cross-SDK equivalence checks.

For generated SVDs, it asserts peripheral/IRQ/register/field counts, selected base addresses, no fieldless registers, zero register arrays and a fixed total number of write constraints (`:114-173`). It does not compare all register offsets, reset values, bit positions, access modes, IRQ ownership or constraint endpoints to a reference. Enumeration, read-action and modified-write counts are included only in printed JSON (`:175-185`). Equal counts can coexist with different semantics.

The source-array figure is copied from `EXPECTED`, not calculated from an original SVD. Any register `dim` in the output is a failure, so the current gate freezes an expanded baseline rather than proving restoration of the original PAC array API (`:160-165,184`). README's BS2X count of 10 differs from the checker's 11; report the code value as the current assertion without claiming to resolve the original-array census (`README.md:71-74`).

`generate.sh:6-12` exports WS63, BS2X and WS53 sequentially. `check.sh:5-17` regenerates, checks structure, then optionally runs `svd2rust` for all three outputs. Absence of that executable produces a warning rather than failure. CI installs `svd2rust` 0.37.1 before invoking the script (`.github/workflows/ci.yml:11-15`); the Python project pins its four direct RDL dependencies (`pyproject.toml:4-9`). This describes configured validation, not evidence of a successful CI run or a compiled PAC.

## Runtime and applicability limits

- No project code, importer, exporter, checker, build, parser smoke test or hardware operation was executed. No network freshness check was performed. Statements about accepted/rejected data are static source deductions, not reproduced test results.
- These scripts generate descriptions; they do not access registers. Their success would not demonstrate register behavior, clock/reset sequencing, safe Rust peripheral ownership, NearLink transport behavior or WS73 compatibility.
- PeakRDL and SystemRDL compiler internals, full chip/IP RDL bodies, original SVD baselines and generated PAC APIs were not audited. A wrapper omission does not prove that a dependency accepts an invalid input, nor that a currently committed map uses the problematic form.
- The repo explicitly says generated SVDs are not authoritative inputs for released PACs (`README.md:23-28`). Adoption still requires source reconciliation, restored array/alias API shape, semantic golden tests and downstream validation (`docs/architecture.md:48-55`).

## Reusable lessons and relative comparisons

1. **Keep migration separate from canonical maintenance.** Shared definitions plus chip-specific integration tables are useful ownership boundaries; bootstrap normalization notes make intentional representation loss visible. This extends, rather than republishes, the register-ownership discussion in [the ecosystem report](NEW-HISPARK-RS-ECOSYSTEM.md#6-register-truth-ws63-pac--bs2x-pac--svd--hisi-registers).
2. **Separate structural acceptance from semantic equivalence.** Counts and parser acceptance are useful early gates, but a PAC migration needs field/reset/access/side-effect comparisons and an API-shape comparison. The [September increment](NEW-HISPARK-RS-SEPT-INCREMENT.md#3-image-and-registers) records existing PAC regeneration work; it does not establish that this experimental RDL pipeline has replaced that path.
3. **Preserve evidence without overstating it.** Input digests identify snapshots; string evidence annotations and printed semantic counts are not proof of their contents. A future implementation could adopt this separation without inheriting any WS63/BS2X register facts into WS73 by default.

The resulting new knowledge is the conversion boundary itself: controlled normalization and several explicit rejection gates coexist with known API-shape loss, semantic mappings that discard distinctions, and validation that remains predominantly structural.
