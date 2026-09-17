---
type: harvest
title: "DSoftBus Wi-Fi event ingress: common-event subscriptions, state notifications, and virtual build selection"
language: en
created: 2026-09-17
tags: [harvest, openharmony, dsoftbus, wifi, bus-center, common-events, lifecycle, source-only]
sources:
  - "https://github.com/openharmony/communication_dsoftbus/blob/d15c795b2a309742f9aef52102d1ad1654922409/adapter/common/bus_center/wlan/lnn_wifiservice_monitor.cpp"
  - "https://github.com/openharmony/communication_dsoftbus/blob/d15c795b2a309742f9aef52102d1ad1654922409/adapter/common/bus_center/wlan/lnn_wifiservice_monitor_virtual.cpp"
  - "https://github.com/openharmony/communication_dsoftbus/blob/d15c795b2a309742f9aef52102d1ad1654922409/adapter/common/bus_center/include/lnn_event_monitor_impl.h"
  - "https://github.com/openharmony/communication_dsoftbus/blob/d15c795b2a309742f9aef52102d1ad1654922409/adapter/common/bus_center/bus_center_adapter.gni"
trust: A
stale_after: 2027-03-17
---

# DSoftBus Wi-Fi event ingress: common-event subscriptions, state notifications, and virtual build selection

## Scope, provenance, and deduplication

One coherent program theme is examined: the Wi-Fi service monitor's subscription to OpenHarmony common events, conversion of status codes into bus-center WLAN-state notifications, initial status sampling, and real/virtual build selection. All four pinned files above were read in full. The inspected source revision is `d15c795b2a309742f9aef52102d1ad1654922409`; a scoped, read-only comparison of these files against that revision produced no differences. Trust A covers direct source observations. Integration lessons are deductions, not verified runtime results.

Knowledge-bundle deduplication preceded source selection and included searches for Wi-Fi/WLAN, DSoftBus, `wifiservice_monitor`, `SubscribeWifi`, `SEMI_STATE`, `SOFTBUS_WIFI_OBTAINING_IPADDR`, and Wi-Fi event/notification/subscription wording. No existing report found covers this adapter's subscription-to-notification contract. Related Wi-Fi callback coverage concerns the separate WS63 service layer. Both existing DSoftBus reports were read as comparison anchors: one covers Linux rtnetlink/carrier observation, the other SLE scaffolding and a transport stub. Neither is the subject of this harvest.

All abbreviated source evidence paths below are relative to `adapter/common/bus_center/` in the pinned repository. Relative prior-report links assume eventual placement in `knowledge/harvest/`.

No source was imported, built, or executed. No network or hardware interaction was performed. The scope ends at the asynchronous notification call sites: the common-event manager, looper helpers, Wi-Fi query providers, and WLAN notification implementation were not inspected. Searches of the available source files did not locate the notification or asynchronous-helper definitions; the source selection was not expanded or fetched.

## Executive findings

1. **Five event families feed a single WLAN-state callback.** Connection, power, hotspot, semi-active, and P2P-connection codes are mapped to selected `SoftBusWifiState` values. Unrecognized codes remain unknown and produce no queued notification. Recognized events allocate a state value and submit `LnnNotifyWlanStateChangeEvent` through the default looper. This is status ingress, not a session or transport implementation. [`wlan/lnn_wifiservice_monitor.cpp:48-158`]
2. **Initialization success is scheduling success, not subscription readiness.** `LnnInitWifiServiceMonitorImpl()` returns the delayed-helper result; the five subscriptions occur later and short-circuit in connection, power, hotspot, semi, P2P order. Only an attempt reporting success for all five invokes the two initial status helpers. [`wlan/lnn_wifiservice_monitor.cpp:282-322`]
3. **The retry budget is persistent and the sequence is not transactional.** The callback uses a function-static counter, rejects only `retry > 20`, and increments on a subscription-chain failure. A fresh, continuously failing chain can execute 21 attempts before the next callback returns without subscribing. Earlier successful subscriptions are not rolled back in this file before a retry restarts the sequence. Duplicate delivery is a possibility requiring framework verification, not a demonstrated outcome. [`wlan/lnn_wifiservice_monitor.cpp:169-230,282-312`]
4. **Initial sampling is narrower than the live event vocabulary.** After subscription success, the adapter samples enabled/disabled and connected/disconnected only. A successful link query maps any returned state other than `SOFTBUS_API_WIFI_DISCONNECTED` to connected. No initial hotspot, semi-active, P2P, or obtaining-IP notification is synthesized here. Despite their names, the capability helpers only submit notifications; no ledger update is visible in them. [`wlan/lnn_wifiservice_monitor.cpp:234-280,299-302`]
5. **A Linux kernel does not imply this real monitor is selected.** All three handled lite branches, including lite Linux, select the virtual Wi-Fi monitor when the LNN frame feature is enabled. The non-lite branch selects the real file only with that feature plus Wi-Fi-service dependence and common-event-service availability. The virtual initializer simply returns `SOFTBUS_OK` and installs nothing. [`bus_center_adapter.gni:25-86,107-128`; `wlan/lnn_wifiservice_monitor_virtual.cpp:16-23`]

## Subscription and startup contract

### Delayed setup and five independent subscriptions

The real initializer obtains `GetLooper(LOOP_TYPE_DEFAULT)` and passes `LnnSubscribeWifiService`, a null parameter, and `DELAY_LEN` to `LnnAsyncCallbackDelayHelper`. `DELAY_LEN` is 1000; the selected file does not define the helper's time unit or execution guarantees, so this is a requested delay argument, not a measured wall-clock deadline. Scheduling failure is logged and returned. There is no synchronous common-event registration in the initializer. [`wlan/lnn_wifiservice_monitor.cpp:29-32,314-322`]

Each subscription method creates its own `MatchingSkills`, matches one action, constructs `CommonEventSubscribeInfo`, creates a separate `shared_ptr<WifiServiceMonitor>`, and calls `CommonEventManager::SubscribeCommonEvent`. A false result maps to `SOFTBUS_NETWORK_SUBSCRIBE_COMMON_EVENT_FAILED`; otherwise the method returns `SOFTBUS_OK`. The semi-state action is the literal `usual.event.wifi.SEMI_STATE` and alone explicitly sets publisher UID 1010 in this file. The other four do not explicitly set a publisher UID here; that observation does not establish the framework's default publisher policy. [`wlan/lnn_wifiservice_monitor.cpp:31-32,169-230`]

| Attempt order | Subscription method | Matched event |
|---|---|---|
| 1 | `SubscribeWifiConnStateEvent` | `COMMON_EVENT_WIFI_CONN_STATE` |
| 2 | `SubscribeWifiPowerStateEvent` | `COMMON_EVENT_WIFI_POWER_STATE` |
| 3 | `SubscribeAPConnStateEvent` | `COMMON_EVENT_WIFI_HOTSPOT_STATE` |
| 4 | `SubscribeWifiSemiStateEvent` | Literal `usual.event.wifi.SEMI_STATE` |
| 5 | `SubscribeWifiP2pConnStateEvent` | `COMMON_EVENT_WIFI_P2P_CONN_STATE` |

Evidence: `wlan/lnn_wifiservice_monitor.cpp:169-230,295-299`. The AP method's name does not mean it subscribes to individual station arrivals: its matched action is hotspot state.

### Retry and partial registration

The `&&` chain stops at its first unsuccessful method. Later methods are skipped on that attempt. Any earlier successful calls are not undone by a corresponding unsubscribe operation in the selected implementation. The next retry starts again at connection-state subscription, rather than resuming at the failed method. [`wlan/lnn_wifiservice_monitor.cpp:295-309`]

The counter starts at zero, increments once per failed subscription chain, and is never reset by success or by `LnnInitWifiServiceMonitorImpl()`. With a fresh counter and persistent subscription failure, values 0 through 20 pass the guard: 21 attempts are possible, each provided its scheduling path runs. The failure at value 20 increments it to 21 and requests another callback, which then returns at the guard. If rescheduling fails, that chain stops there. Allocation failure as represented by the null check returns without incrementing or rescheduling. The code uses ordinary C++ `new`; its exception/allocation configuration was not examined, so the null-check branch is not proof of exhaustive allocation-failure handling. [`wlan/lnn_wifiservice_monitor.cpp:282-312`]

This is a process-lifetime static budget in ordinary execution, not an explicitly per-initialization retry budget. Repeated initialization can request additional delayed callbacks, and no idempotence guard or cancellation handle is visible. Framework retention, duplicate-subscription policy, and callback scheduling must be checked before concluding that partial success or repeated initialization actually produces duplicate notifications. [`wlan/lnn_wifiservice_monitor.cpp:169-230,282-322`]

## Event-to-status translation

Every received event reads `GetCode()` and `GetWant().GetAction()`, initializes a fresh local state to `SOFTBUS_WIFI_UNKNOWN`, then tests the action against the five event families. The conditions are independent `if` statements. Each mapping helper changes the state only for its listed cases; its default branch leaves the value unchanged. [`wlan/lnn_wifiservice_monitor.cpp:48-144`]

| Action family | Accepted platform code | Queued SoftBus state | Evidence |
|---|---|---|---|
| Connection | `ConnState::OBTAINING_IPADDR` | `SOFTBUS_WIFI_OBTAINING_IPADDR` | `wlan/lnn_wifiservice_monitor.cpp:48-64` |
| Connection | `ConnState::CONNECTED` | `SOFTBUS_WIFI_CONNECTED` | Same |
| Connection | `ConnState::DISCONNECTED` | `SOFTBUS_WIFI_DISCONNECTED` | Same |
| Power | `WifiState::DISABLED` / `ENABLED` | `SOFTBUS_WIFI_DISABLED` / `SOFTBUS_WIFI_ENABLED` | `wlan/lnn_wifiservice_monitor.cpp:66-79` |
| Hotspot | `ApState::AP_STATE_STARTED` / `AP_STATE_CLOSED` | `SOFTBUS_AP_ENABLED` / `SOFTBUS_AP_DISABLED` | `wlan/lnn_wifiservice_monitor.cpp:81-94` |
| Semi | `WifiDetailState::STATE_SEMI_ACTIVE` | `SOFTBUS_WIFI_SEMI_ACTIVE` | `wlan/lnn_wifiservice_monitor.cpp:96-106` |
| P2P connection | `P2pConnectedState::P2P_CONNECTED` / `P2P_DISCONNECTED` | `SOFTBUS_WIFI_P2P_CONNECTED` / `SOFTBUS_WIFI_P2P_DISCONNECTED` | `wlan/lnn_wifiservice_monitor.cpp:108-121` |

Other action/code combinations do not result in notification while the state remains unknown. No local previous-state comparison, debounce, or state-transition validation appears: repeated recognized input can submit repeated states. Only the enum value is forwarded; the action string, original code, interface name, address, peer identity, and full event object are not part of the submitted payload. Numeric enum encodings are not established by the selected files. [`wlan/lnn_wifiservice_monitor.cpp:123-158`]

### Initial status sampling after complete subscription success

`UpdateLocalWifiActiveCapability()` allocates a state, calls `SoftBusIsWifiActive()`, maps false to disabled and true to enabled, then requests the same asynchronous WLAN-state callback used by live events. This boolean call site cannot itself distinguish a genuine disabled state from any provider-specific failure encoded as false. The provider's semantics remain unverified. [`wlan/lnn_wifiservice_monitor.cpp:234-253`]

`UpdateLocalWifiConnCapability()` allocates a state, zeroes a `SoftBusWifiLinkedInfo` object with `memset_s` while discarding that function's return, and calls `SoftBusGetLinkedInfo`. Query failure frees the allocation and returns without a notification. Query success maps exactly disconnected to disconnected and every other `connState` value to connected. This is a coarser conversion than the live connection mapping, which preserves obtaining-IP as a separate state. It is not evidence of a usable IP address or SoftBus session. [`wlan/lnn_wifiservice_monitor.cpp:255-280`]

The active helper is invoked before the connection helper, but both are void and handle their own failures; failure in the first does not prevent the second call. Their errors do not trigger subscription retries. The queries are separate, happen after registrations, and use the same notification callback as live events without a snapshot marker. The file establishes call order, not an atomic snapshot, event-versus-snapshot ordering, or end-to-end delivery order. [`wlan/lnn_wifiservice_monitor.cpp:234-280,295-309`]

## Payload and subscriber lifetime boundaries

- **Per-notification allocation:** live events and both initial helpers allocate `sizeof(SoftBusWifiState)` using `SoftBusMalloc`. Allocation failure stops that notification. On asynchronous-helper failure, the producer frees the allocation. The initial connection helper also frees it if the link query fails. [`wlan/lnn_wifiservice_monitor.cpp:144-156,234-280`]
- **Successful asynchronous submission:** the producer does not free the payload after a successful helper return. It passes a heap pointer as `void *`, not a pointer to its stack-local enum or the incoming event. This is consistent with handing cleanup responsibility downstream, but neither copying nor eventual freeing by the helper/callback was verified. A leak or safe end-to-end ownership contract cannot be concluded from these call sites alone. [`wlan/lnn_wifiservice_monitor.cpp:145-156,247-252,274-279`]
- **Subscriber objects:** each method's local `shared_ptr<WifiServiceMonitor>` leaves scope after registration. Continuing subscription therefore depends on the common-event framework's ownership/retention contract, which is outside the read set. The destructor here is empty. [`wlan/lnn_wifiservice_monitor.cpp:36-46,169-230`]
- **Helper versus subscriber:** the `SubscribeEvent` object has only subscription methods and no stored subscriber fields. Deleting it at the end of an attempt is not an explicit unsubscribe operation and should not be confused with destroying framework-retained monitors. [`wlan/lnn_wifiservice_monitor.cpp:160-167,290-311`]
- **Shutdown:** the selected real file has no unsubscribe path, saved monitor handles, callback cancellation, or Wi-Fi deinitializer. The shared header declares Wi-Fi initialization but no corresponding Wi-Fi deinitialization function, while declaring several other monitor deinitializers. Whole-process cleanup or external lifecycle handling remains unverified. [`wlan/lnn_wifiservice_monitor.cpp:16-322`; `include/lnn_event_monitor_impl.h:27-57`]

## Build selection and virtual semantics

The following table describes only `bus_center_adapter.gni`, not an evaluated product build. The outer test is literally `defined(ohos_lite)`, not a boolean-value test of that variable. [`bus_center_adapter.gni:25-26`]

| Branch and conditions | Wi-Fi monitor selected by this fragment | Evidence |
|---|---|---|
| Lite, `liteos_m`, LNN frame enabled | Virtual | `bus_center_adapter.gni:25-40` |
| Lite, `linux`, LNN frame enabled | Virtual | `bus_center_adapter.gni:41-55` |
| Lite, `liteos_a`, LNN frame enabled | Virtual | `bus_center_adapter.gni:56-86` |
| Non-lite, LNN frame enabled, Wi-Fi-service dependence and CES present | Real | `bus_center_adapter.gni:107-122` |
| Non-lite, LNN frame enabled, either of those two conditions absent | Virtual | `bus_center_adapter.gni:116-128` |
| LNN frame disabled | Neither Wi-Fi monitor added by these branches | `bus_center_adapter.gni:25-86,107-128` |

For the real non-lite path, the Wi-Fi branch adds `ability_base:want` and `wifi:wifi_sdk`; the earlier `has_ces_part` branch adds `common_event_service:cesfwk_innerkits` among its external dependencies. These are declared dependencies, not evidence that a specific runtime service is present or ready. Imported settings and final product arguments were not evaluated. [`bus_center_adapter.gni:14,89-106,116-122`]

The virtual implementation exports the same initializer but returns `SOFTBUS_OK` without obtaining a looper, subscribing, querying status, or notifying bus center. Thus there are three materially different facts an integrator must distinguish: a virtual initializer returning success, a real initializer successfully scheduling setup, and an eventual successful registration attempt. None by itself proves delivered notifications or connected peers. [`wlan/lnn_wifiservice_monitor_virtual.cpp:16-23`; `wlan/lnn_wifiservice_monitor.cpp:295-322`]

## Unverified runtime boundaries

- Common-event publication, subscription retention, publisher filtering behavior, duplicate-registration handling, service restart recovery, and callback threading were not exercised or traced through the framework.
- Default-looper availability, delay units and timing, queue ordering, successful payload cleanup, and shutdown handling were not established by the selected files.
- No downstream listener, ledger mutation, capability broadcast, reconnect policy, authentication, or session outcome was demonstrated. Function names containing `Capability` do not establish such effects.
- The boolean Wi-Fi-active provider and linked-info provider were not read. Their error behavior and completeness cannot be inferred from the consumer's two-state conversion.
- The build fragment does not establish deployed feature values. In particular, the lite Linux selection is virtual for this Wi-Fi common-event monitor even though the prior report found real rtnetlink observation in that branch.
- This is ordinary platform integration evidence, not a NearLink/USB transport implementation or hardware validation. No security assessment is intended.

## Reusable integration lessons

1. Separate monitor availability, setup scheduling, subscription readiness, sampled radio state, and peer/session readiness in integration status. A single successful initializer return cannot encode all of them here.
2. Preserve the distinction between power, station connection, hotspot, semi-active, and P2P states. A single connected boolean would discard vocabulary that the live adapter deliberately keeps.
3. Treat initial samples and live events as separate sources of observations even when they share a callback. Document ordering and reconciliation instead of assuming the two queries form an atomic initial state.
4. Before reusing the retry shape, define ownership of successful partial registrations, repeated-init behavior, subscriber retention, and payload cleanup. The selected adapter leaves those contracts partly to external components.
5. Keep build-selected no-op behavior visible in integration diagnostics. The existing real/virtual source boundary is reusable, but its common success code is not proof that observation is enabled.

These are source-informed design lessons only; no implementation change or port was made.

## Comparison anchors

- [DSoftBus Linux link observation](NEW-DSOFTBUS-LINK-OBSERVATION.md) covers `NETLINK_ROUTE` notifications and a carrier probe. This report instead covers OpenHarmony common-event registration, Wi-Fi status conversion, startup queries, and asynchronous payload/subscriber lifetime. It does not re-audit the rtnetlink or linkwatch files. The virtual choice for lite Linux is specific to the Wi-Fi service monitor, not a contradiction of that report's real Linux netlink selection.
- [DSoftBus SLE scaffolding and transport stub](NEW-DSOFTBUS-SLE-STUB.md) covers SLE interface/ledger scaffolding and the public connection stub. This report adds an independent Wi-Fi event-ingress contract and makes no new claim about SLE transport availability, capability propagation, or coexistence behavior.
