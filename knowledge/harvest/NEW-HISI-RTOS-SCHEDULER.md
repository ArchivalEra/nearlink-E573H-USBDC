---
type: harvest
title: hispark-rs/hisi-rtos — a no_std Rust scheduler for HiSilicon: three run policies, 272-byte unified trap frame, deferred preemption, capability-gated porting
language: en
created: 2026-09-13
tags: [rust, no-std, rtos, scheduler, preemption, trap-frame, budgeted, hisilicon, harvest]
sources:
  - url: https://github.com/hispark-rs/hisi-rtos
    note: local clone (pushed 2026-09-10, freshest hispark-rs subrepo); read-only inspection of README and src layout
trust: verified
stale_after: 2026-12-13
---

# hispark-rs/hisi-rtos — a no_std Rust scheduler for HiSilicon: three run policies, 272-byte unified trap frame, deferred preemption, capability-gated porting

- Inspection date: 2026-09-13 (local hispark-rs subrepo, current with upstream 2026-09-10)
- Mode: read-only local program inspection; no build, network, hardware, or PCB access
- Scope: the no_std scheduler/runtime that Rust WS63 firmware runs on — policy model, preemption mechanics, capability gating

## Executive findings

1. **Three per-thread run policies on one scheduler**: `RunPolicy::Cooperative` (yield only when it chooses), `RunPolicy::Budgeted` (periodic CPU quota; task becomes ineligible until replenishment after exhaustion), `RunPolicy::Preemptive` (forcibly switchable, optional equal-priority time slicing). Scheduling is effective-numeric-priority with FIFO within a priority; the policy only controls when the running task may be forcibly switched. [README.md:4-10]
2. **Preemption is deferred through a 272-byte unified task/trap frame**: TIMER and software interrupts acknowledge, record, and wake; the common trap epilogue selects the next task, **re-arms the selected task's deadline**, and restores it with `mret` — one frame shape serves threads and traps, keeping the context-switch path uniform. [README.md:12-14]
3. **Capability-gated policy porting**: `CooperativeConfig` cannot express a preemptive policy; `PortedConfig` is accepted only by `start_with_port`, whose returned capability is required for policy use — a board port must explicitly earn the right to preempt. [README.md:16-18]
4. Operational invariants are part of the public contract: exactly one runtime start before radio-firmware init, caller-owned task-stack storage, and an internal lowest-priority idle thread that cannot be allocated or reconfigured. [README.md:2-3, 11]
5. The repo carries a `spec/` directory and `tests/` — the scheduler is specification-driven with regression tests, unusual rigor for a no_std hobby-adjacent RTOS. [repo tree]

## Boundaries and gaps

- README-level analysis; the dispatch loop and quota replenishment internals in `src/runtime/` were not line-read.
- Single-hart only — no SMP story; multi-core HiSilicon parts are out of scope for this crate.
- Radio-firmware interplay (why "exactly one runtime start before radio init") is asserted, not explained in the read portion.

## Reusable for our stack

- The Budgeted policy (CPU quota with replenishment) is a rare find in no_std RTOSes — the right primitive for co-locating our SSAP stack with vendor radio tasks under deterministic CPU budgets.
- The capability-gated porting pattern (`start_with_port` returning a policy capability) is the clean way to make preemption a board-verified property rather than a compile-time assumption.
- The 272-byte unified frame documents the RISC-V context-switch cost concretely — a sizing input for our deep-module/LTO performance planning.

## Comparison anchors (vs existing reports)

- `NEW-WS63-EXAMPLES-RUST.md`: the examples exercise this runtime (rtos_preemption, rtos_priority_inheritance, rtos_budget_enforcement, rtos_scheduler_stress are all its test surface).
- `RUST-WS73-DEEP-MODULES.md`: the runtime sits under the crate stack our deep-module plan targets.
- `NEW-FBB-WS63-QEMU-FORK.md`: a deterministic single-hart scheduler is ideal for emulator validation (icount deterministic timing pairs well with budgeted policies).
