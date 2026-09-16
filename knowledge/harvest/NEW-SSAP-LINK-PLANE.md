---
type: harvest
title: "OHOS ssap_link plane — a four-state link model (DISCONNECTED/CONNECTING/CONNECTED/DISCONNECTING) shared by server and client, with documented retry semantics"
language: en
created: 2026-09-13
tags: [ohos, ssap, link-state, fsm, four-state, retry-semantics, shared-plane, harvest]
sources:
  - "ArchivalEra/nearlink-E573H-USBDC workspace"
trust: verified
stale_after: 2026-12-13
---

# OHOS ssap_link plane — a four-state link model (DISCONNECTED/CONNECTING/CONNECTED/DISCONNECTING) shared by server and client, with documented retry semantics

- Inspection date: 2026-09-13; shared-infrastructure pass completing the SSAP servm series
- Mode: read-only local program inspection; no build, network, hardware, or PCB access
- Scope: the link plane both server and client halves build on

## Executive findings

1. The link model is **four states, not three**: `SSAP_CONNECT_STATE_DISCONNECTED` (failed or torn down), `CONNECTING`, `CONNECTED`, and `DISCONNECTING` — the transitional disconnect state is modeled explicitly, and its contract is written into the header: "callers must cache requests and retry after the disconnect completes". [`ssap_link_state.h:33-49`]
2. State queries are **address-keyed and null-safe**: both `SsapGetConnectState(addr)`-style accessors document that a NULL address returns DISCONNECTED rather than faulting, making the state plane safe for unguarded callers. [`ssap_link_state.h:38-49`]
3. The plane is deliberately small — `ssap_link.c` (390 lines) plus `ssap_link_state.c` (305 lines) — carrying link records shared by the 4.6K-line server and 4.9K-line client halves; all per-role logic lives in the role modules, all per-link truth lives here. [`wc -l`]
4. The state vocabulary matches the port FSM (`NEW-PORT-PROFILE-FSM.md`) and the app link SM (`ssapc_app_link_sm.c`) at the CONNECTED boundary: three layers (connection plane, app link SM, profile FSM) compose rather than duplicate state.

## Boundaries and gaps

- The transition triggers (which SLE ACL callbacks drive CONNECTING→CONNECTED) live in the .c implementation; only the state contract and query semantics were read this pass.
- No keep-alive/idle-timeout handling was visible at this plane — it is a thin truth-table over the ACL, not a supervision layer.

## Reusable for our stack

- Four states with an explicit DISCONNECTING + retry-contract is the right model for our SSAP link layer: writes arriving during DISCONNECTING must be queued and replayed, and encoding that rule in the state header (as upstream does) is cheap documentation that prevents an entire bug class.
- A thin, address-keyed, null-safe state-query plane shared by both roles keeps role modules honest about link truth.

## Comparison anchors (vs existing reports)

- `NEW-SSAP-SERVM-MODULE-MAP.md`: positions this plane as the shared foundation of the map.
- `NEW-PORT-PROFILE-FSM.md` / `NEW-SSAPS-MULTI-OPERATIONS-CORRECTION.md`: consumers of this plane's states; the retry contract explains the buffered-operation design in the write-multi path.
- `OSPL-CONN-FSM.md`: our earlier connection-FSM intel from the OSPL side matches this vocabulary — cross-dialect confirmation.
