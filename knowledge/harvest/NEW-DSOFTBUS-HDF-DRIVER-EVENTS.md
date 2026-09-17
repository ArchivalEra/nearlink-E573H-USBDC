---
type: harvest
title: "DSoftBus HDF driver events: delayed binding, coarse notifications and lifecycle boundaries"
language: en
created: 2026-09-17
tags: [harvest, openharmony, dsoftbus, hdf, liteos-a, event-ingress, lifecycle, source-only]
sources:
  - "https://github.com/openharmony/communication_dsoftbus/blob/d15c795b2a309742f9aef52102d1ad1654922409/adapter/common/bus_center/driver/lnn_hdf_driver_monitor.c"
  - "https://github.com/openharmony/communication_dsoftbus/blob/d15c795b2a309742f9aef52102d1ad1654922409/adapter/common/bus_center/driver/lnn_driver_monitor_virtual.c"
  - "https://github.com/openharmony/communication_dsoftbus/blob/d15c795b2a309742f9aef52102d1ad1654922409/adapter/common/bus_center/driver/lnn_hdf_driver_request.c"
  - "https://github.com/openharmony/communication_dsoftbus/blob/d15c795b2a309742f9aef52102d1ad1654922409/adapter/common/bus_center/driver/lnn_driver_request_virtual.c"
  - "https://github.com/openharmony/communication_dsoftbus/blob/d15c795b2a309742f9aef52102d1ad1654922409/adapter/common/bus_center/include/lnn_driver_request.h"
  - "https://github.com/openharmony/communication_dsoftbus/blob/d15c795b2a309742f9aef52102d1ad1654922409/adapter/common/bus_center/include/lnn_event_monitor_impl.h"
  - "https://github.com/openharmony/communication_dsoftbus/blob/d15c795b2a309742f9aef52102d1ad1654922409/adapter/common/bus_center/bus_center_adapter.gni"
trust: A
stale_after: 2027-03-17
---

# DSoftBus HDF driver events: delayed binding, coarse notifications and lifecycle boundaries

## Scope, provenance, and deduplication

This source-only harvest examines one narrow program theme: the bus-center adapter consuming events from the named HDF service `hdf_dsoftbus`. The scope includes its deferred binding, two event conversions, teardown, virtual replacement, and build selection. The two request-named companions were also read to establish the boundary between the visible event consumer and any presumed command channel.

All seven source files listed in frontmatter were read completely. Local Git metadata identifies upstream as `openharmony/communication_dsoftbus` and HEAD as `d15c795b2a309742f9aef52102d1ad1654922409`, with commit subject `!12515 merge fix/epoll-abnormal-event-condition into master`. Each selected working-file blob hash matched its entry in that commit's tree. The URLs are revision-pinned provenance references; they were not fetched. Trust A applies to direct source observations, not demonstrated runtime results.

The available source-tree inventory and the complete harvest index were reviewed first. An initial discovery/CoAP candidate was not pursued because the materialized DSoftBus checkout has no discovery implementation. Knowledge-bundle grep searches for `lnn_hdf_driver`, `lnn_driver_request`, `hdf_dsoftbus`, `LwipMonitorReportInfo`, `ProcessLwipEvent`, `LnnInitDriverMonitorImpl`, both `LNN_DRIVER_MODULE_*` names, and `MAX_BIND_HDF_RETRY_COUNT` found no existing coverage of this module. Broader HDF searches found the separate NearLink HDI/DLI integration research, not this DSoftBus service. The existing Linux link-observation and Wi-Fi common-event reports were read in full as comparisons. Their shared notification endpoints do not cover the HDF binding, report layout, or dispatch semantics examined here.

Every source evidence path below is relative to the upstream repository root. Comparison links assume eventual placement beside the prior harvest reports. This is a standalone draft: no repository file was edited, no source tree was expanded, and no build, network exchange, hardware operation, source-program execution, or child-agent work was performed.

## Executive findings

1. **The real HDF monitor is selected only in the handled liteos_a branch of this build fragment.** With `dsoftbus_feature_lnn_frame` enabled, liteos_m, lite Linux, and non-lite branches instead select the virtual driver monitor. The virtual initializer returns `SOFTBUS_OK` without binding or installing an event listener. These are conditional source selections, not a deployed-product inventory. Evidence: `adapter/common/bus_center/bus_center_adapter.gni:25-86,107-115`; `adapter/common/bus_center/driver/lnn_driver_monitor_virtual.c:20-27`.
2. **Real initialization reports delayed-work submission, not driver readiness.** It installs the receive function pointer and returns the delayed-helper result. Binding and event-listener registration occur inside a later callback. A function-static budget permits at most ten binding attempts under a fresh, serial callback sequence; neither initialization nor deinitialization resets it. Evidence: `adapter/common/bus_center/driver/lnn_hdf_driver_monitor.c:30-33,122-163`.
3. **The LWIP branch reduces a structured report to an interface-name notification.** It requires an exact native `sizeof(LwipMonitorReportInfo)` buffer, classifies the interface, and calls `LnnNotifyAddressChangedEvent` only for ETH or WLAN. The report's `event` is logged but does not choose an action; `extInfo.status` is not consulted. No actual address or link-up boolean is forwarded at this call site. Evidence: `adapter/common/bus_center/driver/lnn_hdf_driver_monitor.c:40-78`.
4. **The WLAN branch submits unknown state rather than decoding its payload.** For a non-null incoming sbuf and the WLAN module ID, the code allocates a `SoftBusWifiState`, sets `SOFTBUS_WIFI_UNKNOWN`, and requests the default-looper WLAN callback. This differs from the separate Wi-Fi common-event adapter's recognized-code mappings. The HDF producer does not establish what the downstream callback does with unknown state. Evidence: `adapter/common/bus_center/driver/lnn_hdf_driver_monitor.c:80-120`; `adapter/common/bus_center/include/lnn_driver_request.h:27-31`.
5. **Callback success does not certify that a notification was submitted or consumed.** Except for a null sbuf, the outer receive function returns `SOFTBUS_OK`, including after ignored module IDs or inner parsing, allocation, classification, or scheduling failures. Teardown recycles a stored service pointer but shows no explicit queued-work cancellation or listener-unregister call. Evidence: `adapter/common/bus_center/driver/lnn_hdf_driver_monitor.c:50-120,155-163`.

## Service binding and retry contract

`HdfDriverEventCtrl` stores one `HdfIoService *` and one `HdfDevEventlistener` in the static `g_driverCtrl`. `LnnInitDriverMonitorImpl()` assigns `OnReceiveDriverEvent` to `eventListener.onReceive`, then submits `DelayInitFunction` with a null parameter to `GetLooper(LOOP_TYPE_DEFAULT)`. The delay argument is `BIND_HDF_DELAY`, defined as 1000. The helper implementation was not inspected, so this report does not assign a time unit or claim a measured deadline. Evidence: `adapter/common/bus_center/driver/lnn_hdf_driver_monitor.c:30-38,48,148-153`.

The deferred function's visible decision sequence is:

| Condition | Source action | Counter and continuation |
|---|---|---|
| `retry >= 10` | Return before binding | No new callback requested here |
| `HdfIoServiceBind("hdf_dsoftbus")` returns null | Log and request another delayed callback | Increment `retry`, then return |
| Binding returns a service but listener registration returns non-OK | Recycle the service, clear the global pointer, request another delayed callback | Increment `retry` at the common tail |
| Listener registration returns OK | Keep the service pointer | Increment `retry`; no continuation requested by this attempt |

Evidence: `adapter/common/bus_center/driver/lnn_hdf_driver_monitor.c:122-146`.

With a fresh counter and serial callbacks, values 0 through 9 each permit one bind attempt. Continuous failure requests one further callback after the tenth attempt; that callback would encounter the guard and return without binding, assuming the helper schedules it. Both retry-helper return values are ignored. Thus the source's retry limit is not a promise that ten attempts actually occur, nor that exhaustion is reported to the original initializer's caller. Successful registration also consumes one counter increment. Evidence: `adapter/common/bus_center/driver/lnn_hdf_driver_monitor.c:128-145`.

The counter is function-static, rather than part of a resettable monitor instance. There is no visible already-initialized check before another initializer call queues work; the callback assigns a new bind result directly into the global pointer. These observations identify lifecycle questions for integration, not demonstrated duplicate registrations or resource failures. Whether the looper serializes calls, whether callers initialize only once, and how HDF handles repeated binds were not established. Evidence: `adapter/common/bus_center/driver/lnn_hdf_driver_monitor.c:125-152`.

## Incoming event and payload contracts

### Module selection and outer return value

`LnnDriverModuleId` assigns WLAN parameter events value 0, LWIP monitor events value 1, and the maximum-index sentinel value 2. The receive function ignores the listener and service arguments, rejects null `data` with `SOFTBUS_INVALID_PARAM`, and returns OK without dispatch for `moduleId >= LNN_DRIVER_MODULE_MAX_INDEX`. Otherwise its switch calls one of two void processors. There is no propagated processor result. Evidence: `adapter/common/bus_center/include/lnn_driver_request.h:27-31`; `adapter/common/bus_center/driver/lnn_hdf_driver_monitor.c:96-120`.

An integrator should therefore read this outer OK as the callback's coarse acceptance/ignore convention, not an end-to-end delivery acknowledgment. In particular, the same OK return follows the LWIP processor's early returns and the WLAN processor's allocation or scheduling failure paths. Evidence: `adapter/common/bus_center/driver/lnn_hdf_driver_monitor.c:50-120`.

### LWIP: native-layout input, coarse output

The local report declaration contains:

| Field | Declared representation | Observed use |
|---|---|---|
| `ifName` | `char[16]` | Logging, interface classification, and address-change callback argument |
| `event` | `uint32_t` | Logging only |
| `extInfo.status` | `int32_t` member of a union | Not read by the processor |

Evidence: `adapter/common/bus_center/driver/lnn_hdf_driver_monitor.c:31,40-46,50-78`.

`ProcessLwipEvent()` asks `HdfSbufReadBuffer` for a buffer pointer and length. Failure returns immediately. It then requires a non-null pointer and length exactly equal to `sizeof(LwipMonitorReportInfo)` before casting the buffer to that type. This is a native-structure consumer contract, not an explicitly serialized, fixed-endian protocol definition. No packing directive, byte-order conversion, or independent numeric payload-size constant appears in the selected file. A producer's layout agreement and the HDF buffer contract remain external dependencies. Evidence: `adapter/common/bus_center/driver/lnn_hdf_driver_monitor.c:40-67`.

After logging, the processor asks `LnnGetNetIfTypeByName` to classify `info->ifName`. Classification failure returns. Only `LNN_NETIF_TYPE_ETH` and `LNN_NETIF_TYPE_WLAN` lead to `LnnNotifyAddressChangedEvent(info->ifName)`; other successful classifications do not. No branch tests the event value or status. The call therefore expresses a coarse interface-address-change indication in this consumer, not a proven transition to connected, disconnected, or IP-ready. Evidence: `adapter/common/bus_center/driver/lnn_hdf_driver_monitor.c:69-77`.

The processor neither copies the name to its own storage nor frees the sbuf buffer. It passes a pointer into the incoming record to the notification function. Buffer lifetime, whether the notification copies its argument synchronously, and the producer's string-termination convention were not traced. These are ordinary ABI and ownership boundaries to resolve before reuse, not runtime failure findings. Evidence: `adapter/common/bus_center/driver/lnn_hdf_driver_monitor.c:56-77`.

### WLAN: payload-independent unknown-state submission

`ProcessWlanEvent()` does not read its `data` argument. It allocates exactly `sizeof(SoftBusWifiState)`, returns on allocation failure, writes `SOFTBUS_WIFI_UNKNOWN`, and calls `LnnAsyncCallbackHelper` with the default looper, `LnnNotifyWlanStateChangeEvent`, and the allocated pointer cast to `void *`. A helper error triggers a log and `SoftBusFree`; a successful helper return has no producer-side free. Evidence: `adapter/common/bus_center/driver/lnn_hdf_driver_monitor.c:80-94`.

The source consequently does not support interpreting a WLAN module event as an enabled, connected, or disconnected value. Even though the payload is unused by this processor, the outer receive function still requires a non-null sbuf. Successful asynchronous submission is consistent with handing off a heap payload; it does not prove copying, delivery, downstream reevaluation, or eventual deallocation. Evidence: `adapter/common/bus_center/driver/lnn_hdf_driver_monitor.c:80-120`.

## Teardown and request-side evidence boundary

`LnnDeinitDriverMonitorImpl()` returns if the saved service pointer is null. Otherwise it logs, calls `HdfIoServiceRecycle`, and clears the pointer. There is no explicit unregister, retry-counter reset, cancellation token, stopped flag, or cancellation of pending delayed callbacks in this file. Recycling may perform framework cleanup, but that implementation was not read. If a callback remains pending, this file alone offers no stopped-state guard preventing a subsequent bind attempt; actual post-deinit scheduling was not exercised. Evidence: `adapter/common/bus_center/driver/lnn_hdf_driver_monitor.c:122-163`.

The shared monitor header declares initialization and deinitialization, but no HDF-ready query, readiness callback, or retry-status API. This is a statement about this header's surface, not proof that no diagnostic exists elsewhere. Evidence: `adapter/common/bus_center/include/lnn_event_monitor_impl.h:27-59`.

The request-named companions do not establish an active command path at this revision:

- `lnn_hdf_driver_request.c` defines `DRIVER_SERVICE_NAME` and a static `ParseReply` helper, but contains no caller of that helper and no exported request function. The helper checks the reply pointer, reads one buffer, rejects a response larger than the supplied capacity, and uses `memcpy_s`. It returns a status, not the copied length. This is dormant parser source within the read file, not evidence of a dispatched HDF command. Evidence: `adapter/common/bus_center/driver/lnn_hdf_driver_request.c:16-49`.
- The corresponding header supplies only the module-ID enum, with no request prototype; the virtual request file has includes but no functions. Evidence: `adapter/common/bus_center/include/lnn_driver_request.h:16-39`; `adapter/common/bus_center/driver/lnn_driver_request_virtual.c:1-19`.

These observations prevent extending an event-monitor finding into an unsupported bidirectional driver protocol claim.

## Build-selected availability

The inspected build fragment uses the condition `defined(ohos_lite)`, not the boolean value of that variable. Within the handled lite branches, the LNN frame feature gates the driver source additions. Evidence: `adapter/common/bus_center/bus_center_adapter.gni:25-86`.

| Branch, with LNN frame enabled | Driver monitor and request companions selected |
|---|---|
| Lite, `liteos_m` | Virtual monitor and virtual request |
| Lite, `linux` | Virtual monitor and virtual request |
| Lite, `liteos_a` | HDF monitor and HDF request |
| Non-lite | Virtual monitor and virtual request |

Evidence: `adapter/common/bus_center/bus_center_adapter.gni:25-86,107-115`.

The liteos_a branch also declares HDF include paths and, inside the LNN frame condition, dependencies on `hdf_core`, `hdf_platform`, and `hdf_posix_osal`. This fragment selects virtual Wi-Fi-service and netlink monitors for that same branch, illustrating why this HDF ingress is a separate platform choice rather than an additional Linux rtnetlink transport. Imported settings and final product composition were not evaluated. Evidence: `adapter/common/bus_center/bus_center_adapter.gni:14,56-85`.

The virtual driver monitor's initializer is a direct OK return and its deinitializer is empty. Therefore the API's apparent success must be interpreted alongside the selected implementation. Neither virtual success nor successful scheduling in the real initializer demonstrates a live HDF event subscription. Evidence: `adapter/common/bus_center/driver/lnn_driver_monitor_virtual.c:20-27`; `adapter/common/bus_center/driver/lnn_hdf_driver_monitor.c:148-153`.

## Unverified runtime boundaries

- The producer of `hdf_dsoftbus` reports was not inspected. Actual event meanings, payload layout across ABIs, report frequency, service startup order, and producer string conventions remain unverified.
- HDF bind, register, recycle, and sbuf implementations were outside the read set. Service lifetime, implicit unregistration, callback serialization, buffer ownership, service-death recovery, and repeated-bind behavior cannot be established from these call sites.
- The looper helpers and both downstream notification implementations were not read. Delay units, cancellation, queue order, payload cleanup, name copying, and the meaning of an unknown WLAN state remain unresolved.
- No initial interface or WLAN-state query is visible in this monitor. That does not exclude snapshots or resynchronization elsewhere in DSoftBus.
- No build or test was run. Conditional source lists do not identify a deployed device configuration, and source-level retry arithmetic is not an observed timing result.
- Nothing in this read set establishes NearLink SLE data transport, DLI framing, USB binding, peer discovery, authentication, or session readiness. The relevance is platform-event integration, not a WS73 driver implementation.

## Reusable lessons

1. Distinguish no-op availability, work submission, service binding, listener registration, and downstream delivery in monitor status. The source uses success returns at several different layers with different meanings.
2. When reusing delayed setup, make retry-budget lifetime and stop/cancel ownership explicit. A static attempt count and a recycled pointer do not by themselves describe a restartable monitor lifecycle.
3. Document event information loss. This consumer ignores LWIP event/status fields and submits an unknown WLAN enum; a consumer should not silently promote either into a complete connectivity state.
4. Keep native report layout and pointer lifetime visible in adapter contracts. An exact `sizeof` check identifies one agreement boundary, but it does not document serialization or retention semantics.
5. Separate ordinary platform event ingress from the radio command/data plane. A driver-related filename, service binding, and an unused reply parser are not sufficient evidence of an implemented transport.

These are source-informed integration lessons, not implemented changes or validated porting instructions.

## Comparison anchors

- [DSoftBus Linux link observation](NEW-DSOFTBUS-LINK-OBSERVATION.md): covers rtnetlink subscriptions and the carrier-only readiness probe. This draft adds the distinct liteos_a HDF service binding and native LWIP report consumer, including its event/status information loss and retry/teardown contract. The common address-change endpoint does not make their input protocols interchangeable.
- [DSoftBus Wi-Fi common-event ingress](NEW-DSOFTBUS-WIFI-EVENT-INGRESS.md): covers five common-event subscriptions, recognized state mappings, and initial queries. This draft instead establishes that the HDF WLAN processor does not decode its input and explicitly submits unknown state. Both have asynchronous handoff boundaries, but their event sources and submitted state vocabularies differ.
