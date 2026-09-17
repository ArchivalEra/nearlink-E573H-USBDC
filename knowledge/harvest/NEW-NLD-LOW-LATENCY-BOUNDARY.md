---
type: harvest
title: "Nld low-latency RPC: BS2X-only availability and parameter semantics boundary"
language: en
created: 2026-09-17
tags: [harvest, nld, erpc, bs2x, latency]
sources:
  - "https://gitcode.com/goodspeed1/Nld/blob/68b0e078e96ac30b1b5ae161df62a876e4ff89c7/adapter.mcu/nlddev.h"
  - "https://gitcode.com/goodspeed1/Nld/blob/68b0e078e96ac30b1b5ae161df62a876e4ff89c7/adapter.mcu/nldrpc.cpp"
  - "https://gitcode.com/goodspeed1/Nld/blob/68b0e078e96ac30b1b5ae161df62a876e4ff89c7/erpc_gen/bs2x/sle_sle_low_latency_interface.hpp"
  - "https://gitcode.com/goodspeed1/Nld/blob/68b0e078e96ac30b1b5ae161df62a876e4ff89c7/erpc_gen/bs2x/sle_sle_low_latency_client.cpp"
  - "https://gitcode.com/goodspeed1/Nld/blob/68b0e078e96ac30b1b5ae161df62a876e4ff89c7/erpc_gen/bs2x/sle_sle_low_latency_server.cpp"
trust: A
stale_after: 2027-03-17
---

# Nld low-latency RPC: BS2X-only availability and parameter semantics boundary

## Executive findings

This closes the queued low-latency parameter inspection at revision `68b0e078e96ac30b1b5ae161df62a876e4ff89c7`. Paths below refer to the pinned upstream repository. The result establishes a generated call contract and device-specific availability, not a working low-latency data path or a radio timing guarantee.

1. **The service exists in the BS2X adapter surface, not the WS63 adapter surface.** `adapter.mcu/nlddev.h:46-59,113-124` sets `NLD_DEV_HAVE_LOW_LATENCY` to one for BS2X and zero for WS63. `adapter.mcu/nldrpc.cpp:53-55,87-89,102-104` uses that macro consistently for inclusion, teardown and client binding. Omission from this firmware interface does not prove that the underlying chip lacks a low-latency capability.
2. **The exact identity is service 7, methods 1 and 2.** `erpc_gen/bs2x/sle_sle_low_latency_interface.hpp:21-29` assigns method 1 to a parameterless `sle_low_latency_dongle_enable` and method 2 to `sle_low_latency_set(uint16_t conn_id, uint8_t enable, uint16_t rate)`. The server dispatch mirrors those identifiers and returns InvalidArgument for unknown methods (`sle_sle_low_latency_server.cpp:90-116`).
3. **The set request writes three typed scalar arguments in declaration order.** `sle_sle_low_latency_client.cpp:167-179` writes connection ID, enable, then rate before performing a request and reading the result. The server reads the same three arguments before invoking its handler (`sle_sle_low_latency_server.cpp:164-184`). This is serialized data, not a native padded struct ABI.
4. **RPC failures are collapsed into an application return sentinel.** The generated client calls the error handler and sets the returned result to `0xFFFFFFFFU` when eRPC status is unsuccessful (`sle_sle_low_latency_client.cpp:184-204`; analogous enable path at `:117-137`). A consumer should preserve transport diagnostics rather than interpret every returned integer as a controller-origin status.
5. **Rate units and accepted values remain unestablished.** Neither the inspected signature nor these generated shims specifies a rate unit, allowed range, enable-value enumeration, or mandatory ordering between the two methods. Naming alone cannot justify translating rate into hertz, microseconds, PHY speed or an SSAP delivery interval.

## Request and response boundary

| Operation | Application request fields | Application response | Evidence |
|---|---|---|---|
| Dongle enable | None | `errcode_t`, declared as `uint32_t` | `sle_sle_low_latency_interface.hpp:27`; `sle_sle_low_latency_common.hpp:210`; client `:106-114` |
| Set | `uint16_t conn_id`, `uint8_t enable`, `uint16_t rate` | Same result type | Interface `:29`; client `:167-181`; server `:164-184,198-203` |
| Unknown method | No served operation | eRPC InvalidArgument | Server `:108-112` |

The generated shims require eRPC version number 11400 (`sle_sle_low_latency_client.cpp:15-17`). Transport framing, codec endianness and message headers are separate layers; this report does not infer complete wire-byte offsets from the C++ types.

Client setup binds the optional service to the same client manager as announce, seek, connection and SSAP services. That is initialization evidence, not proof that daemon policy invokes either operation during a connection. The inspected call-site search found generated wrappers and binding references, not an application-level low-latency activation policy.

## Boundaries

- Source-only inspection; no RPC was sent, no hardware was touched, and no build or test suite was run. Remote freshness was not checked for this GitCode source; the revision is explicit.
- `nlddev.h` carries GPL-3.0-or-later. This report records interface facts without importing the implementation.
- The adjacent BS2X comments for connect-role and low-latency macros are swapped (`nlddev.h:56-59`). Actual macro names, values and preprocessor consumers establish the availability conclusion; comment proximity does not override them.
- The prior broad Nld report's statement that two methods confirm a QoS profile switch was too strong. They confirm a control interface only; internal scheduling and delivery behavior remain outside the inspected implementation.
- Neither this Nld interface nor the absence of its WS63 service establishes WS73 USB/DLI compatibility. A real bridge would need its own capability and parameter mapping.

## Reusable

- Represent low-latency control as an optional adapter capability rather than assuming every SLE-backed device exports an identical API.
- Preserve connection-scoped parameters as typed values with unresolved units until vendor documentation or verified implementation supplies semantics.
- Keep transport failure distinct from controller return status in diagnostic records, especially where generated clients use an all-ones sentinel.
- Check client serialization and server decoding together before treating generated header signatures as a wire contract.

## Comparison anchors

- [NEW-NLD-ERPC-CONTRACT](NEW-NLD-ERPC-CONTRACT.md): earlier service inventory; this report adds BS2X/WS63 availability, verified dispatch, argument ordering and the unresolved rate-unit boundary.
- [NEW-NLD-ERPC-PROTOCOL](NEW-NLD-ERPC-PROTOCOL.md): transport-level companion. The scalar contract here is above that framing layer, not a replacement for it.
