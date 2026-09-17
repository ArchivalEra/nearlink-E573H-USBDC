---
type: harvest
title: "hisi-alloc arena lifecycle: pointer-only allocation, resize headroom and capacity diagnostics"
language: en
created: 2026-09-17
tags: [harvest, hispark-rs, rust, no-std, allocator, ownership, resource-admission]
sources:
  - "https://github.com/hispark-rs/hisi-alloc/blob/0de0bf0fe8b566b227c3863ebe885ddddd5bdfad/src/lib.rs"
  - "https://github.com/hispark-rs/hisi-alloc/blob/0de0bf0fe8b566b227c3863ebe885ddddd5bdfad/Cargo.toml"
  - "https://github.com/hispark-rs/hisi-alloc/blob/0de0bf0fe8b566b227c3863ebe885ddddd5bdfad/Cargo.lock"
  - "https://github.com/hispark-rs/hisi-alloc/blob/0de0bf0fe8b566b227c3863ebe885ddddd5bdfad/README.md"
  - "https://github.com/hispark-rs/hisi-alloc/blob/0de0bf0fe8b566b227c3863ebe885ddddd5bdfad/CHANGELOG.md"
  - "https://github.com/hispark-rs/hisi-alloc/blob/0de0bf0fe8b566b227c3863ebe885ddddd5bdfad/.github/workflows/ci.yml"
  - "https://github.com/hispark-rs/hisi-rtos/blob/3c49a1e5f61d931a1604e41d37edf5a7e889cd72/src/storage.rs"
  - "https://github.com/hispark-rs/hisi-rtos/blob/3c49a1e5f61d931a1604e41d37edf5a7e889cd72/Cargo.toml"
trust: B
stale_after: 2027-03-17
---

# hisi-alloc arena lifecycle: pointer-only allocation, resize headroom and capacity diagnostics

## Scope, novelty and provenance

This is one complete software theme: the `hisi-alloc` arena service, from registration through allocation, resize and retirement to capacity observation. The entire 568-line implementation, including its seven inline tests, was read. One narrow scheduler storage adapter was also read completely to establish ordinary integration, not to reopen scheduler policy or radio lifecycle research. No build, test execution, project-code execution, network access or device operation was performed. Trust B denotes pinned implementation evidence and source-derived interpretation, not runtime validation.

The harvest index and bundle-wide searches for `hisi-alloc`, `CHeap`, `AllocationHeader`, `HeapMetrics`, `reallocate_zeroed`, `largest_allocatable` and related arena terminology found no prior implementation-level treatment of this service:

- [The ecosystem overview](NEW-HISPARK-RS-ECOSYSTEM.md), section 7, records only the runtime-neutral `CHeap` and the absence of exported C/global allocator policy. It does not describe allocation metadata, resize semantics, metrics or contiguous-capacity probing.
- [The September increment](NEW-HISPARK-RS-SEPT-INCREMENT.md), section 3, notes public allocator capabilities in generated starters, without tracing their allocation mechanics.
- [The scheduler report](NEW-HISI-RTOS-SCHEDULER.md) explicitly limits itself to README-level scheduling and preemption analysis. The storage adapter below is evidence of this allocator's use, not a second scheduling theme.
- [The RF composition report](NEW-HISI-RF-WS63-COMPOSITION.md) lists `alloc.rs` but explicitly does not line-read adapter internals. This report does not attempt to establish the RF C ABI.
- [The completed NVS report](NEW-HISI-NVS-PERSISTENCE.md) concerns persistent records and compaction; its production implementation uses fixed scratch buffers rather than this heap. Neither its persistence lifecycle nor its recovery analysis is subdivided here.

Both upstream Git origins identify the corresponding `github.com/hispark-rs` repositories. Every file below was read completely and byte-compared with its committed blob using local Git objects. No remote freshness claim follows from this check.

| Upstream repository and revision | File | Git blob |
|---|---|---|
| `hisi-alloc` at `0de0bf0fe8b566b227c3863ebe885ddddd5bdfad` | `src/lib.rs` | `5286fdb54abe1b23e55835563a79ac3e2b82c0f1` |
| same | `Cargo.toml` | `8ba9b1a3740b2cf2d6780c324ec89c386d80b404` |
| same | `Cargo.lock` | `1035eab9b73f98e42430b3987557a6fdc256252d` |
| same | `README.md` | `706121ee06d95e5620e6cfe8fab45b965e8b86da` |
| same | `CHANGELOG.md` | `85fc4dc2fc324512873fb69d0bcd41dbb76ba886` |
| same | `.github/workflows/ci.yml` | `2963c48e8b53046571416924346216504ae37144` |
| `hisi-rtos` at `3c49a1e5f61d931a1604e41d37edf5a7e889cd72` | `src/storage.rs` | `4cf293731bee2afa92a7b2b760ef194a8879fb96` |
| same | `Cargo.toml` | `3924f8a9fae15ecd43171fb6ab76fbb09f54cf5e` |

Evidence paths below include the repository name and use one-based source lines at these revisions.

## Executive findings

1. **Allocation mechanics are independent of memory placement and C symbol policy.** `CHeap` is a `no_std` wrapper around first-fit allocation, synchronized through `critical_section::Mutex<RefCell<State>>`. It accepts a caller-owned, program-lifetime arena and does not provide `malloc`, `osal_kmalloc` or a global allocator. Evidence: `hisi-alloc/src/lib.rs:1-15,118-168`; `hisi-alloc/README.md:3-20`.
2. **Pointer-only retirement is supported by a private ownership header.** Each request reserves a 16-byte header plus alignment slack; the header stores the layout and base offset needed to return the block without a size argument. Free validates metadata and marks it retired before delegating to the underlying heap. This does not remove the unsafe ownership contract. Evidence: `hisi-alloc/src/lib.rs:19-30,235-278,289-318,368-380,388-425`.
3. **Resize is allocate-copy-free, not in-place growth.** A nonzero resize allocates the replacement while the old block is still live. Failure to allocate leaves the original live; successful growth preserves the common prefix and zeroes new bytes. Even shrinking requires temporary replacement capacity. Evidence: `hisi-alloc/src/lib.rs:320-365`.
4. **Free bytes and admissible payload are deliberately different observations.** Metrics describe managed allocation layouts. `largest_allocatable(alignment)` binary-searches candidate payloads by temporarily allocating and returning actual blocks, under one critical section, without updating the wrapper counters. It is an admission diagnostic, not a reservation or cheap hot-path read. Evidence: `hisi-alloc/src/lib.rs:88-116,175-229`.
5. **An ordinary consumer fixes policy outside the primitive.** Scheduler storage selects 16-byte alignment, supplies static arena storage, rejects unsupported task quotas and returns an installed capability carrying heap operations and observations. Task-count admission remains separate from byte availability. Evidence: `hisi-rtos/src/storage.rs:29-42,66-126,140-175,198-219`.

## Source contracts

### Registration, dependencies and ownership

`CHeap::empty()` starts with an empty underlying heap, zero range and zero counters. `init` rejects a null pointer or a range shorter than 32 bytes, checks address addition, then serializes registration. Repeating exactly the same start and end succeeds without resetting the heap; a different range returns `DifferentArena`. Initialization is therefore idempotent registration, not a reset, resize or arena replacement API. The complete public implementation provides no explicit teardown operation. Evidence: `hisi-alloc/src/lib.rs:32-73,124-168`.

The unsafe initialization contract requires valid writable storage that remains allocated for the program lifetime and is accessed only through that heap. Numeric range checks do not establish this lifetime or exclusive ownership. Applications and runtime adapters remain responsible for disjoint arena ownership and for keeping live allocations valid. Evidence: `hisi-alloc/src/lib.rs:132-163`.

The manifest identifies `0.1.0-alpha.3`, edition 2024, with no default features. It requests `critical-section = "1"` and `linked_list_allocator = "0.10"` with that allocator's default features disabled. The checked-in lockfile resolves these to 1.2.0 and 0.10.6 respectively. These are manifest ranges plus a repository lockfile, not proof of every downstream consumer's resolved versions. The external allocator implementation and deployed critical-section backend were not independently inspected. Evidence: `hisi-alloc/Cargo.toml:1-18`; `hisi-alloc/Cargo.lock:5-23`.

### Payload layout, zeroing and retirement

For requested size `S` and alignment `A`, the wrapper first requires a positive size and a positive power-of-two alignment. It chooses effective alignment `E = max(A, 8)` and requests a layout of `S + 16 + E - 1` bytes. Both additions are checked; the size and alignment must fit the header's 32-bit fields and pass Rust `Layout` validation. This is the wrapper's requested layout, not necessarily the exact byte accounting after the underlying allocator's own adjustments. Evidence: `hisi-alloc/src/lib.rs:19-30,235-243,368-380`.

After first-fit succeeds, the wrapper computes an aligned user pointer, places the header immediately before it, and zeroes exactly the requested payload. The header records layout size, effective alignment, base offset and allocation state. This makes a pointer-only free operation possible without putting size bookkeeping in a C caller. No allocation is attempted outside the registered arena by this wrapper. Evidence: `hisi-alloc/src/lib.rs:245-277`.

Zero size, invalid alignment, an unrepresentable layout, uninitialized storage and exhaustion all produce a null pointer. Each rejected allocation records one attempt and one failure using saturating counters. Successful allocation updates live and peak counts and peak underlying used bytes. Evidence: `hisi-alloc/src/lib.rs:235-287`.

`deallocate(null)` succeeds even before initialization and does not affect rejection counters. For a non-null pointer, validation checks heap initialization, the pointer/header location, header alignment and recorded layout/extent consistency. A rejected deallocation increments `deallocation_failures`; an accepted one marks the header retired, returns the reconstructed base/layout pair and decrements the live count. The implementation is defensive metadata checking, not a replacement for returning a uniquely owned live allocation from the correct heap. No adversarial-pointer experiment or security guarantee is inferred. Evidence: `hisi-alloc/src/lib.rs:289-318,388-425`.

### Resize and transitional capacity

`reallocate_zeroed` has three important branches:

| Input case | Implemented behavior |
|---|---|
| Null original pointer | Delegate to `allocate_zeroed` with the new size and alignment. |
| Non-null pointer, new size zero | Attempt deallocation and return null. |
| Non-null pointer, positive new size | Validate the original, allocate a zeroed replacement, copy the common prefix, then retire the original. |

Evidence: `hisi-alloc/src/lib.rs:320-365`.

The original requested size is recovered by reversing the header-layout formula, rather than treating all padding as payload. If original validation fails, the resize returns null before recording an ordinary allocation attempt. If replacement allocation fails, no copy or old-block retirement occurs. The final deallocation result is discarded in the successful replacement path; the API remains unsafe and assumes valid unique original ownership. Evidence: `hisi-alloc/src/lib.rs:343-364`.

The practical sizing consequence is important even for entirely valid use: the heap must temporarily hold both layouts. A smaller requested final object is not sufficient evidence that shrink-by-reallocation can succeed in a crowded arena. Similarly, a successful resize can increase peak-used and peak-live metrics because the replacement is counted before the old block is retired. These follow from the actual call order, not from measured fragmentation or a reproduced allocator failure. Evidence: `hisi-alloc/src/lib.rs:273-275,343-364`.

### Metrics and contiguous admission

`HeapMetrics` is a consistent snapshot taken under the heap's critical section. Arena, used and free byte counts come from the underlying allocator; the wrapper supplies live counts, peaks and rejected-operation counters. Byte counts include allocator layouts rather than just application payload. The source explicitly warns that one allocation of `free_bytes` need not succeed. Evidence: `hisi-alloc/src/lib.rs:88-116,175-192`.

`largest_allocatable` returns zero for invalid alignment or an uninitialized heap. Otherwise it searches from zero through current free bytes using the same layout helper as real allocation. Each successful candidate is immediately deallocated before the next candidate is tried. Wrapper counters and peaks are untouched, but allocator free-list operations do occur; this is not an immutable inspection of metadata. The whole search is serialized, whereas the returned answer is only a snapshot once the critical section ends. Evidence: `hisi-alloc/src/lib.rs:194-229`.

The documentation calls this a pre-initialization admission diagnostic. Since the method itself returns zero before heap initialization, that wording is best understood as admission before a consuming subsystem starts, after arena registration. It must not be read as capacity discovery for an unregistered heap. Evidence: `hisi-alloc/src/lib.rs:194-207`.

## Ordinary integration evidence

`hisi-rtos` declares `hisi-alloc = "0.1.0-alpha.3"`. Its storage module directly imports `CHeap` and `HeapMetrics`, so this is an implementation consumer rather than only a README association. Evidence: `hisi-rtos/Cargo.toml:21-27`; `hisi-rtos/src/storage.rs:8-9`.

`SchedulerArena<BYTES>` uses statically placed, 16-byte-aligned storage. `SchedulerStorage<N>` holds a `CHeap` and an installation flag. `install` rejects zero or unsupported task capacity before setting that flag; repeat installation on the same storage object is rejected. Heap initialization failure resets the flag and maps the error to `InvalidArena`. Successful installation returns a capability containing a context and paired function pointers, plus the admitted task count. These are the observed per-storage gates; they do not independently demonstrate global uniqueness of all arena references in an application. Evidence: `hisi-rtos/src/storage.rs:29-42,66-126`.

The adapter fixes allocation and largest-payload queries at alignment 16, exposes metrics through the same heap and uses a debug assertion on retirement success. Its erased internal capability is copyable, while the public installed object is presented as the value to pass into runtime startup. This establishes policy placement and callback pairing; the complete runtime startup/dispatch chain was not inspected. Evidence: `hisi-rtos/src/storage.rs:140-219`.

The arena documentation budgets task stacks, synchronization objects, allocator metadata and headroom together, while `N` separately controls task quota. This is a useful example of avoiding the assumption that enough task slots implies enough bytes. Evidence: `hisi-rtos/src/storage.rs:29-34,66-70`.

## Test definitions and runtime limitations

The seven allocator tests specify same-arena initialization idempotence, zeroed/aligned allocation with metrics, prefix-preserving resize and zeroed growth, rejection of an outside-arena pointer, rejected/exhausted allocation counts, a largest-payload boundary with unchanged metrics, and invalid/uninitialized capacity queries. Evidence: `hisi-alloc/src/lib.rs:449-568`. The scheduler adapter adds two tests for installation behavior and unsupported task capacity (`hisi-rtos/src/storage.rs:230-255`). These tests were read, not run.

The allocator's host test critical-section implementation has empty acquire/release bodies and explicitly excludes concurrent interrupt contexts. It cannot establish target synchronization or interrupt-latency behavior. The CI workflow declares formatting, host tests, linting, target checking and packaging, but a workflow definition is not a successful run record. Evidence: `hisi-alloc/src/lib.rs:431-440`; `hisi-alloc/.github/workflows/ci.yml:8-21`.

Further limits follow from the read scope and implementation:

- There is no measured worst-case allocation or free latency. First-fit work, payload zeroing and the repeated capacity probe execute inside critical sections; production interrupt and scheduling effects depend on the selected backend and workload (`hisi-alloc/src/lib.rs:194-229,245-277,299-317`).
- Resize performs validation, replacement allocation, copying and retirement as separate steps, not one indivisible heap transaction. Unique ownership of the original payload remains required throughout (`hisi-alloc/src/lib.rs:325-364`).
- Tests do not establish fragmented-arena workloads, failed resize preservation under all conditions, concurrent consumers, target execution or exhaustive boundary behavior. No fragmentation defect is claimed merely because these scenarios are absent from the seven definitions (`hisi-alloc/src/lib.rs:449-568`).
- Zeroed allocation is not secure erasure on free: retirement changes metadata and delegates deallocation without wiping the former payload (`hisi-alloc/src/lib.rs:308-315`).
- Header metadata and validation are not a general memory-safety proof. The allocator's unsafe ownership and arena-lifetime requirements remain part of every adapter's contract (`hisi-alloc/src/lib.rs:137-141,289-295,325-327`).
- The README status still names the extraction release alpha.1, while the manifest and changelog identify alpha.3 with metrics and contiguous-admission additions. The implementation and pinned manifest, rather than that stale status paragraph, define this report's version boundary (`hisi-alloc/README.md:22-25`; `hisi-alloc/Cargo.toml:1-5`; `hisi-alloc/CHANGELOG.md:7-21`).

Nothing here establishes WS73 compatibility, a Linux allocator replacement, a USB transport contract or deployed radio reliability. The reusable evidence is software ownership and admission design.

## Reusable lessons

1. **Separate mechanism, storage ownership and ABI policy.** A small heap service can be reused by a scheduler or protocol shim without choosing linker regions or exporting global C symbols. Preserve those ownership boundaries when integrating vendor-facing code (`hisi-alloc/README.md:3-20`; `hisi-rtos/src/storage.rs:198-219`).
2. **Budget operations, not just final state.** Resize requires old and new allocations to coexist. Account for transitional peaks and alignment/header overhead when sizing startup resources (`hisi-alloc/src/lib.rs:343-379`). This is an in-memory counterpart to transitional-capacity lessons in the NVS report, not evidence that the two services share an implementation.
3. **Report aggregate availability and admissible payload separately.** Free bytes help explain utilization; a same-layout contiguous probe answers a different admission question. Neither result reserves memory for a later caller (`hisi-alloc/src/lib.rs:88-93,194-229`).
4. **Keep telemetry semantics explicit.** Saturating counters, payload-versus-layout distinctions and counter-neutral probes make measurements interpretable. Resize-validation failures and null frees should not be silently assumed to count as ordinary allocation failures (`hisi-alloc/src/lib.rs:235-287,295-298,343-356`).
5. **Use installed capabilities without overstating their proof.** Pair heap context and operations, reject repeat installation on the same storage object, and keep task quota distinct from arena bytes; separately review the application's arena exclusivity and lifetime obligations (`hisi-rtos/src/storage.rs:66-126,140-219`).
6. **Separate source contracts, test intentions and runtime evidence.** Inline assertions are useful specifications, but no-op host synchronization and declarative CI do not establish target timing, concurrency or field reliability (`hisi-alloc/src/lib.rs:431-440`; `hisi-alloc/.github/workflows/ci.yml:8-21`).
