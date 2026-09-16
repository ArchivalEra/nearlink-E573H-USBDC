---
type: harvest
title: "OH-on-WS63 project landscape — hbu teaching case set (restrictive license) and a stub OH SDK repo; gitee/gitcode OH coverage is thin"
language: en
created: 2026-09-13
tags: [openharmony, ws63, hi3863, teaching, license, stub, harvest]
sources:
  - "ArchivalEra/nearlink-E573H-USBDC workspace"
trust: verified
stale_after: 2026-12-13
---

# OH-on-WS63 project landscape — hbu teaching case set (restrictive license) and a stub OH SDK repo; gitee/gitcode OH coverage is thin

- Inspection date: 2026-09-13 (staleness check: pushed 2025-01-21 / 2026-01-13 — ALIVE but slow)
- Mode: read-only structural inspection; no build, network, hardware, or PCB access

## Executive findings

1. **`hbu-dragon/openharmony-nearlink-ws63-cases`** (124M, Hebei University): an OpenHarmony teaching case set for WS63 (Hi3863EV100/V100, SDK 20240628/20241021, SLE badge) under `applications/sample/wifi-iot`. Notable: a **restrictive custom license** — explicit prohibition of commercial use, competitions, teaching, books, theses without compliance — a license pattern to respect when harvesting student-adjacent code.
2. **`hinearlink/nearlink_oh_ws63`** is a README-only stub ("NearLink based on OpenHarmony SDK project development") — placeholder for a future OH SDK project.
3. Net: OH-on-WS63 public coverage outside the official `device_soc_hisilicon` tree is thin — teaching cases and stubs; the real OH WS63 content remains in the vendored SDK we sparse-adopted.

## Boundaries and gaps

- The hbu sample content under wifi-iot was not enumerated per-case this pass.
- Neither repo changes the OHOS NearLink architecture picture from earlier reports.

## Reusable for our stack

- License scan before adopting teaching code: the hbu license is stricter than the OSS norm.
- OH-on-WS63 remains an open gap — the vendored device_soc tree is still the only substantive OH WS63 source.

## Comparison anchors (vs existing reports)

- `NEW-OHOS-DEVICE-SOC-WS63.md`: the substantive OH WS63 source; these repos are the periphery.
- `NEW-OHOS-DK3863-SLE-CURRICULUM.md`: vendor curriculum vs university teaching cases.
