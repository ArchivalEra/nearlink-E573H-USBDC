---
type: harvest
title: "DSoftBus Linux link observation: rtnetlink notifications and carrier-only readiness"
language: en
created: 2026-09-17
tags: [harvest, openharmony, dsoftbus, linux, rtnetlink, bus-center, link-readiness, source-only]
sources:
  - "https://github.com/openharmony/communication_dsoftbus/blob/d15c795b2a309742f9aef52102d1ad1654922409/adapter/common/bus_center/network/lnn_netlink_monitor.c"
  - "https://github.com/openharmony/communication_dsoftbus/blob/d15c795b2a309742f9aef52102d1ad1654922409/adapter/common/bus_center/network/lnn_linkwatch.c"
  - "https://github.com/openharmony/communication_dsoftbus/blob/d15c795b2a309742f9aef52102d1ad1654922409/adapter/common/bus_center/network/lnn_netlink_monitor_virtual.c"
  - "https://github.com/openharmony/communication_dsoftbus/blob/d15c795b2a309742f9aef52102d1ad1654922409/adapter/common/bus_center/network/lnn_linkwatch_virtual.c"
  - "https://github.com/openharmony/communication_dsoftbus/blob/d15c795b2a309742f9aef52102d1ad1654922409/adapter/common/bus_center/include/lnn_event_monitor_impl.h"
  - "https://github.com/openharmony/communication_dsoftbus/blob/d15c795b2a309742f9aef52102d1ad1654922409/adapter/common/bus_center/include/lnn_linkwatch.h"
  - "https://github.com/openharmony/communication_dsoftbus/blob/d15c795b2a309742f9aef52102d1ad1654922409/adapter/common/bus_center/bus_center_adapter.gni"
trust: A
stale_after: 2027-03-17
---

# DSoftBus Linux link observation: rtnetlink notifications and carrier-only readiness

## Scope and provenance

This is a source-only examination of one narrow program theme: the Linux bus-center adapter's asynchronous network-change monitor and its companion synchronous carrier-readiness probe. All seven source files listed above were read in full. They match commit `d15c795b2a309742f9aef52102d1ad1654922409`; a scoped working-tree comparison against that revision produced no differences. Trust A applies to direct source observations, not to demonstrated runtime behavior.

The supplied sparse checkout contains these adapters but does not materialize a session-manager or authentication-handshake implementation. Rather than expanding it, this harvest selects an adjacent, fully available event-ingress path. The endpoint of the trace is the calls into bus-center notification APIs, not their downstream consumers.

Deduplication searched the knowledge bundle for `dsoftbus`, then ledger-related terms, and finally `netlink`, `linkwatch`, `RTM_NEWLINK`, `RTM_NEWADDR`, and `LnnNotifyAddressChangedEvent`. The existing DSoftBus harvest covers SLE transport stubs and capability scaffolding; the other netlink material concerns OpenSparklink's separate Generic Netlink ABI. No prior report found covers this rtnetlink-to-bus-center flow. Relative report links below are intended for placement in `knowledge/harvest/`.

No project program, build, test, network exchange, hardware operation, firmware operation, PCB inspection, or process/device-memory modification was performed.

## Executive findings

1. **Two separate observation mechanisms exist, not one handshake.** The monitor subscribes a `NETLINK_ROUTE` datagram socket to `RTMGRP_LINK | RTMGRP_IPV4_IFADDR` and registers it with the base listener. Separately, `LnnIsLinkReady()` creates a raw route-netlink socket for an `RTM_GETLINK` request. Neither selected implementation calls the other. This is local interface observation, not peer transport or authentication. [`network/lnn_netlink_monitor.c:59-92,252-276`; `network/lnn_linkwatch.c:87-125,145-187`]
2. **Network changes split into an early specialized signal and a later filtered address-change signal.** New addresses produce `IFNAME_IP_UPDATED`, and new links with `IFF_LOWER_UP` produce `IFNAME_LINK_UP`, before interface-type classification. The generic `LnnNotifyAddressChangedEvent()` is limited to Ethernet/WLAN, but accepts both NEW and DEL events. Thus a generic address-change notification is not proof that an interface is up or has acquired an address. [`network/lnn_netlink_monitor.c:104-114,137-202,223-238`]
3. **A WLAN new-address event also requests capability updates.** The adapter reads the current Wi-Fi band, constructs add/delete capability masks, and submits two ledger update calls before the generic address-change notification. An unrecognized band takes the branch that adds both 2.4 GHz and 5 GHz bits. This documents requested mutations, not confirmed ledger contents or peer advertisement. [`network/lnn_netlink_monitor.c:116-164`]
4. **The real readiness API means carrier present, not SoftBus session ready.** It asks for an interface by `IFLA_IFNAME`, reads the one-byte `IFLA_CARRIER` attribute, and returns whether it is nonzero. Null input and failures along the request/response/attribute path return false. No address, route, peer, authentication, or session check appears in this function. [`network/lnn_linkwatch.c:145-187`; `include/lnn_linkwatch.h:28`]
5. **The build-selected virtual contract is deliberately different.** The virtual readiness implementation always returns true, even without inspecting the interface argument; virtual monitor initialization returns success without installing a listener. The build fragment selects real implementations for the lite Linux and non-lite branches when `dsoftbus_feature_lnn_frame` is enabled, and virtual implementations for the liteos_m/liteos_a branches. Success therefore cannot universally mean active Linux monitoring or measured carrier readiness. [`network/lnn_linkwatch_virtual.c:18-22`; `network/lnn_netlink_monitor_virtual.c:20-32`; `bus_center_adapter.gni:25-86,107-128`]

All abbreviated evidence paths in this report are relative to `adapter/common/bus_center/` in the pinned source repository.

## Source path and event contract

### 1. Initialization and subscription

`LnnInitNetlinkMonitorImplNotify()` calls the real initializer and reports `INIT_DEPS_NETLINK` success or failure; on failure it also records the returned error. The underlying initializer first installs `NetlinkOnConnectEvent` and `NetlinkOnDataEvent` through `StartBaseClient(NETLINK, ...)`, then creates the socket and adds a `READ_TRIGGER`. The connect callback only logs and returns success. [`network/lnn_netlink_monitor.c:243-288`]

Socket setup requests a 32 KiB receive buffer, tries `SO_RCVBUFFORCE` first and falls back to `SO_RCVBUF`, sets `nl_pid` to zero for kernel assignment, and binds to the link and IPv4-address multicast groups. No IPv6-address or route-change subscription is requested here. Although the protocol is named `NETLINK_ROUTE`, this monitor is not subscribing to every routing event. [`network/lnn_netlink_monitor.c:55,59-92`]

### 2. Receive and dispatch

The data callback accepts only the `NETLINK` module, `SOFTBUS_SOCKET_IN`, and a nonnegative descriptor. It allocates a fresh 32 KiB buffer, performs one receive, checks the received length, and iterates message headers until the framing predicate fails or `NLMSG_DONE` is reached. Dispatch covers four message types; other types are ignored. The buffer is freed before a normal return. This is one receive per callback, not an explicit drain-until-empty loop. [`network/lnn_netlink_monitor.c:50-55,204-241`]

| Input | Specialized notification | Generic notification / local side effect |
|---|---|---|
| `RTM_NEWADDR` | Resolve index with `if_indextoname`; emit `IFNAME_IP_UPDATED` before classifying the interface | For WLAN, request Wi-Fi capability updates; for ETH/WLAN, emit address changed |
| `RTM_DELADDR` | No IP-updated signal | For ETH/WLAN, emit address changed; no band update here |
| `RTM_NEWLINK` | Obtain `IFLA_IFNAME`; emit `IFNAME_LINK_UP` only if `IFF_LOWER_UP` is set | For ETH/WLAN, emit address changed regardless of whether the specialized link-up condition held |
| `RTM_DELLINK` | No link-up signal | For ETH/WLAN, emit address changed |

Evidence: `network/lnn_netlink_monitor.c:104-114,137-202,223-238`.

The address handler checks the `ifaddrmsg` envelope and resolves its interface index; it does not extract an IPv4 address attribute for delivery. The link handler extracts the interface name from attributes. At the observed API boundary, the arguments carry the interface name, not a complete address configuration or peer record. Failure to classify an interface stops the generic notification but occurs after the specialized notification call. [`network/lnn_netlink_monitor.c:137-202`]

### 3. WLAN side effect, bounded to the call site

| Band returned by `SoftBusGetLinkBand()` | Requested additions | Requested deletions |
|---|---|---|
| `BAND_24G` | `BIT_WIFI_24G`, `BIT_WIFI`, `BIT_WIFI_P2P` | `BIT_WIFI_5G` |
| `BAND_5G` | `BIT_WIFI_5G`, `BIT_WIFI`, `BIT_WIFI_P2P` | `BIT_WIFI_24G` |
| Other | Both band bits, `BIT_WIFI`, `BIT_WIFI_P2P` | No bits set in the delete mask |

The helper initializes separate `CapabilityOption` objects, invokes `LnnSetLocalByteInfo(NUM_KEY_NET_CAP, ...)` for add and delete, and discards their return values. The caller subsequently emits the generic address-change event without checking whether these updates succeeded. Their atomicity, persistence, downstream broadcasts, and final ledger interpretation were not traced. [`network/lnn_netlink_monitor.c:116-164`]

## Companion carrier probe

`LnnIsLinkReady()` builds a bounded request containing `RTM_GETLINK`, `NLM_F_REQUEST | NLM_F_ACK`, an unspecified family, and the null-terminated interface-name attribute. Its sequence comes from `time(NULL)` followed by an increment. `AddAttr()` checks the resulting aligned message size before adding the attribute. [`network/lnn_linkwatch.c:39-58,145-177`]

`RtNetlinkTalk()` creates a separate socket for each call, requires the send result to match the request size, and receives a response. It retries receive errors represented as EINTR/EAGAIN; other receive errors and EOF close the socket and return failure. On a positive receive it closes the socket and delegates to `ProcessNetlinkAnswer()`. There is no explicit timeout or retry budget in this helper; actual blocking behavior depends on socket-adapter semantics outside the read set. [`network/lnn_linkwatch.c:87-125`]

Response processing skips headers with a different sequence, rejects `NLMSG_ERROR`, and accepts a matching non-error header. It does not decode the error payload to distinguish acknowledgment success from an error, and it does not implement a continuing multi-receive transaction after that positive receive. The caller then searches for `IFLA_CARRIER` and returns its nonzero value. These are source contract limits, not observations of a failing kernel exchange. [`network/lnn_linkwatch.c:60-85,122-142,178-187`]

The boolean API collapses unavailable observation and absent carrier into the same false result, while its virtual replacement reports true unconditionally. Consumers need build-context knowledge to interpret that boolean. [`include/lnn_linkwatch.h:28`; `network/lnn_linkwatch.c:145-187`; `network/lnn_linkwatch_virtual.c:18-22`]

## Unverified behavior and lifecycle boundaries

- **No end-to-end readiness claim.** Notification implementations, their consumers, the base-listener implementation, socket adapters, ledger setters, and Wi-Fi-band provider were not read. Notification delivery, ordering across threads, copying of interface-name arguments, or a resulting reconnect/authentication/session action remain unverified.
- **No inference from subscription to complete IP support.** This file subscribes to IPv4 address changes but does not establish what other DSoftBus adapters do for IPv6. No initial address dump, local event deduplication, debounce, or resynchronization request appears in the selected monitor. Those facilities could exist elsewhere. [`network/lnn_netlink_monitor.c:59-92,137-241,252-288`]
- **Lifecycle ownership is not established by this file alone.** Initialization starts the base client before creating the socket and stores the descriptor before adding the trigger. Trigger failure closes the socket. Deinitialization only closes a saved descriptor greater than zero; it does not visibly reset that saved value or remove the trigger/base client. This motivates checking external ownership and idempotence before reuse, but no repeated-init/deinit failure was executed or demonstrated. [`network/lnn_netlink_monitor.c:252-295`]
- **Build fragment, not deployed configuration.** The inspected `.gni` proves conditional source selection only. Its imported configuration, final product arguments, linked target, and running system were not examined. [`bus_center_adapter.gni:14,25-86,107-128`]
- **No SLE transport conclusion.** This report does not revisit the SLE stub or establish whether another public/private implementation is present. It documents an Ethernet/WLAN-oriented observation path, not NearLink framing, authentication, USB transport, or firmware behavior.

## Reusable lessons

1. Keep carrier observation, IP-change notification, and authenticated session readiness as separate states. This source already exposes distinct observations; a port should not promote `LnnIsLinkReady()` to an end-to-end connectivity assertion.
2. Treat interface-change events as prompts to reevaluate state rather than complete state snapshots. Here NEW and DEL messages converge on the same generic callback, with only the interface name supplied.
3. Preserve an explicit distinction between measured readiness, observation failure, and unsupported/virtual observation in any new API. The real and virtual boolean implementations erase different information.
4. Reuse the platform-adapter boundary and conditional implementation selection, not an assumption that a success-returning virtual initializer installs monitoring. A port should document lifecycle ownership and the provenance of its readiness result.

These are design lessons inferred from the observed contracts, not implemented changes or validated porting recipes.

## Comparison anchors

- [Earlier DSoftBus SLE-stub harvest](NEW-DSOFTBUS-SLE-STUB.md): that report establishes SLE scaffolding and the connection stub. This report adds the independent Linux interface-observation contract, its notification ordering, carrier probe, and build-selected virtual semantics; it does not duplicate the SLE capability ledger audit.
- [OpenSparklink host-kernel ABI contract](../intel/OSPL-UAPI-CONTRACT.md): its Generic Netlink `sparklink` family and typed SLE events are a different interface from the Linux `NETLINK_ROUTE` messages inspected here. Shared use of netlink does not imply compatible message types, readiness semantics, or a shared transport. The comparison uses that prior report as an archive anchor; its upstream implementation was not re-audited in this harvest.
