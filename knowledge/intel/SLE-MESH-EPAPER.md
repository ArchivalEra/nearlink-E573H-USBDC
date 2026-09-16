---
type: intel
title: "SLE Mesh ePaper — Multi-hop SLE Mesh Design Deep-Dive"
language: zh
created: 2026-08-17
tags: [intel, mesh, epaper, multi]
sources:
  - "https://github.com/NearLink-ePaper/NearLink-Mesh-ePaper"
trust: B
stale_after: 2027-02-17
---

# SLE Mesh ePaper — Multi-hop SLE Mesh Design Deep-Dive

**Date:** 2026-08-17
**Scope:** Read-only local inspection of `https://github.com/NearLink-ePaper/NearLink-Mesh-ePaper/tree/main/` (repo now cloned; the earlier COMMUNITY-PROJECTS.md noted it missing). No network/build/hardware.
**Context:** Evaluating SLE multi-hop mesh capability for the TV-box (1 box + N sensor nodes) roadmap. This is the closest community implementation of a "NearLink mesh" — AODV/flow-control claims over SSAP on BearPi-Pico H3863 (Hi3863 = WS63 family chip).
**Companion docs:** `COMMUNITY-PROJECTS.md` (prior picture, now outdated on this repo's availability), `OSPL-SSAP-COMPARE.md`, `OSPL-DLI-CROSSCHECK.md`.

## Bottom line (TL;DR)

The README/feature-claims and the actual code diverge on several headline items, and the differences matter for us:

1. **The code is NOT AODV.** The shipped firmware implements a **proactive Distance-Vector (DV) routing** protocol carried inside HELLO-DV broadcasts (with Poison Reverse), not reactive RREQ/RREP/RERR. `mesh_types.h` message enum contains only `UNICAST/BROADCAST/HELLO`; there is no RREQ/RREP/RERR code path anywhere. The README's AODV tables (`README.md:535-635`) describe the earlier design; the DV rewrite (`mesh_route.c:1-3` header) is the current truth.
2. **Several headline features are compile-time DISABLED in the shipped config:** RLE decode (`image_receiver.c:4` `IMG_RLE_ENABLE 0`), per-packet ACK mode (`image_receiver.c:22` `IMG_MESH_NODE_ACK_ENABLE 0`), neighbor PULL repair (`image_receiver.c:55` `IMG_NEIGHBOR_PULL_ENABLE 0`), and Turbo dynamic connection-interval (`mesh_config.h:105` `MESH_TURBO_ENABLE 0`). What actually runs is: **DV routing + windowed bulk transfer + CHECKPOINT/RTT-based AIMD + bitmap selective retransmit + multicast-v3**.
3. **Per-hop forwarding is store-and-forward over one SLE connection per hop** (1 Server + up to 4 Client connections per node = 5 total, a chip hard limit, `mesh_config.h:39-41`). Broadcast/multicast is layer-2 relay flooding with a 256-entry (src,seq) dedup cache + 20 frames/1000 ms rate limit. There is no radio-level multicast.
4. **The mesh rides vendor in-chip SSAP** (`ssaps_*`/`ssapc_*` calls, MTU 520) — same API family our `stack/ssap/` reimplements host-side. Knowledge transfers at the protocol-design level; code does not port (chip-hosted vs our host-side WS73 dongle).
5. Measured perf (their numbers, 6-node tested scale): 10.8 KB 1bpp image 1-hop ~2-4 s, 2-hop ~6-10 s, 3-hop ~12-18 s (`README.md:762-765`). That is the reality check for "SLE multi-hop image streaming": throughput collapses with hop count; it is a *reliability/coverage* feature, not a bandwidth feature.

---

## Sources

All under `https://github.com/NearLink-ePaper/NearLink-Mesh-ePaper/tree/main/sle_mesh_networking/`:

| File | Role | Key refs |
|---|---|---|
| `README.md` / `README.md` (inner) | Design doc / feature claims | AODV, AIMD params, perf table |
| `CMakeLists.txt` | 3 build modes (server-only / client-only / mesh) | `:3-51` |
| `Kconfig` | Node-type choice, ePaper pins | `:24-33` |
| `mesh/common/mesh_types.h` | 16 B frame header, msg enum, conn entry | `:19-49`, `:72-87` |
| `mesh/common/mesh_config.h` | All knobs (92 params) | `:39-41` (conn limits), `:98-108` (SLE params) |
| `mesh/common/mesh_api.h` | App-facing API | `:23-64` |
| `mesh/network/mesh_main.c` | Entry, main loop, topo collect, tree converge | `:989-1276`, `:346-566` |
| `mesh/network/mesh_transport.c` | Conn pool, dual role, zombie, P22 heal, Turbo, QoS | `:124-191`, `:717-828`, `:830-881` |
| `mesh/network/mesh_forward.c` | Frame build/validate, dedup, flood, unicast fwd, HELLO-DV | `:246-309`, `:314-404`, `:480-532` |
| `mesh/network/mesh_route.c` | **DV routing table + poison reverse** | `:228-262`, `:264-308`, `:312-333` |
| `mesh/gateway/ble_gateway.c` | BLE GATT bridge + **AIMD FC engine** + multicast | `:157-210`, `:1258-1316`, `:1319-1504`, `:1513-2160` |
| `mesh/image/image_receiver.c` | RX state machine, bitmap, CHECKPOINT reply | `:307-399`, `:546-645` |
| `sle_uart_server/sle_uart_server.c` | SSAP server: register service/property/descriptor | `:253-289`, `:339-353`, `:376-468` |
| `sle_uart_server/sle_uart_server_adv.c` | Advertising, EFUSE Die-ID address, adv params | `:246-323`, `:25-32` |
| `sle_uart_client/sle_uart_client.c` | Scan/connect/discover, outgoing tracking, backoff | `:443-478`, `:500-715`, `:851-913`, `:1036-1135` |
| `sle_uart.c` | Entry bridging (server-only/client-only modes) | `:285-306` |

Comparison side: our `stack/ssap/` (`include/hwsle_transport.h`, `include/ssap_link.h`).

---

## 架构 (Architecture)

**Component split (4 layers + app):**

- **Transport layer** `mesh_transport.{h,c}` — SLE connection pool (fixed 5 entries: 1 Server + 4 Client, `mesh_config.h:39-41`), per-conn state FSM (IDLE→WAIT_ID→ACTIVE→STALE, `mesh_types.h:64-69`), data send via `ssaps_notify_indicate` (server role) or `ssapc_write_req` (client role) (`mesh_transport.c:370-389`), CRC16 not here but in forward layer, zombie cleanup, Turbo, P22 partition-heal.
- **Routing layer** `mesh_route.{h,c}` — flat static route table (`MESH_ROUTE_TABLE_SIZE=128`, `mesh_config.h:86`), entries `{dest, next_hop, hop_count, expire_ms, valid}` (`mesh_route.h:21-27`), learned from HELLO-DV broadcasts with Poison Reverse.
- **Forwarding layer** `mesh_forward.{h,c}` — 16 B frame header + CRC16-CCITT, unicast/broadcast dispatch, 256-entry dedup ring keyed `(src<<16)|seq` (`mesh_forward.c:22-32,73-93`), flood rate limit 20/1000 ms (`mesh_forward.c:98-114`), reverse-route learning on received frames (`mesh_forward.c:480-498`).
- **Application layer** — `ble_gateway.c` (BLE GATT phone ingress + **AIMD Flow-Control engine** + multicast + topology collection) and `image_receiver.c` (RX reassembly state machine + ePaper render trigger).
- **Link/physical** — vendor SLE stack (`sle_connection_manager`, `sle_device_discovery`, `sle_ssap_server/client`) with vendor SSAP profiles; BLE 5.0 for the phone path; ePaper via SPI.

**Data flow (image path):**
`Phone (BLE GATT write, 0xAA-prefixed) → ble_gateway.c gw_write_req_cbk (:2656) → mesh_gateway_inject (:927) → mesh_forward_send_unicast/broadcast → per-hop forward → receiver image_receiver_on_data → IMG_STATE_DONE → epaper_trigger_mesh_image (mesh_main.c:1258-1268).` For multicast: gateway broadcasts IMG_CMD_MCAST_START (target list), then data as layer-2 floods; each target independently reports CHECKPOINT_ACK / MISSING / RESULT (`ble_gateway.c:2437-2529`).

**Build targets:** single firmware image, one Kconfig choice for node role (`Kconfig:24-33`): `SAMPLE_SUPPORT_SLE_MESH` (full mesh, all nodes identical — the deployed config), plus a legacy `..._SERVER`/`..._CLIENT` UART-passthrough pair (`sle_uart.c:16-27`). Mesh entry is `app_run(sle_mesh_entry)` creating one `SLEMeshTask` 8 KB stack (`mesh_main.c:1342-1355`, `mesh_config.h:114`). All nodes run the same image; roles auto-negotiated (ePaper present → receiver, phone/PC present → gateway, else relay).

---

## Mesh 协议设计 (Mesh protocol design)

### Routing — DV, not AODV (headline finding)

- Route entries have **no sequence numbers** (`mesh_route.h:20-27`); the README's "sequence number anti-loop" AODV design (`README.md:176`) is not in the code. Message enum is only `UNICAST=0x01 / BROADCAST=0x02 / HELLO=0x20` (`mesh_types.h:31-35`). No RREQ/RREP/RERR constants anywhere.
- **HELLO-DV:** every HELLO interval (5 s, `mesh_config.h:49`) each node sends per-neighbor HELLO carrying its full DV summary: `[own_addr 2B][entry_count 1B][(dest 2B | hops 1B)]…` (`mesh_forward.c:612-647`). TTL is forced to 1 (one hop) (`mesh_forward.c:639`). Poison Reverse: routes whose next_hop == target neighbor are advertised as hops=0xFF (`mesh_route.c:264-308`).
- **Learning:** receiver parses sender's DV, adds +1 hop, skips self and >TTL, skips already-direct destinations, refreshes the sender as 1-hop route (`mesh_route.c:228-262`). Reverse routes are also learned from any forwarded frame (`mesh_forward.c:480-498`).
- **Route lifetime:** `4 × HELLO_INTERVAL = 20 s` (`mesh_route.c:53-56`, `MESH_DV_ROUTE_EXPIRE_MULT`). Route cleanup runs every 10 s in main loop (`mesh_main.c:1096-1108`). This is fast, active-learning routing — good for small meshes, chatty for large ones.
- **Link-break handling:** on disconnect, all routes via the broken next_hop are invalidated locally (`mesh_route.c:312-333`); there is **no RERR notification** — recovery relies on the next HELLO cycle + DV relearning. `mesh_route_is_network_stable()` tracks an invalidation burst window so the FC engine delays a transfer during topology churn (`mesh_route.c:19-46`).
- **Unicast forwarding policy** (`mesh_forward.c:314-371`): (1) direct neighbor → send direct; (2) route table next-hop → directed forward; (3) **no route → rate-limited flood** to all neighbors (fallback that makes sparse/partial-topology nets still work). Send-side: if no route, packet is **dropped** (`mesh_forward.c:572-577`) — no on-demand discovery.
- **Topology self-discovery:** gateway broadcasts `0xFE 0x01` (with gateway addr, tripled), each node replies **unicast** `0xFE 0x02` with own addr, root hop, free server/client slots, and merged neighbor+route list (`mesh_main.c:683-757`, `mesh_config.h:126-142`). Gateway collects into a 64-node table and pushes to the phone (`ble_gateway.c:578-733`). There is also a gateway-orchestrated **global topology rebuild** (`0xFE 0x10-0x15`, `ble_gateway.c:735-866`, `mesh_main.c:735-865`) that re-parents nodes toward a tree rooted at the gateway, one node per round, plus a local **tree-convergence loop** that picks the best server-side parent by root hop and prunes redundant uplinks (`mesh_main.c:346-566`). This "gateway-as-root logical tree over a DV mesh" is a notable design to lift.

### Per-hop relay: one SLE connection per hop, store-and-forward

- A node's neighbors are its SLE connections (pool of 5). There is no layer-2 broadcast channel; "broadcast" = `mesh_transport_send_to_all_neighbors` looping over all ACTIVE conn entries (`mesh_transport.c:419-447`).
- Relay nodes are normal mesh nodes that happen to have no app role: `mesh_forward_on_data_received` forwards UNICAST (not-for-me) and re-floods BROADCAST after TTL--, hop++ and CRC recompute (`mesh_forward.c:128-138,314-404`). Forwarding is **paused during an active FC transfer** (global `mesh_transport_tx_lock`, `mesh_forward.c:317-319`) to avoid concurrent SLE sends; QoS backpressure (`sle_link_qos` callback, `mesh_forward.c:321-323`) also stalls forwarding when the link is busy.
- **Fragmentation/reassembly:** is *app-layer*, not mesh-layer. The gateway slices the image into 480 B FC packets (`IMG_FC_PKT_PAYLOAD`, `mesh_config.h:162`; `ble_gateway.c:1115-1148`), each wrapped in a `mesh_broadcast`/`mesh_send` frame whose max payload is 504 B (`mesh_types.h:22`, MTU 520 − 16 B header). Receiver reassembles by seq into a 96000 B static buffer (`image_receiver.h:15-23`), 200 packets max, with a **25-byte rx bitmap**.
- **Reliable transfer — AIMD + CHECKPOINT + bitmap retransmit** (the heart of the project):
  - FC state machine: `FC_IDLE→FC_START_WAIT→FC_SENDING→FC_WAIT_CHKPT→FC_SEND_END→FC_WAIT_RESULT→FC_RETRANSMIT` (`ble_gateway.c:202-210`).
  - Windowed bulk: sends up to `window` packets, then CHECKPOINT; receiver replies CHKPT_ACK with cumulative rx count; gateway computes per-segment loss and calls `fc_adjust` (`ble_gateway.c:1258-1316`): 0% loss → slow-start double window until ssthresh, then linear +8/+5/+3 per hop; <15% loss → unchanged; 15-40% → window/2, +10 ms delay, ssthresh=window/2; >40% → reset to W_MIN, D_MAX, back to slow start. Bounds W=8..80, D=0..50 ms (`ble_gateway.c:159-165`).
  - **Per-hop adaptive init** (`ble_gateway.c:1338-1403`): hops 1→W=25/D=5/TO=1500; 2→W=20/D=7/TO=3000; 3-4→W=15/D=10/TO=5000; 5+→W=12/D=15/TO=10000; multicast→W=10/D=20/TO=8000 conservative (hops=5 assumed).
  - **RTT/RTO** is RFC-6298 SRTT/RTTVAR with Karn's algorithm on retransmitted CHECKPOINTs; RTO = SRTT + 4×RTTVAR clamped to per-hop minimums (600 ms 1-hop … 3500 ms 5+hop, 5000 ms mcast) and 10 s max (`ble_gateway.c:1712-1747`, `:191-198`). CHKPT retry uses exponential backoff; after retry budget, the whole segment is resent from W_MIN (`ble_gateway.c:1827-1932`).
  - **Bitmap repair:** receiver sends `0x87 MISSING` `[cnt 2B][bitmap]` on END; gateway retransmits only marked seqs (`ble_gateway.c:2376-2424`, `image_receiver.c:214-242`), max 5 rounds (`image_receiver.h:26`). Whole-image CRC16 in END (`image_receiver.c:554-611`).
  - **ACK proxy v2.2** (`ble_gateway.c:2632-2651`): in ACK-mode, the gateway locally ACKs the phone immediately and lets the FC engine feed the mesh async — removes phone round-trip from the critical path.
  - **Multicast-v3** (`ble_gateway.c:2437-2529`): broadcast data frames; per-target independent CHKPT_ACK/RESULT; OR-merges MISSING bitmaps; adaptive quorum "方案E" (normal = all targets, degraded = expected−1 when a slow node is detected, `ble_gateway.c:318-329,2325-2340`); sentinel values 0xFFFE (done/cooldown) and 0xFFFF (not receiving) for fast terminal states; MCAST→unicast fallback for failed targets (`ble_gateway.c:374-442`).

### Network hygiene (worth copying)

- 256-entry dedup cache + flood rate limit + self-loop drop (`mesh_forward.c:433-448`).
- Zombie detection 3-D (RX timeout 30 s, ≥10 consecutive send fails, one-way server without peer echo; WAIT_ID timeout) then `sle_disconnect_remote_device` (`mesh_transport.c:717-828`).
- conn_id conflict mitigation, exponential backoff table, reconnect memory list (60 s TTL) (`sle_uart_client.c:98-347`).
- P22 partition healing: bidirectional-connection collapse, isolation slot release, redundant-server loop break, forced re-announce after 120 s of zero connections (`mesh_transport.c:907-1014`, `mesh_main.c:1155-1229`).

---

## SLE 链路用法 (SLE link usage)

**SSAP profiles (vendor in-chip stack):**
- Server: `ssaps_register_server` (app UUID 0x1234) → `ssaps_set_info(MTU 520)` → `ssaps_add_service_sync` (0x2222) → `ssaps_add_property_sync` (0x2323, READ|WRITE perms, READ|NOTIFY op indication) → `ssaps_add_descriptor_sync` (USER_DESCRIPTION, value `{0x01,0x00}` = notify enabled by default) → `ssaps_start_service` (`sle_uart_server.c:253-289`). Send = `ssaps_notify_indicate(server_id, conn_id, …)` per connection (`sle_uart_server.c:339-353`).
- Client: name-prefix scan → `sle_connect_remote_device` → on CONNECTED: `ssapc_exchange_info_req(MTU 520)` → `ssapc_find_structure` (type=PROPERTY, 1..0xFFFF) → write via cached property handle → `ssapc_write_req(0, conn_id, param)` (`sle_uart_client.c:1036-1135`, `:851-913`). Async write confirm feeds zombie accounting (`sle_uart_client.c:1110-1135`).
- **Connection concurrency:** 1 server inbound + 4 client outbound = **5 total per node** — chip hard limit (`mesh_config.h:39-41`). Non-mesh UART client default is 8 (`sle_uart_client.c:50-55`).
- **Connection management:** single unified `sle_connect_state_changed_cbk` in the server module; direction disambiguated by outgoing-address tracking; ghost devices (derived mesh addr 0/0xFFFF/0xFFFE) rejected at accept time (`sle_uart_server.c:376-418`). Pre-set DLE `sle_set_data_len(conn_id, 512)` on connect (`mesh_transport.c:184,250`).
- **PHY/link params:** adv interval 0xC8×125 µs = 25 ms, conn interval 0x64×125 µs = 12.5 ms default (supervision 8 s) (`sle_uart_server_adv.c:25-32`); mesh config re-sets conn interval to 0x28×125 µs = 20 ms (`mesh_config.h:98-100`). Turbo path (disabled) would drive `sle_update_connect_param` to 10 ms + `sle_set_phy_param` 2M/1M + DLE 512 (`mesh_transport.c:830-881`).
- **QoS flow control:** registers `sle_transmission_register_callbacks(&{send_data_cb})`; busy/FLOWCTRL state gates all sends and forwarding (`mesh_transport.c:49-61,884-887`; `mesh_forward.c:321-323`; `ble_gateway.c:1624-1627`).
- **Data plane size:** FC payload 480 B/image packet, mesh max payload 504 B, MTU 520. Perf (their measured): 10.8 KB image FAST 1-hop ~2-4 s, 2-hop ~6-10 s, 3-hop ~12-18 s; 6-node multicast 5-8 s (1-hop) / 15-20 s (2-hop); 60 KB JPEG 1-hop 8-15 s (`README.md:762-768`). 96 KB RX buffer, ~143 KB total heap, FC cache 50 KB on gateway (`README.md:783-793`, `ble_gateway.c:120`).

---

## 与 SSAP 关系 (Relation to standard SSAP)

- The mesh uses the **vendor in-chip SSAP library directly** (`ssaps_*` server / `ssapc_*` client), the same API family as NearLinkSLE and FallDetection in our community batch, with the same registration order and the same **mandatory CCCD/descriptor** (their descriptor value `{0x01,0x00}` default-enables notify, `sle_uart_server.c:215-248`). MTU 520 matches.
- The app protocol is **payload-over-SSAP**: mesh frames ride inside SSAP Write/Notify values; there is no SSAP service-architecture multiplexing — one service (0x2222/0x2323) carries all mesh control+data, plus a separate BLE GATT service (0xFFE0, `ble_gateway.c:32-35`) for the phone.
- Our stack (`stack/ssap/`) differs in two ways that the mesh design does not depend on: (a) we run **host-side** on the WS73 dongle — `hwsle_transport.h:6-11` puts SSAP PDUs onto `/dev/hwsle` ACB frames (`0xA3`, tcid `0x0A` = SLE_SMTC); (b) connection management is **DLI/CM** (`ssap_link.h:20-33`: `DLI_CREATE_CONNECTION 0x1401`, `DLI_SET_DATA_LEN 0x1804`, etc.), not the in-chip `sle_connect_remote_device` API. The mesh layer (DV routing, dedup, AIMD) sits *above* these, so the mesh design ports to our architecture as an application layer — what transfers is the **protocol design and parameterization**, not the link calls.

---

## 可借鉴清单 (Borrow list for TV-box 1 box + N nodes)

1. **Routing model: proactive DV over HELLO beats reactive AODV for a small sensor mesh.** 128-entry flat route table, 3-byte-per-entry DV, poison reverse, 20 s lifetime, per-neighbor HELLO TTL=1. For ≤16-32 nodes this is simple, self-healing, and needs no RREQ flood. Our host-side stack can implement this DV directly over SSAP unicast without radio multicast.
2. **Store-and-forward relay on top of a small connection pool** (1+4) is the right primitive for a dongle/TV-box: every node is a relay; "broadcast" = fan-out over connections. Copy the `send_to_all_neighbors(exclude_conn_id)` semantics and the forwarding pause during bulk transfer (F30 TX lock).
3. **Frame header design:** 16 B `{magic 0xAE, ver, type, ttl, src, dst, seq, len, hop, crc16}` with per-hop TTL--/hop++ and CRC recompute (`mesh_types.h:38-49`); `(src,seq)` dedup ring + 20 f/s flood cap. Directly reusable on our SSAP payload framing.
4. **AIMD engine parameters** are a concrete starting point: W 8..80, D 0..50 ms, ssthresh ~30-50, per-hop init table (1-hop W25/D5/TO1500 … 5+ W12/D15/TO10000), RFC-6298 RTT + Karn, RTO = SRTT+4·RTTVAR with per-hop floors, segment-resend on CHKPT exhaustion. For a TV-box feeding N sensor nodes, use the **multicast conservative profile** (W10/D20/TO8000) and the **quorum/OR-bitmap/0xFFFE-sentinel** machinery — this is exactly the "push update to N devices reliably" problem we face.
5. **Bitmap selective retransmit** (25 B covers 200 pkts) + whole-image CRC16 in END + limited repair rounds (5) + RESULT retransmit (3×1 s) is a clean "best-effort until CRC, then NACK" contract.
6. **Gateway-rooted logical tree** over the flat mesh (tree-convergence + orchestrated re-parenting with PREPARE/COMMIT two-phase switch) is the right answer for a star-around-TV-box topology where the box is the natural root — worth lifting wholesale.
7. **Network hygiene:** zombie 3-D detection, per-peer exponential backoff + give-up + age-out, reconnect-memory, partition-heal slot release. These are the "production hardening" bits that took this project 20+ patches to get right.
8. **Operation knowledge:** per-node MTU 520, DLE pre-set 512, `sle_set_phy_param` 1M/2M switch, QoS `send_data_cb` credit-gating before every send — all consistent with our `sle_measure_sdk`/PHY findings in COMMUNITY-PROJECTS.md.

---

## 局限 (Limitations / transfer boundaries)

**Cannot port directly:**
- Firmware is for BearPi-Pico **H3863/WS63-family** (LiteOS, in-chip `ssaps_*`/`ssapc_*`, `sle_connection_manager`, `sle_device_discovery`). It will not run on the WS73 dongle (host-side Linux, no in-chip SSAP; we drive `/dev/hwsle` + DLI).
- BLE phone-ingress GATT code (`bts_gatt_*`) is chip-side; ePaper/SPI/EFUSE Die-ID code is hardware-specific.
- The 5-connection pool limit is H3863-specific; WS73 may differ (needs HW validation).

**README ≠ code caveats (do not copy the marketing numbers):**
- No AODV/RREQ/RREP/RERR in code — README's routing section is aspirational.
- `RLE`, per-packet ACK, PULL neighbor-repair, and Turbo are compile-time **off** in the shipped image (`image_receiver.c:4,22,55`, `mesh_config.h:105`). The measured perf table (`README.md:762-768`) includes RLE/Turbo rows that do not correspond to what the shipped config runs.
- Tested scale is **6 nodes**, 1-2 hop mixed topology; larger sizes (32-64 node claims) are untested configuration targets.

**Performance reality for our roadmap:** multi-hop SLE is a coverage/reliability feature. Per their own data, going 1→2→3 hops multiplies transfer time ~3-5× per hop even with their flow control; per-packet ACK is ~4× slower than windowed FAST. For a TV-box + N sensor nodes, plan on **single-hop fan-out where possible** and use mesh for coverage/backhaul, not for high-rate fan-in.

**What transfers (knowledge):** the DV routing tables, HELLO-DV/poison-reverse semantics, frame format, dedup/flood-control, AIMD/RTT/RTO parameterization, bitmap repair, multicast quorum/OR-merge, gateway-rooted tree convergence, and the whole network-hygiene playbook. All are host-stack-friendly and match our `stack/ssap/` layering (SSAP over tcid 0x0A).

---

## 结论 (Conclusion: SLE multi-hop mesh feasibility for our roadmap)

1. **Feasible, and now proven twice-over in the community** (LinkNebula Rust mesh + this C mesh): multi-hop SLE mesh works on vendor chips by putting a network layer *above* standard SSAP, one SSAP connection per hop, store-and-forward, DV routing + dedup + flow control. There is no fundamental blocker in SLE for a 1-box + N-node network.
2. **The hard part is not routing — it is throughput collapse and link hygiene.** The project's own numbers show 3-hop transfers 5-9× slower than 1-hop; most of its 20+ patches fight chip-side quirks (conn_id reuse bugs, ghost connects, zombie links, notify path stalls). Expect to spend comparable effort hardening our WS73 link layer before the mesh layer pays off.
3. **Recommended shape for us:** host-side `stack/ssap/` gains (a) a connection-pool abstraction with dual-role + direction tracking, (b) proactive DV routing carried as SSAP payload (this project's HELLO-DV format is directly reusable), (c) the AIMD/bulk-transfer engine parameterized per hop with the multicast conservative profile for 1→N pushes, and (d) the gateway-rooted tree convergence. The TV-box is a natural mesh root; sensor nodes are leaves that also relay.
4. **Next actions:** (1) validate WS73 max concurrent connections (is the 1+4 cap a chip-wide or H3863-specific limit?) via our dongle; (2) prototype single-hop bulk transfer + bitmap repair over our SSAP/DLI stack first (perf gate), then add DV routing; (3) treat this project as the reference design and behavioral oracle, not a code source.
