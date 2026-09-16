---
type: harvest
title: "openharmony/communication_dsoftbus — SLE scaffolding present, transport stubbed in public tree (SLB dual-stack states exposed)"
language: en
created: 2026-09-13
tags: [ohos, dsoftbus, sle, slb, stub, capability-ledger, coexistence, harvest]
sources:
  - "ArchivalEra/nearlink-E573H-USBDC workspace"
trust: verified
stale_after: 2026-12-13
---

# openharmony/communication_dsoftbus — SLE scaffolding present, transport stubbed in public tree (SLB dual-stack states exposed)

- Inspection date: 2026-09-13; metadata clone + sparse checkout (only SLE-related paths materialized)
- Scope: the "46MB softbus competition/coexistence" queue item from issue 11, resolved by evidence
- Mode: read-only local program inspection; no network, build, hardware, or PCB access

## Executive findings

1. The public dsoftbus tree ships a **deliberate SLE transport stub**: `ConnSleInit()` logs `"do not support sle init"` and returns NULL — the connection-manager slot for SLE exists in the architecture but its implementation is closed-source or not yet public. [`core/connection/sle/src/conn_sle_manager_virtual.c:17-22`]
2. The scaffolding around the stub is complete and revealing: the net-ledger carries per-node SLE capability (`JSON_KEY_SLE_MAC`/`JSON_KEY_SLE_CAP`), with ledger APIs to set SLE range capability and address, a MAC-changed sync message handler, and a broadcast-to-all-nodes send — i.e., OHOS devices already advertise SLE reachability through the softbus info-sync plane. [`core/bus_center/lnn/net_ledger/common/include/lnn_sle_capability.h:11-25`]
3. The kit adapter struct exposes the stack-state vocabulary, including a **dual-stack enum**: SLE states AND `SOFTBUS_SLB_STATE_*` states (SLB = SparkLink Basic, the second SLE-family stack), each with TURNING_ON/ON/OFF/TURNING_OFF plus a **TURN_HALF** state not seen in our other captures. [`interfaces/kits/adapter/enhance/softbus_adapter_sle_common_struct.h:9-24`]
4. A bus-center SLE link monitor exists in the adapter layer (`lnn_sle_monitor.h/.c` with a `_virtual` variant), plus unit tests and a `lnnslecapability_fuzzer` — the test plumbing is ahead of the transport. [`adapter/common/bus_center/`, `tests/`]
5. Net assessment: dsoftbus is SLE-READY at interface/ledger level but performs zero NearLink transport today; "coexistence competition" with `communication_nearlink_service` is currently architectural, not runtime.

## Boundaries and gaps

- Only the sparse paths were read; the BLE transport (`conn_ble`) was not re-audited in this pass.
- The private SLE implementation behind the stub was not located in any public repo searched this session.
- TURN_HALF semantics are undocumented in-tree; the name suggests a partial-power/half-stack state but this is inference, not evidence.

## Reusable for our stack

- The capability-ledger pattern (nodes publish SLE MAC + capability via an info-sync plane, consumers route by advertised capability) is a reusable discovery model for our SSAP stack's device discovery work.
- The SLE+SLB dual-stack state enum is new official vocabulary — our tri-mode driver work should reserve state space for both stacks plus transitional states.
- Another confirmed instance of the OHOS "public stub / private implementation" pattern (after the service-layer repos), which calibrates expectations for future OHOS source audits.

## Comparison anchors (vs existing reports)

- `OHOS-CONN-FSM.md` / `OSPL-CONN-FSM.md`: the connection-manager slot pattern matches those reports' FSM layering; SLE is plugged into the same interface as BLE/BREDR transports.
- `OHOS-DEVICE-MGR.md`: the net-ledger SLE capability keys extend the device-manager data model that report covered.
- `NEW-OHOS-NEARLINK-SEPT-INCREMENT.md`: complementary proof of the same stub pattern at the connection layer one month later.
