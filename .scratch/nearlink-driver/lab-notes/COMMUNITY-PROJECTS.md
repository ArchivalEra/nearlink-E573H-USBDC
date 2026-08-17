# Community / Personal NearLink Projects — Health Check

**Date:** 2026-08-17
**Scope:** Read-only local inspection of `/mnt/hdd/nearlink-stuff/` (no network/build/hardware).
**Companion docs:** `OTHER-NEARLINK-IMPLS.md`, `RESEARCH-DIRECTIONS.md` (this report deepens/updates their project list).

**Bottom line:** The local batch contains **no alternative host stacks** (OpenSparklink/sparklink already covered separately). Most projects are *chip-side* (WS63/BS21/Hi2821) application demos running on the vendor SDK's own SSAP library. Value for us splits cleanly: **protocol/API knowledge transfers** (SSAP call sequences, QoS flow control, PHY/MCS tuning, SLE-Link wire spec); **code does not directly reuse** because they run SSAP inside the chip, we run our own SSAP on the host. Best finds: `NearLinkSLE` (cleanest minimal SSAP client+server), `sle_measure_sdk` (PHY/MCS/CI/QoS knobs), `ili9320-i80-hi2821e-spi-bridge` (official **SLE-Link wire protocol spec**), `NearLink_controller` (host-app ↔ SLE mapping + frame-aggregation design).

**Note:** `NearLink-Mesh-ePaper` is **not present** in `/mnt/hdd/nearlink-stuff/` (searched by name and by "mesh/epaper" globs, also deeper). The clone did not land. Its expected content (SLE mesh multi-hop/AODV/AIMD over SSAP, BearPi-Pico H3863) is still worth reviewing from `OTHER-NEARLINK-IMPLS.md` knowledge; not inspectable here.

---

## Sources

All inspected locally under `/mnt/hdd/nearlink-stuff/`. Git activity read from local `.git` (single shallow commit snapshots in most cases).

| # | Project | Git remote (local) | Last local commit | Notes |
|---|---|---|---|---|
| 1 | NLChat | Hny0305Lin/NLChat | 2024-09-05 | snapshot w/ 1 commit |
| 2 | NLChat_Web | Hny0305Lin/NLChat_Web | 2024-11-04 | snapshot |
| 3 | NearLink_controller | shenzhantu/NearLink_controller | 2026-07-30 | snapshot |
| 4 | LinkNebula | GBCLStudio/LinkNebula | 2025-03-28 | snapshot |
| 5 | NearLinkSLE | QTDS138/NearLinkSLE | 2026-08-14 | snapshot (fresh) |
| 6 | sle_measure_sdk | rzy0901/sle_measure_sdk1.0.12 | 2025-08-20 | snapshot |
| 7 | nearlink-firmwares | MiraHikari/nearlink-firmwares | 2025-05-01 | blob:none clone |
| 8 | SparkLink-FallDetection | fyy0619-cell/SparkLink-FallDetection | 2026-08-07 | snapshot |
| 9 | tp78_v3_open | ChnMasterOG/tp78_v3_open | 2026-07-19 | snapshot |
| 10 | FlashKeyboard | JackieCooo/FlashKeyboard | 2025-11-26 | snapshot |
| 11 | ili9320-i80-hi2821e-spi-bridge | lualiliu/ili9320-i80-hi2821e-spi-bridge | 2025-12-11 | snapshot |
| 12 | ws73v100-wifi | gtxaspec/ws73v100-wifi | 2025-09-23 | snapshot |
| — | NearLink-Mesh-ePaper | (absent locally) | — | clone missing |
| — | HopeRun-NearLink | HopeRunORG/NearLink | 2025-04-09 | official, reference only |

---

## 项目画像 (Project-by-project)

### 1. NLChat (`NLChat/`) — Android 星闪聊天
- **定位:** Android chat app that bridges to BearPi Hi2821 / Hi3863 / HopeRun WS63 dev boards over a **CH34x USB-serial chip** (board-side firmware is the vendor "SLE UART Server/Client sample").
- **技术栈:** Java/Android; USB serial via WCH CH34x driver lib (`CH34xUARTDriver.writeData`, `Ch34ReadThread`); SQLite chat history; CI via GitHub Actions.
- **星闪相关度:** **边缘** — the app itself contains **zero SLE/SSAP logic**. `MainActivity.NearLinkChatSendData()` just does `MainAPP.CH34X.writeData(to_send, len)`; the wire format is raw UTF-8 bytes handed to the board's SLE UART transparent-transmission firmware. Config tutorial (`SLE_Device_Configure_Tutorial_Hi2821.md`) shows only Kconfig: "Enable SLE UART Server/Client sample" + `Select sle ble peripheral`.
- **活跃度:** stale since 2024-09; active only during 2024 HiSilicon developer-experience contest period.
- **价值:** **低**（协议侧）/ 中（产品侧）。Confirms the cheapest "host app ↔ chip" pattern is UART-transparent SLE; useful tested-device list (incl. Huawei Mate 50/HarmonyOS). Hardware companion repos (Bearpi_Hi2821_Pico_NLChat) are external. No reusable SSAP code.

### 2. NLChat_Web (`NLChat_Web/`) — Web 端
- **定位:** React + TypeScript (Vite) web counterpart of NLChat, using the **Web Serial API** (`SerialPort`), same CH34x-serial bridge concept.
- **技术栈:** TS/React/Vite/pnpm.
- **星闪相关度:** **边缘** — pure serial passthrough + chat UI; no SLE protocol.
- **活跃度:** 2024-11 (one-shot web launch).
- **价值:** **低**. Only relevance: Web-Serial-based "browser talks to SLE dongle via serial" as a demo pattern — not our path (WS73 = USB CDC/HCC, not CH34x).

### 3. NearLink_controller (`NearLink_controller/`) — 星闪游戏手柄参考设计
- **定位:** AI-agent-driven reference design for a Hi2821 (Lierda **EB21**) gamepad, HarmonyOS NEXT host, 2K–4K Hz polling, targeting Betop G6 Pro-class products. Very well structured (AGENTS.md + specs + plans + research).
- **技术栈:** C firmware (contract-first, mock-backed, Unity tests) + ArkTS HarmonyOS test app; markdown-driven agent workflow.
- **星闪相关度:** **核心（设计意图）但未落地** — the SLE path is a **stub**: `firmware/src/transport/eb21/transport_sle_eb21.c` and `transport_usb_eb21.c` return `DRIVER_E_NOSYS`; SLE GATT/HID profile details, USB endpoint config are explicitly **Open Questions** in the P3 spec. No hardware integration phase started (OQ-P1..P6 gated by hardware).
- **活跃度:** active 2026-07 (P0–P4 merged, no active phase).
- **价值:** **中-高（架构/设计）**. Real transferable value:
  - Unified transport contract (`transport.h`): `transport_init/connect/disconnect/send_report/get_poll_interval`, state machine DISCONNECTED→PAIRING→CONNECTED, non-blocking `controller_poll` semantics, "submit not wait-for-completion".
  - **Frame aggregation + power-saving design** (P3 spec D5/D6): per-source seq tracking, `candidate_report` vs `last_sent_report`, dirty/pending retry on send failure, unconnected handling, report unchanged on empty frame — excellent state-machine logic we can copy for our host SSAP link layer.
  - Poll interval contract: SLE backend declared **250 µs (4KHz)**, USB 250 µs (8K target).
  - `docs/research/04-harmonyos-gamepad-api.md`: how HarmonyOS maps SLE HID (ABS_X/Y/Z/RZ/GAS/BRAKE/HAT0 + BTN_*) — useful if a TV-box route goes through HarmonyOS, and its HID DDK / Game Controller Kit notes.
  - EB21 vs FB36 chip constraint table (SRAM 160KB/Flash 1MB/64MHz; Hi2821 SLE 12Mbps vs Hi3863 4Mbps) — same Hi2821 family our Hi2821E dongle siblings use.

### 4. LinkNebula (`LinkNebula/`) — Rust 星闪 mesh 实验
- **定位:** "AetherLink" — a senior-high experiment: NearLink message passing without internet. Tiny **no_std Rust** mesh over BearPi Hi2821.
- **技术栈:** Rust, `heapless`/`zerocopy`, workspace crates `common/client/forward/server`, `build.rs` links a BearPi Hi2821 linker script; simulator feature (`SimChannel/SimHardware`) for host testing.
- **星闪相关度:** **核心（自有协议，非标准 SSAP）** — implements its own application-layer protocol over an FFI HAL (`nl_init/nl_send/nl_recv/nl_configure` with channel/tx_power/pan_id). Packet formats: `Beacon` + `DataPacket` (header: version, type, 6-byte src/dst node IDs, packet_id, total_fragments/fragment_index, data_length, checksum = header-checksum XOR data-checksum). Routing: distance-vector-style routing table (metric = signal, 5-min expiry, next-hop), service directory + election in `forward/`. Tests: routing_algorithm, protocol_parsing, multi_hop integration.
- **活跃度:** 2025-03 (frozen).
- **价值:** **中（协议/设计知识）**. Not standard SSAP, so not wire-compatible, but the forwarding/mesh ideas (fragmentation, checksum scheme, route expiry, service discovery, next-hop forwarding engine) are directly transferable to any future multi-hop SSAP application we build (TV-box ↔ remote sensor mesh). `forward/` routing engine is small and readable.

### 5. NearLinkSLE (`NearLinkSLE/`) — WS63 SLE UART 透明传输（最干净的 SSAP 双端样例）
- **定位:** Minimal two-firmware pair (server = broadcast + SSAP service; client = auto scan/connect) for HiSilicon 255BE-63 (= WS63) SLE UART transparent transmission. Includes `SLE_HELLO`, `SLE_QT`, `SLE_QT_AD`, `SLE_UART_HE` variants.
- **技术栈:** C on HiSilicon WS63 SDK (LiteOS), vendor SSAP library.
- **星闪相关度:** **核心** — full SSAP usage:
  - Server (`sle_hello_server.c`): `ssaps_register_server` (app UUID 0x1234) → `ssaps_add_service_sync` (service UUID 0x3333) → `ssaps_add_property_sync` (char 0x3434, READ|WRITE + NOTIFY) → **`ssaps_add_descriptor_sync` with notify-enabled value {0x01,0x00}** → `ssaps_start_service`; send via `ssaps_notify_indicate`; re-announce on disconnect.
  - Client (`sle_hello_client.c`): seek by adv name → `sle_connect_remote_device` → `ssapc_exchange_info_req` (MTU 520) → `ssapc_find_structure` (start_hdl=1, end_hdl=0xFFFF) → write CCCD 0x01/0x00 → `ssapc_write_req` data path; notification callback writes to UART.
  - Data plane: UART1 (115200 8N1) ⇄ ring buffer (8 KiB) ⇄ SSAP; tx retry every 2 ms up to 100 attempts, max 128 B per call.
- **活跃度:** fresh 2026-08-14 (very current).
- **价值:** **高** — this is the cleanest full client+server SSAP reference in the batch, and its API names/sequences match the WS73/OHOS family we already cross-check against (our `stack/ssap/` codec). Great as a "happy-path" behavioral oracle: exact ordering (register → add service → add property → add descriptor → start), descriptor-based notify enable, re-announce/re-seek on disconnect, MTU 520.

### 6. sle_measure_sdk (`sle_measure_sdk/`) — WS63 SLE 吞吐/时延测量固件
- **定位:** Measurement firmware family for WS63 (drop-in `application/samples/bt` replacement): speed throughput, latency, device-discovery (DD), connection-establishment (CE), 1-vs-N variants, BLE variants.
- **技术栈:** C on WS63 SDK; Kconfig knobs + bash build scripts.
- **星闪相关度:** **核心** — exposes the full PHY/link tuning surface:
  - `sle_set_phy_param` (tx/rx format = SLE_RADIO_FRAME_1/2, PHY 1M/2M/4M, pilot density 16:1), `sle_set_mcs` (up to 10), `sle_set_data_len` (packet payload up to **1370 B**), `sle_set_conn_param` (conn interval, default 20 × 1.25 ms = 25 ms), tx power (NV 0x20A0).
  - **QoS flow control**: `SLE_QOS_FLOWCTRL_FUNCTION_SWITCH` → `sle_link_qos_state_t` via `sle_transmission_register_callbacks(send_data_cb)`; non-switch fallback reads `gle_tx_acb_data_num_get()`; loop only sends while `sle_flow_ctrl_flag() > 0` — i.e., **ACB credit-gated notify**.
  - Build scripts parameterize conn-interval/pkt-len/frame/phy/mcs/tx-power/per-pair MAC (for multi-pair 1-vs-N throughput sweeps).
- **活跃度:** 2025-08.
- **价值:** **高（性能调优知识）** — maps directly to our WS73 HADM/PHY `SET_MEASURE`/MCS work and to validating throughput on the dongle: known-good PHY/MCS/CI combos, credit-gated send pattern, per-device MAC allocation for multi-pair.

### 7. SparkLink-FallDetection (`SparkLink-FallDetection/`) — WS63 跌倒检测（文档宝库）
- **定位:** WS63 + MPU6050 + Edge Impulse TinyML fall detection, dual protocol (SLE board-to-board, BLE to phone), remote alert chain (Wi-Fi → HTTP → PushPlus WeChat, later 4G Cat.1/DTU).
- **技术栈:** C/C++ on WS63 LiteOS, Edge Impulse/TFLite-Micro, Python backend, Lua on V100C 4G module.
- **星闪相关度:** **核心** — SSAP used end-to-end; **source is not in the repo** (kept in separate SDK project), but `docs/06-系统集成/SLE_SSAP通知链路.md` documents the full SSAP wiring:
  - Server: property must declare READ|WRITE|NOTIFY; **CCCD descriptor is mandatory — without it the stack silently drops Notify** (real debugging finding, matches NearLinkSLE).
  - Client handshake order: enable → seek → connect → pair → **MTU 512** → `ssapc_find_structure` → **activate write 0x01/0x00 (CCCD)** → wait Notify.
  - Alert = Notify `0x05`; client replies ACK via `ssapc_write_req` `0x06` (round-trip ack pattern).
  - Latency measured: Notify one-way ≈ 6–12.5 ms at conn interval 6.25–12.5 ms.
- **活跃度:** very active through 2026-08.
- **价值:** **中-高（协议/产品文档）** — the SSAP notify-chain doc + measured latency + "CCCD missing ⇒ silent drop" pitfall are directly useful for our SSAP server/link tests and for validating real end-to-end latency on the dongle. The dual-role CMake (server or client per board) build trick is also reusable.

### 8. nearlink-firmwares (`nearlink-firmwares/`) — 星闪工具箱 (Tauri/Rust 固件商店)
- **定位:** Cross-platform firmware store/flasher/serial-debug tool (Tauri + Rust): firmware store (`.fwpkg`), one-click flash (HiSilicon + third-party), serial debug, AT-command collections ("serialcuts"), multiple mirror sources.
- **技术栈:** Tauri/Rust (app), JSON metadata; board registry covers Hi3863 (HiHope DK3863, BearPi), Hi2821 (HH-D03, BearPi, Haohanyh board), WS63 (P-E528H-WS63), EBM_H63/H21E encrypted.
- **星闪相关度:** **边缘** — tooling/ecosystem, not protocol. But it curates real firmware packages + command sets.
- **活跃度:** 2025-05.
- **价值:** **低-中** — its `firmwares/*/metadata.json` + `.fwpkg` format and `serialcuts` (AT command JSON collections) are a reference for how the community distributes/test firmware; useful if we want a test-rig or a way to flash/verify our own firmware images. `endpoints.json`/mirror orchestration is infra, not relevant.

### 9. tp78_v3_open (`tp78_v3_open/`) — TrackPoint78 v3 三模键盘 (Hi2821/Hi2821E)
- **定位:** 三模(USB/BLE/**SLE**) mechanical keyboard + dongle, Hi2821/Hi2821E (BS21/BS21E), 1 kHz USB wired / **2 kHz SLE wireless**, VIA web rebinding, TouchBar, OLED.
- **技术栈:** Firmware only, as `.fwpkg` (prebuilt); GPL-3.0; official board license-gated (trial firmware unrestricted but single-mode SLE, no low-latency, no expansions).
- **星闪相关度:** **核心（产品形态）** — a shipping SLE HID product: keyboard + SLE dongle pair. No source here (fwpkg only).
- **活跃度:** 2026-07 (v3.3.4).
- **价值:** **低-中** — proves BS21/BS21E SLE 2 kHz HID polling works with a dongle receiver (the same topology we want for TV-box remote input). Trial fwpkg could be flashed to Hi2821 hardware for testing, but not code. TP78v2 is CH582M (BLE, not SLE) — the SLE part is only v3.

### 10. FlashKeyboard (`FlashKeyboard/`) — BS2x 宏键盘 (BS20/BS21e/BS22)
- **定位:** Macro keyboard with USB/WiFi/BLE/**NearLink** on the BS2x (Hi2821E-family) SDK.
- **技术栈:** HiSilicon BS2x SDK C; `protocol/slp` (SLE-Link protocol component, prebuilt `libslp_client.a`), `protocol/bt` (bgtp controller/gle host), `application/keyboard` app (key_scan, oled, USB keyboard, BLE keyboard, `hal/service_controller.c` with `SERVICE_SLE` mode selection via `CONFIG_SLE_DETECT_PIN`).
- **星闪相关度:** **核心（产品形态）** — SLE support wired via vendor `slp` (SLE-Link) component; no SSAP app code, it's keyboard HID over the vendor stack.
- **活跃度:** 2025-11.
- **价值:** **低-中** — confirms BS2x SLE HID keyboard viability and shows how the vendor `slp` component is wired (CMake/Kconfig patterns). Not directly reusable for WS73 host.

### 11. ili9320-i80-hi2821e-spi-bridge (`ili9320-i80-hi2821e-spi-bridge/`) — BearPi-Pico H2821E SDK + 星闪应用层 SLE-Link 协议规范
- **定位:** BearPi-Pico H2821E (BS21E) SDK snapshot with LCD (ili9320 i80/SPI bridge) app + rich official docs + product samples (RCU dongle, ble_sle_tag, sle_measure_dis, sle_multi_conn, sle_ota_dongle, sle_uart...).
- **技术栈:** HiSilicon BS21E SDK C (LiteOS/non_os), Kconfig, HiSpark Studio; docs are official HiSilicon PDFs/markdown.
- **星闪相关度:** **核心（文档）** — contains **`BS2XV100 星闪应用层SLE-Link协议.md`**: the official wire spec of the **SLE-Link protocol** used between a host SoC application and a chip-side SLE host (chip as co-processor, host on main SoC). Defines the SLE Frame:
  - Header: Flag (fixed **0x1F**), Version, Total Frame, Frame Seq, Rsv, **Service ID** (0x01 Device Discovery, 0x02 Connection Management, 0x03 SSAP Client, 0x04 SSAP Server, 0x05 Factory Test, 0x06 Low Latency), **Command ID** (per-service, from 1), BodyLen (2B), **Body = TLV** (multiple TLVs), **MIC = CRC** (2B). Framing/segmentation via Total Frame/Frame Seq.
  - This is the bridge contract between a host Linux app and a chip-hosted SLE stack — **exactly the "AT/SLE-Link bridge" architecture we flagged as the fastest validation path for WS73** (RESEARCH-DIRECTIONS §六.1).
- **活跃度:** 2025-12.
- **价值:** **高（wire protocol spec）** — the SLE-Link spec is primary-source protocol knowledge for host↔chip SLE bridging, complements our DLI/SSAP work; the repo also carries BS21E product samples (incl. **SLE RCU dongle** with `sle_service_bas/dis/hids/ntf`, `sle_rcu_hid`) that show full SLE service layouts for HID remote control.

### 12. ws73v100-wifi (`ws73v100-wifi/`) — 海思 WS73 官方 Linux 源码 (厂商, 确认用)
- **定位:** A fork of the official WS73 Linux SDK (author gtxaspec) with modules renamed so they load as `ws73v100.ko` etc. **Structurally identical to our `sdk/ws73_sdk_linux_WS73_1.10.110/`** (same top-level tree; only diff is our tree has `output/`). Contains `driver/bsle/{sle_driver,ble_driver}` with `sle_chba`, `sle_hcc`, `sle_socket`, prebuilt `application/bin/<SoC>/sparklink{d,ctrl,chba}` for many SoCs (1155/1156/920/9633/3516V610/rv1126/t23/t41...), an Android demo (`application/sle_android/NearlinkDemo`, Android 9/11/12), and `application/sample/bt/sle/{sle_uart,sle_uuid_client,sle_uuid_server,sle_mouse_server}`.
- **星闪相关度:** **核心（厂商平台，即我们 SDK）** — relationship to our `wifi_soc`: it is the **same vendor SDK family**; confirms our local SDK is the current/parallel tree, and shows the official host daemons + platform bring-up the vendor ships. Not a community invention.
- **价值:** **低（对新知识而言）/ 确认基线** — nothing new beyond our SDK; useful only as a second copy and as evidence the official host daemons (`sparklinkd/ctrl/chba`) are the vendor-supplied user-space SLE host, which we are re-implementing in `stack/ssap/`.

### HopeRun-NearLink (`HopeRun-NearLink/`) — 润和官方 (只引用)
- 28 demos (00_thread → 27_sle_oled, incl. 23_sle_uart, 24_sle_humi, 25_sle_led, 26_sle_gas, 27_sle_oled), HH-D01 board PDFs (incl. WS63V100 AT command use cases), firmware for BS21/WS63/WS63E. Official board material; referenced as the canonical HH-D01/WS63 demo source — already covered, not re-inspected.

---

## 高价值项目深挖

### A. NearLinkSLE — 最小完整 SSAP client+server（行为蓝本）
Wire/call sequences (vendor `sle_ssap_*` API, HiSilicon family — same names OHOS uses):
- **Server:** `ssaps_register_server(app_uuid=0x1234, &server_id)` → `ssaps_add_service_sync(server_id, service_uuid=0x3333, ...)` → `ssaps_add_property_sync(server_id, service_handle, {uuid=0x3434, perms=READ|WRITE, op_ind=READ|WRITE|NOTIFY})` → **`ssaps_add_descriptor_sync(..., type=SSAP_DESCRIPTOR_USER_DESCRIPTION, value={0x01,0x00})`** (explicitly noted as copied from official speed_server to default-enable notify) → `ssaps_start_service`. Send = `ssaps_notify_indicate(server_id, conn_id, {handle, type=SSAP_PROPERTY_TYPE_VALUE, value, value_len})`.
- **Client:** seek by adv name (not UUID) → stop seek → `sle_connect_remote_device` → on `SLE_ACB_STATE_CONNECTED` → `ssapc_exchange_info_req(MTU=520)` → on exchange-info cb → `ssapc_find_structure{type=SSAP_FIND_TYPE_PROPERTY, start=1, end=0xFFFF}` → write CCCD `{0x01,0x00}` via `ssapc_write_req` → `ssapc_write_req` for data; `notification_cb` receives server pushes.
- **What we copy:** exact registration order, descriptor-based notify enable (same pitfall FallDetection found), MTU 520, disconnect → re-announce/re-seek, 128 B per send + 2 ms×100 retry queueing (a concrete non-blocking tx strategy we can mirror in `ssap_link`).

### B. sle_measure_sdk — PHY/MCS/CI/QoS 调优面
- PHY selection: `sle_set_phy_param` (frame format 1/2, PHY 1M/2M/4M, pilot density 16:1) + `sle_set_mcs` (up to 10) + `sle_set_data_len` (payload up to 1370 B) + `sle_set_conn_param` (interval default 20 → 25 ms).
- **Credit-gated send (QoS)**: register `sle_transmission_register_callbacks` → `send_data_cb(conn_id, sle_link_qos_state_t)`; or legacy `gle_tx_acb_data_num_get()`; loop checks `sle_flow_ctrl_flag() > 0` before each notify. This is the ACB/credit flow control equivalent we should model in our host link layer rather than fire-and-forget.
- Multi-pair: per-device MAC byte allocation (`--mac-byte1..6`) + per-pair server firmware = reference setup for our dual-dongle/1-vs-N tests.

### C. NearLink_controller — host-app↔SLE mapping + 帧聚合状态机 (设计可抄)
- Unified transport interface with submit semantics (non-blocking `send_report`); declared poll 250 µs SLE.
- Frame aggregation: per-source `seq` change detection → only re-send on data change; on send failure keep `pending_report`+`dirty` and retry **before** sampling next poll; on disconnect never send but keep filtering; on reconnect resend `last_sent_report` (cached reconnect) or pending. This "report pipeline" design transfers directly to our host SSAP data path (dedupe by content, dirty retry, reconnect replay).
- HarmonyOS mapping: SLE HID → ABS_X/Y/Z/RZ/GAS/BRAKE/HAT0X/Y + BTN_*; 4K polling consumed natively. Relevant if the TV-box route touches HarmonyOS; the general lesson is that SLE HID reports map to standard evdev axes.

### D. ili9320 SDK — SLE-Link 线上协议规范 (主 SOC ↔ 芯片 host)
- The SLE-Link Frame is the missing piece for the "host app ↔ chip-hosted SLE" bridging architecture: Flag 0x1F, Service ID 0x01–0x06 (Discovery/Connection Mgmt/SSAP Client/SSAP Server/Factory Test/Low Latency), per-service Command ID, BodyLen, TLV body, 2-byte CRC MIC, Total Frame/Frame Seq segmentation. If we ever take the "chip runs SLE host, Linux app talks SLE-Link over UART" shortcut for WS73, this doc is the wire contract. Even without it, the TLV+MIC+segmentation frame pattern is a model for our own host↔dongle framing.

---

## 汇总表

| 项目 | 星闪相关度 | 技术栈 | 价值等级 | 一句话理由 |
|---|---|---|---|---|
| NLChat | 边缘 | Java/Android + CH34x serial | 低 | 串口透传到厂商 SLE_UART 固件，APP 无 SSAP 逻辑 |
| NLChat_Web | 边缘 | TS/React/Vite Web Serial | 低 | 同 NLChat 的 Web 串口版，无协议 |
| NearLink_controller | 核心(设计) | C firmware + ArkTS | 中 | SLE transport 是 stub，但帧聚合/电源状态机+鸿蒙映射设计可抄 |
| LinkNebula | 核心(自有协议) | Rust no_std + FFI | 中 | 自研 mesh 路由/分片/服务发现，非标准 SSAP 但思想可迁移 |
| NearLinkSLE | 核心 | C / WS63 SSAP | **高** | 最干净的 SSAP 双端样例，行为顺序可直接对照 |
| sle_measure_sdk | 核心 | C / WS63 | **高** | PHY/MCS/CI/载荷/QoS 信用门控调优面 |
| SparkLink-FallDetection | 核心 | C/C++/TinyML/WS63 | 中-高 | SSAP 通知链+CCCD 坑+实测时延文档宝库（源码不在仓） |
| nearlink-firmwares | 边缘 | Tauri/Rust 工具 | 低-中 | 固件商店/烧写/AT 命令集，生态工具非协议 |
| tp78_v3_open | 核心(产品) | fwpkg 预编译 | 低-中 | BS21 SLE 2K 键盘+dongle 产品验证，无源码 |
| FlashKeyboard | 核心(产品) | C / BS2x SDK | 低-中 | BS21 SLE HID 键盘，厂商 slp 组件接线方式参考 |
| ili9320-i80-hi2821e-spi-bridge | 核心(文档) | C / BS21E SDK | **高** | 官方 SLE-Link 线上协议规范 + BS21E SLE 服务样例 |
| ws73v100-wifi | 核心(厂商) | C / WS73 SDK | 低 | 即我们 SDK 的平行分支，确认基线无新知识 |
| NearLink-Mesh-ePaper | — | — | — | 本地缺失（未克隆到） |

---

## 可借鉴清单（电视盒落地 / SSAP 栈 + WS73 dongle）

1. **SSAP 行为蓝本（NearLinkSLE）**: 服务注册顺序 register→add_service→add_property→**add_descriptor(CCCD 0x01/0x00)**→start；客户端 MTU 520 → find_structure → 激活写 → 等 Notify；断连即 re-announce/re-seek。用于给 `stack/ssap/` 补"通知使能描述符"与断连重连语义的对照用例。
2. **CCCD 缺失静默丢包教训（FallDetection + NearLinkSLE 双源确认）**: 我们的 SSAP server 必须显式处理描述符注册/激活，否则 Notify 会被静默丢弃 — 写进 `ssap_link` 测试用例。
3. **信用门控发送（sle_measure_sdk QoS）**: host 侧发送应模拟 `send_data_cb(link_qos_state)` / ACB 计数门控，替代盲目 fire-and-forget；128B 每包 + 2ms×100 重试队列是现成的非阻塞策略参考。
4. **PHY/MCS/CI 调优面（sle_measure_sdk）**: 用 1M/2M/4M PHY、MCS≤10、payload≤1370B、conn interval 20 的组合做 dongle 吞吐/时延标定；多对 MAC 分配方案用于双 dongle 1-vs-N 测试。
5. **帧聚合/脏重试/重连回放（NearLink_controller P3）**: 主机数据面用 seq 去重、`pending_report`+`dirty` 重试优先、断连缓存最后一帧、重连回放 — 直接对应我们 `ssap_link` 的可靠传输设计。
6. **SLE-Link 线上规范（ili9320 SDK 文档）**: 若走"芯片内 host + Linux 应用桥接"捷径（AT/SLE-Link 桥，见 RESEARCH-DIRECTIONS §六.1），Flag 0x1F / Service ID 0x01–0x06 / TLV Body / CRC MIC / 分帧的帧格式就是线上契约；其 TLV+MIC+分帧模式也是我们 host↔dongle 自研帧的好模板。
7. **BLE+SLE 双协议产品模式（FallDetection）**: 板间 SLE + 手机 BLE 双栈共存与供电预算注意事项，对电视盒"星闪外设+WiFi/BLE 并存"落地有借鉴（WS73 三模一致）。
8. **HID over SLE（tp78/FlashKeyboard/NearLink_controller）**: 2kHz SLE HID 键盘/dongle 与 4K 手柄已被社区验证 — 电视盒遥控器/手柄路线可行性的市场证据；鸿蒙侧 ABS_* 映射表可复用。

---

## 角色差异说明（重要）

- 这些项目全部是**芯片侧 (chip-host)**: SSAP 跑在 Hi2821/WS63/BS21E 芯片内的 LiteOS + 厂商 SSAP 库，app 直接调 `ssaps_*`/`ssapc_*` 同步/回调 API。
- 我们是**主机侧 (host)**: WS73 dongle 提供 controller/LL，我们自己在 Linux 用户态 `stack/ssap/` 实现 SSAP 编解码与链路状态机。
- 因此：
  - **协议知识可迁移**: API 语义、调用顺序、MTU/PHY/CI 参数、通知使能机制、QoS 信用门控、丢包/重连语义 — 全部直接可对照我们的实现。
  - **代码不可直接复用**: `ssaps_*` 是芯片内库；我们要么用相同语义重写（已有 `stack/ssap/`），要么走 AT/SLE-Link 桥直接复用芯片内库（此时 SLE-Link 规范才可复用代码路径）。
  - 例外: 若电视盒采用 WS73 的 **sle_soc/官方 host daemon 路径**（厂商 sparklinkd），则 ws73v100-wifi 中的 host 预编译件/平台适配可直接当黑盒对照。

---

## Open questions

1. **NearLink-Mesh-ePaper 未克隆到本地** — 需重新克隆后再体检（多跳中继/AODV/AIMD over SSAP 的 app 层实现仍待评估）。
2. NearLink_controller 的 `transport_sle_eb21.c` 只是 stub — 若作者后续合入真实 SLE GATT/HID profile 实现，值得回看（其 OQ-T1 SLE GATT HID profile 正是我们电视盒 HID 路线要的东西）。
3. NLChat 硬件侧（`Bearpi_Hi2821_Pico_NLChat` / `Bearpi_Hi3863_Pico`）不在本地 — 若需要 vendor SLE_UART sample 的原始 C 源码（含 UART 透传实现细节），需另行获取。
4. FlashKeyboard 的 `protocol/slp` 只有预编译 `libslp_client.a`（无源码）— 若想深入 BS21 SLE-Link 客户端行为，只有二进制可看。
5. sle_measure_sdk 与我们的 WS73 `SET_MEASURE`/ranging 工作的具体映射（MCS/PHY 枚举值 ↔ dongle HADM 参数）尚未核对 — 建议作为下一步跨文档比对。
6. tp78_v3 正式固件的 license 门控机制（板载 license 校验）说明社区固件生态的授权模式 — 对我们分发烧写工具链有无影响待议。
