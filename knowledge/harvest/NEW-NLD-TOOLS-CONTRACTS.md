---
type: harvest
title: "nld tools — runnable D-Bus API contracts: a KeyboardOnly pairing agent with fixed passkey and a Python SSAP server exporting service/property/CCCD objects"
language: en
created: 2026-09-13
tags: [nld, dbus, pairing-agent, ssap-server, object-manager, cccd, python, harvest]
sources:
  - "ArchivalEra/nearlink-E573H-USBDC workspace"
trust: verified
stale_after: 2026-12-13
---

# nld tools — runnable D-Bus API contracts: a KeyboardOnly pairing agent with fixed passkey and a Python SSAP server exporting service/property/CCCD objects

- Inspection date: 2026-09-13 (same current clone as `NEW-NLD-DBUS-DAEMON.md`)
- Mode: read-only local program inspection; no build, network, hardware, or PCB access
- Scope: the two reference tools packaged with nld — executable specifications of its Agent1 and SsapManager1 APIs

## Executive findings

1. **`agent-test.py` (112 lines) exercises the Agent1 pairing contract**: exports `cn.hinearlink.nl.Agent1` on the session bus, registers with **"KeyboardOnly" capability**, answers every `RequestPasskey`/`DisplayPasskey` with a fixed code (`--passkey`, default 123456). Adapter resolution is `$NLD_ADAPTER` env override or first adapter from `Manager1.Adapters` — the multi-adapter addressing model in practice. [tools/agent-test.py:18-40]
2. **`ssap-app.py` (286 lines) is a full SSAP server example**: exports an **ObjectManager root containing SsapService1 + SsapProperty1 + a client-configuration (CCCD) descriptor**, serves read/write/notify operations, and nld walks the tree on `RegisterApplication`, calling back into the exported objects on remote access. This is the local-server object-tree contract from the D-Bus XML made runnable. [tools/ssap-app.py docstring]
3. Together they are **executable API documentation**: pairing flow (agent export → register → passkey callbacks) and server flow (tree export → register → remote-access callbacks) can be validated without writing C.

## Boundaries and gaps

- Both are Python test tools (GPL-3.0-or-later); production paths are the C daemon.
- The object-tree walk semantics (which order nld registers objects) not traced line-by-line.

## Reusable for our stack

- The pair of tools is a **behavioral spec for our own daemon's pairing-agent and local-SSAP-server surfaces**: implement the same D-Bus callbacks and the tools work against our stack unchanged — a conformance harness we inherit for free.
- ObjectManager-rooted service export is the D-Bus idiom our local service tree should follow.

## Comparison anchors (vs existing reports)

- `NEW-NLD-DBUS-DAEMON.md` / `NEW-NLD-ERPC-PROTOCOL.md`: the daemon and dongle protocol; these tools pin the host-facing API semantics.
- `NEW-OHOS-SA1190-IPC-SURFACE.md`: OHOS IPC vs D-Bus for the same local-server capability — two operator surfaces, now both with runnable examples.
