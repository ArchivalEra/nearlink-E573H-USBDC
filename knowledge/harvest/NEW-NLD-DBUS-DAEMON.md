---
type: harvest
title: "goodspeed1/Nld + nearlinkctl — a complete BlueZ-equivalent userspace for NearLink LE: D-Bus daemon with eRPC dongle adapters, local SSAP server tree, pairing agents, D-Bus firmware update"
language: en
created: 2026-09-13
tags: [nearlink, sle, dbus, daemon, bluez-equivalent, erpc, ssap-server, firmware-update, dongle, harvest]
sources:
  - "/mnt/hdd/nearlink-stuff/Nld"
trust: verified
stale_after: 2026-12-13
---

# goodspeed1/Nld + nearlinkctl — a complete BlueZ-equivalent userspace for NearLink LE: D-Bus daemon with eRPC dongle adapters, local SSAP server tree, pairing agents, D-Bus firmware update

- Inspection date: 2026-09-13 (staleness check: Nld pushed 2026-08-20, nearlinkctl 2026-09-09 — ALIVE)
- Source roots: `/mnt/hdd/nearlink-stuff/Nld`, `/mnt/hdd/nearlink-stuff/nearlinkctl`
- Mode: read-only local program inspection; no build, network, hardware, or PCB access
- Scope: the Linux/Windows NearLink LE userspace architecture — daemon topology, D-Bus API, dongle adapter protocol

## Executive findings

1. **Architecture — the BlueZ pattern applied to NearLink**: the `nld` master daemon exposes the machine's SLE wireless capability as a D-Bus API (well-known name `cn.hinearlink.nl`); it spawns **one `nldadapter.<dev>` process per dongle**, each talking **eRPC** to a HiSilicon dongle (**BS2x, WS63** named explicitly) and registering its own D-Bus service name. Per-chip drivers compile separately (an eRPC limitation), multiple adapters including same-model multi-dongle run concurrently. Linux and Windows both build natively; the Windows build is fully static with a **private D-Bus session bus bundled**. [Nld/README.md:1-10]
2. The D-Bus API (977-line `cn.hinearlink.nl.xml`) is a **14-interface BlueZ mirror**: `Adapter1` (StartDiscovery/StopDiscovery/RemoveDevice/RemovePairedDevice/RemoveAllPairs/SetName/SetAddress), `Device1` (Connect/Disconnect/Pair/CancelPairing/Discover/DiscoverAll/RequestMtu/Bond, 12 properties), `AgentManager1`/`Agent1` (pairing agent: RequestPasskey/DisplayPasskey/Release/Cancel), **`SsapManager1` (RegisterApplication/UnregisterApplication)** with a **local SSAP server object tree** (`SsapService1`/`SsapProperty1`/`SsapDescriptor1` — remote devices can discover and access this machine's services), **`SsapAdvertisingManager1`/`SsapAdvertisement1`** (SSAP announcing, connectable and non-connectable), GATT-style `Service1`/`Property1` (Read/Write/WriteWithoutResponse/StartNotify/StopNotify) for remote attribute access, and **`Firmware1` (Start/Write/Commit/Cancel — firmware update as a D-Bus interface)**. [`cn.hinearlink.nl.xml`, 977 lines]
3. **Reliability contract**: every asynchronous method completes only after **firmware confirmation**, guarded by watchdog timeouts — D-Bus replies are bound to dongle ACKs, not local queuing. [Nld/README.md features]
4. `nearlinkctl` is the `bluetoothctl`-equivalent interactive client (C, meson, GLib/GIO, readline): discovery, connection, pairing, bonding, and SSAP attribute access against the daemon's D-Bus API, with completion support and a test directory. [nearlinkctl/README.md]
5. The `Nld` tree includes `erpc/` + `erpc_gen/` (eRPC IDL codegen for the dongle protocol) and an `adapter.mcu` artifact; `README.WIN32` documents the Windows native path. [repo tree]

## Boundaries and gaps

- Single-author project (goodspeed1), zero stars on gitcode; no CI observed.
- Which HiSilicon USB protocol the eRPC adapter speaks (HCC-like or custom) was not traced into `erpc/` this pass — the dongle-side firmware source is not in the repo.
- Windows private D-Bus bus implementation details not audited.
- The local SSAP server tree's permission/security model beyond passkey pairing was not examined.

## Reusable for our stack

- **This is the reference architecture for our WS73 dongle's userspace story**: total-manager + per-dongle adapter processes + D-Bus API + interactive CLI is exactly the deployment shape our E573H host stack needs to be consumable like BlueZ peripherals are.
- The D-Bus interface set (Adapter/Device/Agent/SsapManager/SsapService/SsapProperty/SsapDescriptor/SsapAdvertisingManager/Firmware) is a ready-made API surface our stack can implement or interoperate with — including the local SSAP server tree and D-Bus firmware update, both features our hardware supports.
- The eRPC-per-dongle-adapter process isolation (with per-chip driver builds and firmware-ACK-bound method completion + watchdogs) solves the multi-dongle and vendor-firmware coupling problems we face with two WS73 units.
- The 272-byte-frame scheduler from `hisi-rtos` and this D-Bus daemon are two ends of the same ecosystem gap our repo targets: device runtime and host userspace.

## Comparison anchors (vs existing reports)

- `NEW-OHOS-SA1190-IPC-SURFACE.md`: OHOS exposes NearLink over its own IPC/SA model; Nld is the freedesktop/D-Bus equivalent for general Linux — two competing userspace exposure models now archived.
- `USB-PROTOCOL.md` / `02-dli-hcc-dialect.md`: our host stack's HCC-over-USB is the transport layer Nld's eRPC adapter replaces or wraps; comparing them tells us whether to interoperate or coexist.
- `NEW-TEKI128-MINIMAL-PAIR.md` / HHD-01 reports: those are device-side SSAP pairs; Nld is the host-side service framework that would drive them.
- `BLE-WIFI-USERLAND-RESEARCH.md`: that research asked for the BlueZ-equivalent for NearLink — this repo IS the answer.
