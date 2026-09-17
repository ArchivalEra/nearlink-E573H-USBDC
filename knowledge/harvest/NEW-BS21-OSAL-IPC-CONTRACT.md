---
type: harvest
title: "Ai-BS21 SDK OSAL message queues and events: declaration-level IPC contracts"
language: en
created: 2026-09-17
tags: [harvest, bs21, osal, msgqueue, event, ipc, timeout, lifecycle, sdk]
sources:
  - "https://github.com/Ai-Thinker-Open/Ai-BS21_SDK/blob/f4f3f28781f4610a1d0a97fe24907d59ed03d09d/kernel/osal/include/msgqueue/osal_msgqueue.h"
  - "https://github.com/Ai-Thinker-Open/Ai-BS21_SDK/blob/f4f3f28781f4610a1d0a97fe24907d59ed03d09d/kernel/osal/include/event/osal_event.h"
  - "https://github.com/Ai-Thinker-Open/Ai-BS21_SDK/blob/f4f3f28781f4610a1d0a97fe24907d59ed03d09d/kernel/osal/test/src/osal_test_msgqueue.h"
  - "https://github.com/Ai-Thinker-Open/Ai-BS21_SDK/blob/f4f3f28781f4610a1d0a97fe24907d59ed03d09d/kernel/osal/test/src/osal_test_event.h"
trust: B
stale_after: 2027-03-17
---

# Ai-BS21 SDK OSAL message queues and events: declaration-level IPC contracts

## Provenance and scope

One software theme: the OSAL payload-queue and event-mask IPC interfaces. Read the two public headers completely, plus both corresponding test headers, from the `Ai-Thinker-Open/Ai-BS21_SDK` repository. Local HEAD is `f4f3f28781f4610a1d0a97fe24907d59ed03d09d`; the configured origin is `https://github.com/Ai-Thinker-Open/Ai-BS21_SDK.git`. A read-only diff against HEAD for all four files was empty, with lazy fetching disabled. Pinned URLs were constructed from that local provenance; no network verification was performed.

**Evidence levels:** signatures, typedefs, and macro definitions are directly observed source; behavioral statements below are header comments, not verified backend behavior. All inspected API functions are declarations without bodies. The test headers contain entry-point declarations, not assertions or test results. No implementation, binary, firmware, build, or test was executed. No hardware or PCB material was examined. The source-research pass made no upstream changes.

Source shorthand in the tables means the exact pinned files in `sources`: **Q** = `kernel/osal/include/msgqueue/osal_msgqueue.h`; **E** = `kernel/osal/include/event/osal_event.h`. Line ranges below have pinned links.

## Executive findings

1. **Queue and event timeouts have different documented units.** Queue write/read/head-write use relative **ticks**, whereas event read uses **milliseconds** and names its argument `timeout_ms`. A shared raw integer timeout would conceal a portability mismatch. Queue sentinel macros optionally inherit `LOS_*`; the event forever macro is fixed at `0xFFFFFFFF`. These are definitions/documentation, not proof of sentinel handling in the backend. [Q:18-28](https://github.com/Ai-Thinker-Open/Ai-BS21_SDK/blob/f4f3f28781f4610a1d0a97fe24907d59ed03d09d/kernel/osal/include/msgqueue/osal_msgqueue.h#L18-L28), [Q:66-93](https://github.com/Ai-Thinker-Open/Ai-BS21_SDK/blob/f4f3f28781f4610a1d0a97fe24907d59ed03d09d/kernel/osal/include/msgqueue/osal_msgqueue.h#L66-L93), [Q:103-131](https://github.com/Ai-Thinker-Open/Ai-BS21_SDK/blob/f4f3f28781f4610a1d0a97fe24907d59ed03d09d/kernel/osal/include/msgqueue/osal_msgqueue.h#L103-L131), [Q:141-168](https://github.com/Ai-Thinker-Open/Ai-BS21_SDK/blob/f4f3f28781f4610a1d0a97fe24907d59ed03d09d/kernel/osal/include/msgqueue/osal_msgqueue.h#L141-L168), [E:18-26](https://github.com/Ai-Thinker-Open/Ai-BS21_SDK/blob/f4f3f28781f4610a1d0a97fe24907d59ed03d09d/kernel/osal/include/event/osal_event.h#L18-L26), [E:69-86](https://github.com/Ai-Thinker-Open/Ai-BS21_SDK/blob/f4f3f28781f4610a1d0a97fe24907d59ed03d09d/kernel/osal/include/event/osal_event.h#L69-L86).

2. **FreeRTOS queue sizing is explicitly not per-operation sizing.** The create contract sets the node size; write, read, and head-write comments say `buffer_size` is unused on FreeRTOS and the creation size controls transfer length. Read nevertheless exposes an in/out size pointer whose general comment promises wanted-size before read and real-size afterward. Treat this as a documented backend difference, not a portable output-size guarantee. [Q:44-56](https://github.com/Ai-Thinker-Open/Ai-BS21_SDK/blob/f4f3f28781f4610a1d0a97fe24907d59ed03d09d/kernel/osal/include/msgqueue/osal_msgqueue.h#L44-L56), [Q:73-93](https://github.com/Ai-Thinker-Open/Ai-BS21_SDK/blob/f4f3f28781f4610a1d0a97fe24907d59ed03d09d/kernel/osal/include/msgqueue/osal_msgqueue.h#L73-L93), [Q:111-131](https://github.com/Ai-Thinker-Open/Ai-BS21_SDK/blob/f4f3f28781f4610a1d0a97fe24907d59ed03d09d/kernel/osal/include/msgqueue/osal_msgqueue.h#L111-L131), [Q:148-168](https://github.com/Ai-Thinker-Open/Ai-BS21_SDK/blob/f4f3f28781f4610a1d0a97fe24907d59ed03d09d/kernel/osal/include/msgqueue/osal_msgqueue.h#L148-L168).

3. **Queue deletion has a documented refusal condition but no status return.** The comment says deletion fails with blocked tasks or concurrent queue reads/writes; the declaration returns `void`. A caller cannot observe that failure through a return value. Event destruction instead returns `int` and says the object must originate from `osal_event_init`; it may free memory. Neither inspected header establishes a shutdown/join protocol. [Q:170-189](https://github.com/Ai-Thinker-Open/Ai-BS21_SDK/blob/f4f3f28781f4610a1d0a97fe24907d59ed03d09d/kernel/osal/include/msgqueue/osal_msgqueue.h#L170-L189), [E:106-122](https://github.com/Ai-Thinker-Open/Ai-BS21_SDK/blob/f4f3f28781f4610a1d0a97fe24907d59ed03d09d/kernel/osal/include/event/osal_event.h#L106-L122).

4. **Events expose mode bits, but not a complete result/mode algebra.** `AND=4U`, `OR=2U`, `CLR=1U`; comments describe all/any matching and immediate clearing after read. Event read returns `int`, documented only as `OSAL_SUCCESS/OSAL_FAILURE`, without a separate matched-mask output. Do not assume it returns event bits, accepts every bitwise mode combination, or has backend-independent clear semantics. LiteOS bit 25 is explicitly forbidden in the event-read mask. [E:21-30](https://github.com/Ai-Thinker-Open/Ai-BS21_SDK/blob/f4f3f28781f4610a1d0a97fe24907d59ed03d09d/kernel/osal/include/event/osal_event.h#L21-L30), [E:61-104](https://github.com/Ai-Thinker-Open/Ai-BS21_SDK/blob/f4f3f28781f4610a1d0a97fe24907d59ed03d09d/kernel/osal/include/event/osal_event.h#L61-L104).

5. **The relevant test files do not validate these contracts.** `osal_test_msgqueue.h` includes `osal_test.h` and declares `void osal_test_msgqueue(void)`; `osal_test_event.h` declares `void osal_test_event(void)`. The latter's description incorrectly names `osal_test_task.c`. The inspected local `kernel/osal/test/` inventory contains headers, not matching test bodies. [osal_test_msgqueue.h:1-12](https://github.com/Ai-Thinker-Open/Ai-BS21_SDK/blob/f4f3f28781f4610a1d0a97fe24907d59ed03d09d/kernel/osal/test/src/osal_test_msgqueue.h#L1-L12), [osal_test_event.h:1-10](https://github.com/Ai-Thinker-Open/Ai-BS21_SDK/blob/f4f3f28781f4610a1d0a97fe24907d59ed03d09d/kernel/osal/test/src/osal_test_event.h#L1-L10).

## Complete public interface inventory

All twelve functions have comments listing LiteOS and FreeRTOS support; these headers do not claim Linux support. This is a documentation matrix, not a link/runtime availability test.

### Queue declarations and documented contracts

| Interface | Declaration / comments | Contract and boundary |
|---|---|---|
| `osal_msg_queue_create` | [Q:30-56](https://github.com/Ai-Thinker-Open/Ai-BS21_SDK/blob/f4f3f28781f4610a1d0a97fe24907d59ed03d09d/kernel/osal/include/msgqueue/osal_msgqueue.h#L30-L56) | Returns `int`; parameters are `const char *name`, `unsigned short queue_len`, `unsigned long *queue_id`, `unsigned int flags`, `unsigned short max_msgsize`. Comments reserve name/flags, constrain length and node size to 1..0xffff, cite `LOSCFG_BASE_IPC_QUEUE_LIMIT`, and say the function is defined only with `LOSCFG_QUEUE_DYNAMIC_ALLOCATION`. The declaration itself is not under that configuration guard. |
| `osal_msg_queue_write_copy` | [Q:58-93](https://github.com/Ai-Thinker-Open/Ai-BS21_SDK/blob/f4f3f28781f4610a1d0a97fe24907d59ed03d09d/kernel/osal/include/msgqueue/osal_msgqueue.h#L58-L93) | `int(unsigned long queue_id, void *buffer_addr, unsigned int buffer_size, unsigned int timeout)`. Writes buffer data; non-null address; queue created first. Comments promise SUCCESS/FAILURE but no distinct full/timeout code. |
| `osal_msg_queue_read_copy` | [Q:95-131](https://github.com/Ai-Thinker-Open/Ai-BS21_SDK/blob/f4f3f28781f4610a1d0a97fe24907d59ed03d09d/kernel/osal/include/msgqueue/osal_msgqueue.h#L95-L131) | `int(unsigned long queue_id, void *buffer_addr, unsigned int *buffer_size, unsigned int timeout)`. Comments describe FIFO and in/out wanted/actual size; FreeRTOS qualification overrides a universal per-read-size interpretation. No oversize, truncation, or undersize behavior specified. |
| `osal_msg_queue_write_head_copy` | [Q:133-168](https://github.com/Ai-Thinker-Open/Ai-BS21_SDK/blob/f4f3f28781f4610a1d0a97fe24907d59ed03d09d/kernel/osal/include/msgqueue/osal_msgqueue.h#L133-L168) | Same parameter types and `int` return as write-copy. Comment says write into a "queue header" and permits size 1..0xffffffff; exact precedence versus ordinary writes is not specified. Do not extend the read comment's FIFO statement to a proof of ordering with head writes. |
| `osal_msg_queue_delete` | [Q:170-189](https://github.com/Ai-Thinker-Open/Ai-BS21_SDK/blob/f4f3f28781f4610a1d0a97fe24907d59ed03d09d/kernel/osal/include/msgqueue/osal_msgqueue.h#L170-L189) | `void(unsigned long queue_id)`. Must be created first; documented failure when tasks are blocked or access is underway, but no status channel in this declaration. |
| `osal_msg_queue_is_full` | [Q:191-211](https://github.com/Ai-Thinker-Open/Ai-BS21_SDK/blob/f4f3f28781f4610a1d0a97fe24907d59ed03d09d/kernel/osal/include/msgqueue/osal_msgqueue.h#L191-L211) | `int(unsigned long queue_id)`, comments say true/false, created queue required. No reservation or atomic check-and-write promise. |
| `osal_msg_queue_get_msg_num` | [Q:213-232](https://github.com/Ai-Thinker-Open/Ai-BS21_SDK/blob/f4f3f28781f4610a1d0a97fe24907d59ed03d09d/kernel/osal/include/msgqueue/osal_msgqueue.h#L213-L232) | `unsigned int(unsigned long queue_id)`, comments say count or `OSAL_INVALID_MSG_NUM`. This header does not define that sentinel's numeric value. |

Queue IDs are declared as `unsigned long`; the comments repeatedly describe them as an address on FreeRTOS and an integer on LiteOS (Q:40-42,75-77,113-115,150-152,180-182,200-202,221-223). This does not specify a stable serialized handle or a fixed-width cross-platform ABI. Do not narrow the handle to an assumed 32-bit integer in a binding.

`OSAL_MSGQ_WAIT_FOREVER` aliases `LOS_WAIT_FOREVER` if already defined, otherwise `0xFFFFFFFF`; `OSAL_MSGQ_NO_WAIT` aliases `LOS_NO_WAIT` if defined, otherwise `0`. Thus the inspected preprocessor contract permits configuration/include-context dependence; it does not establish the actual backend values. [Q:18-28](https://github.com/Ai-Thinker-Open/Ai-BS21_SDK/blob/f4f3f28781f4610a1d0a97fe24907d59ed03d09d/kernel/osal/include/msgqueue/osal_msgqueue.h#L18-L28).

### Event declarations and documented contracts

`osal_event` is a public one-field wrapper, `struct { void *event; }`, not a visible backend control-block layout. `OSAL_EVENT_FOREVER` is `0xFFFFFFFF`; no separate event no-wait constant is defined here. [E:18-30](https://github.com/Ai-Thinker-Open/Ai-BS21_SDK/blob/f4f3f28781f4610a1d0a97fe24907d59ed03d09d/kernel/osal/include/event/osal_event.h#L18-L30).

| Interface | Declaration / comments | Contract and boundary |
|---|---|---|
| `osal_event_init` | [E:32-44](https://github.com/Ai-Thinker-Open/Ai-BS21_SDK/blob/f4f3f28781f4610a1d0a97fe24907d59ed03d09d/kernel/osal/include/event/osal_event.h#L32-L44) | `int(osal_event *event_obj)`, initialize event control block. Initial bit state and required pre-initialization contents are unspecified here. |
| `osal_event_write` | [E:46-59](https://github.com/Ai-Thinker-Open/Ai-BS21_SDK/blob/f4f3f28781f4610a1d0a97fe24907d59ed03d09d/kernel/osal/include/event/osal_event.h#L46-L59) | `int(osal_event *event_obj, unsigned int mask)`. Writes event mask; comment does not explicitly define OR-versus-replace, repeated-write counting, or wake-one/wake-all behavior. |
| `osal_event_read` | [E:61-86](https://github.com/Ai-Thinker-Open/Ai-BS21_SDK/blob/f4f3f28781f4610a1d0a97fe24907d59ed03d09d/kernel/osal/include/event/osal_event.h#L61-L86) | `int(osal_event *event_obj, unsigned int mask, unsigned int timeout_ms, unsigned int mode)`. Valid object memory required; task blocking/scheduling described; LiteOS mask bit 25 forbidden. Return documented as SUCCESS/FAILURE, not matched bits. |
| `osal_event_clear` | [E:88-104](https://github.com/Ai-Thinker-Open/Ai-BS21_SDK/blob/f4f3f28781f4610a1d0a97fe24907d59ed03d09d/kernel/osal/include/event/osal_event.h#L88-L104) | `int(osal_event *event_obj, unsigned int mask)`. Text describes setting the specified event to zero and labels mask as event to clear; the backend mask equation, especially complement conventions, remains unverified. |
| `osal_event_destroy` | [E:106-122](https://github.com/Ai-Thinker-Open/Ai-BS21_SDK/blob/f4f3f28781f4610a1d0a97fe24907d59ed03d09d/kernel/osal/include/event/osal_event.h#L106-L122) | `int(osal_event *event_obj)`. Comments say may free memory and object should originate from init. They do not specify which allocation is freed, whether the wrapper pointer is reset, or handling of active waiters/double destruction. |

Every event function's return comment is `OSAL_SUCCESS/OSAL_FAILURE`. Numeric definitions for those constants are outside these inspected headers, so this draft does not assign them or infer native RTOS errors.

## Context restrictions and documentation defects

- Queue write, read, and head-write comments prohibit use in software timer callbacks and before LiteOS initialization (Q:68-74,107-112,142-149). The wording "Do not read or write a queue in unblocking modes such as interrupt" is awkward but is not an authorization to use timeout zero from an interrupt. Conservatively keep these operations in task context; ISR-safe alternatives are not declared in these headers.
- Event read explicitly prohibits interrupt use, but only discourages software timer callback use (E:69-72). The other event operations have no equivalent context promise. Absence of a warning is not proof of ISR safety.
- Queue head-write marks the source buffer `[out]` (Q:155), despite write semantics; queue read describes its output as data "to be written" (Q:118). The "full storage and rounding" FreeRTOS explanation (Q:89-90,127-128,164-165) does not define a rounding algorithm or granularity.
- Event comments reference `eventCB` and `eventId` rather than actual parameters (E:37,51-52,76-77,93-94). These are reasons to preserve the distinction between declaration evidence and comment interpretation, not evidence of any specific implementation defect.

The linked inventory ranges above contain each cited restriction and inconsistency.

## Reusable contract for a future adapter

These are design recommendations derived from the headers, not existing SDK behavior or implemented changes.

1. **Separate timeout types/conversion boundaries.** Keep queue ticks distinct from event milliseconds. Treat infinite wait as an explicit option rather than an ordinary duration. Actual tick frequency, conversion rounding, overflow limits, and sentinel acceptance require backend evidence.
2. **Use fixed-size queue envelopes for a portable subset.** Store the creation-time item size in the wrapper; supply a full-size initialized source and sufficiently large destination for FreeRTOS. Do not rely on the returned read size being updated there. Do not infer pointer-pointee ownership transfer merely because the API names contain `copy`.
3. **Keep handles opaque and ABI-correct.** Use the target C `unsigned long` for queue IDs and preserve the one-pointer event wrapper. Never serialize backend IDs as portable identifiers. Successful creation/init should be the prerequisite for wrapper use.
4. **Provide application-level quiescence before destruction.** Stop new producers/consumers and resolve waits before deleting. Queue deletion cannot acknowledge success through its return type; the header supplies no portable cancel-and-join operation. Do not assume destroying an event wakes blocked readers safely.
5. **Treat fullness/count as observations, not admission guarantees.** Avoid check-then-write correctness dependencies. A queue operation's own result remains necessary even after a favorable query.
6. **Treat event mode combinations and result decoding as backend-dependent until verified.** The constants suggest composable flags but do not enumerate valid combinations, zero-mask handling, forbidden masks outside LiteOS read, or how errors and matched events are represented. Reserve LiteOS bit 25 in any portable event-mask allocator.

## Unverified behaviors and next evidence needed

No source bodies or executable tests were inspected; none of the following is established by this harvest:

- Queue variable-length storage on LiteOS, oversize/undersize behavior, truncation versus failure, priority/head ordering under contention, waiter fairness, blocking deadlines, cancellation, and copy completion/ownership details.
- The value of `OSAL_INVALID_MSG_NUM`, numeric success/failure codes, actual configuration support, linked backend implementation, and whether invalid handles or concurrent deletion are diagnosed.
- Event initial state, write accumulation, duplicate-event coalescing, supported mode combinations, zero-timeout semantics, AND/OR return mask behavior, CLR scope/atomicity, explicit clear polarity, simultaneous reader behavior, and destruction with waiters.
- Synchronization visibility, ordering guarantees, and ISR support beyond the explicit comments. The two headers provide no backend implementation evidence for these properties.
- Passing tests: the two test declarations name entry points only. Obtain the corresponding bodies/backend sources and a separately authorized test plan before elevating any behavior to verified status. This draft neither executes nor proposes a build as evidence already obtained.

## Existing coverage comparison and deduplication

Coverage was checked before drafting across the knowledge bundle for exact `osal_msg_queue` and `osal_event` symbols; no files matched. The harvest inventory was also checked for related topics, and the following reports were read as comparison anchors. This supports a narrow missing-contract finding, not a claim that all generic queue/event discussion was absent.

- [NEW-BS21-OSAL-SCHEDULE-CONTRACT](NEW-BS21-OSAL-SCHEDULE-CONTRACT.md) covers completion/wait/task units and ownership. New here: payload queues versus event masks, FreeRTOS fixed-item sizing, queue deletion's unobservable return status and event mode/result gaps. The groups are complementary, not evidence of identical backend semantics.
- [BS21-WS63-SDK-COMPARISON](../intel/BS21-WS63-SDK-COMPARISON.md) covers radio and SDK-family relationships. This report instead records local RTOS IPC; it establishes no SSAP packet, transport interoperability or WS73 OSAL equivalence.

One deduplicated theme combines the two IPC primitives because their timeout, buffer/mask and lifecycle differences constrain the same future adapter boundary. No upstream code was imported or executed.
