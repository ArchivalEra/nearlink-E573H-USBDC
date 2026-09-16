---
type: harvest
title: "Fresh scan round: AI-Agent-over-SLE night-fishing controller, Hi3863 aquaponics pair, hispark-rs upstream all-current"
language: en
created: 2026-09-16
tags: [harvest, ai-agent, sle, scan, freshness]
sources:
  - "github.com/CC-ZCL/night-fishing-nearlark-agent"
  - "github.com/XieMoMoJuan/hi3863-smart-aquaponics"
  - "github.com/hispark-rs/hisi-rf-ws63 (pushed 2026-09-10)"
trust: B
stale_after: 2027-03-16
---

# Fresh scan round 0916

## Executive findings

**1. night-fishing-nearlark-agent — "AI Agent as an interface layer" over SLE, graduation-design grade.** FS-W63 (WS63/Hi3863) night-fishing light controller with four layers: perception/execution (BH1750 ambient light + HC-SR04 fish detection + PWM stepless dimming; **SM2/SM3/SM4 national-crypto engine** claimed on-device), SLE 1vN network (~12Mbps empty-air figure quoted), **AI Agent service (FastAPI + WebSocket)** — user chats in natural language ("help me lure fish", "dim it a bit") on a Flutter app, agent maps intent to structured commands that drive multiple lights over SLE 1vN — and a mobile layer. This is the **fourth independent appearance of the LLM-agent-over-NearLink pattern** (after xiaozhi-MCP MEDBOX, on-device MimiClaw, xiaohong voice agent) and the first with a self-hosted FastAPI agent instead of a vendor cloud. MIT.

**2. hi3863-smart-aquaponics — dual-board SLE sibling in the standard shape.** Two BearPi Pico Hi3863 boards (execution node: water level/turbidity/TDS/water-temp + pump/fan/humidifier/light + **PID-closed-loop roller motor for light adjustment**; interaction node: TJC serial screen for thresholds and control), SLE between them, WiFi/MQTT to Huawei IoT, WeChat mini-program, **ASRPRO offline voice (5th appearance in the corpus)**, WS2812B status lighting, five operation modes (manual/auto/day/night-maintenance/show).

**3. hispark-rs upstream verified current.** `hispark-rs/hisi-rf-ws63` pushed 2026-09-10 05:55 UTC, HEAD ce68c14 "test: capture NET0 stop runtime progress before probe attach" — the same NET0 RX-stop campaign our September-increment report (sync 71) already covered ("NET0 RX-stop ownership contract"); local library HEAD matches. No new hispark-rs content since the increment report; the Rust WS73 backend remains the freshest code side of our own stack's upstream.

**4. GitHub window (pushed >2026-09-14) otherwise empty.** Top recency: our repo, communication_nearlink_service (verified 0-ahead earlier today), Terrydev5/NearLink (squat #4, sync 93), teki128 (current). The NearLink ecosystem's public-GitHub surface continues to be thin; GitCode + the competition corpus remain where the mass is.

## Boundaries

- Both projects cloned/read at README+structure level; night-fishing agent's intent→command mapping and SM2/3/4 claims not code-verified (SM-crypto on WS63 would need SDK evidence; treat as claimed).
- Aquaponics PID/roller-motor specifics not traced.
- hispark-rs check = upstream metadata + local diff only.

## Reusable

- FastAPI+WebSocket self-hosted agent as the privacy-preserving alternative to vendor-cloud agents for NearLink device control — portable to our dongle host (replace FastAPI with a local process, WebSocket with SSAP).
- PID roller motor + threshold UI pattern — reusable greenhouse/lighting shape.
- SM2/3/4 mention as a lead: if a WS63 SDK sample provides SM-crypto, it would pair with SSAP ENCRYPTION_NEED permission bits (sync 89 contract).

## Comparison anchors

- vs. sync 93/97 agent reports: four agents now mapped across the spectrum — vendor cloud (xiaozhi), on-device (MimiClaw), product voice (xiaohong), self-hosted FastAPI (this) — the agent-transport matrix is complete enough to pick from.
- vs. 10102/15307: aquaponics is another instance of the Client/Server/gateway triad — no new topology.
- vs. hispark-rs reports (sync 71): no action needed; next trigger = a new pushed_at beyond 2026-09-10.
