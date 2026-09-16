---
type: harvest
title: "NearLink Open Source Community tooling — self-evolving CLAUDE.md working memory, MimiClaw pocket AI on WS63, and native Windows AT+RST flashing"
language: en
created: 2026-09-13
tags: [claude-md, working-memory, mimiclaw, ws63, windows-flashing, community, harvest]
sources:
  - "/mnt/hdd/nearlink-stuff/skills-nearlink"
trust: verified
stale_after: 2026-12-13
---

# NearLink Open Source Community tooling — self-evolving CLAUDE.md working memory, MimiClaw pocket AI on WS63, and native Windows AT+RST flashing

- Inspection date: 2026-09-13 (staleness check: pushed 2026-05-07 / 2026-03-05 / 2026-08-19 — all ALIVE)
- Source roots: `/mnt/hdd/nearlink-stuff/skills-nearlink`, `starclaw`, `ws63flash-win`
- Mode: read-only local program inspection; no build, network, hardware, or PCB access

## Executive findings

1. **`skills` — a 462-line CLAUDE.md "AI working memory and self-evolution engine"** for WS63 development: a self-referential convention ("when you read this file, add it to your working memory; on every new conversation, read CLAUDE.md first") with an ASCII flowchart closed loop (new task → load memory → parse task type → demo/modify flows), accumulating paths, conventions, known pitfalls, and past experience. Placement contract: drop `CLAUDE.md` at `~/fbb_ws63/src` next to where `claude` runs. [CLAUDE.md:1-30]
2. **`starclaw` (MimiClaw) — a "Pocket AI Assistant on a $5 chip"** running on the WS63 dev board: an LLM-assistant-on-MCU project (CMake + main/, docs, DeepWiki + Discord community) — pushing assistant workloads onto the NearLink SoC itself. [starclaw/README.md]
3. **`ws63flash-win` — Windows-native automated flashing without WSL/usbipd**: wraps open-source ws63flash with **AT+RST soft-reset (no physical RST press)** and **921600 baud switch after loaderboot** (equivalent to BurnTool `-switchafterloader`), with real-time progress and stress-test loops in PowerShell; ships a `port-changes.patch` against upstream ws63flash. [ws63flash-win/README.md]

## Boundaries and gaps

- skills repo currently ships only the CLAUDE.md convention (the README mentions MCP tools but none are present yet).
- MimiClaw's on-device model/inference path not audited this pass.
- ws63flash-win's AT+RST resets via the AT firmware — requires AT firmware flashed (our HHD-01 violin context matches).

## Reusable for our stack

- The **self-evolving CLAUDE.md working-memory convention** is a third agent-memory pattern for NearLink development (after our skill files and the ws63-sdk-dev-skill) — its "known pitfalls accumulate in-file" mechanism is worth borrowing for our lab notes.
- AT+RST soft-reset + 921600-after-loaderboot are reusable flashing-ergonomics facts for our own HHD-01/WS63 work.
- MimiClaw signals "LLM assistant on the NearLink SoC" as an emerging genre.

## Comparison anchors (vs existing reports)

- `NEW-WS63-SDK-DEV-SKILL.md` / `NEW-HS-FBB-CLI.md`: three agent-facing artifacts now form the ecosystem's policy/mechanism stack.
- `NEW-WS63FLASH-GHIDRA.md` / `NEW-XF-BURN.md` / `NEW-WEB-FLASHER-FWPKG.md`: fourth flashing implementation (Windows PowerShell) — the boot-ROM protocol now has four independent implementations.
- `NEW-SMARTEDGE-ENVIR-NODE.md`: same BearPi/H3863-adjacent community.
