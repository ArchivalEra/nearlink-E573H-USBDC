---
type: harvest
title: "nearlink-firmwares firmware store decoded: Tauri+Rust toolbox, distributed metadata registry, NLChat/SLE-keyboard ready fwpkg pairs — sync-100 capstone"
language: en
created: 2026-09-16
tags: [harvest, firmware-store, tauri, registry, nlchat, capstone]
sources:
  - "https://github.com/MiraHikari/nearlink-firmwares"
trust: A
stale_after: 2027-03-16
---

# nearlink-firmwares firmware store (sync-100 capstone)

## Executive findings

**1. The community already runs a firmware-store infrastructure: XingShan Toolbox (Tauri + Rust).** `nearlink-firmwares` (MiraHikari) is a one-stop firmware manager combining a **firmware shop** (developer upload → user one-click flash), local library management (delete/rename/import/favorites), **built-in serial flashing** (HiSilicon AutoBurn path or third-party tools), and saved AT-command shortcuts. Desktop stack mirrors the Rust flasher family (Tauri, like ws63flash v4's GUI) — the Rust/Tauri combo is becoming the ecosystem's default desktop toolchain.

**2. The registry is a distributed, mirror-friendly JSON design.** `endpoints.json` lists multiple `metadata-collections` sources with urlPrefix/urlSuffix (maintainer default + community mirrors incl. `haohanyh_ctcc`/`haohanyh_cucc` ISP endpoints, "recommended" flag) — a resilient app-store distribution model without a central server. Each firmware dir holds `metadata.json` (`{online, packageName, description, author, categories, chips, brands, files[]}`) + readme + assets; collections aggregate per-package entries.

**3. The catalog itself is an ecosystem census: 12 firmware families.** BearPi Hi2821/BS21 AT (official), BearPi EB-MH21E/MH63E AT, BearPi GFSK/test/UART, HiHope AT + **HiHope HHD03 AT** (our HHD-0x board family), **NLChat client/server pairs in three silicon variants** (bs21_all_in_one_client/server.fwpkg, hihopews63 client/server, ws63 client/server — NLChat source we harvested now has ready-made cross-chip firmware pairs), **SLE keyboard dongle+keyboard pair** (bs21_all_in_one_dongle1/keyboard1.fwpkg — a shipping SLE HID input device, sibling of TP78v3/FlashKeyboard), and TiangongX M528H. Names + metadata prove: AT firmware, chat over SLE, SLE HID, GFSK are the four shipping firmware categories of the community store.

**4. Capstone note at sync 100.** This is the 122nd report closing the harvest's first 100 syncs. The knowledge bundle now covers: the full WS63 UART boot-ROM protocol (quadruple-verified), fwpkg format, USB HCC transport + DLI opcode/struct tables + eRPC contract (three depth layers), SSAP dialect + service model, SLE discovery/announce/connection parameters (MCS enum, HARQ feedback codes), four mesh topologies, Channel Sounding from IQ to trilateration, the complete agent-transport matrix, the flasher/store tooling family, and the competition corpus (IOT track fully swept). Open slots intentionally left for the next 100: `dli_cmd_struct.h` remaining families, HADM multi-anchor aggregation internals, Nld eRPC argument-layout details, AIOT SS928 track depth, and anything the ecosystem ships after 2026-09-16.

## Boundaries

- Firmware binaries/fwpkg files present in the repo are NOT read per standing constraint (metadata/registry only).
- Toolbox GUI (src) not traced; registry schema documented from JSON instances.
- Store's flash implementation (which flasher it wraps) unverified.

## Reusable

- Distributed endpoint + per-package metadata.json schema — a ready model for publishing our own dongle firmware updates (matches fwpkg artifacts).
- NLChat/SLE-keyboard fwpkg pairs as reference firmware for testing our USB/serial flashing paths against real community packages.
- HiHope HHD03 AT firmware entry — a lead for HHD-0x board support beyond our HHD-01 violin backup.
- Tauri+Rust as the de-facto desktop stack — align our tooling choices with it for ecosystem consistency.

## Comparison anchors

- vs. hinearlink/firmware_repo (batch 15): official index (flat, curated) vs. community store (distributed registry, GUI, mirrors) — the store supersedes the index for end users.
- vs. ws63flash family (sync 96/98): the store is the consumer-facing front-end that would wrap one of those flashers; five back-ends, one GUI layer now.
- vs. our E573H project: the missing "dongle firmware" category in the store is exactly the slot our project fills; the registry is the distribution channel it could eventually use.
