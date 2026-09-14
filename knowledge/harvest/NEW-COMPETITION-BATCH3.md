---
type: harvest
title: 2026 competition batch 3 — smart transport cold-chain, WS63E smart door lock (NFC+mmWave+I2S), mini-quadcopter
language: en
created: 2026-09-13
tags: [ws63, ws63e, competition, cold-chain, nfc, mmwave, quadcopter, harvest]
sources:
  - url: https://gitcode.com/HiSpark/2026_embedded_competition
    note: IOT/10019 + IOT/12648 + IOT/14624; cloned 2026-09-13, pushed 2026-09-07
trust: verified
stale_after: 2026-12-13
---

# 2026 competition batch 3 — smart transport cold-chain, WS63E smart door lock (NFC+mmWave+I2S), mini-quadcopter

- Inspection date: 2026-09-13 (competition corpus batch round)
- Mode: read-only README + tree-level; no build, network, hardware, or PCB access

## Executive findings

1. **10019 smart transport** (cold-chain fruit/vegetable): WS63 master + temp/humidity + auto-humidifier + fan + air-pump + vibration + GPS + solar-LiFePO4 + LCD touch + WeChat mini-program MQTT cloud, dual auto/manual mode. A complete agricultural IoT closed loop. [10019 README]
2. **12648 WS63E smart door lock**: NFC card auth + MQTT remote + **I2S voice intercom** + OLED + **mmWave radar human detection** + LED/buzzer/servo actuators + HarmonyOS App remote — the densest single-board peripheral integration in the corpus, and **WS63E** usage confirmation. [12648 README]
3. **14624 mini-quadcopter**: WS63 flight controller (README empty/stub at time of writing; structure queued for deeper read). [14624 tree]

## Boundaries and gaps

- README-level; code quality and depth not verified this pass.

## Reusable for our stack

- WS63E + I2S voice + mmWave radar + NFC on one board confirms the E-variant's peripheral richness for door-access products.
- Cold-chain transport monitoring is a new domain cluster (agricultural IoT) beyond the safety/eldercare clusters already mapped.

## Comparison anchors (vs existing reports)

- `NEW-2026-COMPETITION-SURVEY.md`: the classification taxonomy; these fill the transport/lock/robot clusters.
- `NEW-SMARTEDGE-MULTI-ENTRY.md`: WS63E multi-peripheral density comparison.
