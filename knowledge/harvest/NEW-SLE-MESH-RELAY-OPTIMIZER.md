---
type: harvest
title: "sle_mesh_new relay optimizer deep-dive: 12dB hysteresis, stability-gated optimization, capacity-capped relay tree"
language: en
created: 2026-09-15
tags: [harvest, mesh, sle, relay, leader-election, ws63]
sources:
  - ""https://github.com/BH4ME/sle_mesh_new (src/sle_team_relay_optimizer.c, src/sle_team_node.c, include/sle_team_packet.h)""
trust: A
stale_after: 2027-03-15
---

# sle_mesh_new relay optimizer deep-dive

## Executive findings

Complements the sync-68 team-mesh-CLI report (pairing approval/allowlist surface) with the routing internals — the most mature SLE mesh state machine in the local library, portable C (no vendor SDK includes in `include/sle_team_packet.h`).

**1. Two-layer on-air format with four route types.** `include/sle_team_packet.h:21-27`: mesh envelope (`sle_team_mesh_packet_t`) wraps the app packet (`team/src/dst/seq`); route types `sle_team_route_type_t` distinguish logical DIRECT from physical TRANSPORT_DIRECT plus FLOOD/TRANSPORT_FLOOD. 12 payload types (REQ/RESPONSE/TEXT/ACK/ADVERT/GROUP_TEXT/GROUP_DATA/ANON_REQ/PATH/TRACE/MULTIPART/CONTROL/RAW_CUSTOM), max payload 184B, path record up to 64B, per-member `last_seq` for replay/dedup, `SLE_TEAM_RSSI_UNKNOWN=127` sentinel.

**2. Relay optimizer is stability-gated, not periodic.** `src/sle_team_relay_optimizer.c:56-77` (`opt_has_unstable_member`): optimization is *frozen* while pairing is enabled, a relay recovery is pending, any allowlisted member is missing/offline, any record has policy_pending or relay_recovery_candidate, or a half-offline record (offline but parent/next_hop still set). Rationale in-line: pairing/allowlist gaps mean the group is still changing. This "quiescence before reconfiguration" discipline is the key design idea — routing churn is refused during membership churn.

**3. Conservative tuning constants.** `sle_team_relay_optimizer.c:5-7`: RSSI floor -92dBm, **12dB parent-switch hysteresis** (`SLE_TEAM_OPT_RSSI_HYSTERESIS_DB`), relay child cap 7 (default cap in node.c:13; leader-direct cap overridable via config, node.c:53-72). Unknown RSSI scores -128 — deliberately worse than any measurement. First-generation optimizer only considers **leader-direct nodes** as relay candidates (`opt_is_leader_direct`), i.e., it rebalances the first tier before going multi-hop.

**4. Capacity-aware relay tree with ingress preference.** `src/sle_team_node.c:208-271`: relay selection picks the best online leader-direct relay with free downstream capacity (`child_count < max_downstream`, default 7); a forwarded child HELLO prefers the relay that physically delivered it (`sle_team_ingress_relay_can_parent` — relay must itself be online, allowed, parented to self or leader, and under cap). Recovery keeps the usable next hop while a member is being recovered (node.c:578). Allowlists are replaced by de-duplicated valid route ids (node.c:877). The CLI prints the full tree state per member (role/online/tier/parent/next_hop/child_count/last_seq/last_seen, cli.c:309-315) — the operator surface matches the state machine one-to-one.

## Boundaries

- The optimizer optimizes **tier 1 only** (leader-direct relays); deep multi-hop trees are out of scope by design ("first optimizer").
- Packet header is vendor-independent, but the transport binding (WS63 direct links vs. relay forward) lives elsewhere in the repo — this report covers state machines, not radio scheduling.
- 4193 total LOC across src+include — digestible, but the leader heartbeat/recovery timers were read only where they gate the optimizer.

## Reusable

- Stability-gated reconfiguration: freeze route optimization while pairing/recovery/membership churn is active — directly applicable to any mesh controller, including our SSAP session topology.
- 12dB hysteresis + -92dBm floor + child cap 7 as a tested starting point (contrast: helmet 6dB/10s, batch 4).
- Unknown-RSSI = -128 scoring idiom and wrap-safe elapsed check (`opt_elapsed_exceeds`) — small, bug-free utilities.
- Envelope+app two-layer packet with logical/physical route distinction — clean split for SLE group networks.

## Comparison anchors

- vs. batch 4 helmet DAG bitmap: sle_mesh_new is per-hop relay trees with capacity caps; helmet is whole-topology gossip. The hysteresis constants differ (12dB conservative vs. 6dB aggressive) — both validated, pick by churn tolerance.
- vs. 10102 two-level star: same 7-child cap idea as the WS63 3-connection practical limit, generalized with configurable caps.
- vs. sync-68 report: that covered operator/pairing surface; this covers the optimizer — together the repo is fully mapped.
