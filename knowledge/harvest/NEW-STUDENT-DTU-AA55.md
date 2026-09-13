---
type: harvest
title: 2026 competition — ws63_data_transfer_unit: a layered student DTU with AA55 binary config protocol, RS485 bridge, and a tree-mesh preview
language: en
created: 2026-09-13
tags: [ws63, dtu, config-protocol, aa55, rs485, tree-network, layering, harvest]
sources:
  - url: https://gitcode.com/HiSpark/2026_embedded_competition
    note: IOT/23778_ws63_data_transfer_unit; cloned 2026-09-13, pushed 2026-09-07; 26 C/H files
trust: verified
stale_after: 2026-12-13
---

# 2026 competition — ws63_data_transfer_unit: a layered student DTU with AA55 binary config protocol, RS485 bridge, and a tree-mesh preview

- Inspection date: 2026-09-13 (competition corpus, DTU deep-dive queue item)
- Source root: `IOT/23778_ws63_data_transfer_unit` (26 C/H files)
- Mode: read-only local program inspection; no build, network, hardware, or PCB access

## Executive findings

1. The DTU is organized as **explicit facades**: `dtu_main.c` (startup) → `manager/dtu_service` (master entry: storage/board/transport init, input demux, unified config-reply egress) → `config/` (CONFIG facade: byte-stream entry, parser state machine, command dispatch), `run/` (RUN mainline: **SLE / UART0 / UART1(RS485) transparent bridging**, with `run/mesh/` reserved for future networking), `storage/` (defaults, NV read/write, mode cache). The layering discipline (CONFIG vs RUN planes, per-plane facades) is above average for the corpus. [README file-layer table, tree]
2. The config plane speaks a **binary AA55 protocol**: `dtu_config_protocol.c` implements an AA55 parser state machine (fields: cmd, seq, len, CRC0/CRC1 states) with CRC validation and response-frame packing; `dtu_config_commands.c` holds the command table (GET_MODE_STATUS, REBOOT, config handlers). A binary config protocol rather than AT strings — the third config-plane style after AT text and JSON. [`config/dtu_config_protocol.c:12-81`]
3. **`sle_tree_v1/` previews a tree-topology mesh** (ST_test bench/cfg harness files) — the student DTU plans tree networking, matching the industry DTU pattern (DS10's tier config) and the competition's positioning systems. [`sle_tree_v1/`]

## Boundaries and gaps

- RUN-plane file naming drifted from the README table (`run/dtu_run.c` not present at that exact path); the bridge implementation lives elsewhere in the tree.
- The tree protocol itself (sle_tree_v1) is test scaffolding, not a full implementation.

## Reusable for our stack

- The CONFIG/RUN plane split (config binary protocol at rest, transparent bridging at run) is the standard DTU shape — our dongle's control/data plane separation matches it.
- AA55+seq+len+CRC16 binary config framing is a compact alternative to AT for our own tooling.
- RS485 bridging over SLE extends the transparent-transport family to industrial buses.

## Comparison anchors (vs existing reports)

- `NEW-DS10-SLE-DTU.md`: the commercial DTU benchmark; this student DTU independently converges on the same CONFIG/RUN + transparent-bridge + tree-networking shape.
- `NEW-TXSTAR-DS10-REMOTE.md` / `NEW-STARBRIDGE-EDGE-BRIDGE.md`: serial split-point family (binary AA55 joins AT/JSON/binary-frames).
- `NEW-SLE-TEAM-MESH-V456.md`: tree networking intent matches the team-mesh route types.
