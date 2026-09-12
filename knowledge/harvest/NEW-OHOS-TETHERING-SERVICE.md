---
type: harvest
title: xingkaiyueying/tethering_nearlink — OHOS NearLink Tethering Service (SLE Port Profile + Local Socket Data Plane)
language: en
created: 2026-09-13
tags: [ohos, nearlink, sle, tethering, socket, port-profile, ssap, datatransfer, harvest]
sources:
  - url: https://github.com/xingkaiyueying/tethering_nearlink
    note: cloned 2026-09-13, pushed 2026-09-11 (fresh, not archived), C++, ~5MB, OHOS subsystem layout
trust: verified
stale_after: 2026-12-13
---

# xingkaiyueying/tethering_nearlink — OHOS NearLink Tethering Service (SLE Port Profile + Local Socket Data Plane)

- Inspection date: 2026-09-13 (staleness check: pushed 2026-09-11, not archived — ALIVE)
- Source root: `/mnt/hdd/nearlink-stuff/tethering_nearlink`
- Mode: read-only local program inspection; no network, build, hardware, or PCB access
- Scope: tethering architecture, SLE port profile, socket data plane, datatransfer service, app-to-device mapping

## Executive findings

1. This is an OpenHarmony-style NearLink service subsystem implementing network sharing ("xingqiu wangluo gongxiang" - NearLink network sharing) as a three-layer pipeline: app-facing local sockets → `SleDataTransferService` → SLE Port Profile over SSAP → the prebuilt NearLink stack. The repo carries only stack EXT headers plus version scripts/blocklists for `libnearlink_stack`, linking the vendor stack rather than vendoring it. [`services/stack/` layout: `BUILD.gn`, `libnearlink_stack.versionscript`, `include/ext/nlstk_*_ext.h`]
2. The local-socket data plane is an epoll + non-blocking-fd pump: `PortInfo(port, address, mtu, fd)` wraps each accepted socket with `InputStream`/`OutputStream` pairs; buffers are 50 KB with a 40 KB report threshold; `SetNonblock` is applied via `fcntl`. [`socket/nearlink_socket_manager.h:10-40`]
3. `OutputStream::Write` returns a 4-state `SocketTransState` — SUCCESS / INTERNAL_FAULT / CACHE_FULL / INVALID_PARAM — making CACHE_FULL an explicit backpressure signal rather than an exception path. [`socket/nearlink_socket_outputstream.h:11-27`]
4. The SLE transport is a **Port Profile** (`cp/bal/profile/port/`) built directly on SSAP client types: a port is identified by the triple (portId, manufactureId, `NLSTK_SsapUuid_S uuid`), with per-address connection-state and per-address connected-device-count queries — i.e., a serial-port-like transparent byte transport layered over SSAP, not GATT-style per-characteristic access. [`services/stack/src/cp/bal/profile/port/src/port_type.h:12-30`]
5. `SleDataTransferService` (1462 lines) is the bridge daemon: it maintains a safe map of app connect params keyed by local port → `SleDataTransferCache` (`InsertAppConnectParamMapping(uuid, port, tokenId, uid, pid)`), resolves ports by UUID+tokenId (`FindPortByUuidAndTokenId`), observes SLE ACL state via `ISlePeripheralCallback::OnAcbStateChanged`, routes on `GetRemotePortByConnectionState(addr, state, oldState)`, and drives a `PortService` through a `PortServiceObserver`. [`services/service/src/datatransfer/nearlink_sle_datatransfer_service.cpp:68-180, 311, 417-427`]
6. The service caches per-app send state in `SleDataTransferCache` entries and freezes/restores app state via `NotifyRssAppStateChanged(tokenId, uid, isFreeze)` — a per-app flow-control hook keyed by OHOS tokenId/uid/pid. [`nearlink_sle_datatransfer_service.cpp:163-179`]
7. A vehicle-infotainment path exists: `ConnectCarReqStateChanged(addr)` handles car connection requests, and IPC parsing of `GetPortConnects()` lists suggests multi-device port sharing. [`nearlink_sle_datatransfer_service.cpp:160, 208, 256`]
8. OHOS hygiene surfaces are present and reusable as templates: `hisysevent.yaml` + `hisysevent_ue.yaml` (structured events), `sa_profile/` (SA registration), `ipc_parcel/`, `test/fuzztest/` + `test/unittest/`, and `nearlink_feature.gni`/`nearlink_config.gni` build-time feature gates. [repo root]

## Boundaries and gaps

- No actual IP forwarding/packet parsing was found in the socket layer — it moves raw bytes with port/MTU metadata; any real "network sharing" (NAT/DHCP/relay) lives outside this repo.
- The stack itself is linked prebuilt (`libnearlink_stack`); the EXT headers expose extension APIs but the port profile sources under `services/stack/src/cp/` are partial in-repo fragments, not a buildable full stack.
- MTU handling is metadata-only in the socket layer (PortInfo carries mtu); no segmentation logic was inspected in the port profile fragments.
- Single upstream, zero stars, no CI; treat as a working vendor/community service tree, not a hardened product.

## Reusable for our stack

- SLE Port Profile pattern: (portId, manufacturerId, UUID) triple + per-address state machine over SSAP is a clean alternative to per-characteristic GATT-style streaming for our WS73 host stack — matches the transparent-pipe need of the E573H dongle use case.
- CACHE_FULL as an explicit enum return in stream writes is directly portable to our SSAP/transport write paths (we already gate with pending-frame + MAX_INFLIGHT credit; this adds a named backpressure state).
- Per-app cache + tokenId/uid/pid mapping is a multi-tenant session pattern for a shared daemon serving multiple local apps over one SLE adapter.
- The hisysevent YAML + SA profile + feature-gate GNI layout is a ready template if we ever package our daemon as an OHOS service.

## Comparison anchors (vs existing reports)

- `OHOS-FRAMEWORK-LAYER.md` / `OHOS-SA-SERVICE.md`: those covered the NearLink service/SA surfaces of the main stack repo; this repo is the sibling tethering service with its own SA and a socket data plane absent from both.
- `NEW-OHOS-NEARLINK-SEPT-INCREMENT.md`: the main `communication_nearlink_service` repo got frame-4 antenna pinning and fuzzers in the same window; this tethering tree stayed quieter (pushed 2026-09-11) but carries the only in-repo SLE Port Profile implementation we have seen.
- `SSAP-DIALECT-COMPARISON.md`: the Port Profile confirms a second SSAP consumer dialect beyond client/server app APIs — profile-over-SSAP reuse, consistent with that report's dialect mapping.
- `SLE-UART-VARIANTS.md`: the Port Profile is the OHOS-side analogue of the SLE UART/transparent-transport variants digested earlier — same byte-pipe intent, realized as a typed port service.
