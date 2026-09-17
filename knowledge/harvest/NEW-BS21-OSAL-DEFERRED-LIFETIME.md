---
type: harvest
title: "Ai-BS21 OSAL deferred work and timers: execution, cancellation and lifetime contracts"
language: en
created: 2026-09-17
tags: [harvest, bs21, sdk, osal, deferred-work, timers, lifecycle]
sources:
  - "https://github.com/Ai-Thinker-Open/Ai-BS21_SDK/blob/f4f3f28781f4610a1d0a97fe24907d59ed03d09d/kernel/osal/include/schedule/osal_workqueue.h"
  - "https://github.com/Ai-Thinker-Open/Ai-BS21_SDK/blob/f4f3f28781f4610a1d0a97fe24907d59ed03d09d/kernel/osal/include/schedule/osal_delaywork.h"
  - "https://github.com/Ai-Thinker-Open/Ai-BS21_SDK/blob/f4f3f28781f4610a1d0a97fe24907d59ed03d09d/kernel/osal/include/time/osal_timer.h"
trust: B
stale_after: 2027-03-17
---

# Ai-BS21 OSAL deferred work and timers: execution, cancellation and lifetime contracts

## Scope and evidence quality

One software theme: the public contract for deferring execution and ending callback lifetimes. All three headers were read in full from the `Ai-Thinker-Open/Ai-BS21_SDK` repository, at local HEAD `f4f3f28781f4610a1d0a97fe24907d59ed03d09d`; the scoped git status and diff reported no header changes. Upstream URLs below are commit-pinned provenance links, not network-verified fetches.

These files contain type definitions, function declarations and comments, **not function implementations**. Statements described as documented below are vendor comment contracts, not observed runtime behavior. Inspection of `kernel/osal/src/` found only `liteos/osal_inner.h`; the OSAL directory also contains `libosal.a`, which was not analyzed. This does not establish how any backend implements the declarations, nor prove implementation absence elsewhere in the SDK. Trust B applies to the source-contract reading, not cross-OS behavioral verification.

## Executive findings

1. **Ordinary work scheduling documents coalescing, not an event-counting queue.** `osal_workqueue` exposes `queue_flag`, an opaque `work` pointer and a handler receiving the wrapper pointer. Scheduling is documented to insert a job into the kernel-global workqueue only if not already queued, otherwise preserving its position. It returns `int`, documented only as True/False; the header does not explain the numeric mapping or whether false means already queued rather than failure. The meaning of `queue_flag` is unspecified. Do not treat repeated schedule calls as guaranteed one-for-one callbacks. [osal_workqueue.h:18-53](https://github.com/Ai-Thinker-Open/Ai-BS21_SDK/blob/f4f3f28781f4610a1d0a97fe24907d59ed03d09d/kernel/osal/include/schedule/osal_workqueue.h#L18-L53).

2. **Flush is a conditional completion barrier, not permanent shutdown.** `osal_workqueue_flush` waits for the last queueing instance; its idle-on-return guarantee explicitly depends on the work not being requeued since flush started. `destroy` may free memory and requires work originating from `init`, but its comments do not promise cancellation or a wait for a running callback. Init/schedule/destroy list Linux, LiteOS and FreeRTOS; flush lists only Linux and LiteOS. [osal_workqueue.h:25-85](https://github.com/Ai-Thinker-Open/Ai-BS21_SDK/blob/f4f3f28781f4610a1d0a97fe24907d59ed03d09d/kernel/osal/include/schedule/osal_workqueue.h#L25-L85).

3. **Delayed work is explicitly Linux-only in this header.** Its wrapper has an opaque pointer and wrapper-pointer callback. All four operations list Linux support. Scheduling takes a signed `int timeout` measured in **jiffies**, with zero documented for immediate execution; this does not promise inline execution before return. `cancel_sync` explicitly cancels and waits for completion, while `destroy` only documents destruction and possible freeing. Initialization requires eventual destruction. Neither duplicate-schedule behavior nor requeue races during cancellation are specified here. [osal_delaywork.h:18-90](https://github.com/Ai-Thinker-Open/Ai-BS21_SDK/blob/f4f3f28781f4610a1d0a97fe24907d59ed03d09d/kernel/osal/include/schedule/osal_delaywork.h#L18-L90).

4. **The ordinary timer has millisecond configuration and a callback-data indirection contract.** Its public fields are an opaque timer pointer, `handler(unsigned long)`, `data` and `unsigned int interval` in milliseconds. Init requires handler/data assignment beforehand and says they cannot subsequently be changed. `mod` takes milliseconds and is documented to activate an inactive timer. Callback parameters are explicitly not directly usable: `osal_timer_get_private_data(const void *sys_data)` must recover the usable value. The `unsigned long` callback versus `const void *` accessor signatures do not establish the correct backend-specific conversion or ownership. [osal_timer.h:18-23](https://github.com/Ai-Thinker-Open/Ai-BS21_SDK/blob/f4f3f28781f4610a1d0a97fe24907d59ed03d09d/kernel/osal/include/time/osal_timer.h#L18-L23), [osal_timer.h:42-100](https://github.com/Ai-Thinker-Open/Ai-BS21_SDK/blob/f4f3f28781f4610a1d0a97fe24907d59ed03d09d/kernel/osal/include/time/osal_timer.h#L42-L100), [osal_timer.h:156-171](https://github.com/Ai-Thinker-Open/Ai-BS21_SDK/blob/f4f3f28781f4610a1d0a97fe24907d59ed03d09d/kernel/osal/include/time/osal_timer.h#L156-L171).

5. **Timer callback-context prose contains an abstraction mismatch.** The start comment says the kernel calls back from the timer interrupt and refers to `->expires` and `->function`, neither present in the public `osal_timer` struct. Treat this as documentation requiring backend confirmation, not proof of identical interrupt context across Linux/LiteOS/FreeRTOS. Workqueue comments likewise do not specify worker priority, CPU affinity, permitted blocking, serialization or ISR-safe submission. [osal_timer.h:18-23,63-82](https://github.com/Ai-Thinker-Open/Ai-BS21_SDK/blob/f4f3f28781f4610a1d0a97fe24907d59ed03d09d/kernel/osal/include/time/osal_timer.h#L18-L82), [osal_workqueue.h:39-53](https://github.com/Ai-Thinker-Open/Ai-BS21_SDK/blob/f4f3f28781f4610a1d0a97fe24907d59ed03d09d/kernel/osal/include/schedule/osal_workqueue.h#L39-L53).

6. **Timer stop, destroy and synchronous destruction are not interchangeable contracts.** Stop deactivates active or inactive timers; return `1` is documented as successful stopping of a pending timer on Linux/LiteOS, while `OSAL_SUCCESS` denotes an already stopped timer. Ordinary destroy is required to free timer resources, but neither stop nor ordinary destroy promises callback completion. Linux-only `destroy_sync` does promise no queued timer or running handler on any CPU, subject to explicit restart/context/lock rules. Its prose describes deactivation and waiting rather than explicitly repeating ordinary destroy's freeing contract, so the name alone cannot prove resource-release behavior or justify calling both destroy variants. [osal_timer.h:119-193](https://github.com/Ai-Thinker-Open/Ai-BS21_SDK/blob/f4f3f28781f4610a1d0a97fe24907d59ed03d09d/kernel/osal/include/time/osal_timer.h#L119-L193).

## Comparison: different kinds of deferred execution

Support entries are header claims, not a verified availability matrix for this particular BS21 library.

| Surface | Scheduling/time contract | Completion/lifetime boundary | Documented support |
| --- | --- | --- | --- |
| Ordinary work | Global queue; already queued item stays in place | Flush waits, but requeue defeats idle guarantee; destroy may free | Init/schedule/destroy: Linux, LiteOS, FreeRTOS; flush: Linux, LiteOS |
| Delayed work | Global queue after jiffies delay; zero for immediate execution | Cancel-sync cancels and waits; destroy/free remains a separate declaration | Linux only |
| Ordinary timer | `interval` and `mod` use milliseconds; mod activates inactive timer | Stop is deactivation; ordinary destroy frees; sync variant adds explicit callback quiescence rules | Basic operations: Linux, LiteOS, FreeRTOS; start-on and destroy-sync: Linux only |
| High-resolution timer | Interval field still uses milliseconds; callback returns restart/no-restart enum | Create requires eventual destroy; destroy does not explicitly promise to wait for callback completion | LiteOS only |

The first three rows are grounded in the executive citations. High-resolution evidence is [osal_timer.h:284-350](https://github.com/Ai-Thinker-Open/Ai-BS21_SDK/blob/f4f3f28781f4610a1d0a97fe24907d59ed03d09d/kernel/osal/include/time/osal_timer.h#L284-L350): `osal_hrtimer` has `handler(void *timer)` returning `osal_hrtimer_restart`; create freezes handler/interval according to its comment. Start is documented as inserting into a global linked list and returns `-1` for failure, `0` for success, and `1` for already present. These statuses differ from both ordinary work's True/False prose and timer-stop's positive-success meaning. The name "high-resolution" does not establish nanosecond configuration, accuracy or scheduling latency.

Additional whole-header boundaries:

- `osal_timer_start_on` takes `unsigned long delay` plus CPU, but its comment documents neither the delay parameter nor its units. Do not borrow delayed-work jiffies or ordinary timer milliseconds by analogy. [osal_timer.h:102-117](https://github.com/Ai-Thinker-Open/Ai-BS21_SDK/blob/f4f3f28781f4610a1d0a97fe24907d59ed03d09d/kernel/osal/include/time/osal_timer.h#L102-L117).
- `osal_get_jiffies` returns `unsigned long long`; `osal_msecs_to_jiffies` returns `unsigned long`; `osal_jiffies_to_msecs` accepts and returns `unsigned int`, with documented output saturation at `0xFFFFFFFF`. Conversion does not by itself prove a value fits delayed work's signed `int`. Tick frequency, rounding, valid negative delays, and wrap handling remain unspecified. [osal_timer.h:209-253](https://github.com/Ai-Thinker-Open/Ai-BS21_SDK/blob/f4f3f28781f4610a1d0a97fe24907d59ed03d09d/kernel/osal/include/time/osal_timer.h#L209-L253).
- The same header defines timeval/RTC structs and declares nanosecond `osal_sched_clock` (Linux/LiteOS), cycles-per-tick (LiteOS), and `osal_gettimeofday` (Linux/LiteOS/FreeRTOS). It does not establish their epoch, monotonicity or cross-clock equivalence; these are not alternate deferred-work primitives. [osal_timer.h:25-40](https://github.com/Ai-Thinker-Open/Ai-BS21_SDK/blob/f4f3f28781f4610a1d0a97fe24907d59ed03d09d/kernel/osal/include/time/osal_timer.h#L25-L40), [osal_timer.h:195-281](https://github.com/Ai-Thinker-Open/Ai-BS21_SDK/blob/f4f3f28781f4610a1d0a97fe24907d59ed03d09d/kernel/osal/include/time/osal_timer.h#L195-L281).

## Execution, cancellation and lifetime guidance

These are conservative consumer-design recommendations, not verified backend behavior or an executable teardown recipe.

- Distinguish **submission**, **completion/quiescence**, and **resource release**. None of these headers authorizes freeing a callback's owner merely because scheduling or ordinary timer stop returned.
- Gate external producers and callback self-requeue/restart before relying on a completion barrier. This is explicit for ordinary-work flush and timer destroy-sync; delayed-work cancel-sync does not spell out the corresponding producer-race guarantee.
- Timer destroy-sync explicitly requires callers to prevent restart, forbids interrupt-context calls unless the timer is IRQ-safe, forbids holding locks needed for handler completion, and forbids the handler from calling `add_timer_on`. The public timer struct exposes no IRQ-safe selection field. Its interrupt-context exception is therefore not established as usable through these three headers. [osal_timer.h:173-193](https://github.com/Ai-Thinker-Open/Ai-BS21_SDK/blob/f4f3f28781f4610a1d0a97fe24907d59ed03d09d/kernel/osal/include/time/osal_timer.h#L173-L193).
- Keep wrapper storage, callback-associated state and callback code alive until the chosen backend proves callback completion. The headers do not state copying/moving rules, ownership transfer for `data`, or that destruction frees caller-owned wrapper storage. Do not manipulate opaque pointers or assume bitwise copying a live wrapper is safe.
- If timer callbacks submit work, draining the timer alone does not drain that work: completion barriers are documented for their individual objects, not an entire callback chain. A future abstraction should model both lifetimes and prevent submissions during shutdown.
- Preserve backend capabilities and time units explicitly in any C/Rust wrapper. Do not silently emulate Linux-only delayed work with a timer callback: the callback-context and cancellation contracts are not shown to be equivalent. Do not classify every nonzero return as failure.

Open questions requiring backend evidence remain: callback context and blocking permissions; interrupt-safe submission; callback self-cancel behavior; concurrent schedule/cancel ordering; allocation ownership; ordinary-timer periodicity; sync-destroy resource release; and concrete BS21 symbol availability. No WS73 backend compatibility or source-code reuse is established.

## Prior coverage and deduplication

Read comparison anchors in the existing knowledge bundle:

- [NEW-BS21-WTSL](NEW-BS21-WTSL.md) already records the SDK's closed OSAL packaging. This draft does not repeat its radio/API or sample inventory; it adds the three-header execution and teardown contract.
- [NEW-BS21-NV-PERSISTENCE-CONTRACT](NEW-BS21-NV-PERSISTENCE-CONTRACT.md) distinguishes storage completion, flush and notification. The useful analogy is to separate lifecycle events rather than equate a return with completion. Its NV flush is a persistence contract; OSAL work flush is callback completion with a requeue caveat. Neither establishes the other's timing or context.

Case-insensitive searches across `knowledge/` for OSAL, workqueue, delaywork, deferred execution and timer symbols found adjacent scheduler/SDK notes, plus `knowledge/intel/SHIFU-BUILD-LIST.md:86` mentioning a WS73 `osal_timer.c` rename and `knowledge/intel/WS73-BT-GAP-AUDIT.md:184` mentioning timer-related link objects. No dedicated coverage of these Ai-BS21 workqueue/delayed-work/timer lifecycle contracts was found. This is one deduplicated theme, not three separate API harvests.

## Reproducible local evidence

SHA-256 values of the inspected files, relative to the upstream repository root:

| Header | SHA-256 |
| --- | --- |
| `kernel/osal/include/schedule/osal_workqueue.h` | `0826c18a48764e1a5c3d295d0312e409e740b6500db333987fc91ca0d4af771f` |
| `kernel/osal/include/schedule/osal_delaywork.h` | `ca10cd44973a4acdadaa3ba156521c6d6075acb0d241806f93c24a1ecfcc8fae` |
| `kernel/osal/include/time/osal_timer.h` | `77da27f6676c30eed88652f24cc7408c994b5eabab2cc351a6cc9855df0c5d55` |

No network access, hardware/PCB investigation, build, upstream-code execution, archive analysis or persistent-memory update was performed during the source research. No runtime behavior or backend availability was verified.
