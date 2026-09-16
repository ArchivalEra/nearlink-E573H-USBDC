---
type: harvest
title: "sle_mesh_new CLI — the team-mesh operator surface: join/leave/pairing-approval/allowlist plus telemetry injection commands"
language: en
created: 2026-09-13
tags: [sle, mesh, cli, pairing, allowlist, team, operator, harvest]
sources:
  - "ArchivalEra/nearlink-E573H-USBDC workspace"
trust: verified
stale_after: 2026-12-13
---

# sle_mesh_new CLI — the team-mesh operator surface: join/leave/pairing-approval/allowlist plus telemetry injection commands

- Inspection date: 2026-09-13 (same current clone as `NEW-SLE-TEAM-MESH-V456.md`)
- Mode: read-only local program inspection; no build, network, hardware, or PCB access
- Scope: the operator CLI over the team-mesh core

## Executive findings

1. The operator surface is a **serial CLI with 20+ verbs** covering the full mesh lifecycle: `join <team> <leader> <channel>` / `leave` (membership), `pairing start|stop|approve <id> [relay|norelay]|pending` (**operator-approved pairing with a relay-capability decision at approval time**), `allow all|only <id...>|add <id>|del <id>` (**member allowlist management**), `members`/`state` (introspection). [src/sle_team_cli.c:145-196]
2. **Telemetry injection commands** mirror the packet types one-to-one for testing: `hello`, `hb [battery] [rssi] [fix]` (heartbeat), `pos [lat_e6] [lon_e6] [speed] [heading] [battery] [fix] [sat]` (position with GPS fields), `alert [lost_id] [reason] [last_lat] [last_lon] [last_ts]` (**lost-member alert with last-known-position**), `ack [ack_seq] [acked_type] [status]` — each injectable to any destination for test scenarios. [sle_team_cli.c:148-160]
3. Peripherals (led/rgb/buzz/disp) each get their own help subtree — hardware actions are CLI-first-class for demos. [sle_team_cli.c:160-163]

## Boundaries and gaps

- The alert/lost-member semantics imply a member-liveness tracker; the timeout policy sits in the node module (defer-callback from the header).

## Reusable for our stack

- **Pairing-approval with a relay-capability decision** (`approve <id> relay|norelay`) is a governance primitive our mesh work should expose at the operator level.
- The allowlist (`allow only/add/del`) + lost-member alert with last-known-position complete the operational vocabulary for team-style meshes.

## Comparison anchors (vs existing reports)

- `NEW-SLE-TEAM-MESH-V456.md`: the core this CLI drives.
- `NEW-NLD-DBUS-DAEMON.md`: D-Bus vs serial-CLI operator surfaces for the same class of capability.
