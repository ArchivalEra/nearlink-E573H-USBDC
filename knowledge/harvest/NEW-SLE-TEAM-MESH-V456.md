---
type: harvest
title: BH4ME/sle_mesh_new — most mature public SLE team-mesh engineering tree (v4.5.56): portable packet core, relay optimizer, on-board Web API, release evidence chain
language: en
created: 2026-09-13
tags: [ws63, sle, mesh, relay, packet-format, web-api, flashing, release-engineering, harvest]
sources:
  - url: https://github.com/BH4ME/sle_mesh_new
    note: cloned 2026-09-13, pushed 2026-06-18 (fresh, not archived), 33MB; hardware/ and cad/ directories excluded per standing rule
trust: verified
stale_after: 2026-12-13
---

# BH4ME/sle_mesh_new — most mature public SLE team-mesh engineering tree (v4.5.56): portable packet core, relay optimizer, on-board Web API, release evidence chain

- Inspection date: 2026-09-13 (staleness check: pushed 2026-06-18, not archived — ALIVE, ~3 months quiet)
- Source root: `/mnt/hdd/nearlink-stuff/sle_mesh_new`
- Mode: read-only local program inspection; hardware/ and cad/ trees excluded per standing rule
- Scope: team-mesh packet protocol, relay optimizer, board Web API, flash/release engineering

## Executive findings

1. Maturity level is a step above every mesh repo we hold: a versioned release line (`v4.5.56-minimal`), packaged release firmware, **release evidence JSON** from hardware proof runs, serial-probe and relay-recovery hardware tests, simulator plus C regressions, remote build wrappers, and multi-board flashing automation (PowerShell + Python, CH340 retry). [`README.md` table, `release/evidence/`, `automation/ws63/`]
2. The portable core (`include/` + `src/`, 3546 lines, bare C with stdint/stdbool only) splits cleanly: `sle_team_packet`, `sle_team_node` (state machine), `sle_team_relay_optimizer`, `sle_team_location` + `sle_team_nmea` (GPS), `sle_team_cli`, `sle_team_web_api` — a replicable module taxonomy for an SLE mesh product. [`src/`, `include/` listings]
3. The wire format is a bounded, explicit packet: max 254 bytes total (1 type + 4 + 1 path-count + up to 64 path bytes + 184 payload), four route types (`TRANSPORT_FLOOD` / `FLOOD` / `DIRECT` / `TRANSPORT_DIRECT` — flood and direct both first-class), packet types REQ/RESPONSE/TEXT/ACK, a `leader_term` field (leader epoch), an RSSI sentinel of 127 for unknown, and a firmware-compatibility byte. [`include/sle_team_packet.h:12-31`]
4. The **relay optimizer** runs as a periodic tick (`sle_team_relay_optimizer_tick(node, now_ms, interval_ms, last_run_ms)`) over the node state machine — reconnection/recovery decisions are time-sliced rather than event-storm driven. [`include/sle_team_relay_optimizer.h:11-13`]
5. The **board serves its own Web API** (`sle_team_web_api.c` + `webui/` with API contract tests): an HTTP service on the WS63 board exposes team state to a browser UI — mesh management without a separate host application. [`src/sle_team_web_api.c`, `webui/index.html`]
6. Unified leader/member/relay firmware in one tree (`xc/ws63_team_network/`) with ST7789/LVGL display, WS2812 status LED, GPS and battery hooks — role is configuration, not a fork. [`README.md` table]

## Boundaries and gaps

- ~3 months quiet; single-maintainer hobby-professional grade, but with unusually strong release/test discipline for its class.
- Hardware schematics and CAD STLs exist in-tree and were excluded from analysis per the PCB rule.
- The mesh does not interoperate with the leader-rooted tier design in `sle_mesh` (v4.4.9) — two independent packet formats despite the shared lineage.

## Reusable for our stack

- The explicit bounded packet format (path-counted routing header inside a 254-byte envelope, route-type nibble, leader term, FW-compat byte) is the most complete public SLE mesh wire spec — a direct comparison point for our own mesh ambitions and for the AIGC frame's mesh (`NEW-WS63E-MESH-AIGC-FRAME.md`).
- Tick-sliced relay optimization and release-evidence JSON are adoption-ready engineering patterns for any SLE deployment we operate.
- On-board Web API + contract tests show a self-contained management plane that would suit our dongle's status/diagnostic surface.

## Comparison anchors (vs existing reports)

- `NEW-SLEMESH-RUST.md`: that tree's leader-rooted tier routing vs this repo's flood/direct route-type packets — the third independent SLE mesh design archived, and the most production-shaped.
- `NEW-MESHGATEWAY-APP-PROTOCOL.md` / `NEW-WS63E-MESH-AIGC-FRAME.md`: image-transfer meshes vs this team/telemetry mesh — different payload classes over the same SLE bearers.
- `NEW-BEARPI-H3863-DOCS.md`: same BearPi H3863 hardware base and flashing ecosystem (xf_burn_tools appears here as vendored tooling).
