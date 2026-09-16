---
type: harvest
title: "NEW: sle_mesh v4.4.x Delta + fbb_bs2x_rust (Rust-in-Firmware Precedent)"
language: zh
created: 2026-09-05
tags: [harvest, mesh, delta, bs2x]
sources:
  - "https://github.com/BH4ME/sle_mesh"
  - "https://github.com/sanchuanhehe/fbb_bs2x_rust"
trust: B
stale_after: 2027-03-05
---

# NEW: sle_mesh v4.4.x Delta + fbb_bs2x_rust (Rust-in-Firmware Precedent)

**Date:** 2026-09-03
**Scope:** Read-only local inspection of two new trees on the archive disk. No network/build/hardware.
- `https://github.com/BH4ME/sle_mesh/tree/main/` — **BH4ME/sle_mesh**, latest release **v4.4.138** (2026-06-10, commit `cc0b0dc`); v4.4.9 was 2026-05-31. 19 MB, 164 release directories under `versions/`.
- `https://github.com/sanchuanhehe/fbb_bs2x_rust/tree/master/` — **sanchuanhehe/fbb_bs2x_rust**, Rust fork of the HiSilicon **FBB bs21e (BS2Xe)** firmware SDK, single shallow commit `08be0e4` (2025-07-11). 659 MB.
**Context:** (a) delta vs the earlier ePaper mesh analysis in `SLE-MESH-EPAPER.md` (HELLO-DV routing, AIMD, dedup — what is new in the v4.4 series); (b) the first local precedent for **Rust compiled into a HiSilicon NearLink firmware image**, as a reference for `rust-ws73/firmware` (ticket 04, option B).
**Companion docs:** `SLE-MESH-EPAPER.md`, `LINKNEBULA-MESH.md`, `RUST-WS73-UNSAFE-FFI.md`, `PERF-RUST-PATTERNS.md`.

---

# Part A — sle_mesh (BH4ME) v4.4.x: what changed vs the ePaper mesh

## A.0 Identity and version anchoring

- This is a **different project from NearLink-Mesh-ePaper**, not a new version of it. ePaper (BearPi H3863, image streaming over SLE mesh) vs sle_mesh (WS63/BearPi H3863, "team network" walkie-talkie-style group mesh with leader/member/relay roles, GPS position reports, WebUI). Both share the BearPi H3863/WS63 + vendor in-chip SSAP substrate, and both reuse the `sle_uart_server`/`sle_uart_client` sample skeleton, but the mesh layers are unrelated designs.
- Version story: task brief said "v4.4.9"; the repo is far past that. `versions/` holds **164 release dirs** from v1.2.2 to **v4.4.138** (`versions/` listing; `versions/README.md:5-9`). v4.4.9 is dated **2026-05-31** (`versions/v4.4.9/VERSION.md:3`), v4.4.138 is dated 2026-06-10 (git log, commit `cc0b0dc "Release WS63 firmware v4.4.138"`). So the v4.4 series shipped **~140 point releases in ~6 weeks** — a relentlessly iterative release cadence, one behavioral change per release, each with a VERSION.md + MANIFEST.md evidence record.
- What v4.4.9 itself was: a **reconnect fix** for member reboot. Root cause hypothesis: client `ssapc_exchange_info_req()` sent too early — from the CONNECTED branch even when `pair_state == SLE_PAIR_NONE`, instead of deferring until `pair_complete` (`versions/v4.4.9/VERSION.md:23-33`). Fix: branch on `SLE_PAIR_NONE` → request pairing; `SLE_PAIR_PAIRED` → exchange immediately; otherwise defer to pair-complete (`versions/v4.4.9/VERSION.md:35-40`). Notable detail: member-side **MTU 208** (not 520 as in ePaper) seen in the disconnect log (`versions/v4.4.9/VERSION.md:17-22`).
- After v4.4.9 the series continued through exchange-info dedup (v4.4.8), adv-restart placement contract tests (v4.4.6: "forbids `sle_uart_server_adv_restart()` in the connected branch"), scan-start placement (v4.4.7), NV allowlist persistence (v4.4.4, key `0x5002`, `versions/v4.4.4/VERSION.md:16`), battery ADC (v4.4.100), relay failover route cleanup (v4.4.130), and ends on a **WS2812C reset-pulse fix (80→320 µs)** in v4.4.138 (`versions/v4.4.138/VERSION.md:15-17`).

## A.1 Architecture of the v4.4 "team network" firmware

**Unified firmware, runtime roles.** One `.fwpkg` for all nodes; leader/member (and derived relay) role is selected at runtime by serial `cfg` commands or the board WebUI, persisted, and applied on reboot (`README.md:57-63` serial command set; `xc/ws63_team_network/README.md:10-16`). This is the headline difference from ePaper, whose roles auto-negotiated from hardware presence (ePaper receiver/gateway detection).

**Layer split** (cleaner than ePaper's monolith):
- **Protocol core is platform-free:** `src/sle_team_packet.c` (347 lines) + `include/sle_team_packet.h` implement framing with zero SDK dependencies. `src/sle_team_node.c` (1580 lines) is the mesh FSM, also SDK-free — it talks to the platform only through an ops vtable (`include/sle_team_node.h:90-102`: `send/now_s/rssi_dbm/battery_percent/log/on_joined/on_position/on_alert/on_relay_offline/should_defer_member_timeout`). This is why they can unit-test it on the host (`examples/relay_failover_suite.c:1-8` includes only `sle_team_node.h` + stdio) and simulate the whole protocol in Python (`tools/sle_team_python_sim.py`).
- **Platform glue:** `xc/ws63_team_network/src/ws63_team_network_app.c` (8863 lines) — SSAP server/client adapters, connection tracking, route table, Web API, ST7789 display, WS2812, buzzer, GPS, CLI.

## A.2 Protocol delta: team-ID envelope replaces the 16-byte MAC frame

ePaper frame (`mesh_types.h`): 16 B header `{magic 0xAE, ver, type, ttl, src MAC, dst MAC, seq, len, hop, crc16}`, 504 B payload, MTU 520.

sle_mesh v4.4 packet (`include/sle_team_packet.h`):
- Two-layer envelope: **mesh packet** `{version, payload_type, route_type, transport_code_1/2, path_hash_size, hop_count, path[64], payload_len, payload[184]}` (`sle_team_packet.h:60-72`) wrapping an **app packet** `{app_msg_type, flags, seq, team_id, src_id, dst_id, ttl, body}` (`sle_team_packet.h:74-83`). Max payload is only **184 B** (`sle_team_packet.h:11`) — this protocol is for small control/telemetry frames, not bulk image transfer.
- Addresses are **1-byte logical member IDs** (team_id + src_id + dst_id, broadcast `0xFF`), not MACs. Real MAC is carried once in the HELLO body for the WebUI (`sle_team_packet.h:85-92`). Route identity comes from the advertising payload / SLE address → `team_route_id_from_mac()` derives self_id from the device MAC (`ws63_team_network_app.c:3055`, `:3163`).
- Group frames carry a **1-byte channel_hash + 2-byte cipher MAC** wrapper for group-channel discrimination (`sle_team_wrap_mesh_group_data`, `sle_team_packet.h:166-171`); receiver drops frames whose `channel_hash` mismatches (`sle_team_node.c:1372-1375`) and rejects foreign `team_id` (`sle_team_node.c:1380-1383`). Cheap "channel + team" filtering, no real crypto.
- **Path source records:** optional per-hop path of 1-3 byte **path hashes**, up to 63 hops and 64 B of path (`sle_team_packet.h:11`, `sle_team_make_path_length` at `sle_team_packet.c:45-53`). The packet header packs `(hop_count, path_hash_size)` into one byte (`sle_team_packet.c:98-99,148-154`).
- Message taxonomy is much richer than ePaper's `UNICAST/BROADCAST/HELLO`: `REQ/RESPONSE/TEXT/ACK/ADVERT/GROUP_TEXT/GROUP_DATA/ANON_REQ/PATH/TRACE/MULTIPART/CONTROL/RAW_CUSTOM` (`sle_team_packet.h:31-47`) and app-level `HELLO/HEARTBEAT/POS_REPORT/ALERT/CONFIG/ACK/ROUTE_UPDATE` (`sle_team_packet.h:50-58`). Routing control is now an explicit in-band message: `ROUTE_UPDATE` body `{parent_id, next_hop_id, parent_state, flags}` with a **relay-grant flag** (`sle_team_packet.h:127-133`) — the leader grants/revokes relay capability over the air.

## A.3 Routing delta: tiered directed relay replaces HELLO-DV flooding

This is the biggest conceptual change vs `SLE-MESH-EPAPER.md` finding #1 (proactive DV over HELLO, poison reverse, 128-entry route table, fallback flooding).

**sle_mesh v4.4 has no HELLO-DV, no poison reverse, no fallback flood.** Instead:

1. **Leader-rooted tier tree via "buckets".** A member's tier is a pure function of its member ID: `(id-1) % 3 + 1`, leader = bucket 0 (`team_route_bucket_from_ids()`, `ws63_team_network_app.c:3237-3244`). Relay forwarding direction is decided by comparing source bucket vs self bucket: src closer to leader than self → send downstream; src further → send upstream; leader-bound always upstream (`team_sle_send()`, `ws63_team_network_app.c:8088-8104`). Traffic funnels toward the leader by construction.
2. **Directed relay policy, not flooding.** `sle_team_should_relay_packet()` (`sle_team_node.c:427-445`): relay a broadcast **only if it originated from the leader**; relay unicast only if src==leader or dst==leader. A plain member never relays member-to-member traffic. Compare ePaper: every node re-flooded broadcasts with TTL-- plus a 256-entry dedup ring and a 20 frames/s flood cap. sle_mesh avoids the dedup problem *by not flooding* — there is **no dedup cache at all** in the node core; per-member `last_seq` is used only for stale-member pruning, not duplicate suppression (`sle_team_node.c:1012`, `:1176`, `:1208`).
3. **Capability-gated relaying.** A member only relays if the leader granted `relay_allowed` and `relay_enabled` (`sle_team_relay_may_bridge_packet()`, `sle_team_node.c:403-425`); a `relay_discovery_only` mode restricts bridging to HELLO/ROUTE_UPDATE/CONFIG/ACK control frames (`sle_team_discovery_only_allows()`, `sle_team_node.c:395-401`). Pre-join and reselecting members may bridge only discovery traffic (`sle_team_node.c:416-424`). Demotion is enforced at the link layer: when a CONFIG packet revokes relay, the glue **drops all downstream child connections** (`team_member_drop_relay_children("config-demote")`, `ws63_team_network_app.c:8371-8401`).
4. **Route table is per-connection, not per-neighbor DV.** `team_route_entry_t {member_id, conn_id, dir(UPSTREAM/DOWNSTREAM), next_hop_id, last_seen_s}` (`ws63_team_network_app.c:462-469`), with lookup that validates the conn is still active and falls back to next-hop routes (`team_route_find()`, `ws63_team_network_app.c:3792-3816`), stale-entry sweeping (`:3982`), and **clear-by-conn / clear-by-next-hop with failover-preservation hooks** (`:3819-3860`). v4.4.130 fixed relay failover by "clearing stale routes through the lost relay instead of preserving dead next-hop routes" (`versions/v4.4.130/VERSION.md`, relay failover item).
5. **Parent switching is explicit.** Member tracks `upstream_parent_id/state` (IDLE/DISCOVERING/CONNECTED/RESELECTING, `sle_team_node.h:31-36`) and runs `sle_team_node_try_parent_switch()` when the parent goes quiet (`parent_timeout_s = heartbeat_timeout/2`, `ws63_team_network_app.c:8317`; tick logic `sle_team_node.c:1313-1320`). Un-joined members re-HELLO the leader every **3 s** until joined (`sle_team_node.c:1322-1332`).
6. **Connection budget:** the ePaper 1-server+4-client cap is gone from the design — the leader runs a **"1vs8" client with 8 concurrent SLE connections** (`SLE_UART_CLIENT_MAX_CON 8`, `xc/ws63_team_network/sle_uart_client/sle_uart_client.c:36,62`), and a relay member runs server (upstream) + client (downstream) simultaneously. So on WS63/H3863, **8 concurrent client connections are achievable**, which directly informs our WS73 "max connections" validation question (`SLE-MESH-EPAPER.md` next-action 1).

## A.4 Application layer delta: telemetry/teaming replaces image streaming

- Body codecs for HELLO `{device_id, role, battery, mac}` (`sle_team_packet.h:85-92`), HEARTBEAT `{battery, rssi, fix_status}` (`:94-98`), POS_REPORT `{lat_e6, lon_e6, speed_cms, heading_deg, battery, fix, sats}` (`:100-106`), ALERT `{lost_member_id, reason∈{distance,timeout,low-battery,leave}, last position+time}` (`:108-114`), CONFIG `{report/heartbeat intervals, warn/lost distance, heartbeat timeout, relay_allowed, relay_tier, max_downstream}` (`:116-125`), ACK `{ack_seq, acked_msg_type, status_code}` (`:134-138`).
- Timing defaults: heartbeat every **1 s**, position report every **5 s**, heartbeat timeout **3 s** (`xc/ws63_team_network/Kconfig:298-320`). Leader prunes members on timeout and broadcasts an ALERT with the last known GPS fix (`sle_team_prune_stale_members()`, `sle_team_node.c:454-495`), with a defer hook so a relay loss does not instantly time out its hidden children (`sle_team_should_defer_member_timeout`, `sle_team_node.c:42-49`).
- This is effectively a **Meshtastic-shaped SLE mesh** (team channel hash, member IDs, position/telemetry/alert messages, pairing/allowlist) — a second, independent community proof that a multi-hop SLE mesh is viable, but optimized for tiny periodic payloads rather than bulk transfer.

## A.5 Reliability delta: what replaced AIMD/CHECKPOINT/bitmap

The ePaper AIMD flow-control engine (window/CHECKPOINT/RTT/bitmap repair) has **no counterpart** in sle_mesh — there is no bulk path to control. Its reliability machinery is instead:
- App-level ACK message with per-message seq (`sle_team_build_ack`, `sle_team_packet.h:134-138`; leader ACKs HELLO at `sle_team_node.c:1035`).
- Link-level failure accounting: TX failure reasons logged (`NO_MEMBER/NO_ROUTE/WRITE_FAIL/NOT_READY/RESELECT_PARENT/FORMAT`, `ws63_team_network_app.c:8004-8143`) and fed into `team_member_upstream_recover_after_tx_fail()` which drives reconnection.
- The hardening effort went into the **SLE connection lifecycle** instead: the exchange-info/pair-state ordering saga (v4.4.7-v4.4.10, §A.0), adv-restart/scan-start placement contract tests (v4.4.6/v4.4.7), per-connection exchange dedup (v4.4.8), pairing windows, allowlist NV persistence (v4.4.4). For our host-side SSAP stack this is the transferable lesson: **on these chips, MTU exchange vs pairing state ordering is the #1 reconnect bug class**, and it maps directly onto our DLI/CM connect sequence (`ssap_link.h` create-connection → exchange-MTU ordering).

## A.5b Connection-management details from the glue (v4.4 state machine)

- **Direction-tagged connection tracking.** Every inbound/outbound SSAP packet is tagged `TEAM_LINK_UPSTREAM` or `TEAM_LINK_DOWNSTREAM` before decode (`team_bind_packet_source()`, called at `ws63_team_network_app.c:8409` and `:8432`), and each connection carries `{conn_id, dir, route_id, route_id_provisional}` (`team_conn_track_t`, `ws63_team_network_app.c:446-452`). Provisional route IDs solve the "member_id not yet known on this conn" problem — identity is later confirmed from packet contents rather than assumed from the connect event.
- **Pending-connection table** `{sle_addr_t, route_id, last_seen_s}` (`team_pending_conn_t`, `ws63_team_network_app.c:454-460`) bridges the gap between scan results and completed connects — the same ghost-connect problem ePaper solved with derived-address rejection.
- **Member upstream recovery:** repeated TX failures with reason codes feed a recover state machine (`team_member_upstream_recover_after_tx_fail("not-ready"/"write-fail", ...)`, `ws63_team_network_app.c:8049,8075`) and successes clear it (`team_member_upstream_recover_clear("tx")`, `:8078`) — a per-link health tracker our SSAP layer could adopt verbatim.
- **Web event log:** every TX/RX packet is recorded into an in-RAM event ring surfaced by the WebUI (`team_web_record_packet(SLE_TEAM_WEB_EVENT_TX/RX, ...)`, `ws63_team_network_app.c:8029,8411`) — the board is its own protocol analyzer, which is how they debugged the v4.4.8→v4.4.10 reconnect saga on real hardware.
- **HELLO handling on the leader** does the pairing work: unknown members get a pending slot, allowed-list members get marked joined, and a unicast ACK is issued per HELLO (`sle_team_handle_hello()`, `sle_team_node.c:946-1058`). Staged-pairing HELLOs gate when a new member may appear (`sle_team_should_stage_pairing_hello`, `sle_team_node.c:594`).
- **Relay lifecycle on the member:** relay client is started lazily only after (role configured) AND (SLE started) AND (joined) AND (relay_allowed) AND (has upstream server conn) (`team_relay_start_client_if_ready()`, `ws63_team_network_app.c:8460-8481`) — a five-condition gate that prevents the half-relayed states that plagued earlier releases.

## A.5c Where the "192.168.43.1 WebUI" rides

The board WebUI is a second radio, not SLE: the WS63's Wi-Fi SoftAP (`CONFIG_SLE_TEAM_WIFI_AP_ENABLE`, `ws63_team_network_app.c:509-513`) serves an HTTP console with a JSON API (`/api/status`, `/api/nodes`, `/api/pairing?action=start|stop|approve&id=...&relay=0|1`, `/api/member/select?team=...&leader=...&channel=...`, `xc/ws63_team_network/README.md:44-58`). The browser WebUI (`webui/`, Vite/TypeScript) is contract-tested against this API (`webui/tests/ws63-api-contract.test.mjs`, listed in `versions/v4.4.9/MANIFEST.md:13`). Phone GPS ingress arrives over the same HTTP path ("手机定位上报", `xc/ws63_team_network/README.md:28`) and is then injected into the SLE mesh as POS_REPORT packets — dual-radio architecture: SLE for the mesh, Wi-Fi/BLE for human interfaces.

## A.6 Process innovations worth copying

- **Per-release VERSION.md + MANIFEST.md** with observed board logs as evidence, expected behavior, and a "To be filled after board run" verification section (`versions/v4.4.9/VERSION.md` whole file; `versions/v4.4.9/MANIFEST.md:3-14` changed-file list). 164 of these = a complete behavioral changelog of the firmware.
- **Contract tests as regression guards on source shape:** e.g. a test that *forbids* `sle_uart_server_adv_restart()` in the connected branch (v4.4.6) and forbids `sle_uart_start_scan()` in the leader connected path (v4.4.7) — tests that encode SLE-stack ordering rules learned the hard way.
- **Host-runnable protocol core + Python simulator + multi-board orchestration** (`tools/sle_team_python_sim.py`, `automation/ws63/`, `examples/team_node_regression_test.c`) — the mesh FSM is testable without any board because of the ops-vtable split (§A.1).
- **Board WebUI over SoftAP** (SSID `SLE-TEAM-V4-XXXX`, `http://192.168.43.1/`, endpoints `/api/status|nodes|events|pairing|member/select|factory-reset`, `README.md:76-96`, `xc/ws63_team_network/README.md:44-58`) as the debug/ops surface — a cheap pattern for our own dongle bring-up tooling.

## A.7 Delta summary vs SLE-MESH-EPAPER.md

| Aspect | ePaper mesh (SLE-MESH-EPAPER.md) | sle_mesh v4.4.138 |
|---|---|---|
| Routing | Proactive HELLO-DV + poison reverse, 128-entry table, fallback flood | Leader-rooted tier/bucket tree, directed upstream/downstream relay, no flood, no dedup cache |
| Addressing | 2-byte MACs, (src,seq) dedup ring | 1-byte member IDs + team_id + channel_hash, per-member last_seq for liveness only |
| Addressing capacity | 256-entry dedup, 64-node topo table | 30 logical members, 8 direct conns on leader |
| Bulk transfer | AIMD window/CHECKPOINT/RTT-6298/bitmap repair, 480 B packets | None — 184 B max payload, per-message ACK only |
| Relay authorization | Implicit (every node relays) | Leader-granted over air (ROUTE_UPDATE relay-grant flag), revocation drops children |
| Role model | Auto-negotiated (ePaper/phone/relay) | Runtime-configured unified firmware (`cfg leader/member`), pairing + allowlist + NV |
| Lifecycle hardening | Zombies, partition heal, Turbo | Exchange-info/pair-state ordering, adv/scan placement, 1vs8 conn manager |
| Verification | README perf table (partly aspirational) | 164 evidence-backed releases, contract tests, host tests, Python sim |
| App payload | Images to ePaper | Team telemetry: heartbeat/position/alert/text |

---

# Part B — fbb_bs2x_rust: full Rust-integration architecture

## B.0 What this tree is

HiSilicon's **fbb_bs2x** SDK (BS21e/BS2Xe NearLink solution, LiteOS, RISC-V; "FBB = Family Big Box unified framework", `README.md:3-5`) with a **`rust_app` component added**, forked by sanchuanhehe (shallow clone, one commit `08be0e4`, 2025-07-11). Boards targeted: HH-D03 (Runhe), BearPi-Pico H2821E, DK221 mouse (`README.md:34-44`). CI builds the `standard-bs21e-1100e` fwpkg (`.github/workflows/build.yml:51-59`).

The Rust content itself is modest — a GPIO blinky task (`src/application/rust_app/src/lib.rs`) — but the **integration skeleton is complete and is exactly the pattern ticket 04 option B needs**: custom target, no_std staticlib, bindgen against SDK headers, C-glue entry via the SDK's own app-registration macro, and CMake linkage into the LiteOS image.

## B.1 Rust runtime: no_std, no allocator, LiteOS IS the runtime

- **Crate shape** (`src/application/rust_app/Cargo.toml`): `crate-type = ["staticlib"]` (`Cargo.toml:7`); the only dependency is `cty = "0.2"` (`Cargo.toml:10`); build-dep `bindgen = "0.69"` (`Cargo.toml:13`). Release profile: `opt-level = "s"`, `lto = true`, `codegen-units = 1`, `panic = "abort"`, `strip = true` (`Cargo.toml:15-21`) — explicitly tuned to mirror C `-Os`.
- **no_std + no_main + hand-rolled panic handler**: `#![no_std] #![no_main]` and `#[panic_handler] fn panic() -> ! { loop {} }` (`lib.rs:1-2,66-69`). There is **no RTIC, no embedded-hal, no embassy, no heap** — the "runtime" is LiteOS: the Rust task is created with `osal_kthread_create()` and sleeps with `osal_msleep()`, both called directly from Rust (`lib.rs:22-45`).
- **Custom target JSON** `riscv32imfc-unknown-none-elf.json`: `arch riscv32`, features `+m,+c,+f`, ABI `ilp32f`, `panic-strategy abort`, `relocation-model static`, `max-atomic-width 32`, linker `rust-lld` (`riscv32imfc-unknown-none-elf.json:1-22`). Toolchain pinned to **nightly** with that single target (`rust-toolchain.toml:1-3`).
- Lesson for us: on these HiSilicon LiteOS chips the pragmatic Rust model is **Rust as a no_std "task library" inside the C RTOS**, not a Rust-owned boot/RTOS stack. Atomics are usable (max 32-bit); floats are hard-float `ilp32f` and must match the C toolchain ABI (`cc_riscv32_musl_fp`, the musl float-ABI RISC-V GCC at `tools/bin/compiler/riscv/cc_riscv32_musl_b010/`, referenced in `build.rs:59-60`).

## B.2 Bindings: bindgen against SDK headers, with a manual fallback

- `build.rs` (`src/application/rust_app/build.rs`):
  - Builds an include-path list of **existing** SDK dirs — `include/`, `include/driver`, `include/middleware`, `drivers/chips/bs21e/board/board_config`, `drivers/chips/bs2x`, `kernel/osal/include`, `kernel/liteos`, `middleware/utils/...` — filtering by existence (`build.rs:17-31`).
  - **Generates `wrapper.h` at build time** with `__has_include` guards plus manual fallback declarations for `uapi_pin_set_mode/uapi_gpio_*` and `osal_*` functions (`build.rs:71-137`). This makes bindgen robust to SDK header churn.
  - bindgen invocation: `-target riscv32-unknown-none-elf -march=rv32imfc -mabi=ilp32f`, optional `--sysroot` into the vendor GCC tree, allowlists **`uapi_.*`, `osal_.*`, `app_.*`** functions and `gpio_/osal_` types, `CONFIG_/GPIO_/HAL_` vars, `use_core()`, `ctypes_prefix("cty")`, `osal_task` opaque (`build.rs:66-107`).
  - If bindgen fails (clang missing, header breakage), it writes a **hand-maintained bindings fallback** so the build never hard-fails (`build.rs:140-200`); a checked-in copy exists at `src/application/rust_app/src/bindings.rs:1-32`.
- **SLE API bindings are absent but trivially addable.** The full SLE stack headers are present: `src/include/middleware/services/bts/sle/` with `sle_connection_manager.h` (`sle_connect_remote_device`, `sle_disconnect_remote_device`, `sle_update_connect_param`, ... at `sle_connection_manager.h:882-979`), `sle_ssap_server.h`, `sle_ssap_client.h`, `sle_device_discovery.h`, `sle_transmition_manager.h`. The current allowlist (`uapi_/osal_/app_`) deliberately excludes them because the sample only blinks GPIO; extending to `sle_.*|ssaps_.*|ssapc_.*` is a one-line allowlist change plus callback ABI work. This is the exact FFI surface our `RUST-WS73-UNSAFE-FFI.md` plan anticipates, already proven against the sibling SDK.

## B.3 Coexistence with the C SDK: three seams

1. **Link seam.** The Rust crate produces a plain static archive; the SDK's component CMake links it. `src/application/rust_app/CMakeLists.txt`: `COMPONENT_NAME "rust_app"`, `LIBS = .../target/riscv32imfc-unknown-none-elf/debug/librust_app.a` (`CMakeLists.txt:29-31`), `WHOLE_LINK true` (`:33-34`) so the linker keeps sections despite no C-side references. `build_component()` is the standard FBB component macro — no special-casing in the SDK build at all.
2. **Entry seam.** A 7-line C file, `src/application/rust_app/src/rust_glue.c`: `extern void rust_app_init(void);` + **`app_run(rust_app_init)`** — the SDK's standard app-init registration macro used by every C sample. Rust "just works" as another application component; LiteOS calls it, and Rust then creates its own threads via OSAL (`lib.rs:47-62` creates `BlinkyTask`, prio 24, 0x1000 stack).
3. **Registration seam.** `src/application/CMakeLists.txt:15` — one added line `add_subdirectory_if_exist(rust_app)`. Total invasiveness in the 659 MB SDK: **one directory + one line**.
- Memory layout: there is **no custom linker script and no reserved Rust section**. Because `crate-type = "staticlib"`, rustc only emits the `.a` (the target JSON's `rust-lld`/`gnu-lld` settings are inert for staticlib); the SDK's normal LiteOS link (C toolchain, SDK linker script) places Rust code/data like any C object. The only ABI requirements are ilp32f + soft-float-compatible struct passing, which bindgen/cty guarantee at the type level.

## B.4 Build/CI wiring (and two defects to avoid)

- CI: install nightly rust + `rust-src` + target (`.github/workflows/build.yml:37-40` runs `cargo build`; `config.sh:44-50` and `.devcontainer/Dockerfile:47-53` install rustup) — **but they install `riscv32imac-unknown-none-elf` while the crate actually needs the custom `riscv32imfc` JSON**; only `rust-toolchain.toml` is correct. `rust-src` is what enables building `-none-elf` core from source under a custom target.
- **Defect 1:** the release profile is tuned (`Cargo.toml:15-21`) but the linked archive is the **debug** one (`CMakeLists.txt:30` references `.../debug/librust_app.a`) and CI runs bare `cargo build` — the shipped image is unoptimized. Fix: `cargo build --release` + point LIBS at `release/`.
- **Defect 2:** `adapter/` directory is empty — an apparently abandoned plan for a C adapter layer; the actual seam ended up being `src/rust_glue.c` (listed in the same CMakeLists `SOURCES`, `CMakeLists.txt:6-8`).
- Note the glue's include of `app_init.h` (`rust_glue.c:1`) is the only C header dependency, and bindgen's `app_.*` allowlist exists to see `app_run`-related declarations (`build.rs:77`).

## B.5 Applicability to rust-ws73/firmware (ticket 04 option B)

- **Scope check first:** our local WS73 SDK trees are **host-side stacks, not FBB firmware trees** — `sdk/ws73_sdk_linux_WS73_1.10.110/` and `https://github.com/gtxaspec/ws73v100-wifi/tree/master/` both have `application/{bin,dft,lib,sample,sle_android}` layouts with **no `build_component()`/`app_run()` anywhere** (greps returned only Android sepolicy false-positives). So the fbb_bs2x_rust precedent applies to **firmware** targets — the WS63-family firmware SDK (e.g. `https://github.com/x-eks-fusion/fbb_ws63/tree/master/`, same FBB framework as sle_mesh's build) — not to the WS73 dongle host stack, where Rust is ordinary Linux userspace (already covered by `RUST-WS73-*` notes). "Option B" must therefore target whatever firmware SDK tree we adopt for dongle-firmware work, and the移植 path is B.3's three seams, unchanged.
- **Port checklist distilled from this precedent:**
  1. Write a target JSON for the firmware core's RISC-V ISA/ABI (match the vendor GCC exactly: march/mabi/sysroot), pin nightly + `rust-src` in `rust-toolchain.toml`.
  2. `staticlib` crate, `no_std`, `panic=abort`, `cty` only; `#[panic_handler]` = log via `osal_printk` then spin.
  3. `build.rs` with existence-filtered include list, generated `wrapper.h` with `__has_include` guards, allowlists per subsystem, manual-fallback bindings.
  4. C glue file using the SDK's own registration macro (`app_run(...)`); one `add_subdirectory_if_exist(rust_app)` line; `LIBS` with `WHOLE_LINK true`, pointing at the **release** archive.
  5. Threads/timers/queues come from OSAL (LiteOS), not from Rust crates — do not import an RTOS.
  6. Contract-test the toolchain: assert the built fwpkg embeds the Rust section (sle_mesh-style package string guards are a model).
- **What this precedent does NOT answer:** interrupt safety/`unsafe` conventions at ISR level, stack sizing for Rust frames under LiteOS threads, and DMA-backed SLE data-path FFI (the sample touches only GPIO/OSAL). Those remain open items for `RUST-WS73-UNSAFE-FFI.md`.

## B.6 Cross-check against our prior Rust notes

- `PERF-RUST-PATTERNS.md` and `RUST-WS73-LTO-EXTREME.md` assumed a host-side Linux Rust build for the WS73 dongle; this tree confirms the *firmware-side* profile that our size modeling assumed: `opt-level="s" + lto + codegen-units=1 + panic=abort + strip` (`Cargo.toml:15-21`) is exactly the community-accepted minimal-footprint configuration for HiSilicon RISC-V LiteOS targets.
- `RUST-WS73-UNSAFE-FFI.md` planned bindgen-generated bindings with hand-written fallbacks; this tree is the first local proof that plan works against real HiSilicon SDK headers, including the quirks we predicted (generated `wrapper.h`, opaque OSAL types, `cty` mapping, sysroot pointing into the vendor GCC tree rather than a system sysroot, `build.rs:66-75`).
- `BS21-WS63-SDK-COMPARISON.md` placed BS21e as the low-power sibling of WS63; this repo shows the **FBB build framework is shared** across these chip families (`build_component()`, `app_run()`, `build.py -c -ninja`, fwpkg output), so any Rust integration we build for one FBB firmware SDK transfers to the others with only the target JSON and include paths changed.
- Contrast with LinkNebula (`LINKNEBULA-MESH.md`), which is a Rust *host-side* mesh stack. Between LinkNebula (Rust above the host stack), fbb_bs2x_rust (Rust inside the firmware image), and sle_mesh (mature C firmware mesh with evidence-driven process), the three community reference points for our two-track plan are now all locally documented.

---

## 8-line summary

1. sle_mesh (BH4ME) is a *different* project from NearLink-Mesh-ePaper: a Meshtastic-shaped SLE "team network" (leader/member/relay, GPS/heartbeat/alerts), now at v4.4.138 (2026-06-10); v4.4.9 (2026-05-31) was a single reconnect fix.
2. Its routing replaces ePaper's HELLO-DV+flood+dedup with a **leader-rooted tier/bucket tree and strictly directed relay** — no flooding, hence no dedup cache; only leader-origin traffic is ever relayed (`ws63_team_network_app.c:8088-8104`).
3. Relay is **leader-granted over the air** (ROUTE_UPDATE relay-grant flag) and revocation drops child connections — an authorization layer ePaper lacks.
4. No AIMD/bulk path at all (184 B payload cap); its hardening budget went to SLE lifecycle bugs — the exchange-info vs pair-state ordering saga is directly relevant to our SSAP/DLI connect sequence, and member MTU observed was 208.
5. WS63 supports **8 concurrent client connections** (`SLE_UART_CLIENT_MAX_CON 8`) — new data point for our WS73 connection-limit validation.
6. Process gold: 164 evidence-backed VERSION.md/MANIFEST.md releases, source-shape contract tests, host-runnable ops-vtable protocol core + Python simulator — a template for our own tracker/verification discipline.
7. fbb_bs2x_rust is the first local **Rust-in-HiSilicon-firmware** precedent: `no_std` staticlib on a custom `riscv32imfc-unknown-none-elf` target (ilp32f, abort, nightly), bindgen against SDK headers with manual fallback, LiteOS as the runtime via `osal_kthread_*`.
8. Its three seams (staticlib+`WHOLE_LINK` in `build_component()`, `app_run(rust_app_init)` C glue, one `add_subdirectory` line) are the copy-paste pattern for rust-ws73/firmware option B — but note our in-repo WS73 SDK is host-side (no FBB macros), so this applies to a WS63-family firmware SDK; SLE bindings are a one-line allowlist away.

**Note file:** `.scratch/nearlink-driver/lab-notes/NEW-SLEMESH-RUST.md`
