---
type: harvest
title: "LinkNebula evidence correction: intended mesh is not an executable SLE stack"
language: en
created: 2026-09-17
tags: [harvest, rust, mesh, evidence, simulator]
sources:
  - "https://github.com/GBCLStudio/LinkNebula/tree/d9324c4fceb9e40e2c9a3ef93b964a969d2d57de"
  - "https://github.com/GBCLStudio/LinkNebula/blob/d9324c4fceb9e40e2c9a3ef93b964a969d2d57de/common/src/protocol/mod.rs"
  - "https://github.com/GBCLStudio/LinkNebula/blob/d9324c4fceb9e40e2c9a3ef93b964a969d2d57de/common/src/protocol/data.rs"
  - "https://github.com/GBCLStudio/LinkNebula/blob/d9324c4fceb9e40e2c9a3ef93b964a969d2d57de/common/src/hal/simulator.rs"
  - "https://github.com/GBCLStudio/LinkNebula/blob/d9324c4fceb9e40e2c9a3ef93b964a969d2d57de/forward/src/main.rs"
  - "https://github.com/GBCLStudio/LinkNebula/blob/d9324c4fceb9e40e2c9a3ef93b964a969d2d57de/tests/integration/multi_hop.rs"
trust: A
stale_after: 2027-03-17
---

# LinkNebula evidence correction: intended mesh is not an executable SLE stack

## Executive findings

This is a corrective delta to [the earlier LinkNebula deep-dive](../intel/LINKNEBULA-MESH.md), not a newly discovered mesh implementation. Source inspection at `d9324c4fceb9e40e2c9a3ef93b964a969d2d57de` reveals that several previous descriptions of intended behavior were stronger than the implementation supports. All source paths below are relative to the pinned upstream tree in `sources`. No build, test execution, or hardware operation was performed.

- **The hardware and simulator integration is incomplete.** `common/src/hal/mod.rs:1-2` includes both backends unconditionally. `common/src/hal/bearpi_hi2821.rs:15-22,57-58` defines `BearPiHal` and an implementation of `HalInterface`, whereas role entry points import `BearPiHardware`; the public abstraction declares `Hardware` and `RadioInterface`. The C `nl_*` declarations do not prove an implemented SLE transport. `common/src/lib.rs:6` declares `utils`, but neither `common/src/utils.rs` nor `common/src/utils/mod.rs` exists in the inspected tree.
- **Packet sizes and checksums need exact descriptions.** `common/src/protocol/mod.rs:7-22` declares a packed 21-byte header and 252-byte payload: 273 bytes, not the commented 256. `common/src/protocol/data.rs:7-25,36-46` instead declares a 22-byte data header and limits constructor payloads to 234 bytes. Its checksum is the XOR of two separately initialized CRC computations, not a CRC of concatenated header and payload (`data.rs:56-90`).
- **The service envelope never selects its intended control types.** Requests, responses, path establishment and confirmation are created with `DataPacket::new`, which always sets `PacketType::Data as u8` (`data.rs:39-46`; `client/src/service_client.rs:50-60`; `forward/src/main.rs:269-282,363-374,433-444`). Receivers dispatch on ServiceRequest, ServiceResponse or PathConfirm. Moreover, several call sites compare a `u8` header field directly with enum variants. These are source-level integration gaps, not evidence of a functioning service exchange.
- **Route expiration is only a dormant helper.** `forward/src/routing/dynamic_forwarding.rs:38-63,84-106` initializes `cleanup_timer` to zero and uses it for route timestamps without advancing it. The inspected forwarder loop invokes service-directory cleanup, not route cleanup (`forward/src/main.rs:88-92`). The previous claim that routes expire during the running main loop is unsupported.
- **Multi-hop tests are sketches, not passing evidence.** `tests/integration/multi_hop.rs:13` contains invalid Rust literals such as `0xS1`. Its manually reconstructed relay packet changes the source to the relay (`:36-40,50-52`). The fixture therefore neither proves preservation of original source identity nor demonstrates the actual forwarding state machine.

## Encoding and control-flow corrections

| Concern | Observed behavior | Evidence |
|---|---|---|
| Expiry width | Serializer retains the high two bytes of a `u32`; decoder rebuilds those bytes followed by two zero bytes. Low 16 bits are discarded; the client's intended 60-second expiry round-trips as zero. | `common/src/protocol/mod.rs:164-167,192-193`; `client/src/main.rs:103` |
| Response minimum length | Both serializer and decoder explicitly require at least 11 bytes. A claim that response decoding lacks a minimum-length check is incorrect. | `common/src/protocol/mod.rs:206-208,227-229` |
| Receive polling | Service request is sent once, followed by up to ten polls separated by one-second delays; these are not retransmissions. | `client/src/service_client.rs:57-60,67-103` |
| Simulator delivery | Shared queues remove the first packet or beacon from a different sender. They do not filter by destination or fan out broadcasts to every node. | `common/src/hal/simulator.rs:43-74` |
| Data acceptance | Simulator checks header presence and advertised payload length, but does not call `DataPacket::is_valid()` before returning a packet. | `common/src/hal/simulator.rs:129-151` |
| Discovery role | Any valid beacon is a potential server; there is no server-capability discriminator. | `client/src/discovery.rs:48-57` |
| Route learning | Beacon source becomes both destination and next hop; metric updates are unconditional. This is a neighbor table, not exchanged distance-vector routing information. | `forward/src/routing/dynamic_forwarding.rs:84-118` |
| Election priority | A receiver answers a higher-priority sender; otherwise it may initiate its own election. Completion always elects self, without retained candidate responses. | `forward/src/directory/election.rs:89-94,164-187,192-207` |
| Command queue capacity | Sixteen physical slots with one reserved to distinguish full from empty give fifteen usable queued commands. Configure and reboot handlers only acknowledge. | `server/src/api/cli.rs:11,30-36,84-98,120-134` |

The service directory remains a useful conceptual split between eligibility filters and preference scoring. However, beacon handling supplies assumed VideoRelay capabilities instead of decoding advertised capabilities (`forward/src/main.rs:169-193`). The score is not bounded to the previous report's claimed 110 points: bandwidth, latency and reliability terms multiply their base weights by margins (`forward/src/directory/service_directory.rs:46-87`). These values are policy examples, not measured link guarantees.

The application payloads also disagree. Client `send_video_data` emits 21 bytes containing a discriminator, service ID, frame number and three big-endian floats (`client/src/main.rs:194-227`). The server interprets the same discriminator as a six-byte sensor record beginning at offset zero, including that discriminator in temperature (`server/src/main.rs:122-136`). The receiver therefore does not decode the client's schema.

## Boundaries

- The upstream README explicitly calls the project an experiment. The inspected license is Eclipse Public License 2.0; this note summarizes behavior without importing source.
- No successful build, executed test, actual SLE packet exchange, SDK bridge, or chip architecture was established. BearPi-named files and a Cortex-M entry annotation cannot establish WS73-family compatibility.
- Role manifests enable `common/simulator`, but role source tests its own `feature = "simulator"`, which is not declared in those manifests (`client/Cargo.toml:10-13`; same pattern in forward and server). `common` uses `zerocopy` without a direct dependency in its manifest. Root dependencies do not automatically become member dependencies.
- `server/src/storage/mod.rs:1-28` is an unresolved DMA-oriented fragment, not the module declarations and Storage/SensorRecord definitions expected by the circular-buffer source. The presence of that separate file does not demonstrate an integrated storage component.
- Test files below nested `tests/unit/` and `tests/integration/` directories need a declared test target or importing harness; their existence alone is not Cargo test discovery evidence. The inspected root manifest declares neither.
- This harvest does not reproduce failure-triggering inputs or modify upstream code. It excludes PCB, firmware blobs, radio operations and security testing. Remote freshness was not established; conclusions are pinned to the inspected revision.

## Reusable

1. Keep the conceptual separation between node/time services and typed radio operations when designing an adapter above the WS73 SSAP transport, but implement and test a real backend rather than importing unresolved FFI names.
2. Specify a single versioned message envelope, distinguish final destination from next hop, and require source-level send/receive agreement before describing a control flow as implemented.
3. Distinguish simulated delivery, test source presence, executed tests and real-air evidence in future harvest verdicts. Destination-aware delivery and per-recipient broadcast semantics are prerequisites for credible mesh simulation.
4. Treat fixed-capacity neighbor and service tables as design references only. Their actual update clock, cleanup call sites and usable capacity must be verified before reuse.

## Comparison anchors

- [LINKNEBULA-MESH](../intel/LINKNEBULA-MESH.md): this note supersedes its claims of a 256-byte outer packet, active route-expiry integration, executable default simulator, 110-point maximum score and proven Hi2821/WS73-family mapping. The legacy document is retained unchanged under the knowledge-language rule.
- [SLE-MESH-EPAPER](../intel/SLE-MESH-EPAPER.md): provides the comparison vocabulary for real routing dissemination and SSAP-backed transport; LinkNebula's neighbor table and unresolved FFI must not be counted as equivalent implementations.
