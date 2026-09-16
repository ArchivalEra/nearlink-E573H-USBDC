---
type: harvest
title: "Agent-contract doc + BYLE SDK docs + fresh-scan verdicts: skills-nearlink CLAUDE.md anatomy, byleFN KWS stack, two more name-squats"
language: en
created: 2026-09-15
tags: [harvest, agent-contract, kws, verdict, ws63]
sources:
  - "https://gitcode.com/hinearlink/skills"
  - "https://gitcode.com/HiSpark/YunZhiSheng_WS63AI"
  - "github.com/Terrydev5/NearLink"
  - "github.com/Eironax/Qwac"
trust: B
stale_after: 2027-03-15
---

# Agent contract + BYLE SDK + scan verdicts

## Executive findings

**1. skills-nearlink `CLAUDE.md` (462 lines) — a complete, self-evolving AI-agent working contract for WS63 development.** The StarFish community's skills repo ships a CLAUDE.md designed to be dropped into `~/fbb_ws63/src/` so Claude gains persistent working memory for that tree: a load-memory → parse-task → execute → self-evolve loop where new pitfalls/APIs discovered during work are written back into the file. Concrete content beyond the meta-loop: directory access rules (only `/application/samples/peripheral` writable, only `/include` includable — no private headers from `/drivers`/`/kernel`, no `#define` overriding Kconfig), build surface (`python3 build.py ws63-liteos-app -j16 -ninja`, `-def=MACRO=1` / `-def=-:MACRO` / `-component=XXX`), fwpkg outputs (`_all.fwpkg` full vs `_load_only.fwpkg` app-only), a mandatory 5-step demo workflow (dir + parent CMakeLists + parent Kconfig + entry mount), file naming/task-signature conventions, Apache-2.0 header template with author attribution, and an anti-hallucination clause (no PLC or unrelated modules). This is the most complete public example of an AI-agent repo contract for embedded work — a design reference for any `AGENTS.md`-class file we publish.

**2. BYLE AI Audio UI SDK docs (YunZhiSheng_WS63AI) — the closed AI-audio alternative stack.** BYLE's byleFN-series chips (byleF6/3/5, NOT HiSilicon): KWS offline wake-word + LLM dialog + opus codec + BT + WiFi/4G + display/LED, uC/OS RTOS, pi32v2 clang, ByleStudio IDE with JSON-config → generated C headers, prebuilt `liba/` (opus, KWS), **barge-in (voice interruption)**, product templates (speaker, voice lamp, fan, intercom, printer). Access gate is email-to-account (closed SDK). Value for us: the feature checklist (KWS + barge-in + codec + UI + NV) that a WS63-class AI-audio product is expected to have, and evidence the NearLink-adjacent AI-audio market has a second chip vendor competing with FBB.

**3. Fresh-scan verdicts (GitHub created >2026-08-25 window).** `Terrydev5/NearLink` (2026-09-15): Bonjour/DNS-SD discovery + WebSocket control + temp TCP file transfer with SHA-256 + one-time token — **zero NearLink radio; name-squat #4**; its token-gated ephemeral-port transfer design is itself a neat security pattern, but not NearLink. `Eironax/Qwac` (2026-08-26): announced "alternative to Sparklink Playjoy, open source and lightweight" — **repo contains only a LICENSE (GPL-3.0), zero code**; a watch-list candidate for the Playjoy HID ecosystem, recheck on next scan. No other genuinely-NearLink new repos this window.

## Boundaries

- CLAUDE.md is advice-to-agents, not executable truth: its build flags were not executed (would require the full SDK); treat command surface as documented intent.
- BYLE content is docs-only (SDK itself is account-gated); chip capabilities unverified beyond the doc.
- Qwac/Terrydev5 verdicts at clone level; recheck policy: only on content change.

## Reusable

- Self-evolving CLAUDE.md loop (read-on-start, write-back new pitfalls) + directory access matrix + anti-hallucination clause — template for publishing our own agent contract (e.g. for `assets/stack/ssap/`).
- `_all.fwpkg` vs `_load_only.fwpkg` distinction — standard fwpkg vocabulary worth adding to fwpkg docs.
- BYLE checklist (KWS/barge-in/JSON-config/IDE) as the feature table for AI-audio-on-microcontroller comparisons.
- Token-gated ephemeral TCP transfer (Terrydev5) — transferable idea for dongle-hosted file services.

## Comparison anchors

- vs. hs-fbb-cli + nearlink-vip (GitCode batch): skills-nearlink completes the triad of agent-facing artifacts (CLI tool / knowledge org / repo contract) — the StarFish AI-developer surface is now fully mapped.
- vs. our AGENTS.md: theirs is single-tree, self-mutating, with hard access rules; ours is multi-plane (knowledge/assets) — their access matrix + evolve-loop are worth adopting selectively.
- vs. MimiClaw (sync 92): both put AI workflow discipline into repos; MimiClaw is runtime agent firmware, skills-nearlink is build-time agent guidance.
