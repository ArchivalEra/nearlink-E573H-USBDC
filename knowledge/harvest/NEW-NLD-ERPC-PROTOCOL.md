---
type: harvest
title: Nld eRPC dongle protocol — nine service groups over dual serial ports (host/device_manager/seek/announce/connection/ssap_client/ssap_server/firmware/low_latency)
language: en
created: 2026-09-13
tags: [nld, erpc, dongle-protocol, bs2x, serial, ssap, low-latency, firmware-update, harvest]
sources:
  - url: https://gitcode.com/goodspeed1/Nld
    note: local clone (current); read-only inspection of erpc_gen/bs2x generated code and daemon/nldconfig.c
trust: verified
stale_after: 2026-12-13
---

# Nld eRPC dongle protocol — nine service groups over dual serial ports (host/device_manager/seek/announce/connection/ssap_client/ssap_server/firmware/low_latency)

- Inspection date: 2026-09-13 (same current clone as `NEW-NLD-DBUS-DAEMON.md`)
- Mode: read-only local program inspection; no build, network, hardware, or PCB access
- Scope: the dongle-side protocol decomposition — eRPC services and transport wiring

## Executive findings

1. The BS2x dongle protocol is decomposed into **nine eRPC service groups**, each generated as a bidirectional client/server pair (host→dongle calls AND dongle→host callbacks): `host`, `device_manager`, `seek` (discovery), `announce` (advertising), `connection`, `ssap_client`, `ssap_server` (the dongle-hosted local SSAP server), `firmware` (firmware update over the same link), and **`low_latency`** (a dedicated low-latency mode service). [`erpc_gen/bs2x/` generated file set]
2. The transport is **serial with optional dual-port split**: `nldconfig.c` binds each adapter to a device path and supports a `device:serial:event_serial` notation — separate command and event serial ports — with a `bs2x:` prefix shorthand for the adapter's default port, and path handling that tolerates colons inside `/dev/serial/by-path/...` device names. [`daemon/nldconfig.c:83-147`]
3. The generated code sits on the vendored eRPC framework (`erpc/infra`, `erpc/transports`, `erpc/port`), with `erpc_gen/bs2x_idl/` reserved for the IDL source — the protocol is IDL-defined and code-generated, not hand-rolled. [erpc_gen/ tree]
4. The `firmware` service plus the D-Bus `Firmware1` interface (Start/Write/Commit/Cancel) together implement dongle firmware update through the same management link — no separate flash tool needed for the BS2x under Nld.
5. `low_latency` as a distinct service implies the dongle firmware supports a mode switch (likely trading throughput for latency) that the daemon can negotiate per use case — the same concern our split `write`/`write_all` and poll-jitter measurements target from the host side.

## Boundaries and gaps

- The eRPC method signatures inside the generated headers were not dumped; the wire codec details (serialization version, framing CRCs) live in the eRPC infra.
- The dongle-side firmware implementing these eRPC servers is not in the repo (only the `adapter.mcu` artifact) — the firmware source remains closed.
- WS63 adapter generation (`erpc_gen/bs2x` only in-tree) suggests the WS63 profile is pending or built from a different branch.

## Reusable for our stack

- The nine-service decomposition (host/device_manager/seek/announce/connection/ssap_client/ssap_server/firmware/low_latency) is a clean taxonomy for our own dongle protocol — it maps 1:1 onto the capabilities our WS73 hardware has, including a firmware-update service and a low-latency mode we have not yet exposed.
- Dual-serial-port support (command/event separation) is a transport option worth noting for our USB transport: splitting event callbacks from command responses removes head-of-line blocking.
- eRPC IDL codegen over serial is a lighter alternative to our HCC dialect for new tooling — worth comparing for our host-side toolchain plans.

## Comparison anchors (vs existing reports)

- `NEW-NLD-DBUS-DAEMON.md`: the D-Bus layer above these eRPC services.
- `02-dli-hcc-dialect.md` / `USB-PROTOCOL.md`: our HCC-over-USB dialect vs Nld's eRPC-over-serial — two vendor dongle transport designs now documented side by side.
- `NEW-DATATRANSFER-CACHE-INTERNALS.md`: the low_latency service echoes the TCID/transfer-mode session fields in the OHOS datatransfer cache.
