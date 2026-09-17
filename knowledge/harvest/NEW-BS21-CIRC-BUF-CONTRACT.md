---
type: harvest
title: "Ai-BS21 inline byte ring: external ownership, capacity ambiguity and publication boundaries"
language: en
created: 2026-09-17
tags: [harvest, bs21, ring-buffer, ownership, capacity, wrap, concurrency, source-contract]
sources:
  - "https://github.com/Ai-Thinker-Open/Ai-BS21_SDK/blob/f4f3f28781f4610a1d0a97fe24907d59ed03d09d/middleware/utils/dfx/diag_dfx_cmd/diag_audio_sample_data/include/circ_buf.h"
  - "https://github.com/Ai-Thinker-Open/Ai-BS21_SDK/blob/f4f3f28781f4610a1d0a97fe24907d59ed03d09d/middleware/utils/dfx/diag_dfx_cmd/diag_audio_sample_data/CMakeLists.txt"
trust: B
stale_after: 2027-03-17
---

# Ai-BS21 inline byte ring: external ownership, capacity ambiguity and publication boundaries

## Scope and evidence

This is a source-contract reading of `middleware/utils/dfx/diag_dfx_cmd/diag_audio_sample_data/include/circ_buf.h` at revision `f4f3f28781f4610a1d0a97fe24907d59ed03d09d`. The inspected module files match that revision. Pinned GitHub links identify provenance; they are not a network verification.

**The ring implementation is present as static inline function bodies, not merely declarations.** The module contains `CMakeLists.txt` and three headers, with no separate ring-buffer implementation source. Its build description names `libdfx_audio_sample_data.a` and publishes the include directory; that reference does not establish the archive's contents, availability or actual use of this header. [circ_buf.h:20-220][all], [CMakeLists.txt:5-11][build].

Trust B distinguishes directly visible source behavior from inferred consumer obligations. No runtime behavior, calling context, linked consumer policy, transfer performance or WS73 equivalence is established. The subject is only ring storage and cursor ownership, capacity, wrap and concurrency; diagnostic control and device operations are outside scope.

## Executive findings

1. **Storage is borrowed, including the cursor cells.** The descriptor retains pointers to two externally supplied `td_u32` positions and a byte array. Initialization resets both positions; flush resets them without clearing data. No allocation, release or lifetime management is supplied. [circ_buf.h:20-25,40-55][ownership].
2. **Reported free space is not an enforced writable-capacity contract.** Busy size is modular distance; equal positions mean zero busy bytes. Free size is `size - busy`, with no reserved-slot deduction or full flag. `circ_buf_write` does not check free space. A conventional unambiguous FIFO therefore needs an external capacity policy, such as preserving one unused byte; the header neither implements nor documents that policy. [circ_buf.h:57-65][sizes], [circ_buf.h:107-125][write-copy], [circ_buf.h:151-162][write].
3. **Wrap is one subtraction, not saturation or general modulo.** `saturate_add` subtracts the size once after addition. The copy helpers split into a tail span and a head span, not an arbitrary number of laps. Valid position, length and arithmetic bounds remain caller obligations. [circ_buf.h:32-38][wrap], [circ_buf.h:76-125][copies].
4. **Observation, copying and cursor commitment are separate operations.** Read consumes after a successful full-length copy; `peak` copies without consuming. `cast_read` returns a starting offset without reserving or promising a contiguous span, while the literally spelled `cast_relese` clamps release to currently observed busy bytes. [circ_buf.h:137-149][read], [circ_buf.h:176-220][cast-peak].
5. **Volatile cursor accesses do not establish concurrent publication.** The header supplies no lock, atomic protocol or explicit memory barrier. It cannot alone support a portable lock-free or interrupt-safe claim, including for a single producer and single consumer. [circ_buf.h:20-25][ownership], [circ_buf.h:127-174][positions].

## Ownership and operation contract

The `write` and `read` fields are pointers to volatile-qualified cursor values, not pointers into the payload array. Offsets and lengths operate on `td_u8` storage. The descriptor does not own or copy an allocation, and copying the descriptor would retain aliases to the same positions and bytes. A consumer must keep the descriptor, separate cursor storage and byte storage valid for every access. These lifetime and non-aliasing requirements are derived from the pointer-based implementation, not an explicit upstream lifetime promise. [circ_buf.h:20-25,40-49][ownership].

| Operation | Visible behavior | Consumer boundary |
| --- | --- | --- |
| `circ_buf_init`, `circ_buf_flush` | Both set both cursor cells to zero; init also installs pointers and size. | Reset requires exclusive lifecycle control. Flush is a logical discard, not data erasure or a storage-release operation. [circ_buf.h:40-55][ownership] |
| `circ_buf_query_busy`, `circ_buf_query_free` | Read both cursor cells and calculate a count. | A query reserves nothing; its separate cursor loads do not by themselves guarantee a coherent two-cursor snapshot under concurrent modification. Whether external synchronization makes the observation coherent is not established here. [circ_buf.h:127-135][positions] |
| `circ_buf_read` | Rejects zero length or insufficient observed data; advances read only if the copy helper returns exactly the requested length. | No partial count is exposed by this wrapper. A zero result does not promise an unchanged destination if a split copy has partly succeeded. [circ_buf.h:67-87][read-copy], [circ_buf.h:137-149][read] |
| `circ_buf_write` | Rejects zero length or a copy-count mismatch; advances write on a full-count result. | It does not perform admission against the read position or free count. Copy failure does not imply rollback of byte storage. [circ_buf.h:98-125][write-copy], [circ_buf.h:151-162][write] |
| `circ_buf_read_data`, `circ_buf_write_data` | Copy at the supplied offset without committing a cursor. | Availability, valid offset, backing capacity and serialization must be established separately. The write helper explicitly tells callers to check free space. [circ_buf.h:76-125][copies] |
| `circ_buf_update_read_pos`, `circ_buf_update_write_pos`, `circ_buf_poke` | Advance a cursor using the single-wrap helper, without checking availability or capacity. | These are unchecked commitment primitives, not reservation or bounds-validation APIs. [circ_buf.h:164-174][updates], [circ_buf.h:214-220][poke] |

With a null destination, `circ_buf_read_data` returns the requested length without copying. Consequently, checked `circ_buf_read` can discard available bytes; `circ_buf_peak` with that destination does not advance the cursor. Neither operation wipes storage. The write helper documents null input as mute-data generation and selects zero filling; that intent is separate from any guarantee about all C edge cases or the secure-library implementation. [circ_buf.h:76-95][read-copy-zero], [circ_buf.h:116-125][write-copy], [circ_buf.h:137-149][read], [circ_buf.h:201-212][peak].

The two segment calls are operands of `+`; the expression does not specify which call executes first. Their returned counts are summed, and there is no rollback path. The secure-copy helpers receive lengths supplied or derived here rather than independently discovered allocation sizes. Their presence is not a substitute for a validated caller storage contract. [circ_buf.h:67-125][copy-all].

## Capacity and wrap: the missing invariant

Let `N` denote the configured byte-array size and assume normalized positions `0 <= read, write < N`. The visible count formula is the forward distance from read to write. It represents `0` through `N - 1` busy bytes; cursor equality has only the empty interpretation. No occupancy counter, generation bit or separate full state appears in `circ_buf`. [circ_buf.h:20-25][ownership], [circ_buf.h:57-65][sizes].

This creates a specific contract gap: the free query includes the last byte that would make the write cursor equal the read cursor. Merely following the comment to check free space is insufficient if equality is accepted as permission to fill all reported space. Since the write wrapper does not enforce admission, coherent FIFO use requires an external invariant that prevents a full ring from becoming indistinguishable from empty. Reserving one byte gives usable capacity `N - 1` for `N > 0`; **that is a proposed consumer rule, not a capacity guarantee already implemented here**. An independently maintained full state would instead require different accounting rather than blind reliance on these query functions. [circ_buf.h:57-65][sizes], [circ_buf.h:107-125][write-copy], [circ_buf.h:152-161][write].

For single-subtraction wrapping to normalize a position, the mathematical sum must be below `2N`, and the `td_u32` addition must not overflow before the comparison. The ordinary bounded one-lap case can satisfy that, but the unchecked update APIs do not validate it. No power-of-two constraint is encoded: the mechanism is subtraction, not bit masking. [circ_buf.h:32-38][wrap], [circ_buf.h:164-174][updates].

The copy split is `min(len, size - off)` followed by any remainder at offset zero. It is compatible with a bounded one-lap operation and a valid offset; it is not proof that arbitrary lengths or invalid positions are accepted safely. Initialization validates neither size nor storage. These are source-derived preconditions, not reproduced runtime failures. [circ_buf.h:40-49][ownership], [circ_buf.h:76-125][copies].

## Zero-copy observation is not ownership transfer

`circ_buf_cast_read` checks total busy bytes and returns the current read offset plus the requested count. It neither advances the cursor nor limits the count to the bytes physically remaining before the array end. Therefore a successful return does **not** promise one contiguous span. A consumer must distinguish total available bytes from the tail/head split. [circ_buf.h:176-187][cast].

`circ_buf_cast_relese` recomputes busy size and advances by the smaller of that value and the requested release length. It carries no reservation token, original-offset check or pairing with an earlier cast. `circ_buf_peak` similarly retains no lease on the data it observes. Retaining a zero-copy view needs a separate lifetime and exclusion policy, especially across another reader, reset or producer activity. The header supplies no such policy. [circ_buf.h:189-212][release-peak].

## Concurrency and reusable boundary

The code has an intuitive sequence: copy payload, then assign the corresponding cursor. That source order is not a demonstrated cross-thread publication protocol. Volatile-qualified cursor values do not provide C atomic synchronization, acquire/release ordering or exclusion for the ordinary byte array. The query functions also load the two positions separately. No OSAL primitive is invoked by these bodies. [circ_buf.h:20-25][ownership], [circ_buf.h:127-174][positions].

A coherent consumer contract must therefore define:

- Who owns write advancement and who owns read advancement, including raw update and discard APIs.
- How count observation, admission, payload access and cursor commitment are serialized or otherwise ordered.
- How init/flush obtain exclusive access to both positions, and when outstanding views expire.
- How the unused-slot invariant, normalized positions, arithmetic bounds and caller allocation sizes are maintained.
- Whether a zero result permits partial byte effects, rather than assuming transactional copying.

These are reusable review criteria, not an imported implementation or a verified synchronization fix. External synchronization might already exist in a consumer; the inspected module does not establish it. Neither single-producer/single-consumer safety nor multiple-producer/multiple-consumer safety can be inferred from the field qualifiers.

## Prior coverage and novelty

Knowledge searches for the module name, `circ_buf`, `saturate_add`, and ring/circular-buffer terms found no existing report of this header's contract. Relative links below follow the harvest-directory convention.

- [SLE UART variants](NEW-SLE-UART-VARIANTS.md) already discuss an 8 KiB ring plus 128-byte retry chunks and contrast buffering with direct callbacks. This report adds a different component's actual storage/cursor representation, full-versus-empty ambiguity, unchecked commitment and split-span semantics. It does not transfer those UART sizes or retry policies to this module.
- [Mesh transport substrate](NEW-MESH-TRANSPORT-SUBSTRATE.md) describes a rotating deduplication cache keyed by source and sequence. That cache is not this producer/consumer byte FIFO; a shared ring-buffer label does not imply shared occupancy or ownership semantics.
- [BS21 OSAL locking contract](NEW-BS21-OSAL-LOCKING-CONTRACT.md) separates lock declarations and ordering evidence. Here the inline bodies are visible and contain no explicit synchronization primitives, while consumer synchronization remains unknown. OSAL API availability does not supply a missing publication protocol automatically.

The substantive addition is one connected contract: borrowed storage and mutable cursor cells require a caller-owned lifecycle; modular distance requires an explicit capacity invariant; split copies require bounded spans; and cursor publication requires external synchronization evidence. None of those source findings establishes diagnostic reachability, device behavior or a deployed runtime defect.

[all]: https://github.com/Ai-Thinker-Open/Ai-BS21_SDK/blob/f4f3f28781f4610a1d0a97fe24907d59ed03d09d/middleware/utils/dfx/diag_dfx_cmd/diag_audio_sample_data/include/circ_buf.h#L20-L220
[build]: https://github.com/Ai-Thinker-Open/Ai-BS21_SDK/blob/f4f3f28781f4610a1d0a97fe24907d59ed03d09d/middleware/utils/dfx/diag_dfx_cmd/diag_audio_sample_data/CMakeLists.txt#L5-L11
[ownership]: https://github.com/Ai-Thinker-Open/Ai-BS21_SDK/blob/f4f3f28781f4610a1d0a97fe24907d59ed03d09d/middleware/utils/dfx/diag_dfx_cmd/diag_audio_sample_data/include/circ_buf.h#L20-L55
[sizes]: https://github.com/Ai-Thinker-Open/Ai-BS21_SDK/blob/f4f3f28781f4610a1d0a97fe24907d59ed03d09d/middleware/utils/dfx/diag_dfx_cmd/diag_audio_sample_data/include/circ_buf.h#L57-L65
[wrap]: https://github.com/Ai-Thinker-Open/Ai-BS21_SDK/blob/f4f3f28781f4610a1d0a97fe24907d59ed03d09d/middleware/utils/dfx/diag_dfx_cmd/diag_audio_sample_data/include/circ_buf.h#L32-L38
[copies]: https://github.com/Ai-Thinker-Open/Ai-BS21_SDK/blob/f4f3f28781f4610a1d0a97fe24907d59ed03d09d/middleware/utils/dfx/diag_dfx_cmd/diag_audio_sample_data/include/circ_buf.h#L76-L125
[read-copy]: https://github.com/Ai-Thinker-Open/Ai-BS21_SDK/blob/f4f3f28781f4610a1d0a97fe24907d59ed03d09d/middleware/utils/dfx/diag_dfx_cmd/diag_audio_sample_data/include/circ_buf.h#L67-L87
[read-copy-zero]: https://github.com/Ai-Thinker-Open/Ai-BS21_SDK/blob/f4f3f28781f4610a1d0a97fe24907d59ed03d09d/middleware/utils/dfx/diag_dfx_cmd/diag_audio_sample_data/include/circ_buf.h#L76-L95
[write-copy]: https://github.com/Ai-Thinker-Open/Ai-BS21_SDK/blob/f4f3f28781f4610a1d0a97fe24907d59ed03d09d/middleware/utils/dfx/diag_dfx_cmd/diag_audio_sample_data/include/circ_buf.h#L98-L125
[copy-all]: https://github.com/Ai-Thinker-Open/Ai-BS21_SDK/blob/f4f3f28781f4610a1d0a97fe24907d59ed03d09d/middleware/utils/dfx/diag_dfx_cmd/diag_audio_sample_data/include/circ_buf.h#L67-L125
[read]: https://github.com/Ai-Thinker-Open/Ai-BS21_SDK/blob/f4f3f28781f4610a1d0a97fe24907d59ed03d09d/middleware/utils/dfx/diag_dfx_cmd/diag_audio_sample_data/include/circ_buf.h#L137-L149
[write]: https://github.com/Ai-Thinker-Open/Ai-BS21_SDK/blob/f4f3f28781f4610a1d0a97fe24907d59ed03d09d/middleware/utils/dfx/diag_dfx_cmd/diag_audio_sample_data/include/circ_buf.h#L151-L162
[positions]: https://github.com/Ai-Thinker-Open/Ai-BS21_SDK/blob/f4f3f28781f4610a1d0a97fe24907d59ed03d09d/middleware/utils/dfx/diag_dfx_cmd/diag_audio_sample_data/include/circ_buf.h#L127-L174
[updates]: https://github.com/Ai-Thinker-Open/Ai-BS21_SDK/blob/f4f3f28781f4610a1d0a97fe24907d59ed03d09d/middleware/utils/dfx/diag_dfx_cmd/diag_audio_sample_data/include/circ_buf.h#L164-L174
[cast]: https://github.com/Ai-Thinker-Open/Ai-BS21_SDK/blob/f4f3f28781f4610a1d0a97fe24907d59ed03d09d/middleware/utils/dfx/diag_dfx_cmd/diag_audio_sample_data/include/circ_buf.h#L176-L187
[cast-peak]: https://github.com/Ai-Thinker-Open/Ai-BS21_SDK/blob/f4f3f28781f4610a1d0a97fe24907d59ed03d09d/middleware/utils/dfx/diag_dfx_cmd/diag_audio_sample_data/include/circ_buf.h#L176-L220
[release-peak]: https://github.com/Ai-Thinker-Open/Ai-BS21_SDK/blob/f4f3f28781f4610a1d0a97fe24907d59ed03d09d/middleware/utils/dfx/diag_dfx_cmd/diag_audio_sample_data/include/circ_buf.h#L189-L212
[peak]: https://github.com/Ai-Thinker-Open/Ai-BS21_SDK/blob/f4f3f28781f4610a1d0a97fe24907d59ed03d09d/middleware/utils/dfx/diag_dfx_cmd/diag_audio_sample_data/include/circ_buf.h#L201-L212
[poke]: https://github.com/Ai-Thinker-Open/Ai-BS21_SDK/blob/f4f3f28781f4610a1d0a97fe24907d59ed03d09d/middleware/utils/dfx/diag_dfx_cmd/diag_audio_sample_data/include/circ_buf.h#L214-L220
