---
type: harvest
title: "Ai-BS21 OSAL waits and tasks: timeout units, backend claims and teardown uncertainty"
language: en
created: 2026-09-17
tags: [harvest, bs21, osal, completion, waitqueue, task, lifecycle]
sources:
  - "https://github.com/Ai-Thinker-Open/Ai-BS21_SDK/blob/f4f3f28781f4610a1d0a97fe24907d59ed03d09d/kernel/osal/include/schedule/osal_completion.h"
  - "https://github.com/Ai-Thinker-Open/Ai-BS21_SDK/blob/f4f3f28781f4610a1d0a97fe24907d59ed03d09d/kernel/osal/include/schedule/osal_wait.h"
  - "https://github.com/Ai-Thinker-Open/Ai-BS21_SDK/blob/f4f3f28781f4610a1d0a97fe24907d59ed03d09d/kernel/osal/include/schedule/osal_task.h"
trust: B
stale_after: 2027-03-17
---

# Ai-BS21 OSAL waits and tasks: timeout units, backend claims and teardown uncertainty

## Executive findings

All three schedule headers were read at revision `f4f3f28781f4610a1d0a97fe24907d59ed03d09d`. This is declaration/comment evidence, not verified runtime behavior. The compiled OSAL implementation was not examined or executed.

1. **Wait APIs use different timeout units.** Completion timeout uses jiffies on Linux and ticks on LiteOS (`osal_completion.h:77-84`). Wait-queue timeout arguments are milliseconds (`osal_wait.h:96-104,115-123`). `osal_kthread_schedule` documents nanoseconds (`osal_task.h:229-234`), while `osal_msleep` documents milliseconds (`:403-410`). A shared integer timeout must not be passed between these interfaces without an explicit conversion. [Completion timeout](https://github.com/Ai-Thinker-Open/Ai-BS21_SDK/blob/f4f3f28781f4610a1d0a97fe24907d59ed03d09d/kernel/osal/include/schedule/osal_completion.h#L68-L84).
2. **Completion-timeout error prose conflicts with the unsigned return type.** Documentation lists zero for timeout, positive remaining time for completion, and -1 for failure, but the declaration returns `unsigned long` (`osal_completion.h:79-84`). If the implementation returns a converted -1 sentinel, a positive-value success check would misclassify it. Actual failure encoding requires backend evidence; neither signed comparison nor a blanket nonzero-success rule resolves the contradictory contract.
3. **Interruptibility is explicitly reduced for one LiteOS API, not proven absent across the whole group.** `osal_wait_interruptible` says it is equivalent to the uninterruptible wait on LiteOS (`osal_wait.h:51-66`), and interruptible wakeup has a similar comment (`:151`). The timeout-interruptible declaration lists Linux/LiteOS/FreeRTOS but does not repeat that degradation (`:87-104`). Do not generalize the non-timeout warning into a claim about every wait. Signal-only cancellation is not portable for the documented non-timeout operation; no hang was reproduced. [Wait contract](https://github.com/Ai-Thinker-Open/Ai-BS21_SDK/blob/f4f3f28781f4610a1d0a97fe24907d59ed03d09d/kernel/osal/include/schedule/osal_wait.h#L42-L104).
4. **The waiter tests a callback; the producer must wake it after relevant state changes.** `osal_wait_condition_func` is an `int` callback taking `const void *`; wait comments say its result is checked when woken, and producers must wake after changing variables affecting the condition (`osal_wait.h:25-26,48-52,73-78`). Timeout return encodings, first-check ordering and synchronization of the shared predicate are not fully documented. A recommended shutdown predicate is separate from an assertion that this wrapper implements any particular Linux macro.
5. **Task destruction has explicit ownership warnings but incomplete synchronization semantics.** The header says destroy stops a specified thread, may free the task, requires a task from `osal_kthread_create`, and tells callers to clear their pointer. It warns against the thread ending before destruction (`osal_task.h:197-218`). The `stop_flag` explanation is incomplete. No separate join declaration was found in the inspected header; this does not establish that destroy itself lacks an internal wait, nor prove that completion cannot be observed. [Task destroy](https://github.com/Ai-Thinker-Open/Ai-BS21_SDK/blob/f4f3f28781f4610a1d0a97fe24907d59ed03d09d/kernel/osal/include/schedule/osal_task.h#L197-L218).

## Lifecycle and backend boundaries

The public wrappers contain opaque pointers: `osal_completion`, `osal_wait` and `osal_task` (`osal_completion.h:18-20`, `osal_wait.h:21-23`, `osal_task.h:49-51`). The task factory returns a pointer whereas completion and wait initialization accept caller-provided wrapper pointers. Opaque fields do not authorize copying a live object or accessing backend structures.

Completion initialization is spelled `osal_completion_init`; its destruction symbol is actually spelled `osal_complete_destory`. The destruction comments incorrectly cite `osal_complete_init` as provenance in the completion header (`:110`) and even in `osal_wait_destroy` (`osal_wait.h:162`). The wait initializer is `osal_wait_init`. These are documentation inconsistencies; bindings must preserve real declared symbol spelling. Do not invent a third initializer from the comments.

`osal_complete` documents a full memory barrier when waking a task and ordered wakeup (`osal_completion.h:43-44`); `osal_complete_all` repeats the barrier statement (`:92`). The inspected wait-queue comments do not provide the same explicit barrier guarantee. This is a difference in documentation, not proof that backend wait queues lack synchronization.

The declared support lists are not uniform:

| Interface | Documented support | Interpretation boundary |
|---|---|---|
| Completion init, complete, wait without timeout, destroy | Linux, LiteOS, FreeRTOS | Header claims only |
| Completion timeout and complete-all | Linux, LiteOS | Omission is not proof of missing library symbols |
| Wait timeout interruptible | Linux, LiteOS, FreeRTOS | No complete timeout return convention stated |
| Wait timeout uninterruptible | Linux, LiteOS | Do not infer the other variant's support |
| Kthread schedule with nanosecond delay | Linux | Not a cross-backend sleep primitive |

Support anchors: `osal_completion.h:22-114`, `osal_wait.h:87-123`, and `osal_task.h:220-234`, at the pinned sources above.

Task-priority constants also depend on the build backend: `OSAL_TASK_PRIORITY_HIGH` is 3 for LiteOS/FreeRTOS and 90 for Linux (`osal_task.h:18-48`). The priority setter documents only HIGH/MIDDLE/LOW despite the larger constant inventory (`:87-95`). Numeric priority ordering and acceptance of additional constants must be checked against the actual backend; the header alone does not settle them.

## Reusable guidance and comparison

- Keep duration units in names or types. Treat completion remaining-time returns separately from millisecond waits and sleep APIs.
- Model task ownership and shutdown handshake explicitly. Obtain backend evidence for whether destroy waits, what stop_flag means, and when callback/task storage may be reclaimed.
- Preserve predicate synchronization and producer wakeup as separate obligations. Do not substitute POSIX signal cancellation for a documented LiteOS uninterruptible wait.
- Test backend capabilities rather than assuming a function name promises identical semantics everywhere. These are recommendations, not implemented fixes or verified failure cases.

[NEW-BS21-NV-PERSISTENCE-CONTRACT](NEW-BS21-NV-PERSISTENCE-CONTRACT.md) establishes the same header-contract evidence standard for persistence; it does not justify a universal SDK callback or join policy. [NEW-SLEMESH-RUST](NEW-SLEMESH-RUST.md) previously records opaque task bindings and [NEW-HiSilicon-Assessment](NEW-HiSilicon-Assessment.md) records task-creation call sites. Neither supplied this wait/time-unit and ownership comparison.

This report does not equate Ai-BS21 firmware with the WS73 USB host backend. No code was imported, hardware/PCB inspected, build run, implementation library analyzed, or runtime behavior tested.
