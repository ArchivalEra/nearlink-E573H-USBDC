---
type: harvest
title: "HiSpark/hs-fbb-cli — official fbb framework CLI with an explicit AI-agent contract: mechanism/policy split and a one-call JSON situational probe"
language: en
created: 2026-09-13
tags: [cli, fbb, ai-agents, mechanism-policy, json-contract, sdk-autodetect, harvest]
sources:
  - "https://gitcode.com/HiSpark/hs-fbb-cli"
trust: A
stale_after: 2026-12-13
---

# HiSpark/hs-fbb-cli — official fbb framework CLI with an explicit AI-agent contract: mechanism/policy split and a one-call JSON situational probe

- Inspection date: 2026-09-13 (staleness check: pushed 2026-06-24 — ALIVE)
- Source root: `https://gitcode.com/HiSpark/hs-fbb-cli` (2.9M Python)
- Mode: read-only local program inspection; no build, network, hardware, or PCB access

## Executive findings

1. `fbb` is the **single CLI for the whole fbb framework family** (`fbb_ws63`, `fbb_bs2x`, …): install once, then build/flash/monitor every fbb SDK with SDK auto-detection (`build.py` + `target_config/` chip discovery), mirror configuration, and a component manager. [README.md:1-8]
2. **An explicit "For AI agents & skills" contract section** declares the design: "`fbb` is the **mechanism layer**. Skills are the **policy layer** — they decide when to call a verb and how to react. The contract between them is everything in JSON output & exit codes." — the vendor community is codifying agent-facing tool contracts. [README.md, For AI agents section]
3. **`fbb describe --json` is a one-call situational-awareness probe** returning the full planning context: schema version, toolchain resolved paths (ninja/riscv_gcc/flasher/hsflash), SDK location/name/chips/build-entry/supported-verbs/targets/templates/examples. Agents plan from one JSON call instead of probing the filesystem. [README.md describe table]
4. Agent ergonomics: every `fbb` call **self-activates its environment** (no shared shell state needed across calls), idempotent `uv tool install … && fbb setup` bootstrap, JSON output + exit codes documented as the interface. [README.md]

## Boundaries and gaps

- Command verbs surfaced: build/flash/monitor/sdk (+component management); the full verb table is in the README's command reference (not fully dumped).
- The skills that consume this contract (the "policy layer" examples) live in the the NearLink Open Source Community skills repo (MCP toolset) — queued, not yet digested.

## Reusable for our stack

- **The mechanism/policy contract pattern** (CLI returns machine-readable state; skills decide actions) is exactly the split our nearlink-harvest skill and future tooling should formalize — our scripts currently output human text.
- `describe --json` as a preflight probe is the pattern for our own harvest preflight (one call: toolchain present? SDK found? targets?).
- Self-activating per-call environments solve the shell-state problem in long unattended agent sessions.

## Comparison anchors (vs existing reports)

- `NEW-NEARLINK-VIP-KNOWLEDGE-ECOSYSTEM.md`: knowledge layer of the same agent-first ecosystem; hs-fbb-cli is the mechanism layer.
- `NEW-WS63-SDK-DEV-SKILL.md`: that third-party skill is a consumer of exactly this CLI's contract.
- `NEW-STACK-FUZZER-ANATOMY.md`: both show vendor ecosystems building agent-operable infrastructure.
