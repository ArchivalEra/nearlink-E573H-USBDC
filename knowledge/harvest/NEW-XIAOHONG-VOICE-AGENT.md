---
type: harvest
title: "xiaohong-fbb_ws63 decoded: production voice-agent on WS63 with CI1302 audio coprocessor, mongoose WebSocket agent, LVGL UI"
language: en
created: 2026-09-15
tags: [harvest, ai-agent, voice, ws63, ci1302, websocket, production]
sources:
  - "gitcode.com/xiaohong-ai/xiaohong-fbb_ws63"
trust: A
stale_after: 2027-03-15
---

# xiaohong-fbb_ws63 production voice-agent

## Executive findings

**1. Architecture: WS63 host + CI1302 audio coprocessor over UART.** The XiaoHong AI assistant (tagline translated: "one wake word, all things respond": cloud AI companion + NearLink smart-home control on fbb_ws63 + OpenHarmony LiteOS-M, CMSIS-RTOS2 tasks) offloads all audio AI to a **ChipIntelli CI1302 chip**: `src/audio/ci1302/` implements `ci1302_uart_ring` (UART ring protocol), `ci1302_opus_decode` (Opus→PCM with the documented budget "16kHz mono max frame 120ms = 1920 samples, same as libopus"), `ci1302_tts_downlink`. A companion offline KWS/TTS firmware ships in-repo (the CI1302 mono-mic Chinese KWS firmware bin (V00729, UART1 115200, 2M)). The wake-word → cloud-dialog split is dual-chip: wake locally on CI1302, dialog on cloud.

**2. The agent layer is a clean protocol-instance design.** `src/protocols/mongoose/` (vendored mongoose) carries the WebSocket transport; `agent_task.c` reads WS URL + token + protocol version from Settings (NV) on every connect attempt ("avoid OTA being later than first create" — config may change after OTA), applies them via `apply_mg_ws_config_from_settings` → `mg_ws_set_config(proto, url, token, device_id, device_id, pver)`, then runs the agent state machine (`agent_state.h`). Uplink gating: `agent_should_uplink_ci1302_opus()` decides when CI1302 audio streams to cloud; a binary-frame queue drains before Opus frames arrive ("clear first then post, avoid dropping Opus while AgntTask not yet SPEAKING") — real-world backpressure discipline.

**3. Product-grade surroundings.** LVGL on ST7789 display, littlefs + shell, OTA module, mongoose HTTP server (net_http_server.c), boards/ abstraction, hals/, `fw_protocol/` holds **released artifacts** (two `ws63-liteos-app_all_*.fwpkg` builds dated 20260308/20260317, the CI1302 bins, command-word protocol command-word/announcement protocol xlsx) — a vendor-style release layout inside a community repo. This is the most complete open WS63 AI-product codebase found in the ecosystem (as opposed to competition prototypes).

**4. NearLink role confirmed at product level.** The NearLink integration is the SDK's SLE stack driven from the app for smart-home control (no custom `sle_*.c` in the xiaohong tree) — i.e., voice-assistant + SLE-device-control is now a shipping product pattern on fbb_ws63, validating the ecosystem direction (BYLE, xiaozhi, MimiClaw-WS63-intention) with an actual released fwpkg.

## Boundaries

- Cloned and read at module level; agent state machine and mg_ws protocol internals not fully traced.
- fw_protocol binaries excluded per standing constraint (names/versions recorded only).
- The cloud server side is closed (WebSocket endpoint + token); protocol version semantics beyond `pver` field unknown.
- SLE control commands (which device types, what profile) not enumerated in the app tree.

## Reusable

- CI1302 coprocessor pattern (UART ring + Opus decode budget + TTS downlink) — the way to add offline wake/TTS to a WS63 board without porting a full audio stack.
- Per-connect settings re-read + protocol-version negotiation over WebSocket — OTA-resilient cloud config discipline.
- Binary-frame drain-before-audio queue rule — backpressure handling for mixed-priority streams.
- Release-artifact layout (fwpkg + coprocessor firmware + command-word table) as the shape for publishing our own dongle firmware artifacts.

## Comparison anchors

- vs. MimiClaw (sync 92): MimiClaw = agent loop on the radio chip itself; xiaohong = agent on WS63 + dedicated audio coprocessor — two hardware splits of the same product.
- vs. 10347 MEDBOX (batch 5): both use a second chip for voice; MEDBOX's doubao board runs xiaozhi MCP, xiaohong's CI1302 is a fixed-function KWS/TTS engine — spectrum from generic to fixed-function audio offload.
- vs. WS63-AI-ECOSYSTEM (sync 66): xiaohong is the missing production reference in that landscape — ecosystem report's "AI as leading direction" now has a released-fwpkg proof.
