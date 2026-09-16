---
type: harvest
title: "cxl0928/hi3863-sle-1v8-vehicle — SLE 1-Client-to-8-Server Vehicle Sample (WS63, SBUS + UWB Follow)"
language: en
created: 2026-09-13
tags: [sle, ws63, hi3863, one-to-many, connection-topology, sbus, uwb, pid, harvest]
sources:
  - "/mnt/hdd/nearlink-stuff/hi3863-sle-1v8-vehicle"
trust: verified
stale_after: 2026-12-13
---

# cxl0928/hi3863-sle-1v8-vehicle — SLE 1-Client-to-8-Server Vehicle Sample (WS63, SBUS + UWB Follow)

- Inspection date: 2026-09-13 (staleness check: pushed 2026-08-17, not archived — ALIVE)
- Source root: `/mnt/hdd/nearlink-stuff/hi3863-sle-1v8-vehicle`
- Mode: read-only local program inspection; no network, build, hardware, or PCB access
- Scope: SLE 1-vs-8 connection topology, SBUS parsing, UWB follow, motor/encoder/PID integration

## Executive findings

1. The repo is an embedded-competition sample ("child safety car") for the HiSpark Studio WS63 SDK; all key code lives under `application/samples/products/sle_uart_1_vs_8/`, with vendor driver scaffolding deleted and only CMake/Kconfig skeletons left elsewhere. [`README.md`, `daima-shuoming-wenjian.md` (code-walkthrough doc) lines 1-12]
2. The one-client-to-eight-server topology is realized with a fixed connection table: `SLE_UART_CLIENT_MAX_CON = 8`, arrays `g_sle_uart_conn_macs[8]` (connected MACs, used to skip duplicates during discovery) and `g_sle_uart_conn_id[8]` (live connection IDs). [`application/samples/products/sle_uart_1_vs_8/sle_uart_client/sle_uart_client.c:31-39`]
3. The client runs a continuous discovery-connect-rescan loop: the seek callback accepts a found device only while `g_sle_uart_conn_num < 8`, then calls `sle_connect_remote_device`; on connect it stores the conn_id and issues `ssapc_exchange_info_req` (MTU exchange), then immediately restarts scanning to keep filling the table; on disconnect it removes the conn_id by shifting the array down and restarts scanning. [`sle_uart_client.c:100, 119, 142-170`]
4. Control delivery is broadcast-style: the client task parses SBUS, runs the UWB follow algorithm, and sends control frames to every conn_id in the table ("broadcast control commands to all servers"); each server (motor node) receives on its own connection. [`sle_uart_client.c:60-70`, code-walkthrough doc lines 14-27]
5. SBUS parsing is the standard 25-byte serial frame: header at byte 0, footer at byte 24, sixteen 11-bit channels unpacked with shift/mask chains (`& 0x07FF`). [key-functions summary txt lines 33-57]
6. Build-time role split: the same sample compiles as client or server via `CONFIG_SAMPLE_SUPPORT_SLE_UART_CLIENT_1_VS_8` vs `..._SERVER_1_VS_8`, each spawning its own osal task from `sle_uart_entry()`. [key-functions summary txt lines 9-27]
7. Peripherals integrated on the server side: dual rear-wheel drive + front-wheel steering (`sle_motor.c`), MT6816 14-bit magnetic encoder for steering angle (`sle_mt6816.c`), wheel-speed PID closed loop (`sle_pid_control.c`), UWB module and LiDAR on separate UARTs. [`application/samples/products/sle_uart_1_vs_8/` file set]

## Boundaries and gaps

- Competition code: no encryption/pairing handling beyond stock API calls, no error-recovery beyond rescan-on-disconnect, and application-level framing is a simple UART dialect (same family as `SLE-UART-VARIANTS.md` samples).
- The "UWB follow" is UART-module integration (GC-P2304) — the repo consumes distance/angle, it does not implement UWB itself; consistent with our PCB/hardware-knowledge exclusion, only the integration code was reviewed.
- Driver files are stripped; the tree alone does not build without a HiSpark Studio SDK checkout.
- No OOB pairing or allowlist: any server broadcasting is connectable, dedup is MAC-based only.

## Reusable for our stack

- The scan→connect→MTU-exchange→rescan loop with a fixed-size conn_id table and MAC dedup is the canonical WS63/Hi3863 1-to-many pattern — directly applicable to our WS73 dongle if we ever drive multiple SLE peripherals from one dongle.
- Array-compaction disconnect handling (shift-down + rescan) is a simple, race-free single-threaded topology maintenance idiom.
- Role split by Kconfig macro in one sample tree keeps client/server builds from diverging — a reusable layout for our sample sketches.

## Comparison anchors (vs existing reports)

- `SLE-UART-VARIANTS.md`: same sle_uart sample lineage; this repo's delta is the 1-vs-8 multi-connection management replacing the 1-to-1 flow.
- `NEW-SLEMESH-RUST.md`: complementary topology — that report covered mesh-style routing; this shows the star topology limit (8 connections) with a client-side scan loop.
- `NEW-NEARLINK-UWB-LIKE-RANGING.md`: both use per-UART sensor modules; this one confirms the UART-module integration style is common across WS63 community projects.
