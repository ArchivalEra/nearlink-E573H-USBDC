---
type: harvest
title: "hisi-nvs plaintext persistence: record commits, page compaction and recovery boundaries"
language: en
created: 2026-09-17
tags: [harvest, hispark-rs, rust, no-std, ws63, nvs, persistence, compaction]
sources:
  - "https://github.com/hispark-rs/hisi-nvs/blob/f1436d2dd3c03c45948fb73422e2a27f4dd5331c/src/lib.rs"
  - "https://github.com/hispark-rs/hisi-nvs/blob/f1436d2dd3c03c45948fb73422e2a27f4dd5331c/Cargo.toml"
trust: B
stale_after: 2027-03-17
---

# hisi-nvs plaintext persistence: record commits, page compaction and recovery boundaries

## Scope, provenance and deduplication

This topic covers the complete `hispark-rs/hisi-nvs` crate implementation: plaintext lookup, append replacement, interrupted-tail handling, erase-capability-gated compaction, error semantics and inline test fixtures. It is a coherent persistence service, not a subset of unrelated declarations. The evidence revision is `f1436d2dd3c03c45948fb73422e2a27f4dd5331c`. Both selected files were read completely and their bytes matched their commit blobs:

| Upstream-relative file | Git blob identity |
|---|---|
| `src/lib.rs` | `dd8c9faad7fcf88337579b710935ad13ee536fbd` |
| `Cargo.toml` | `fa339784f96770d1009d692a7ab0bf074c88d376` |

All source evidence paths below are relative to that upstream repository and refer to this revision. Trust B reflects implementation evidence with unverified backend and runtime behavior, not executed validation.

The harvest index and knowledge-bundle searches for `hisi-nvs`, transactional compaction and related store names found only ecosystem-level coverage of this crate. [The ecosystem overview](NEW-HISPARK-RS-ECOSYSTEM.md) characterizes it as a read-only parser and leaves writing to future work. This revision actually contains `NvWriter::write` and `write_with_gc` (`src/lib.rs:84-157,424-441`), so the new contribution is the complete mutation and recovery implementation, not another ecosystem inventory. [The September increment](NEW-HISPARK-RS-SEPT-INCREMENT.md) concerns radio lifecycle and image tooling, not NVS transactions. [The BS21 NV report](NEW-BS21-NV-PERSISTENCE-CONTRACT.md) covers a vendor public API declaration; it does not establish this Rust crate's on-storage behavior.

## Executive findings

1. **The crate is no longer read-only, despite some stale comments.** It is `no_std`, implements a generic reader and plaintext writer, and exposes compaction only with an `EraseStorage` bound. The manifest identifies version `0.1.0-alpha.3` and exactly pins `hisi-storage =0.1.0-alpha.3`. The broad comment that the writer never compacts is contradicted by the explicit GC method; the method bodies define the useful boundary. Evidence: `src/lib.rs:6-14,61-84,104-108,424-441`; `Cargo.toml:1-12`.
2. **A replacement has a distinct publication byte and later cleanup.** The writer programs header bytes after the magic, then value, padding and CRC, then the `0xa9` magic byte. It reads back the new record before invalidating older same-key records. Within a selected page, a later CRC-valid plaintext record supersedes an earlier one. This is an implemented ordering protocol, not proof of arbitrary power-loss tolerance. Evidence: `src/lib.rs:123-156,294-363,772-805`.
3. **Page identity is logical, but canonical selection is physical and deterministic.** Matching store ID and complemented metadata admit a page; the largest sequence for a logical page wins, with a later physical page breaking ties. Compaction copies records into an erased target before publishing a higher-sequence page header. The old page remains physically present. Evidence: `src/lib.rs:365-405,475-505,534-548,678-719`.
4. **Free bytes elsewhere do not guarantee a replacement can succeed.** Ordinary append keeps an existing key on its current logical page. GC requires room for all currently valid records plus the new record, and a reclaimable noncanonical or invalid-header physical page. It preserves the old value during the copy, rather than budgeting only the final replacement. Evidence: `src/lib.rs:164-193,443-476,508-548`.
5. **The read API has explicit integrity, capacity and encryption boundaries.** It returns an unpadded length, reports `BufferTooSmall { required }` only after selecting a CRC-valid candidate, and recognizes encrypted records without decrypting them. Encrypted matches can suppress an earlier plaintext candidate in the same page. Evidence: `src/lib.rs:643-670,746-805,864-890`.
6. **The tests support particular transitions, not a universal durability claim.** Inline fixtures model one-to-zero programming, replacement, interrupted-tail sealing, duplicate-page precedence, successful compaction and rejection of an entire final page-header write. They do not model every torn write or establish deployed backend behavior. Evidence: `src/lib.rs:942-1023,1105-1236,1238-1344`.

## Substantive contracts

### Region, format and initialization

`NvReader::try_new` and `NvWriter::try_new` validate region shape, not the existence of a usable store: the store ID cannot be zero or `0xffff`; page size must be a power of two and at least the combined minimum headers and CRC; capacity must be positive and a page multiple. A fully erased but correctly sized region can pass construction and still have no readable or writable page. There is no public format/create-store operation in the complete implementation. Evidence: `src/lib.rs:84-102,164-193,623-641,701-719`.

The WS63 preset uses store ID `0x254d` and 4096-byte pages. A page has a 16-byte header. The details, complement, sequence and sequence complement are little-endian 32-bit words; the store ID occupies the low 16 details bits and the logical page index the high eight. The reader does not independently validate the intervening details byte. Evidence: `src/lib.rs:16-26,44-59,701-718`.

A plaintext record consists of a 16-byte header, value padded to a four-byte boundary and a four-byte big-endian CRC. Header offsets include magic at byte zero, validity at byte one, little-endian length at bytes 2-3, key at bytes 6-7 and encryption type at bytes 8-9. New plaintext records begin with an all-`0xff` header and set encryption type to zero; padding is zero-filled. CRC includes the entire final header and padded value, using reflected polynomial `0xedb88320`, all-one initialization and final inversion. Evidence: `src/lib.rs:109-152,604-620,892-910`.

For the WS63 preset, the maximum plaintext value fitting an otherwise empty page is **4060 bytes**, derived from `16 + 16 + align4(length) + 4 <= 4096`. The `u16` length field does not imply a 65535-byte supported value. The writer checks both representability and page fit (`src/lib.rs:109-121`); an inline fixture writes the 4060-byte boundary (`src/lib.rs:1238-1247`).

### Read selection and output validity

A lookup iterates logical indices 0 through 255. For each, it rescans physical pages to choose the canonical copy and returns the first logical page that yields `Found`. Consequently, there is no global latest-key ordering across different logical pages. Later records win within a page, while page sequences resolve copies of the same logical index. Evidence: `src/lib.rs:648-698,772-806`.

The reader skips a byte when magic is unrecognized, stops at an entirely erased key header, and can continue past a record whose computed size crosses the page boundary. For target plaintext records it first computes CRC without output, retaining the latest valid candidate. A later bad-CRC plaintext match does not erase an already retained valid candidate. An encrypted match clears that candidate and records an encrypted state; a subsequent valid plaintext match can replace that state. Evidence: `src/lib.rs:735-797`.

After selection, insufficient capacity yields `BufferTooSmall` with the complete required length. With enough capacity the candidate is read again and copied while CRC is recomputed. A backend error or changed data during that second pass can therefore leave output modified without a successful read result. Consumers must only interpret output after success; this is not an atomic snapshot across two backend reads. Evidence: `src/lib.rs:798-845`.

Nonzero encryption types use 16-byte alignment; type `0x52` reserves 32 integrity bytes while other types reserve four. These are traversal rules, not decryption or authenticity checks. If no logical page succeeds, an observed encrypted target takes precedence over a corrupt target, followed by `NotFound`. Evidence: `src/lib.rs:604-620,648-670,746-775`.

### Append, recovery and completion

Append scans canonical physical pages. If a page contains the existing valid key, it must accommodate the replacement in its own tail; otherwise `NoSpace` is returned even if an earlier page offered free space. A new key can use the first suitable canonical tail. Evidence: `src/lib.rs:164-223`.

The write sequence is:

1. Write all record header bytes except magic.
2. Write the value and zero padding, then the CRC calculated for the final header.
3. Program magic, publishing the record to the reader.
4. Read back the exact header, compare value bytes and check CRC.
5. Clear the validity byte of other matching records.

Evidence: `src/lib.rs:123-156,294-363`. The last cleanup scans pages with accepted headers, not only canonical copies (`src/lib.rs:299-324`). An error during verification or cleanup occurs **after** the publication step. Thus `Err` cannot be interpreted as proof that no new value became visible. This interface has no transaction identifier, rollback result or separate durable-completion callback (`src/lib.rs:108-157,864-890`).

Before appending, the writer may seal one interrupted tail record. It requires erased magic, some programmed remaining header bytes, a size that fits, and an entirely erased suffix after the computed record. It clears validity before programming normal magic, turning the abandoned span into a skippable invalid record. It does not attempt unrestricted recovery of arbitrary corruption. Evidence: `src/lib.rs:225-291`.

Because tail discovery can perform this sealing, even append-position search is not purely observational. Recovery can consume a record-sized allocation before a later condition returns an error. Tests describe both an accepted interrupted tail and refusal when later programmed data exists (`src/lib.rs:1183-1236`).

### Compaction, spare capacity and progress

`write_with_gc` first attempts normal append. It invokes compaction only for `NoSpace`, propagates other errors directly, and retries append once after one compaction. It is not a repeated region-wide collector. Evidence: `src/lib.rs:424-441`.

The source-page selection counts every record whose validity byte is `0xff`; it does not discard an older same-key valid duplicate, subtract the value being replaced, or validate CRC while counting. It requires room for those bytes plus the proposed new record. It prefers a fitting page containing the key and otherwise remembers the first fitting fallback page. Evidence: `src/lib.rs:443-475,508-531`.

The target is the first other physical page with an invalid/unrecognized header or a noncanonical header. The collector checks erase-size divisibility, erases one full page and verifies all target bytes are `0xff`. It copies all validity-marked records in 64-byte chunks, preserving their raw encoding, then writes and verifies a complete header with the same logical identity and sequence plus one. Sequence advancement is checked rather than wrapping, although this check occurs after erase and copy. Evidence: `src/lib.rs:443-505,534-590`.

Important limits of this implementation follow directly:

- The old canonical page is not erased in the same compaction. Once the new header is accepted, the old page becomes a future reclaimable copy (`src/lib.rs:385-405,488-505,534-548`).
- Copied records are not CRC-validated or read back at the destination before page-header publication. Erase verification and header verification must not be described as full copied-data verification (`src/lib.rs:484-504,550-590`).
- If the existing key's page cannot fit all valid bytes plus the replacement, a different fallback page may be compacted. The subsequent append still encounters the original full key page and can return `NoSpace`. A failed call can therefore have changed page layout without updating the requested key (`src/lib.rs:182-186,438-440,458-475`).
- No spare invalid/noncanonical page means no compaction, even if a source contains reclaimable invalid records. Aggregate unused bytes are not a sufficient admission test (`src/lib.rs:475-483,534-548`).

## Test evidence and unverified runtime limits

The inline source tests assert CRC acceptance/rejection, required buffer reporting and corruption precedence (`src/lib.rs:1058-1103`); duplicate-page sequence and tie rules, invalid-header rejection and corrupt-prefix scanning (`src/lib.rs:1105-1148`); replacement and pre-invalidation precedence (`src/lib.rs:1150-1181`); interrupted-tail handling (`src/lib.rs:1183-1236`); no-space and no-cross-logical-page movement (`src/lib.rs:1238-1271`); and GC success plus old-page retention on header-commit rejection (`src/lib.rs:1273-1344`). These are test definitions, not reported test results.

`TestFlash` models writes by asserting that no zero-to-one transition is requested and applying bitwise AND. Its erase fixture fills a page with `0xff`. `FailHeaderCommitFlash` rejects a write starting at the final page-header offset before modifying any of that write's bytes. This supports a specific pre-header-commit failure scenario, not a partially programmed header, silent payload-write failure, interrupted erase or exhaustive power-cut campaign. Evidence: `src/lib.rs:942-1023`.

The selected crate imports storage traits but does not implement the deployed backend (`src/lib.rs:12-14`; `Cargo.toml:11-12`). Program granularity, ordering, persistence upon return, cache effects and failure semantics remain outside the established evidence. Generic `&mut self` methods do not alone prevent a second handle from accessing the same physical region; the public write documentation explicitly assigns serialization to the caller (`src/lib.rs:104-108`). Stable reads also matter for the reader's two-pass validation.

Neither these sources nor the test fixtures establish flash endurance, wear distribution, timing bounds, runtime allocation by a backend, actual board support or WS73 compatibility. The implementation itself uses fixed-size scratch arrays and no production heap allocator, but lookup performs repeated physical-page scans and candidate CRC passes; no latency figure follows from `no_std` (`src/lib.rs:277-291,678-698,777-834`). The module says its format follows vendor files, but those vendor implementations were not independently compared here (`src/lib.rs:3-8`). There is no basis to treat this crate as a drop-in replacement for the BS21 NV API or as a Linux USB protocol component.

## Reusable lessons and comparison anchors

- **Make erase authority explicit.** The `WriteStorage` versus `EraseStorage` split prevents ordinary append from silently introducing erase operations. This is a useful API pattern for persistence layers, independent of chip support (`src/lib.rs:84-108,424-441`).
- **Distinguish visibility from successful cleanup.** Record magic publication precedes readback and invalidation. Model an error after publication as potentially committed, not as automatic rollback. This complements the submission/completion distinctions in [BS21 NV persistence](NEW-BS21-NV-PERSISTENCE-CONTRACT.md), without assuming its callback API shares this implementation.
- **Budget transitional capacity.** Old and new data must coexist during safe replacement, and page migration needs a spare physical page. Capacity planning should account for the transaction, not just the final value set (`src/lib.rs:458-476,534-548`).
- **Separate record publication from page publication.** A record uses magic-last; a compacted page becomes selectable only through its completed complemented header and newer sequence. Both state transitions deserve distinct failure models (`src/lib.rs:139-155,488-504,701-718`).
- **Preserve the difference between source tests and durability evidence.** The existing header-failure fixture is a useful specification seed, but does not justify declaring all interrupted operations safe (`src/lib.rs:999-1008,1309-1344`).

Relative comparisons are intended for placement alongside the existing harvest reports. [The original Rust ecosystem map](NEW-HISPARK-RS-ECOSYSTEM.md) supplies the service-layer context; this report replaces its read-only characterization with implementation-level evidence at the pinned revision. [The RF composition-root report](NEW-HISI-RF-WS63-COMPOSITION.md) concerns assembly of a device-side Rust radio stack, whereas this report concerns the independent persistence primitive. No claim is made that a particular radio consumer uses this writer or GC path without examining that integration separately.
