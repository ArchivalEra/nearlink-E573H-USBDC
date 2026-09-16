---
type: harvest
title: "SLE HID class decoded: TP78v3 keyboard (SLE 2K polling on BS21E), FlashKeyboard four-chip firmware, xinghongpai open board"
language: en
created: 2026-09-15
tags: [harvest, hid, keyboard, sle, bs21e, polling, hardware]
sources:
  - ""/mnt/hdd/nearlink-stuff/tp78_v3_open""
  - ""/mnt/hdd/nearlink-stuff/FlashKeyboard""
  - ""/mnt/hdd/nearlink-stuff/xinghongpai-nearlink-dev-board""
trust: B
stale_after: 2027-03-15
---

# SLE HID class: keyboards and open board

## Executive findings

**1. TP78v3 (tp78_v3_open, GPL-3.0) — the flagship SLE HID product with hard performance numbers.** TrackPoint78 three-mode mechanical keyboard (75% layout, TrackPoint, TouchBar mouse buttons, OLED, USB hub, magnetic dock) on **Hi2821/Hi2821E (BS21E-class) main controller: USB/BLE/SLE tri-mode with wired 8K polling and SLE 2K polling** — the first published SLE input-device polling rate found anywhere in the corpus. Features hardware key scanning (8K wired), VIA web key remap, online layout editor + firmware export, Windows dynamic lighting, bundled **receiver dongle scheme** (integrated wireless NearLink + receiver). Guidance documentation ships as 12 versioned PDFs (docs/), firmware as tp78_v3 / tp78_v3_demo / tp78_v3e. This is the real-world benchmark for SLE HID latency work.

**2. FlashKeyboard — a four-chip-portable macro keyboard framework.** "USB, WiFi, BLE and most importantly NearLink" macro keyboard; firmware tree carries **bs20 / bs21e / bs22 / bs2x chip targets** with per-chip application entries (`application/bs2x/standard/main.c` pattern) and a shared keyboard app: BLE HID keyboard server (`ble_hid_keyboard_server.c` — HID-over-GATT profile implementation usable as the BLE input reference), USB keyboard service, `hal/service_controller.c` for service toggling. This is the companion of the Rust keyboard-cli harvested earlier (Playjoy HID protocol). SLE support lives in the chip trees — same multi-chip Kconfig pattern as competition Kconfig-role projects, but productized.

**3. xinghongpai-nearlink-dev-board (HuaqiuOpenHardware) — an honest open-hardware archive.** WS63V100/Hi3863 open dev board with KiCad hardware + BOM + firmware examples migrated from the original JLC (LiChuang) package. Notable for its governance hygiene: `CHANGES_FROM_ORIGINAL.md` tracking deviations, `downloads/MANIFEST.md` for asset checksums, and a README that explicitly states the bundled examples reference an external SDK and are not standalone-buildable — the correct way to publish vendor-derived examples without redistributing SDK code. Hardware dirs excluded per standing constraint (only README/governance read).

## Boundaries

- TP78 guidance is PDF-only (12 versions) — content not parsed; SLE 2K polling is the README claim, not measured.
- FlashKeyboard's SLE HID transport (if implemented beyond chip-tree support) not traced; BLE HID server read at file level only.
- xinghongpai board pinouts/electrical design untouched (PCB constraint); its value here is governance practice + board existence, not electronics.

## Reusable

- SLE 2K / wired 8K as the reference polling-rate pair for any SLE input-device project (vs BLE ~1K) — the concrete "SLE for HID" pitch numbers.
- Receiver-dongle pairing scheme (keyboard + dedicated SLE receiver) — the product shape closest to our own dongle goals.
- FlashKeyboard's per-chip application-entry layout (`application/<chip>/standard/main.c` + shared app) — cleaner multi-chip template than Kconfig-only role switching.
- HID-over-GATT server implementation as the BLE-side twin for tri-mode input devices.
- xinghongpai's manifest/changes governance for republishing vendor-derived assets.

## Comparison anchors

- vs. FlashKeyboard Playjoy protocol (keyboard-cli, sync earlier): FlashKeyboard = own firmware with standard HID profile; Playjoy = reverse-engineered vendor protocol — the two ends of the input-device spectrum.
- vs. 10714 badminton broadcast: TP78 needs 2K bidirectional-ish low jitter (connection mode), 10714 needs many-to-one (broadcast mode) — HID pushes SLE firmly into the connected camp.
- vs. our E573H dongle: a SLE-HID receiver is exactly the kind of top-level product the dongle could host; TP78's receiver scheme is the market validation.
