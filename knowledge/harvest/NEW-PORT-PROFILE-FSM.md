---
type: harvest
title: "tethering port_stm.h — the Port Profile connection FSM: eight states from IDLE to CONNECTED via SSAP service discovery and network set"
language: en
created: 2026-09-13
tags: [sle, port-profile, state-machine, ssap, fsm, tethering, harvest]
sources:
  - "ArchivalEra/nearlink-E573H-USBDC workspace"
trust: verified
stale_after: 2026-12-13
---

# tethering port_stm.h — the Port Profile connection FSM: eight states from IDLE to CONNECTED via SSAP service discovery and network set

- Inspection date: 2026-09-13 (same current clone as `NEW-OHOS-TETHERING-SERVICE.md`)
- Mode: read-only local program inspection; no build, network, hardware, or PCB access
- Scope: the Port Profile state machine — the third and final angle on the SLE Port service for this session

## Executive findings

1. The Port Profile connects through an explicit eight-state FSM: `IDLE → REGISTER_APP → CREATE_LINK → GET_SERVICE → FIND_SERVICE → READ_PROPERTY → SET_NET → CONNECTED` — application registration precedes link creation, and two separate discovery steps (GET_SERVICE, FIND_SERVICE) precede property read and network configuration. [`services/stack/src/cp/bal/profile/port/src/port_stm.h:9-19`]
2. The event set is driver-shaped rather than request-shaped: `PORT_ON_USER_CONNECT/_DISCONNECT`, `PORT_ON_REGISTER_APP`, `PORT_ON_STATE_CHANGED`, `PORT_ON_GET_SERVICE`, `PORT_ON_FIND_SERVICE`, `PORT_ON_READ_PROPERTY`, `PORT_ON_SET_NTF`, `PORT_ON_PROPERTY_CHANGED` — user connect/disconnect are FSM events at the same rank as SSAP callbacks, so a user-level session rides the same machine as the link. [`port_stm.h:21-32`]
3. `SET_NET` as a distinct pre-CONNECTED state confirms the profile carries network configuration (address/MTU negotiation) as an explicit handshake phase before the port is usable — networking is part of the profile contract, not an afterthought. [`port_stm.h:14-15`]

## Boundaries and gaps

- Transition table (state × event → action) lives in the .c file or prebuilt portions not yet inspected; the state and event enums are fully visible but edge labels are inferred from names.
- `PORT_ON_SET_NTF` vs `PORT_ON_PROPERTY_CHANGED` semantics (setting notify vs notification arrival) inferred from naming.

## Reusable for our stack

- This FSM is a complete blueprint for a SSAP-client bring-up state machine in our stack: register app → create link → discover service → find structure → read property → configure network → connected maps 1:1 onto our link/session layering, and validates our earlier plan to keep app registration before link creation.
- The user-session-events-inside-the-link-FSM design is a cleaner alternative to separate connection and session state machines for single-purpose profiles.

## Comparison anchors (vs existing reports)

- `NEW-OHOS-TETHERING-SERVICE.md`: same tree; this adds the FSM internals behind the datatransfer service.
- `NEW-HIDITING-SLE2-EVIDENCE.md`: the `bs_sle_port_*` 2.0 API is the evolved interface over this 1.x FSM — three-source convergence on Port as a transport abstraction complete.
- `NEW-SSAPS-FIND-REFERENCE.md`: FSM states GET_SERVICE/FIND_SERVICE map onto that report's find-family implementation.
