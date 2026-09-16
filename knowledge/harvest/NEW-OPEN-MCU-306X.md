---
type: harvest
title: "HiSpark/open_mcu — the 306x MCU family (3061M/3065H/3066M, 128KB motor-control parts): FBB-adjacent but NOT NearLink"
language: en
created: 2026-09-13
tags: [mcu, 3061m, motor-control, no-nearlink, vendor, harvest]
sources:
  - "https://gitcode.com/HiSpark/open_mcu"
trust: A
stale_after: 2027-03-13
---

# HiSpark/open_mcu — the 306x MCU family (3061M/3065H/3066M, 128KB motor-control parts): FBB-adjacent but NOT NearLink

- Inspection date: 2026-09-13 (staleness check: pushed 2026-09-08 — fresh)
- Source root: `https://gitcode.com/HiSpark/open_mcu` (884M)
- Mode: read-only structural inspection; no build, network, hardware, or PCB access

## Executive findings

1. The 306x family (3061M/3065H/3066M/3065P, 128KB flash, QFN32/QFP32/QFN48/QFP48 packages) are **HiSilicon MCUs without NearLink** — motor-control/fan reference designs (`high_speed_fan_1shunt`), no SLE/NearLink stack found in the application tree. [README.md purchase table, src tree]
2. The repo is FBB-adjacent (same HiSpark org, same src/layout conventions, HiSparkStudio 1.0.0.10+ support) with ICKEY purchase part numbers — vendor distribution information for the MCU line.
3. Net: peripheral to our NearLink harvest; tracked so the chip-family map does not falsely attribute NearLink to the 306x parts.

## Boundaries and gaps

- Structural census only; 884M not analyzed beyond the NearLink-absence check.

## Reusable for our stack

- Chip-family map correction: 306x = motor-control MCUs, no NearLink — prevents future false matches on "HiSilicon + HiSpark org" searches.

## Comparison anchors (vs existing reports)

- `NEW-FBB-WS53-SDK.md`: contrast — WS53 is a Combo SoC with SLE; 306x is MCU-class without it.
- `NEW-WS63-AI-ECOSYSTEM.md`: ModelZoo targets WS63/HiDiTing/Hi1156E, not the 306x.
