---
type: harvest
title: "Nld daemon internals: 15 D-Bus interfaces, 9 eRPC service pairs, Windows COM-TCP bridge, and the VID-109B CDC dongle identity"
language: en
created: 2026-09-15
tags: [harvest, nld, dbus, erpc, dongle, usb-cdc, windows]
sources:
  - ""/mnt/hdd/nearlink-stuff/Nld (daemon/, adapter.mcu/, erpc_gen/bs2x/, cn.hinearlink.nl.xml, README.md)""
trust: A
stale_after: 2027-03-15
---

# Nld daemon internals deep-dive

## Executive findings

**1. The D-Bus object tree is now fully enumerable: 15 interfaces, 112 methods/signals/properties.** `cn.hinearlink.nl.xml` exports Adapter1, Device1, Advertisement1, Service1, Property1, SsapManager1/SsapService1/SsapProperty1/SsapDescriptor1, SsapAdvertisingManager1/SsapAdvertisement1, AgentManager1/Agent1, Manager1, **Firmware1** (15 total — one more than our earlier count of 14; Firmware1 handles dongle firmware operations). Documentation quality is BlueZ-grade: every method doc-commented, errors as named D-Bus errors (e.g. IN_PROGRESS on double StartDiscovery). Adapter holds `GHashTable *devices` + separate paired/bonded lists and single ssapc_id/ssaps_id — one SSAP client + one SSAP server session per adapter instance.

**2. eRPC contract: 9 service pairs generated for the BS2X dongle.** `erpc_gen/bs2x/` contains client+server generated code for: announce, connection, device_manager, firmware, host, low_latency, seek, ssap_client, ssap_server — the dongle-side service list, each as `c_sle_*_client.cpp` (host→dongle) + `c_sle_*_server.cpp` (dongle→host callbacks) plus C++ interface/common layers. **`low_latency` as a first-class dongle service** is notable — an explicit QoS channel beyond the standard parameter sets. This is the complete, buildable wire contract for our E573H dongle work.

**3. Windows story is instructive for a cross-platform daemon.** `daemon/nld.c:41-60`: instead of GLib's win32 session-bus autolaunch, nld ships `dbus-daemon.exe` next to `nld.exe`, spawns it, and parses the printed bus address; falls back to autolaunch if absent. `adapter.mcu/comtcp.c` is a **transparent COM↔TCP loopback relay** (`nld-bridge <com-port> [listen-port] [parent-handle]`): Windows can't open a COM port as a CRT fd and nld's eRPC transport is fd-based, so the bridge owns the COM port and nld connects over loopback TCP, learning the port from the bridge's first stdout line `NLD_BRIDGE_PORT <port>`. The listener exists only while the COM port is open — **when the dongle resets (USB CDC re-enumeration) the relay drops the client and reopens the port, so nld sees exactly the tty disconnect it would see on POSIX**. Liveness via an inherited parent process handle (pid-wait is unreliable), broken-stdin as manual-mode exit signal, 8KB buffer.

**4. Dongle identity cross-check: Nld expects a USB CDC serial at VID 109B.** README.md:176: "find the dongle's COM port (USB VID 109B 'HiSilicon'; the CH340 port is not it)". Our E573H dongle is `ffff:3733` vendor-bulk — **two distinct USB dongle flavors exist in the ecosystem**: (a) HiSilicon VID 109B CDC-serial (nld's eRPC-over-serial target), (b) 3733 vendor-protocol bulk device (our driver). Any tooling we build must not assume the other's transport; nld's `--serial`/`--event-serial` split-config (two ports: command + event) further shows the CDC flavor can present dual ports.

**5. Adapter MCU side is a full port too.** `adapter.mcu/` (7.6k LOC: comtcp, nldadapter, nldagent, nldannounce, nlddevice, nldfirmware, nldproperty, nldservice, nldssap, rpc_cb, main) implements the dongle firmware counterpart of the same object model — daemon and MCU mirror each other's object graphs.

## Boundaries

- GPL-3.0: contract shapes are reusable knowledge; verbatim code reuse must respect the license.
- eRPC service semantics beyond names need reading the generated `.hpp` interfaces (next dive if we adopt eRPC).
- The `low_latency` service has no README documentation yet — treated as QoS-tuning surface, not protocol.

## Reusable

- COM↔TCP bridge pattern with `NLD_BRIDGE_PORT` stdout handshake and CDC-re-enumeration resilience — directly reusable for any Windows tool on serial dongles, including ours.
- Parent-handle liveness + broken-stdin fallback — the correct Windows process-coupling idiom.
- D-Bus error-as-name (IN_PROGRESS), one-SSAP-session-per-adapter simplification, paired vs. bonded list split — BlueZ-compatible modeling decisions to copy.
- 9-service dongle contract incl. low_latency — the checklist of dongle capabilities to support or explicitly reject in our own driver.

## Comparison anchors

- vs. our E573H USB-PROTOCOL.md: nld documents the CDC-serial flavor; ours is vendor-bulk — the USB-PROTOCOL report should now record both flavors explicitly.
- vs. BlueZ: same object-graph philosophy (Adapter/Device/Service/Characteristic↔Property), plus Firmware1 which BlueZ delegates to external tools.
- vs. earlier Nld report (sync 63): that mapped interfaces; this one adds the daemon lifecycle, Windows transport, and full eRPC service list.
