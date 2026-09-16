---
type: harvest
title: "starclaw/MimiClaw verdict: $5-chip pure-C AI agent loop (ESP32-S3 today, WS63 slot open in the NearLink org)"
language: en
created: 2026-09-15
tags: [harvest, ai-agent, esp32, verdict, architecture]
sources:
  - "/mnt/hdd/nearlink-stuff/starclaw"
trust: B
stale_after: 2027-03-15
---

# starclaw/MimiClaw verdict

## Executive findings

**1. What it is: a bare-metal AI agent loop in pure C on a $5 ESP32-S3.** MimiClaw (upstream, MIT) implements the full OpenClaw-style assistant stack with no Linux and no Node.js: Telegram long-polling channel (core 0) → inbound queue → agent loop (core 1: context builder → LLM call over HTTPS → tool_use branch → web_search tool → outbound queue), a local WebSocket server on :18789 as a second channel, serial CLI, and 12MB SPIFFS for persistent memory that survives reboots. Dual providers (Anthropic + OpenAI) switchable at runtime. Modules in `main/`: agent, bus, channels/{telegram,feishu}, cron, gateway, heartbeat, llm, memory, ota — a complete micro-agent firmware taxonomy. GitCode mirror lives in the hinearlink (StarFlash open community) org described as "runnable on NearLink WS63 boards".

**2. Why it matters to us despite being ESP32: it is the blueprint for a WS63 NearLink assistant.** The NearLink ecosystem's AI direction (FBB ModelZoo, BYLE voice, xiaozhi-server, this repo) all converge on the same shape: tiny always-on radio chip + cloud LLM + tool loop + local memory. Porting the *interfaces* (not the ESP-IDF internals) to WS63 means swapping the transport: Telegram-over-WiFi → an SLE/SSAP channel or Nld D-Bus channel; WiFi WebSocket → NearLink link. The architecture doc's queue/bus/agent/tool layering is radio-agnostic C. **No WS63/NearLink code exists in the tree yet** (grep confirms only feishu_bot.c mentions it incidentally) — the GitCode description is an intention, not a port. This is the open slot the community org is signaling.

**3. The $5-chip constraint validates our dongle-class ambitions.** MimiClaw's headline (0.5W, 24/7, flash-only memory, HTTPS-capable TLS on device) shows a microcontroller-class chip can host a real agent loop; WS63 has comparable RAM class plus the NearLink radio. The missing piece for a WS63 variant is the same one our SSAP stack addresses: a device-side service surface an agent could call (SSAP server = its "tools").

## Boundaries

- Read at architecture + tree level; agent_loop/llm module internals not traced (radio-agnostic, low NearLink relevance).
- The hinearlink fork's delta vs upstream was not diffed (single-clone local library); assume = upstream HEAD 3a1c1e9 (feishu integration merge).
- ESP-IDF-specific code (idf_component.yml, sdkconfig.defaults.esp32s3, partitions.csv) is not portable; only the module taxonomy and protocol shapes are.

## Reusable

- Module taxonomy (agent/bus/channels/cron/gateway/heartbeat/llm/memory/ota) as the checklist for any firmware-side agent design.
- Queue-per-direction + bus decoupling between channels and agent loop — the pattern that keeps transport swappable (our SLE channel would slot in as a third channel).
- Runtime-switchable LLM provider behind one interface.
- Persistent-memory-on-flash with cron/heartbeat — the "assistant that survives reboots" baseline.

## Comparison anchors

- vs. WS63-AI-ECOSYSTEM report (BYLE/MimiClaw sync 66): this closes the loop — MimiClaw source is now mapped; the ecosystem report's claims are verified at tree level.
- vs. xiaozhi-server (10347 MEDBOX, batch 5): same agent-with-tools idea; xiaozhi runs in the cloud with device as peripheral, MimiClaw runs the loop on-device — two different splits of the same stack.
- vs. our SSAP stack: an on-device agent needs a tool surface; SSAP services are the natural tool API — candidate experiment: expose one SSAP service as an MimiClaw-style tool on the dongle host.
