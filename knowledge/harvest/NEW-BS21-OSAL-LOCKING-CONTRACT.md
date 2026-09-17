---
type: harvest
title: "Ai-BS21 OSAL locks: ownership, interrupt state and memory-ordering evidence limits"
language: en
created: 2026-09-17
tags: [harvest, bs21, osal, locking, mutex, spinlock, rwlock, memory-ordering]
sources:
  - "https://github.com/Ai-Thinker-Open/Ai-BS21_SDK/blob/f4f3f28781f4610a1d0a97fe24907d59ed03d09d/kernel/osal/include/lock/osal_mutex.h"
  - "https://github.com/Ai-Thinker-Open/Ai-BS21_SDK/blob/f4f3f28781f4610a1d0a97fe24907d59ed03d09d/kernel/osal/include/lock/osal_spinlock.h"
  - "https://github.com/Ai-Thinker-Open/Ai-BS21_SDK/blob/f4f3f28781f4610a1d0a97fe24907d59ed03d09d/kernel/osal/include/lock/osal_rwlock.h"
  - "https://github.com/Ai-Thinker-Open/Ai-BS21_SDK/blob/f4f3f28781f4610a1d0a97fe24907d59ed03d09d/kernel/osal/include/atomic/osal_atomic.h"
trust: B
stale_after: 2027-03-17
---

# Ai-BS21 OSAL locks: ownership, interrupt state and memory-ordering evidence limits

## Scope and evidence

One theme: the public locking contract and the limits of its shared-memory ordering evidence. The three lock headers were read completely, with the atomic header read completely only to resolve the adjacent ordering question. The inspected revision is `f4f3f28781f4610a1d0a97fe24907d59ed03d09d`; the scoped source diff was empty. Pinned GitHub links identify the source revision, not a network-verified fetch.

**Declarations and definitions** establish signatures, field types and constants. **Comments** document intended behavior and backend support. **Verified implementation behavior: none.** These headers contain no function bodies; no implementation, binary or runtime evidence was examined. Absence of a documented guarantee is not proof that a backend lacks it. Trust B applies to this source-contract reading, not to cross-platform behavioral verification.

## Executive findings

1. **Mutex ownership is documented, but recursion is backend-qualified.** Locking sleeps when unavailable; the same task must unlock, must not exit while holding the mutex, and must not free its storage while locked. General prose forbids recursive locking, while the same block says LiteOS and FreeRTOS support nested locking. This is a generic rule with a documented backend exception, not proof of a deadlock bug. [osal_mutex.h:40-63][mutex-lock].
2. **Interrupt variants are not interchangeable portable locks.** Spinlock comments direct task/interrupt sharing to `osal_spin_lock_irqsave`. The `_bh` lock documents Linux soft-interrupt suppression but LiteOS/FreeRTOS scheduling suppression. These are distinct documented effects; neither names nor comments establish a uniform CPU-wide or cross-core mechanism. [osal_spinlock.h:41-74][spin-lock].
3. **The try-IRQ-save declaration has no return status.** Unlike the two `int` trylock variants, `osal_spin_trylock_irqsave` returns `void` and takes `unsigned long *flags`. Its comment describes a try, saving IRQ state and disabling interrupts, but gives no acquisition-result encoding or failure-state rule. This is an incomplete public contract, not evidence that the implementation blocks, fails, or unlocks incorrectly. [osal_spinlock.h:76-120][spin-try].
4. **Reader/writer locking is a sparse, Linux-only documented surface.** Six declarations cover init, read lock/unlock, write lock/unlock and the literally spelled `osal_rwlock_destory`. The comments do not settle fairness, reader concurrency policy, upgrades, recursion, blocking context or destruction synchronization. Do not infer a particular Linux primitive from the wrapper name. [osal_rwlock.h:18-100][rw-all].
5. **No explicit acquire/release or full-barrier contract appears in the inspected lock or atomic headers.** Atomic comments describe operations as atomic; the struct contains `volatile int counter`. Neither that field qualifier nor atomic API names establish publication ordering, sequential consistency, lock freedom, or permission to bypass the functions with direct concurrent field accesses. Missing ordering documentation is not proof of missing backend barriers. [osal_mutex.h:18-165][mutex-all], [osal_spinlock.h:18-193][spin-all], [osal_rwlock.h:18-100][rw-all], [osal_atomic.h:18-177][atomic-all].

## Mutex contract: ownership, waiting and release

The wrapper is `struct { void *mutex; }`; `OSAL_MUTEX_WAIT_FOREVER` is defined as `(-1)`. These are visible definitions, not a backend control-block layout or a proved wait-sentinel implementation. [osal_mutex.h:18-22][mutex-type].

| API group | Declaration evidence | Comment contract and limits |
| --- | --- | --- |
| Init | `int osal_mutex_init(osal_mutex *)` | SUCCESS/FAILURE; Linux, LiteOS, FreeRTOS. [osal_mutex.h:24-38][mutex-init] |
| Lock | `int osal_mutex_lock(osal_mutex *)` | Exclusive task ownership, sleeping wait, SUCCESS/FAILURE; Linux, LiteOS, FreeRTOS, with nested-lock exceptions described above. Initialization required; zeroing with `memset` is forbidden. The static-definition alternative mentioned in prose has no initializer macro in this header. [osal_mutex.h:40-63][mutex-lock] |
| Timeout lock | `int osal_mutex_lock_timeout(osal_mutex *, unsigned int timeout)` | SUCCESS/FAILURE; LiteOS, FreeRTOS; nested locks supported. Timeout units, zero behavior and sentinel acceptance are not stated. Passing `-1` to this unsigned parameter converts it to the unsigned maximum in C; this does not verify that the backend recognizes it as forever. [osal_mutex.h:65-81][mutex-timeout] |
| Interruptible lock | `int osal_mutex_lock_interruptible(osal_mutex *)` | Linux, LiteOS; LiteOS nested-lock exception. Signal during sleep returns without acquisition; SUCCESS/FAILURE/EINTR documented. Despite “until timeout” in the brief, the declaration has no timeout parameter. [osal_mutex.h:83-99][mutex-interruptible] |
| Trylock | `int osal_mutex_trylock(osal_mutex *)` | Nonwaiting; TRUE/FALSE rather than the ordinary lock's SUCCESS/FAILURE; Linux, LiteOS, FreeRTOS, with nested-lock exceptions. Numeric constants are not defined here. [osal_mutex.h:101-116][mutex-try] |
| Unlock / query | `void osal_mutex_unlock(osal_mutex *)`; `int osal_mutex_is_locked(osal_mutex *)` | Unlock forbids interrupt context and unlocking an unlocked mutex; all three systems listed. Query says true when locked, false when unlocked, and lists only Linux/FreeRTOS. It provides no acquisition or ownership reservation. [osal_mutex.h:118-147][mutex-unlock] |
| Destroy | `void osal_mutex_destroy(osal_mutex *)` | All three systems listed. Comment says it frees memory and caller should clear a pointer to NULL, but does not identify the exact allocation/pointer or promise waiting for holders/waiters. [osal_mutex.h:149-165][mutex-destroy] |

Do not import timeout units from the adjacent IPC or schedule APIs. Nor does an unlocked observation establish that no other task can acquire the mutex before destruction. These are consumer-design boundaries, not reproduced failures.

## Spinlocks: context and saved-state contracts

`osal_spinlock` exposes only `void *lock`. Initialization returns `int` with SUCCESS/FAILURE comments; init and destroy list Linux/LiteOS, require their lifecycle pairing and document memory release. The destroy comment's phrase “init returns” does not mean init returns a lock pointer: its actual return type is `int`. [osal_spinlock.h:18-39][spin-init], [osal_spinlock.h:177-193][spin-destroy].

Ordinary lock is documented to wait cyclically under contention, require initialization, and deadlock on repeated acquisition in the same task. Ordinary lock/unlock list Linux/LiteOS. This is an explicit comment warning, not a tested failure. [osal_spinlock.h:41-59][spin-ordinary], [osal_spinlock.h:122-134][spin-unlock].

The backend and state distinctions are material:

- `osal_spin_lock_bh` / `osal_spin_unlock_bh` are `void` functions and list Linux/LiteOS/FreeRTOS. Lock prose says Linux **soft interrupts**, while unlock prose says it enables CPU **interrupts**; LiteOS/FreeRTOS are described as disabling/resuming scheduling. The Linux terminology mismatch cannot establish which interrupt class actually changes. [osal_spinlock.h:61-74][spin-bh-lock], [osal_spinlock.h:136-147][spin-bh-unlock].
- Plain trylock lists Linux/LiteOS and returns true on immediate success, false otherwise. `osal_spin_trylock_irq` lists Linux only, adds CPU-interrupt disabling, and uses the same return prose. `osal_spin_trylock_irqsave` lists Linux only but returns `void`; `flags` has no documented success encoding. Neither IRQ try variant specifies interrupt state after unsuccessful acquisition. [osal_spinlock.h:76-120][spin-try].
- `osal_spin_lock_irqsave` and `osal_spin_unlock_irqrestore` both return `void`, both take `unsigned long *flags`, and both list all three systems. Acquisition documents saving CPU IRQ status, acquiring and disabling interrupts; release says both “restores” status and “enables” interrupts. That wording does not resolve behavior when the saved state was already disabled. Preserve the declared pointer type rather than substituting a familiar native macro signature. [osal_spinlock.h:149-175][spin-save].

The header declares FreeRTOS support for selected variants but not init/destroy or ordinary lock/unlock. This is an incomplete documented portable lifecycle, not proof of absent linked symbols. No standalone `osal_spin_unlock_irq` is declared in this complete header; the intended release pairing for `osal_spin_trylock_irq` is not explained here. [osal_spinlock.h:18-193][spin-all].

## Reader/writer locks and ordering evidence

`osal_rwlock` is another one-pointer wrapper. Init returns `int` with SUCCESS/FAILURE comments; the remaining five functions return `void`. Every support block says Linux. Preserve the declaration spelling `osal_rwlock_destory` in any binding. Its short “Release Lock” description provides no explicit allocation, waiter-draining or destruction-status contract. Sparse documentation and spelling alone are not implementation bugs. [osal_rwlock.h:18-100][rw-all].

The atomic header was necessary to check whether an explicit ordering contract existed adjacent to the locks. It does not close the gap:

- Read/set and increment/decrement, including increment/decrement-with-result, list Linux/LiteOS/FreeRTOS. The result variants document the updated value. [osal_atomic.h:18-56][atomic-basic], [osal_atomic.h:73-95][atomic-dec], [osal_atomic.h:112-122][atomic-dec-void].
- Add-with-result, add and unsigned-count subtraction list FreeRTOS only. [osal_atomic.h:58-71][atomic-add-return], [osal_atomic.h:97-110][atomic-sub], [osal_atomic.h:124-137][atomic-add].
- Increment/decrement-and-test and increment-if-nonzero list Linux only. The test variants document testing the updated value for zero; increment-if-nonzero returns true if increment performed. None documents a reference-count lifetime or publication protocol. [osal_atomic.h:139-177][atomic-tests].

No inspected declaration exposes a memory-order argument, and the complete headers provide no explicit acquire/release, full-barrier or sequential-consistency statement. That does **not** imply relaxed behavior: implementations may enforce stronger ordering. Interrupt/scheduling suppression comments likewise do not by themselves prove cross-core exclusion or memory visibility. Resolving these questions needs the selected backend bodies and architecture/compiler contracts, not inference from Linux-like names.

## Prior coverage and reusable boundary

Knowledge searches for the lock/atomic symbols and OSAL ordering terms found no dedicated Ai-BS21 locking-contract report. The three adjacent reports were read as deduplication anchors; their relative links use the existing harvest-directory convention.

- [OSAL schedule contracts](NEW-BS21-OSAL-SCHEDULE-CONTRACT.md) already cover wait/task units, ownership and completion comments claiming full barriers. New here: mutex ownership, spinlock interrupt-state differences, sparse reader/writer semantics and the absence of an equivalent explicit ordering claim in these four headers. A completion-specific comment cannot supply an undocumented lock or atomic guarantee.
- [OSAL IPC contracts](NEW-BS21-OSAL-IPC-CONTRACT.md) cover queue/event units, results and deletion. Mutex timeout units remain unspecified rather than inheriting queue ticks or event milliseconds. No queue/event semantics are re-harvested.
- [OSAL deferred lifetime](NEW-BS21-OSAL-DEFERRED-LIFETIME.md) already distinguishes submission, callback completion and resource release, including timer destruction's lock restrictions. This report adds lock-owner and saved-interrupt-state contracts; owning a lock is not a callback-drain guarantee, and lock destruction does not gain wait-for-callback semantics by analogy.

For a future adapter, retain backend-specific capabilities, distinguish Boolean try results from SUCCESS/FAILURE results, preserve opaque handles and target C integer/pointer types, and make lock ownership and saved IRQ state explicit. Confirm timeout units, failed-try state, recursion policy and memory ordering before promising a uniform abstraction. Establish application quiescence before releasing lock-associated storage rather than assuming destroy performs it. These are recommendations, not imported code or verified fixes.

No backend implementation, runtime defect, WS73 equivalence, fairness guarantee, priority-inheritance policy, cross-core exclusion mechanism or memory-order strength is established by this source-only report.

[mutex-type]: https://github.com/Ai-Thinker-Open/Ai-BS21_SDK/blob/f4f3f28781f4610a1d0a97fe24907d59ed03d09d/kernel/osal/include/lock/osal_mutex.h#L18-L22
[mutex-init]: https://github.com/Ai-Thinker-Open/Ai-BS21_SDK/blob/f4f3f28781f4610a1d0a97fe24907d59ed03d09d/kernel/osal/include/lock/osal_mutex.h#L24-L38
[mutex-lock]: https://github.com/Ai-Thinker-Open/Ai-BS21_SDK/blob/f4f3f28781f4610a1d0a97fe24907d59ed03d09d/kernel/osal/include/lock/osal_mutex.h#L40-L63
[mutex-timeout]: https://github.com/Ai-Thinker-Open/Ai-BS21_SDK/blob/f4f3f28781f4610a1d0a97fe24907d59ed03d09d/kernel/osal/include/lock/osal_mutex.h#L65-L81
[mutex-interruptible]: https://github.com/Ai-Thinker-Open/Ai-BS21_SDK/blob/f4f3f28781f4610a1d0a97fe24907d59ed03d09d/kernel/osal/include/lock/osal_mutex.h#L83-L99
[mutex-try]: https://github.com/Ai-Thinker-Open/Ai-BS21_SDK/blob/f4f3f28781f4610a1d0a97fe24907d59ed03d09d/kernel/osal/include/lock/osal_mutex.h#L101-L116
[mutex-unlock]: https://github.com/Ai-Thinker-Open/Ai-BS21_SDK/blob/f4f3f28781f4610a1d0a97fe24907d59ed03d09d/kernel/osal/include/lock/osal_mutex.h#L118-L147
[mutex-destroy]: https://github.com/Ai-Thinker-Open/Ai-BS21_SDK/blob/f4f3f28781f4610a1d0a97fe24907d59ed03d09d/kernel/osal/include/lock/osal_mutex.h#L149-L165
[mutex-all]: https://github.com/Ai-Thinker-Open/Ai-BS21_SDK/blob/f4f3f28781f4610a1d0a97fe24907d59ed03d09d/kernel/osal/include/lock/osal_mutex.h#L18-L165
[spin-init]: https://github.com/Ai-Thinker-Open/Ai-BS21_SDK/blob/f4f3f28781f4610a1d0a97fe24907d59ed03d09d/kernel/osal/include/lock/osal_spinlock.h#L18-L39
[spin-lock]: https://github.com/Ai-Thinker-Open/Ai-BS21_SDK/blob/f4f3f28781f4610a1d0a97fe24907d59ed03d09d/kernel/osal/include/lock/osal_spinlock.h#L41-L74
[spin-ordinary]: https://github.com/Ai-Thinker-Open/Ai-BS21_SDK/blob/f4f3f28781f4610a1d0a97fe24907d59ed03d09d/kernel/osal/include/lock/osal_spinlock.h#L41-L59
[spin-bh-lock]: https://github.com/Ai-Thinker-Open/Ai-BS21_SDK/blob/f4f3f28781f4610a1d0a97fe24907d59ed03d09d/kernel/osal/include/lock/osal_spinlock.h#L61-L74
[spin-try]: https://github.com/Ai-Thinker-Open/Ai-BS21_SDK/blob/f4f3f28781f4610a1d0a97fe24907d59ed03d09d/kernel/osal/include/lock/osal_spinlock.h#L76-L120
[spin-unlock]: https://github.com/Ai-Thinker-Open/Ai-BS21_SDK/blob/f4f3f28781f4610a1d0a97fe24907d59ed03d09d/kernel/osal/include/lock/osal_spinlock.h#L122-L134
[spin-bh-unlock]: https://github.com/Ai-Thinker-Open/Ai-BS21_SDK/blob/f4f3f28781f4610a1d0a97fe24907d59ed03d09d/kernel/osal/include/lock/osal_spinlock.h#L136-L147
[spin-save]: https://github.com/Ai-Thinker-Open/Ai-BS21_SDK/blob/f4f3f28781f4610a1d0a97fe24907d59ed03d09d/kernel/osal/include/lock/osal_spinlock.h#L149-L175
[spin-destroy]: https://github.com/Ai-Thinker-Open/Ai-BS21_SDK/blob/f4f3f28781f4610a1d0a97fe24907d59ed03d09d/kernel/osal/include/lock/osal_spinlock.h#L177-L193
[spin-all]: https://github.com/Ai-Thinker-Open/Ai-BS21_SDK/blob/f4f3f28781f4610a1d0a97fe24907d59ed03d09d/kernel/osal/include/lock/osal_spinlock.h#L18-L193
[rw-all]: https://github.com/Ai-Thinker-Open/Ai-BS21_SDK/blob/f4f3f28781f4610a1d0a97fe24907d59ed03d09d/kernel/osal/include/lock/osal_rwlock.h#L18-L100
[atomic-all]: https://github.com/Ai-Thinker-Open/Ai-BS21_SDK/blob/f4f3f28781f4610a1d0a97fe24907d59ed03d09d/kernel/osal/include/atomic/osal_atomic.h#L18-L177
[atomic-basic]: https://github.com/Ai-Thinker-Open/Ai-BS21_SDK/blob/f4f3f28781f4610a1d0a97fe24907d59ed03d09d/kernel/osal/include/atomic/osal_atomic.h#L18-L56
[atomic-dec]: https://github.com/Ai-Thinker-Open/Ai-BS21_SDK/blob/f4f3f28781f4610a1d0a97fe24907d59ed03d09d/kernel/osal/include/atomic/osal_atomic.h#L73-L95
[atomic-dec-void]: https://github.com/Ai-Thinker-Open/Ai-BS21_SDK/blob/f4f3f28781f4610a1d0a97fe24907d59ed03d09d/kernel/osal/include/atomic/osal_atomic.h#L112-L122
[atomic-add-return]: https://github.com/Ai-Thinker-Open/Ai-BS21_SDK/blob/f4f3f28781f4610a1d0a97fe24907d59ed03d09d/kernel/osal/include/atomic/osal_atomic.h#L58-L71
[atomic-sub]: https://github.com/Ai-Thinker-Open/Ai-BS21_SDK/blob/f4f3f28781f4610a1d0a97fe24907d59ed03d09d/kernel/osal/include/atomic/osal_atomic.h#L97-L110
[atomic-add]: https://github.com/Ai-Thinker-Open/Ai-BS21_SDK/blob/f4f3f28781f4610a1d0a97fe24907d59ed03d09d/kernel/osal/include/atomic/osal_atomic.h#L124-L137
[atomic-tests]: https://github.com/Ai-Thinker-Open/Ai-BS21_SDK/blob/f4f3f28781f4610a1d0a97fe24907d59ed03d09d/kernel/osal/include/atomic/osal_atomic.h#L139-L177
