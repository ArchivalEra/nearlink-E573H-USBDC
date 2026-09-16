---
type: intel
title: "LinkNebula (AetherLink) — Rust no_std SLE Mesh Deep-Dive"
language: zh
created: 2026-08-17
tags: [intel, linknebula, aetherlink, rust]
sources:
  - "https://github.com/GBCLStudio/LinkNebula"
trust: B
stale_after: 2027-02-17
---

# LinkNebula (AetherLink) — Rust no_std SLE Mesh Deep-Dive

**Date:** 2026-08-17
**Scope:** Read-only inspection of `https://github.com/GBCLStudio/LinkNebula/tree/master/` (all Rust sources, Cargo.toml, tests, README). No network/build/hardware.
**Context:** LinkNebula is a community Rust no_std mesh experiment on NearLink (SLE). The prior `COMMUNITY-PROJECTS.md` (line 64-69) assessed it as "mid-value — protocol/design knowledge transferable." This report validates that assessment and extracts the protocol design and implementation patterns in detail.
**Companion docs:** `COMMUNITY-PROJECTS.md` (lines 64-69, 172), `SLE-MESH-EPAPER.md` (the C-mesh counterpart).

---

## Sources

| File | Role | Key refs |
|---|---|---|
| `README.md` | Project orientation | Senior-high experiment, "NearLink without internet" |
| `Cargo.toml` | Workspace root, deps | `heapless 0.7`, `zerocopy 0.6` |
| `common/src/lib.rs` | Root no_std lib | Re-exports protocol, HAL, utils |
| `common/src/protocol/mod.rs` | Protocol constants, packet types, QoS, service req/resp serialization | `PacketType` enum, `NodeId`, `ServiceType`, `QosRequirements`, `NetworkPacket` zero-copy, `serialize/deserialize_service_request/response` |
| `common/src/protocol/beacon.rs` | Beacon packet | CRC-16 checksum, `#[repr(C, packed)]`, `is_valid()` |
| `common/src/protocol/data.rs` | Data packet | Header + borrowed `&[u8]` data, XOR-checksum scheme |
| `common/src/hal/mod.rs` | HAL traits | `RadioInterface` (send/recv beacon/data, configure, get_rssi), `Hardware` (node_id, radio, battery, timestamp, delay, low_power) |
| `common/src/hal/bearpi_hi2821.rs` | BearPi Hi2821 HAL | FFI to `nl_init/nl_send/nl_recv/nl_configure`, channel=15, tx_power=20, PAN=0x1234 |
| `common/src/hal/simulator.rs` | Simulator HAL | `SimChannel` (Arc<Mutex<VecDeque>>), `SimRadio`, `SimHardware` with battery drain sim |
| `common/src/utils/aligned_buffer.rs` | DMA-aligned buffer | `#[repr(align(4))]`, `MaybeUninit` for no_std, `copy_from_slice` |
| `common/src/utils/checksum.rs` | CRC-16-CCITT | Polynomial 0x1021, init 0xFFFF, test vectors |
| `forward/src/main.rs` | Forward node main loop | Beacon every 60s, election every 5min, service cleanup every 30s, packet dispatch loop |
| `forward/src/routing/dynamic_forwarding.rs` | ForwardingEngine + routing table | 32-entry fixed array, 5-min expiry, metric = RSSI, next_hop = direct for discovered nodes |
| `forward/src/directory/election.rs` | Leader election | 3-phase (Start/Response/Result), priority = node_id[0], 5s collection window |
| `forward/src/directory/service_directory.rs` | NetworkServiceDirectory | 32-slot table, weighted QoS scoring (BW 40, latency 30, reliability 20, load 10, battery 5, signal 5), 5-min expiry, cleanup every 30s |
| `client/src/main.rs` | Client main loop | Discovery via broadcast beacons, service request with QoS, path establish, data send loop |
| `client/src/discovery.rs` | Service discovery | Broadcast beacon → receive server response, 30s max, 1s retry |
| `client/src/service_client.rs` | Service request/response | `ServiceEndpoint` struct (server_id, relay_id, service_type, hops), request → wait response → path establish → data |
| `client/src/sensor_driver.rs` | Sensor abstraction | `SensorData` (temp/humidity/pressure), simulator generates dynamic values |
| `server/src/main.rs` | Server node | Stores sensor data, processes commands (query/config/clear/reboot) |
| `server/src/storage/circular_buffer.rs` | Data storage | 1024-record ring buffer, serialize to big-endian bytes |
| `server/src/api/cli.rs` | Command processor | Ring-buffer command queue, Query/Configure/Clear/Reboot command types |
| `tests/unit/routing_algorithm.rs` | Routing tests | Basic ops, metric update, no-self-route |
| `tests/unit/protocol_parsing.rs` | Protocol tests | Beacon/DataPacket creation, checksum validation, NodeId equality |
| `tests/unit/service_discovery.rs` | Service discovery E2E | Client → Forwarder → Server path establishment flow |
| `tests/integration/multi_hop.rs` | Multi-hop integration | 3-node client→forwarder→server relay test |
| `tests/integration/dynamic_config.rs` | Dynamic config test | Client → server config command roundtrip |

---

## 1. 项目定位 (Project Orientation)

**What it is:** A senior-high student experiment in building a NearLink (SLE) mesh network entirely in Rust, targeting no_std embedded. The project name is "AetherLink" (internal package name `aether_link`).

**Target hardware:** BearPi Hi2821 (HiSilicon WS73-family chip, Cortex-M based). The `build.rs` links a BearPi-specific linker script (`-Tbearpi_hi2821.ld -nostartfiles`) under the `bearpi` feature flag (`build.rs:6-9`). Default build is `simulator` for host-side testing.

**Communication medium:** NearLink SLE radio. The HAL calls FFI functions `nl_init`, `nl_send`, `nl_recv`, `nl_configure` with a `NearlinkConfig` struct containing channel (default 15), tx_power (default 20 dBm), and PAN_id (default 0x1234) (`common/src/hal/bearpi_hi2821.rs:1-13`).

**Protocol layer:** Custom application-layer protocol, NOT standard SSAP. The `RadioInterface` trait abstracts send/receive of `Beacon` and `DataPacket` structs directly. There is no SSAP service registration, no GATT/ATT layer, no CM/DLI calls — the FFI is a thin raw-radio wrapper.

**Workspace structure:** 4 crates — `common` (protocol + HAL + utils), `client` (sensor nodes), `forward` (relay/routing nodes), `server` (data aggregation nodes). All no_std except the simulator feature path (`Cargo.toml:7-13`).

**Maturity:** Frozen as of 2025-03. Self-described as "a toy by a Senior High" (`README.md:12`). No firmware binaries — source-only experiment.

---

## 2. 网络层设计 (Network Layer Design)

### 2.1 Routing Protocol — Passive Beacon-Driven Distance Vector

The routing is a simple **passive DV** driven by beacon reception. There is no active route discovery (no RREQ/RREP, no AODV):

**Route table:** Fixed 32-entry array of `Option<RouteEntry>` (`forward/src/routing/dynamic_forwarding.rs:34`):
```
RouteEntry { destination: NodeId, next_hop: NodeId, metric: i8, timestamp: u64 }
```

**Route learning:** When a beacon is received from node X, the forwarder calls `update_route(X, beacon.rssi)`. If X is already in the table, its metric and timestamp are refreshed. If not, a new entry is added with `next_hop = X` (direct route). If the table is full, the first slot is overwritten — there is no LRU or best-metric eviction (`dynamic_forwarding.rs:84-119`).

**Route expiry:** `ROUTE_EXPIRY_MS = 300_000` (5 minutes). Routes not refreshed by a beacon within 5 minutes are cleaned up during the main loop's periodic `cleanup()` call. This is significantly slower than the ePaper mesh's 20-second lifetime (`SLE-MESH-EPAPER.md:71`).

**Metric:** RSSI (signal strength, i8) only. No hop count, no composite metric. The test confirms: `engine.update_route(destination, -60)` then `engine.update_route(destination, -80)` does NOT overwrite — but note: the current code always replaces the metric unconditionally, it does NOT pick the better value. The test at `tests/unit/routing_algorithm.rs:56-71` confirms "update not add" but does not test metric comparison.

**Next-hop lookup:** Linear scan of 32 entries, returning the `next_hop` field. For discovered nodes, `next_hop == destination` (direct routing). There is **no multi-hop relay logic** in the forwarding engine itself — the `handle_data_packet` function in `forward/src/main.rs:199-233` forwards packets to the next hop only if the destination is known and not broadcast. If no route exists, the packet is silently dropped.

**Key limitation:** The routing table only stores first-hop neighbors. There is no mechanism for a forwarder to learn routes through other forwarders. Multi-hop only works if every intermediate forwarder has a direct beacon link to the next hop. This is fundamentally different from the ePaper mesh's full DV with HELLO-DV broadcasts and poison reverse (`SLE-MESH-EPAPER.md:66-72`).

### 2.2 Packet Format and Fragmentation

**Packet header** (`common/src/protocol/mod.rs:15-23`):
```
PacketHeader {
    magic: u16,          // 0xAA55
    version: u8,         // 0x01
    packet_type: PacketType,
    ttl: u8,
    src_mac: [u8; 6],
    dest_mac: [u8; 6],
    checksum: u32,
}
```

**`NetworkPacket`** = header + 252-byte payload = 256 bytes total (`mod.rs:7-9`).

**`DataHeader`** (`data.rs:7-26`): version, packet_type, source, destination, packet_id, `total_fragments: u8`, `fragment_index: u8`, `data_length: u16`, `checksum: u16`.

**Fragmentation fields exist but are never used.** `DataHeader` declares `total_fragments` and `fragment_index` fields, but there is no code anywhere that creates multi-fragment packets or reassembles them. The `DataPacket::new()` constructor always sets `total_fragments=1, fragment_index=0` (`data.rs:45-46`). The `ForwardingEngine::process()` in `routing/mod.rs` receives into a 256-byte buffer and forwards as-is — no fragment assembly.

**Checksum scheme:** CRC-16-CCITT (polynomial 0x1021, init 0xFFFF) computed as `header_checksum XOR data_checksum` (`data.rs:67-74`). The beacon has a simpler `calculate_checksum` over the entire struct with checksum field zeroed (`beacon.rs:44-54`). This is lighter than the ePaper mesh's per-hop CRC16 recompute.

### 2.3 Reliable Transport

**Mostly unimplemented.** The protocol defines `PacketType::Ack = 0x03` (`mod.rs:61`) but no code sends or processes ACK packets. The client's `service_client.rs` waits for a `ServiceResponse` (not an ACK) with a retry loop (10 retries, 1s delay), but data packets have no acknowledgment.

There is no windowed flow control, no AIMD, no bitmap repair, no CHECKPOINT mechanism. Compare with the ePaper mesh's sophisticated AIMD engine with W=8..80, D=0..50ms, RTT/RTO, bitmap repair, multicast quorum (`SLE-MESH-EPAPER.md:81-88`).

### 2.4 Service Discovery

The most complete subsystem. The forward node maintains a `NetworkServiceDirectory` (`forward/src/directory/service_directory.rs:93-97`) with:

- **32-slot fixed array** of `ServiceEntry` (node_id, service_type, load, capabilities, metrics, last_update_time)
- **QoS-weighted scoring** (`service_directory.rs:46-89`): bandwidth (40 pts), latency (30 pts), reliability (20 pts), load (10 pts), battery (5 pts), signal strength (5 pts). Total = 110 max.
- **`find_best_service(service_type, qos)`**: iterates all entries, picks highest-scoring match
- **Expiry:** 5 minutes, cleanup every 30 seconds (`service_directory.rs:110-128`)

**Service types defined:** Storage, Processing, Gateway, VideoRelay, AudioRelay, DataRelay, SensorCollection (`mod.rs:88-96`).

**Service flow:** Client sends `ServiceRequest` (service_type + QoS + expiry) → forward node queries directory → forward node sends `ServiceResponse` (service_id, server_node_id, status) → forward node sends `PathEstablish` to server → server sends `PathConfirm` back through forwarder to client with hop count.

**Leader election** (`forward/src/directory/election.rs`): 3-phase (ElectionStart/ElectionResponse/ElectionResult), priority = first byte of node ID, 5-second collection window, runs every 5 minutes. Simplified — always declares self as master in `finish_election` (`election.rs:90-93`).

---

## 3. SLE 用法 (SLE Link Usage)

**LinkNebula does NOT use SLE/SSAP at all in its standard form.** The BearPi HAL (`common/src/hal/bearpi_hi2821.rs`) wraps four FFI functions:

```c
nl_init(config: *const NearlinkConfig) -> i32
nl_send(dest: *const u8, data: *const u8, len: usize) -> i32
nl_recv(buf: *mut u8, max_len: usize, actual_len: *mut usize) -> i32
nl_configure(channel: u8, tx_power: i8) -> i32
```

This is a **custom raw-radio FFI**, not the standard `ssaps_*`/`ssapc_*` API seen in ePaper mesh (`SLE-MESH-EPAPER.md:101-108`) or our `stack/ssap/`. The `NearlinkConfig` has channel, tx_power, and PAN_id — closer to 802.15.4/LR-WPAN framing than SLE SSAP semantics.

The radio configuration (channel 15, 20 dBm, PAN 0x1234) and the 6-byte MAC addressing suggest this FFI targets a **PHY/MAC-layer interface**, possibly via a custom firmware on the Hi2821 or a host-side SLE driver that exposes raw frame send/recv. There is no SSAP service registration, no connection management, no MTU negotiation, no DLE — the `RadioInterface` trait's `configure(channel, power)` is the only radio parameterization.

**Contrast with ePaper mesh:** The ePaper project uses vendor in-chip `ssaps_register_server`/`ssapc_write_req` with MTU 520, connection pool management, and per-hop SLE connections (`SLE-MESH-EPAPER.md:101-108`). LinkNebula bypasses all of this. Its FFI layer would need replacement to run on our WS73 dongle (which uses host-side DLI/CM as shown in `SLE-MESH-EPAPER.md:116`).

**Implication:** LinkNebula's SLE usage is not wire-compatible with standard SLE SSAP. Knowledge transfers at the **protocol design** level only, not at the link-call level.

---

## 4. 与 NearLink-Mesh-ePaper 的对比 (Comparison with ePaper Mesh)

| Dimension | LinkNebula (Rust) | NearLink-Mesh-ePaper (C) |
|---|---|---|
| **Language/no_std** | Rust, `#![no_std]`, `heapless`/`zerocopy`, embedded-hal | C, LiteOS, vendor SDK |
| **Target chip** | BearPi Hi2821 (WS73 family) | BearPi-Pico H3863 (WS63 family) |
| **SLE usage** | Custom FFI (`nl_init/nl_send/nl_recv`), raw radio | Vendor in-chip SSAP (`ssaps_*`/`ssapc_*`), MTU 520, 5-conn pool |
| **Routing** | Passive beacon-driven DV, 32-entry table, no multi-hop relay logic | Proactive HELLO-DV with Poison Reverse, 128-entry table, full multi-hop relay |
| **Route lifetime** | 5 minutes (300s) | 20 seconds (4x HELLO interval) |
| **Packet size** | 256 bytes (header + 252 payload) | 520 bytes MTU (16B header + 504 payload) |
| **Fragmentation** | Header fields exist but unimplemented | App-layer: 480B FC packets, 96KB RX buffer, bitmap reassembly |
| **Reliability** | ACK type defined but unimplemented; no flow control | AIMD windowed transfer, CHECKPOINT, bitmap selective retransmit, RTT/RTO |
| **Service discovery** | QoS-weighted scoring, 32-slot directory, leader election | Gateway-orchestrated topology collection, tree convergence, 64-node table |
| **Dedup** | None | 256-entry `(src,seq)` dedup ring + 20f/s flood rate limit |
| **Flow control** | None | AIMD W=8..80, D=0..50ms, per-hop adaptive init, RFC-6298 RTT |
| **Network hygiene** | None | Zombie 3-D detection, P22 partition heal, exponential backoff, ghost-reject |
| **Test scale** | 3-node integration test (client→forwarder→server) | 6-node measured: 1-hop 2-4s, 2-hop 6-10s, 3-hop 12-18s |
| **Code maturity** | Frozen 2025-03, ~2000 lines Rust | Active, 20+ patches, ~10K lines C |

**Key similarities:**
- Both use beacon-based neighbor discovery and proactive DV-style routing tables
- Both have a client-forwarder-server architecture with service directories
- Both use `#[repr(C, packed)]` / equivalent fixed-layout packet formats
- Both implement CRC-16 checksums
- Both target BearPi hardware with NearLink SLE radios

**Key differences:**
- LinkNebula is architecturally simpler — it is a proof-of-concept, not a production mesh
- The ePaper mesh has production-grade flow control, dedup, fragmentation, and network hygiene that LinkNebula completely lacks
- LinkNebula's HAL abstraction is cleaner (trait-based) but its SLE usage is non-standard (raw FFI, not SSAP)
- The ePaper mesh's routing is a true multi-hop DV with HELLO-DV and poison reverse; LinkNebula's is single-hop-only in practice

---

## 5. 可迁移思想 (Transferable Design Patterns for C Stack)

These are the Rust design patterns and architectural ideas from LinkNebula that can be adapted to our C stack for the TV-box sensor mesh scenario:

### 5.1 Zero-Copy Packet Handling (via `zerocopy` crate + `#[repr(C, packed)]`)

`common/src/protocol/mod.rs:2-3` uses `zerocopy::{AsBytes, FromBytes}` on `NetworkPacket` to enable zero-copy reinterpretation of byte buffers as typed structs. The `as_beacon()` method (`mod.rs:135-142`) casts payload bytes directly to `BeaconPayload` without copying.

**C equivalent:** Use `__attribute__((packed))` structs with `memcpy`-free casting via `union` or pointer aliasing (with strict-aliasing-safe patterns). Our `hwsle_transport.h` already uses packed structs for ACB frames — extend this pattern to the mesh protocol layer.

### 5.2 Trait-Based HAL Abstraction (via `RadioInterface` + `Hardware` traits)

`common/src/hal/mod.rs:7-54` defines two traits (`RadioInterface` and `Hardware`) that abstract all hardware interactions. The forwarder/client/server code is generic over `H: Hardware`, enabling the same logic to run on simulator or real hardware.

**C equivalent:** Function-pointer-based HAL structs (vtable pattern). Define a `radio_interface_t` with function pointers for `send_beacon`, `receive_data`, `configure`, etc. Implement once for WS73 dongle (via DLI), once for simulator. This is a standard embedded C pattern — LinkNebula just demonstrates it cleanly.

### 5.3 QoS-Weighted Service Scoring

`forward/src/directory/service_directory.rs:46-89` implements a weighted scoring function for service provider selection: bandwidth (40), latency (30), reliability (20), load (10), battery (5), signal (5). Providers that fail hard constraints (min bandwidth, max latency, min reliability) return score 0.

**C transfer:** Port the scoring function directly. For the TV-box scenario, this enables the box to select the best sensor relay node based on signal quality, battery level, and current load — critical for a multi-node deployment.

### 5.4 Structured Packet Types with Type Discriminant

`common/src/protocol/mod.rs:57-68` defines `PacketType` as a `#[repr(u8)]` enum with 8 types: Beacon, Data, Ack, Control, ServiceRequest, ServiceResponse, PathEstablish, PathConfirm. The first byte of the payload dispatches to the correct handler (`forward/src/main.rs:101-119`).

**C equivalent:** An `enum` or `#define` constants + a switch statement in the forward loop. Our SSAP payload framing already needs a similar discriminator. LinkNebula shows a clean taxonomy for a mesh protocol.

### 5.5 Aligned Buffer for DMA

`common/src/utils/aligned_buffer.rs:7-88` implements `AlignedBuffer<N>` with `#[repr(align(4))]` and dual-mode initialization (`MaybeUninit` for no_std, zero-init for simulator). Provides `as_mut_slice()` / `as_slice()` / `set_len()` / `copy_from_slice()`.

**C equivalent:** `__attribute__((aligned(4)))` arrays with a length-tracking wrapper struct. Directly applicable to our DMA-based SLE transport buffers.

### 5.6 Fixed-Array Service Directory with Periodic Cleanup

`forward/src/directory/service_directory.rs:93-97,110-128` uses a 32-slot fixed array (no heap allocation) with timestamp-based expiry and periodic cleanup. This is the right pattern for an embedded mesh node with limited RAM.

**C transfer:** Replace dynamic `Vec`/linked-list service tables with fixed-size arrays and cleanup ticks. Our TV-box has more RAM than a sensor node, but the fixed-array pattern avoids fragmentation.

### 5.7 Simulator Pattern for Host-Side Testing

`common/src/hal/simulator.rs` implements `SimChannel` (shared `Arc<Mutex<VecDeque>>`), `SimRadio`, and `SimHardware` — enabling the entire mesh stack to run on a host machine without hardware. Feature-gated: `#[cfg(feature = "simulator")]` vs `#[cfg(feature = "bearpi")]`.

**C equivalent:** Compile-time HAL switch (`#ifdef SIMULATOR` vs `#ifdef WS73`) with a POSIX-thread-based simulator backend. Essential for protocol validation before hardware bring-up.

### 5.8 Workspace Crate Decomposition

The 4-crate workspace (`common/client/forward/server`) cleanly separates concerns: protocol+HAL in `common`, node-role-specific logic in each crate. This is a model for organizing our C codebase into `protocol/`, `hal/`, `forward/`, `app/` directories with clear dependency edges.

---

## 6. 结论 (Conclusion: Maturity Assessment and Need for Self-Development)

### Technical Maturity of LinkNebula

**Low — proof-of-concept, not production mesh.** The project demonstrates correct architectural thinking (trait-based HAL, zero-copy packets, service discovery with QoS, beacon-driven routing, workspace decomposition) but has significant gaps:

1. **No multi-hop relay logic.** The forwarding engine only handles first-hop neighbors. True multi-hop requires HELLO-DV broadcasts, reverse-route learning, and poison reverse — none of which exist in the code.
2. **No reliability.** ACK packets are defined but never sent/received. No windowed transfer, no retransmission, no flow control.
3. **No fragmentation/reassembly.** Header fields exist but the implementation always sends single-fragment packets.
4. **No dedup.** Broadcast packets can loop indefinitely.
5. **No network hygiene.** No zombie detection, no connection management, no exponential backoff.
6. **Frozen and untested at scale.** Only 3-node integration tests. No firmware binaries. The 2025-03 snapshot is final.

### Do We Need to Self-Develop?

**Yes.** LinkNebula's value is architectural inspiration, not a codebase to fork. Specifically:

- **What to borrow:** The architectural patterns (5.1-5.8 above), the packet type taxonomy, the QoS scoring model, the service directory design, the HAL abstraction approach.
- **What NOT to borrow:** The routing implementation (single-hop-only, no HELLO-DV), the lack of reliability, the custom FFI HAL (non-SSAP), the 256-byte packet limit.
- **What the ePaper mesh provides that LinkNebula does not:** The production-grade DV routing, AIMD flow control, bitmap repair, dedup, network hygiene, and measured multi-hop performance numbers. For our TV-box mesh, the ePaper mesh is the behavioral oracle; LinkNebula is the architectural pattern library.

### Recommended Path

1. **Design phase** (now): Use LinkNebula's trait-based HAL pattern + packet type taxonomy as the starting skeleton for our mesh protocol design.
2. **Routing:** Adopt the ePaper mesh's HELLO-DV with poison reverse (not LinkNebula's passive beacon DV). Port the 128-entry route table, 20s lifetime, and per-neighbor HELLO format.
3. **Flow control:** Start with single-hop bitmap repair + AIMD (ePaper mesh parameters). Add multi-hop only after single-hop throughput is validated.
4. **SLE integration:** Use our existing `stack/ssap/` DLI-based transport, not LinkNebula's raw FFI. The mesh layer sits above SSAP as application-layer payload, same as the ePaper mesh.
5. **Testing:** Build a simulator backend (LinkNebula pattern) for protocol validation, then validate on WS73 hardware.

---

*Report generated 2026-08-17. All file references use absolute paths under `https://github.com/GBCLStudio/LinkNebula/tree/master/`.*
