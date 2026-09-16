---
type: harvest
title: "2026 competition — distributed multi-node human motion detection: BNO085/BMI270 wearables over WiFi bridge with SlimeVR protocol and CD4052 UART multiplexing"
language: en
created: 2026-09-13
tags: [ws63, h3863, bno085, bmi270, slimevr, motion-capture, bridge, uart-mux, competition, harvest]
sources:
  - "ArchivalEra/nearlink-E573H-USBDC workspace"
trust: A
stale_after: 2026-12-13
---

# 2026 competition — distributed multi-node human motion detection: BNO085/BMI270 wearables over WiFi bridge with SlimeVR protocol and CD4052 UART multiplexing

- Inspection date: 2026-09-13 (competition corpus deep-dive)
- Source root: `IOT/24897_WS63_Distributed_Multi-Node_HumanMotionDetectionSystem` (BearPi-Pico H3863 firmware + web-studio PC app)
- Mode: read-only local program inspection; no build, network, hardware, or PCB access

## Executive findings

1. **Multi-node posture capture with a Bridge topology**: multiple WS63 wearable sender nodes BNO085 attitude data → a Bridge node provides unified network access → PC receives posture over UDP → WS63 WEBSTUDIO renders live motion capture, tracker status, UDP statistics, action replay, and rehab-training exercises (sit-stand, arm-raise). [README.md architecture + features]
2. **Cost-reduction hardware trick**: chest node connects one BNO085 directly; limb nodes use a **CD4053 analog switch to time-multiplex TWO BNO085 sensors on a single UART** — halving the peripheral interface count per node. [README.md feature 2.1]
3. The firmware integrates **both BNO085 and BMI270** IMU drivers plus a **SlimeVR client** (`slimevr.c`) — the open VR body-tracking ecosystem protocol running on WS63, and a quaternion module (`quaternion.c`) — positioning this as an open alternative body tracker. [ ws63-firmware peripheral tree]
4. The wireless leg is **WiFi (Bridge + UDP), not SLE** — despite the NearLink competition track, the motion data rides WiFi; the NearLink role is the platform/competition context. Contrast with the SLE-audio project which used SLE for the heavy stream. [README.md feature 2.2]
5. PC web-studio includes a ** rehab-training module** (sit-stand/arm-raise exercises with feedback) — motion data applied to health training rather than just visualization. [README.md feature 2.4]

## Boundaries and gaps

- README + tree-level analysis; the UDP payload format and SlimeVR message mapping were not traced line-by-line.
- Sync quality (sample rates per node over WiFi) is not measured in the docs.

## Reusable for our stack

- The **CD4053 UART multiplexing** trick (two I2C/UART sensors on one port) is a hardware-cost pattern for multi-IMU wearables — worth remembering for dongle-attached sensor designs.
- SlimeVR-over-WS63 confirms the vendor WiFi stack handles the open body-tracking protocol — a ready application template for our dongle if positioned as a WiFi device.
- The Bridge-topology (wearables → bridge → PC/UDP) is the WiFi analogue of the SLE bridge projects.

## Comparison anchors (vs existing reports)

- `NEW-SLE-AUDIO-SPEAKER.md`: same competition, opposite transport choice (WiFi vs SLE) for heavy streams — the corpus reveals developers choosing WiFi for bandwidth, SLE for identity/low-power.
- `NEW-BEARPI-H3863-DOCS.md`: same BearPi H3863 platform and SDK tree.
- `NEW-SMART-CABINET-FULLCHAIN.md`: same bridge→UDP→PC visualization shape.
