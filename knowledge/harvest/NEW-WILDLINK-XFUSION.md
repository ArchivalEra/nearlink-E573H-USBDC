---
type: harvest
title: "WildLink pair deep-dive: three-transport field telemetry (SLE+BLE+LoRA) on H3863; xfusion framework verdict (BLE yes, SLE no)"
language: en
created: 2026-09-15
tags: [harvest, h3863, sle, lora, telemetry, xfusion]
sources:
  - ""https://github.com/twyora/WildLinkClient""
  - ""https://github.com/twyora/WildLinkServer""
  - ""https://github.com/x-eks-fusion/xfusion""
trust: B
stale_after: 2027-03-15
---

# WildLink pair + xfusion

## Executive findings

**1. WildLinkClient/Server — a wilderness-safety field pair on BearPi H3863 with three transports in one device.** Client (field node): per-sensor LiteOS tasks (aht20, bmp280 over **soft-I2C**, sh1106 OLED with a `free_menu` UI, atk_lora, ble_server, emergency_alarm, sle_client) sharing one telemetry struct (`shared/telemetry/node_telemetry.h`: id, need_help flag, heart_rate min/max/now, blood_oxygen + low flag, air humidity, timestamp — a compact fixed-layout safety-telemetry record). SLE client task uses the standard SDK trio `sle_connection_manager.h + sle_device_discovery.h + sle_ssap_client.h` (53 ssap/pair call sites — the SSAP client surface used at real density). LoRA task frames with sync byte 0xAA (ATK module over UART). Server (rescue node): sle_server + max30102 + max30205 tasks — the receiving end with its own vitals sensors. Design significance: **SLE for the pair link, BLE for phones, LoRA for beyond-SLE-range** — a transport-ladder device where SLE is the middle tier, plus belt-and-suspenders vitals sensing on both ends.

**2. xfusion framework (x-eks-fusion) — component-OSD for cross-SDK embedded work; BLE abstracted, SLE absent.** The framework behind our local `fbb_ws63-xeks` port: xf_utils/xf_log/xf_heap/xf_init/xf_hal/xf_task components, plus xf_fal (flash), xf_lp (low power), **xf_nal (network abstraction: xf_netif + collect tooling)** and **xf_ble**; the design doc (DETAILS.md) states the goal as clean roles for bottom-porter / app-dev / component-contributor across different SDKs. NearLink/SLE has **no xf_* abstraction yet** — xf_ble exists but nothing for SLE. That is a concrete open slot: an xf_sle component modeled on xf_ble would be the framework-idiomatic way to make our SSAP stack portable.

**3. Covergence note: 0xAA frame sync recurs.** ATK LoRA sync byte 0xAA (WildLink), SparkSafe 0xA5, NearMeet 0xAA..0x55 — UART-framed radio modules in the ecosystem overwhelmingly use a single sync-byte start with fixed or length-prefixed bodies; our USB-PROTOCOL framing notes should reference this family as the low-end convention.

## Boundaries

- WildLinkClient/Server READMEs are one-liners — all facts here come from the code tree; no build docs.
- The telemetry struct was read, not the full SLE session state machine (53 call sites listed, not traced).
- xfusion assessed at component/README level; xf_ble API surface not read.
- No PCB/hardware dirs read (standing constraint).

## Reusable

- Fixed-layout telemetry record with min/max/now vitals fields + need_help flag — a ready-made safety-telemetry schema for SLE payloads.
- Transport-ladder device design (SLE pair link / BLE phone / LoRA long-range) — validates multi-radio coexistence beyond the WS63 (H3863 here).
- soft-I2C driver + free_menu OLED UI as reusable task-isolated H3863 peripherals.
- xf_sle as an open contribution slot in xfusion — potential home for a portable SSAP abstraction.

## Comparison anchors

- vs. 15307 CoreAqua (batch 6): both run SLE + BLE concurrently on one board; WildLink adds LoRA as the third tier — the fullest multi-transport field device in the library.
- vs. WildLinkServer pair: two-sided vitals (client senses, server senses too) is unusual — most corpus pairs are one-way.
- vs. our SSAP stack: WildLink's 53-call-site ssap_client usage is a real-world density benchmark for API ergonomics of the vendor SSAP surface.
