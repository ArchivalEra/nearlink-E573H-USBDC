---
type: harvest
title: "Competition batch 11 (IOT-track sweep): six remaining projects — glasses 3-node split, radar-lamp 27B frames, and pattern-confirmations"
language: en
created: 2026-09-15
tags: [harvest, competition, sweep, glasses, radar, sle]
sources:
  - ""competition-2026/IOT/17183_Smart_glasses_platform""
  - ""competition-2026/IOT/17316_Smart_Lamp_System_for_Epileptic_Patients""
  - ""competition-2026/IOT/16781_WS63E_Electric_Safety_Control_System""
  - ""competition-2026/IOT/16946_WS63_Based_Smart_Eldercare_Home_Environment_Guardian_System""
  - ""competition-2026/IOT/17725_WS63_Smart_Car_Collaborative_Workstation_Material_Transport_System""
  - ""competition-2026/IOT/18459_WS63_CarControl""
  - ""competition-2026/IOT/18026_TennisPalBot""
trust: B
stale_after: 2027-03-15
---

# Competition batch 11: IOT-track sweep

## Executive findings

**1. Smart glasses platform (IOT/17183) — the cleanest responsibility-split for wearables.** Three WS63 nodes: G (glasses: mic, glasses display, keys), D (data: **owns the SLE link + cloud ASR/LLM/translate**, session coordination), C (control: host-screen UI, SD storage, history browsing, SPI master). Meeting transcription/schedule/AI chat/translation all ride this split. The D-node-as-radio-and-cloud-broker pattern keeps the head-worn node minimal — a directly citable architecture for our own tri-mode accessory design.

**2. Epileptic-patient radar lamp (IOT/17316) — fixed-frame multi-node telemetry done right.** Two independent nodes (R60AFD1 mmWave + WS63E + 24V LED strip with MOSFET PWM), one WS63E gateway receiving **two simultaneous 27-byte "V2" SLE frames**, summarizing state, targeting lights, uplink to IoTDA. Ships wiring photos, architecture SVG, MOSFET PWM driver module, 24V→5V buck; notable for an unusually complete disclaimer section (not a medical device) and local-first processing per node. The fixed 27-byte frame with version tag is the practical middle ground between SparkSafe's framed protocol and NearMeet's bare writes.

**3. Pattern-confirmations (sweep verdicts).** 16781 electric safety: three-board star (A=Client sensors, B=Server gateway+WiFi, C=Server actuators) — third appearance of the exact Client/Server/gateway triad. 16946 eldercare: LiteOS-M self-organizing SLE net of room nodes, dialect voice recognition (Mandarin + Guanzhong), e-paper terminal, one-key SOS — architecture assembles previously-seen parts. 17725 material-transport car: car + control boards over sle_uart1 derivative, MPU6050 on the control end (gesture-steering), HiSpark .hiproj project files. 18459 CarControl: motor API layering (motor_control.h/.c/motor_demo.c) — teaching-grade PWM differential-drive module. 18026 TennisPalBot: wristband (SU-03T offline voice, 11 commands) + mecanum-wheel ball collector over SLE with heartbeat sync + WiFi/MQTT web dashboard.

## Boundaries

- Sweep projects read at README/architecture level only — no line-level code anchors; their value is confirming pattern frequencies and completing track coverage.
- 17183's cloud ASR/LLM choice is unmentioned in README (vendor unknown); D-node contract not read.
- 17316's R60AFD1 is the same radar family as batch 4/5 health projects — no new radar protocol surface.

## Reusable

- G/D/C three-node split for head-worn devices (radio+cloud broker node separate from display node).
- 27-byte version-tagged fixed frame for periodic multi-node telemetry — simplest robust upgrade path from bare writes.
- The Client/Server/gateway triad now confirmed 3× — safe default proposal for any campus/home SLE deployment.
- SU-03T offline voice as the recurring voice-IO module (4th appearance) — it is the de-facto voice peripheral for WS63 competition builds.

## Comparison anchors

- vs. 18384 glove KPIs (batch 10): 17183 is the other wearable with a display, but splits radio out of the head node instead of keeping it there.
- vs. 10102/17513: same gateway + N-node shape; 17316 adds per-node local processing before SLE (send verdicts, not raw data) — the only sweep project doing edge inference at the node.
- vs. competition corpus as a whole: IOT track now fully swept (all projects README-covered); remaining depth targets are AIOT SS928 projects, which are mostly out of NearLink scope.
