---
type: harvest
title: "tethering ohos-nearlinkControl — the official OHOS CLI for NearLink enable/disable with auto-connect policy parameters"
language: en
created: 2026-09-13
tags: [ohos, cli, nearlink-control, auto-connect, policy, harvest]
sources:
  - "ArchivalEra/nearlink-E573H-USBDC workspace"
trust: verified
stale_after: 2026-12-13
---

# tethering ohos-nearlinkControl — the official OHOS CLI for NearLink enable/disable with auto-connect policy parameters

- Inspection date: 2026-09-13 (same current clone as the tethering reports)
- Mode: read-only local program inspection; no build, network, hardware, or PCB access
- Scope: the OHOS-side NearLink control CLI shipped inside the tethering tree

## Executive findings

1. The tool is the **OHOS-side NearLink switch**: `ohos-nearlinkControl enable [--autoConnPolicy N]` / `disable` — confirming that NearLink enable/disable and **auto-connect policy selection** are operator-level controls in the OHOS world (the same `persist.nearlink.switch_enable` system parameter the SA profile gates on). [tools/ohos-nearlinkControl/README.md, main.cpp]
2. Error handling is structured: `ERR_NL_INVALID_COMMAND` machine codes with human hints ("NearLink service is not available or disconnected") — a machine+human dual error surface consistent with the hisi-rf-core diagnostic philosophy. [main.cpp:178-195]
3. Built via the OHOS hb build system as a foundation/communication/nearlink component. [README build command]

## Boundaries and gaps

- 396 lines covering enable/disable/policy; the full subcommand set beyond the README examples was not dumped.

## Reusable for our stack

- Auto-connect policy as an explicit CLI parameter mirrors the connection-manager policy surface our stack should expose.
- Machine-code + human-hint error output is the right CLI shape for agent-operable tools.

## Comparison anchors (vs existing reports)

- `NEW-OHOS-SA1190-IPC-SURFACE.md`: this CLI is the operator front-end to the SA 1190 service.
- `NEW-HS-FBB-CLI.md`: two CLI designs — OHOS service control vs fbb framework build tool.
