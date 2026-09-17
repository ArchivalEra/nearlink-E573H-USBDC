---
type: harvest
title: "hisi-crypto entropy service: checked seeding, request-budgeted reseeding and output contracts"
language: en
created: 2026-09-17
tags: [harvest, hispark-rs, rust, no-std, entropy, drbg, service-lifecycle]
sources:
  - "https://github.com/hispark-rs/hisi-crypto/blob/56b619a25d4fafc6b7b46a1c14773c6ab69d5aad/src/rng.rs"
  - "https://github.com/hispark-rs/hisi-crypto/blob/56b619a25d4fafc6b7b46a1c14773c6ab69d5aad/src/lib.rs"
  - "https://github.com/hispark-rs/hisi-crypto/blob/56b619a25d4fafc6b7b46a1c14773c6ab69d5aad/src/secret.rs"
  - "https://github.com/hispark-rs/hisi-crypto/blob/56b619a25d4fafc6b7b46a1c14773c6ab69d5aad/Cargo.toml"
  - "https://github.com/hispark-rs/hisi-crypto/blob/56b619a25d4fafc6b7b46a1c14773c6ab69d5aad/README.md"
  - "https://github.com/hispark-rs/hisi-crypto/blob/56b619a25d4fafc6b7b46a1c14773c6ab69d5aad/CHANGELOG.md"
trust: B
stale_after: 2027-03-17
---

# hisi-crypto entropy service: checked seeding, request-budgeted reseeding and output contracts

## Scope and provenance

This is one portable software service: adapting an entropy source, checking seed blocks, instantiating an explicitly chosen deterministic random-bit generator (DRBG), enforcing a successful-request budget, and handling reseed completion or failure. It covers the complete `src/rng.rs` implementation and its inline tests, with the public trait definitions, byte-storage wrapper and package metadata needed to interpret that lifecycle. It does not analyze hardware entropy generation, peripheral access, protocol authentication or cryptographic attacks.

The evidence repository is `hispark-rs/hisi-crypto`, revision `56b619a25d4fafc6b7b46a1c14773c6ab69d5aad`, whose commit timestamp is 2026-08-12. Its recorded Git origin identifies the same GitHub repository as `Cargo.toml:9`. The manifest identifies version `0.1.0-alpha.5`; the changelog groups the entropy adapters and request-bounded DRBG additions under that release (`Cargo.toml:1-11`; `CHANGELOG.md:5-21`). These facts identify a source snapshot, not current remote freshness or a separately verified release artifact.

All six files below were read in full. Their unfiltered file hashes match their entries in the revision's Git tree. Source citations throughout are upstream-relative and refer to this revision.

| File | Lines | Git blob identity |
|---|---:|---|
| `src/rng.rs` | 605 | `19496614f8d196466c2421f7ab6271f3458c9136` |
| `src/lib.rs` | 604 | `b48ef460a04e36d59de96ba1992480978a763e18` |
| `src/secret.rs` | 73 | `efe43af9db459e4861c77dd5b9c3867e8e91bf18` |
| `Cargo.toml` | 44 | `39f9af88b3080e3398cfde22201e62c62281ee34` |
| `README.md` | 43 | `cca80a120498dcd3f2761951e7646ffd8ead82d4` |
| `CHANGELOG.md` | 55 | `fdafa5eaf318e0703e9b7c374fb46848761d49e5` |

Trust B denotes source-derived contracts and interpretation. Test definitions are evidence of intended assertions, not executed results or proof of a deployed entropy source.

## Available software and deduplication

The available source collection includes vendor SDKs (`Ai-BS21_SDK`, `fbb_ws63`, `device_soc_hisilicon`), host services (`Nld`, `nearlinkctl`, `communication_nearlink_service`), application samples, and the hispark-rs runtime, service and tooling family. Within the preferred hispark-rs group:

| Software family | Available implementation surfaces | Topic boundary |
|---|---|---|
| `hisiflash` | CLI configuration, monitor, native/WebAssembly ports, SEBOOT/YMODEM, package reader | Existing ecosystem and flashing reports already establish the broad tool/protocol role; not selected here. |
| `hisi-fwpkg` | ELF handling, image planning, patching, container production, CLI and parity tests | Image format and load-address semantics already appear in the ecosystem and September reports; not selected here. |
| `hisi-nvs`, `hisi-alloc` | Persistence and arena services | Complete recent implementation reports; explicitly excluded from this topic. |
| `hisi-crypto` | Portable capability contracts, entropy adapters, DRBG, typed key and arithmetic modules | Selected only for the complete entropy-to-DRBG service lifecycle. |
| `hisi-crypto-ws63`, PAC/SVD and register packages | Chip-specific backends and device descriptions | Outside the portable-software scope. |
| `hisi-rtos`, radio crates and examples | Scheduling, radio composition and consumers | Existing broad coverage; no scheduler or radio reslicing. |

The [harvest index](index.md) and bundle searches for `hisi-crypto`, `EntropyRng`, `HealthCheckedEntropy`, `ReseedingCryptoRng`, `HmacSha256Drbg`, continuous-block checks and reseeding terminology reveal only the brief provider overview in [the ecosystem report](NEW-HISPARK-RS-ECOSYSTEM.md), section 8. That overview already states the distinction between raw and qualified entropy and mentions zeroizing byte storage. Those are context, not novel discoveries here.

The new contribution is implementation-level accounting: 16-byte sample consumption, checker state retention, 32-byte seeding, exactly when successful requests trigger reseeding, empty-request handling, backend-specific limits, and nontransactional error boundaries. [The September increment](NEW-HISPARK-RS-SEPT-INCREMENT.md) concerns radio lifecycle and image tooling rather than these contracts. [The NVS persistence report](NEW-HISI-NVS-PERSISTENCE.md) concerns publication and page recovery; [the allocator lifecycle report](NEW-HISI-ALLOC-ARENA-LIFECYCLE.md) concerns arena ownership and allocation capacity. Neither implementation is reopened or subdivided here.

## Executive findings

1. **Raw entropy adaptation and DRBG admission are distinct APIs.** `EntropyRng<E>` implements only the fallible `TryRngCore` interface. `ReseedingCryptoRng<R, E>` requires both a `TrySeedableCryptoRng` backend and an explicitly marked `CryptoEntropySource`. The marker records a provider's qualification claim; it does not run qualification itself. Evidence: `src/lib.rs:153-163`; `src/rng.rs:21-75,151-162,292-305,380-385`.
2. **The continuous check consumes complete 16-byte samples and retains the last accepted sample.** Short destination tails still consume a full checked sample, with unused bytes discarded. A normal 32-byte seed needs two source calls. The same checker survives initial construction and later reseeds. Evidence: `src/rng.rs:10-19,77-149,285-312,327-332`.
3. **Reseeding is request-counted, not byte-counted or timed.** Exactly N successful nonempty requests are permitted for interval N; the next nonempty request reseeds before producing output. Empty fills neither generate output nor consume budget, even when reseeding is due. Evidence: `src/rng.rs:280-290,335-377`.
4. **The explicit software profile has separate per-call bounds.** `HmacSha256Drbg` accepts at most 65,536 output bytes and 256 personalization bytes, with a fixed 32-byte seed. These are this backend's checks, not limits enforced by the generic reseeding wrapper on every backend. Evidence: `src/rng.rs:10-17,238-277,297-305,368-375`.
5. **An error is not a universal rollback guarantee.** Failed required reseeding prevents that request from reaching generation, but earlier checked samples can already have advanced checker state. Direct checked fills can leave a successful prefix in the destination. No failure latch or automatic fallback is implemented. Evidence: `src/rng.rs:96-109,139-148,327-339,354-375`.
6. **The package remains an explicit service, not ambient randomness.** The crate is `no_std`; enabling its default software feature exposes the DRBG but does not select a source or change `RustCryptoProvider::fill_random`, which still returns `Unsupported`. Evidence: `src/lib.rs:1-20,342-344`; `Cargo.toml:13-38`; `README.md:8-11,19-28`.

## Substantive contracts

### Ownership and capability boundaries

`EntropySource::fill_entropy` takes `&self`, an output slice and a fallible result. `EntropyRng` owns its source, permits immutable source access or consuming extraction, and forwards slice requests directly. Its integer helpers obtain four or eight bytes and decode little-endian values. It does not add continuous checking, buffering or reseed policy (`src/lib.rs:153-163`; `src/rng.rs:39-75`).

`HealthCheckedEntropy` also owns the source, but requires `&mut self` for `fill_checked` because it retains a previous-sample state. It offers source access and consuming extraction, not automatic promotion to `CryptoEntropySource`. `ReseedingCryptoRng` owns both the chosen generator and this checker; its ordinary inspection accessors expose immutable references and the request count (`src/rng.rs:113-149,285-290,315-325`).

The `CryptoEntropySource` trait is a public, non-automatic marker. Its documentation assigns qualification evidence to the implementer. The continuous duplicate-block test explicitly does not replace source-specific startup checks or qualification (`src/lib.rs:158-163`; `src/rng.rs:77-82`). This division is a software composition contract, not evidence that any particular platform source qualifies.

`TrySeedableCryptoRng` requires `TryCryptoRng` and `TryRngCore<Error = CryptoError>`, and specifies fallible construction and reseeding from `SecretBytes<32>`. It does not impose a universal request-length or personalization limit. Choosing the provided HMAC profile gives its explicit bounds; choosing another implementation requires checking that implementation's contracts (`src/rng.rs:151-162,259-277`).

### Checked sampling and state retention

For a requested checked output of L bytes, the checker makes `ceil(L / 16)` entropy calls of exactly 16 bytes, or zero calls for L = 0. It compares each complete sample with the last accepted sample before copying the requested prefix into the destination. For example, a 17-byte destination consumes 32 source bytes; 15 checked bytes are discarded, not cached for the next request. This follows directly from the chunk loop (`src/rng.rs:139-148`).

The first sample is accepted without a predecessor. Later equal adjacent samples return `EntropyHealthCheckFailed`; only acceptance overwrites the stored previous sample. The error leaves that previous accepted value in place. The next call can accept a different sample: the implementation has no permanently failed state (`src/rng.rs:83-110`).

This state persists across successful `fill_checked` calls and across initial seeding and reseeding in one wrapper instance. It is not global state shared among all wrappers or all users of a source (`src/rng.rs:117-127,285-312,327-332`).

If failure occurs after earlier chunks succeeded, the destination already contains those chunks and the checker already remembers their last accepted sample. The method offers a fallible fill, not an atomic buffer replacement. Callers must only use a complete result after success (`src/rng.rs:140-146`).

### Construction, request budget and reseed completion

Construction creates one checker, obtains a 32-byte checked seed, then invokes `R::try_from_seed` with caller-supplied personalization. Only successful construction produces a wrapper with count zero. Consequently, a software-backend personalization rejection occurs after entropy collection, not before it (`src/rng.rs:264-265,297-312`).

The reseed interval is `NonZeroU32`; zero cannot be supplied through this API. With interval N:

| Operation | Accounting and ordering |
|---|---|
| Successful integer generation | Check whether count is at least N; reseed if needed; generate; increment once. |
| Successful nonempty slice generation | Same accounting, regardless of slice length. |
| Empty slice generation | Return immediately without checking the count, reseeding or incrementing. |
| Explicit `force_reseed` | Collect a new checked seed; reseed backend; reset count only after both succeed. |
| Backend generation error | Propagate error without incrementing the count. |

Evidence: `src/rng.rs:327-377`.

An interval of two therefore permits two successful generation calls before the third attempts reseeding. This is also the transition asserted by `successful_requests_trigger_bounded_reseed` (`src/rng.rs:515-533`). A one-byte and a 65,536-byte successful fill each consume one request when using the HMAC backend. No clock, lifetime timer, accumulated-byte threshold or prediction-resistance switch appears in this service.

The wrapper checks its budget before asking the backend to generate. Thus an oversized nonempty HMAC request submitted when reseeding is due can first consume fresh entropy and reset the count, then fail with `InvalidLength`. A failed call does not necessarily mean no service state changed (`src/rng.rs:327-338,368-375`, together with `src/rng.rs:238-240`).

If a required reseed fails, the generator is not called for that request and the count is not reset. A later request retries the due reseed. An explicit forced reseed that fails before the interval is exhausted likewise leaves the old count; it does not install a universal output-disable latch. A custom backend may have its own state effects before returning an error, since no rollback method is part of `TrySeedableCryptoRng` (`src/rng.rs:155-162,327-339,354-375`).

### Explicit HMAC-SHA-256 profile

The profile stores two 32-byte values, key and value, as `SecretBytes`. Construction initializes them to zero and one respectively, then applies an update over seed and personalization. Updates always perform the separator-zero pass; a separator-one pass follows when supplied data contains any nonempty part. Reseeding updates with the supplied seed (`src/rng.rs:164-219,259-277`).

A nonempty generation request repeatedly updates the value with HMAC and copies up to 32 bytes per iteration, then performs one update with no supplied data. Empty generation is a no-op. As a result, call boundaries are meaningful: two separate nonempty requests each perform a final update, unlike one combined request. The profile is not an arbitrary byte stream whose request partitioning is irrelevant (`src/rng.rs:238-252`).

The 65,536-byte request maximum bounds the output loop to 2,048 iterations in this backend. It is not a measured execution-time limit. The bare `HmacSha256Drbg` type has no reseed counter or entropy source; automatic reseeding belongs to `ReseedingCryptoRng`, not to the primitive itself (`src/rng.rs:171-174,238-277,285-290`).

The software backend is behind `rustcrypto`, which is enabled by default. The manifest requests `rand_core` 0.9.5 without default features and optional `hmac` 0.12 and `sha2` 0.10 for this profile. These are manifest requirements, not claims about every downstream dependency resolution. The generic adapters remain available without the software implementation (`Cargo.toml:13-38`; `src/lib.rs:13-19`).

### Byte lifetime and error reporting

Checker history, seed scratch, and HMAC key/value storage use `SecretBytes`. That type derives `Zeroize` and `ZeroizeOnDrop`, omits `Clone` and `Copy`, exposes bytes through explicitly named methods, and renders a redacted `Debug` value. The implementation establishes clearing behavior for the wrapper's owned storage, not a guarantee about every temporary inside external hash implementations or every copy made by a caller (`src/secret.rs:5-48`; `src/rng.rs:84-85,142-145,171-174,303-305,328-330`).

The caller's output slice is not owned or automatically cleared by this service. Direct entropy forwarding can inherit source-specific partial-write behavior; checked fills can publish successful prefixes; a generic generator can likewise modify output before an error. The specific required-reseed failure path blocks generation before touching that output through the generator, but should not be generalized into transactional behavior for every error (`src/rng.rs:57-74,139-148,354-375`).

`CryptoError` separates invalid lengths, failed entropy checks and backend statuses. `code()` maps the entropy-check failure to `0xffff000c` and returns backend status values unchanged. This gives portable callers a common error type without erasing provider diagnostics (`src/lib.rs:22-57`).

## Source tests and runtime limits

The seven inline RNG tests define these assertions (`src/rng.rs:493-604`):

- Duplicate adjacent samples are rejected.
- An initial source failure prevents wrapper construction.
- Two successful requests with interval two cause reseeding before the third.
- A due-reseed entropy failure leaves the output and exhausted request count unchanged in that fixture.
- An empty request leaves the count unchanged.
- One fixed 64-byte HMAC output matches a literal expected vector.
- Oversized output and personalization are rejected by the HMAC profile.

The test source contains deterministic counter and constant entropy providers, explicit failing providers and a mock DRBG. Their marker implementations support control-flow tests; they are not real entropy qualification evidence (`src/rng.rs:398-491`). The known-answer test is named as independent, but its source provides the expected bytes rather than an independently inspected derivation (`src/rng.rs:568-587`). None of these definitions is a reported successful execution.

The compile-fail example states that raw `EntropyRng` cannot satisfy `TryCryptoRng` (`src/rng.rs:21-38`). The byte wrapper has assertions for redacted formatting and explicit clearing, not an observed target-level destruction test (`src/secret.rs:51-73`).

Further boundaries matter for ordinary integration:

- There is no time bound on a source call, no cancellation interface, and no scheduler synchronization policy in the selected service. Synchronous fallibility does not imply a deadline (`src/lib.rs:153-163`; `src/rng.rs:139-148,327-339`).
- Fixed-size state and scratch buffers avoid explicit heap allocation in the selected production RNG module, but do not establish an exact stack budget, dependency behavior, throughput or total object size for arbitrary generic providers (`src/rng.rs:83-86,142-145,171-174,285-290`).
- The tests do not exhaustively establish partial-tail consumption, inter-call duplicate detection, recovery after a failed forced reseed, custom-backend reseed mutation or exact maximum-length acceptance. The corresponding contract discussion above is derived from implementation order, not additional executed cases.
- The source's HMAC-DRBG description is not independent certification, entropy assessment or a claim of complete standards conformance (`src/rng.rs:164-169`).
- No inspected consumer establishes which radio application actually constructs this wrapper. Nothing here proves WS73 compatibility, a Linux USB interface, device behavior or field reliability. The crate explicitly owns primitives rather than protocol state machines or persistent keys (`README.md:8-11`).

## Reusable lessons and comparison anchors

1. **Separate raw input, qualification and generation policy.** The source trait, explicit marker, checker and selected DRBG occupy different roles. Preserve those distinctions rather than treating a raw byte source as a fully configured random service (`src/lib.rs:153-163`; `src/rng.rs:151-162,292-305`). This develops the brief service-layer distinction in [the ecosystem overview](NEW-HISPARK-RS-ECOSYSTEM.md).
2. **Name the unit of a budget.** A successful-request interval is neither a byte quota nor a time deadline. Empty calls and failures need defined accounting, and backend request limits need separate documentation (`src/rng.rs:280-284,327-377`).
3. **Document state effects before error returns.** Checked-prefix acceptance, completed reseeding before a later length error, and count reset after backend success are distinct transitions. This is analogous to the importance of completion boundaries in [NVS persistence](NEW-HISI-NVS-PERSISTENCE.md), but it is a different service with no persistent publication or compaction.
4. **Keep capacity observations separate from admission policy.** Here, a backend's maximum output size and the wrapper's request budget answer different questions. [The allocator report](NEW-HISI-ALLOC-ARENA-LIFECYCLE.md) makes an analogous distinction between free bytes and admissible payload, without sharing this implementation.
5. **Do not confuse an available default backend with an automatically configured service.** The software feature is enabled by default, while construction still requires a source and explicit reseed policy; the legacy random-provider operation remains unsupported (`Cargo.toml:26-38`; `src/rng.rs:297-305`; `src/lib.rs:342-344`).
