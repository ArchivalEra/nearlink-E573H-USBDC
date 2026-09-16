---
type: harvest
title: "Competition batch 10: XingYu sign-language glove micro-net (25ms/50Hz), NearMeet e-paper badge with calibrated-RSSI proximity, OROS env node"
language: en
created: 2026-09-15
tags: [harvest, competition, sle, wearable, rssi, epd, edge-ai]
sources:
  - ""/mnt/hdd/nearlink-stuff/2026_embedded_competition/IOT/18384_WS63_XingYu_SLE_SignLanguage_Glove""
  - ""/mnt/hdd/nearlink-stuff/2026_embedded_competition/IOT/17661_NearMeet_Badge""
  - ""/mnt/hdd/nearlink-stuff/2026_embedded_competition/IOT/17664_OROS""
trust: B
stale_after: 2027-03-15
---

# Competition batch 10: glove + badge + env node

## Executive findings

**1. XingYu sign-language glove (IOT/18384) — the best-characterized SLE wearable link in the corpus.** Two WS63 glove nodes (10 flex sensors each on a custom 10-channel voltage-divider PCB quantized by WS63 on-chip ADC to 0-330, 50Hz; JY901P 9-axis with Kalman fusion, Yaw drift <±1°) form an **SLE one-master-two-slave micro-net at 25ms connection interval, both hands 50Hz synchronized, <0.01% frame loss over 10 minutes** — the first competition project to publish link-quality numbers rather than just parameters. Edge AI: Inception-Transformer, 52 sign classes, <30ms inference on Orange Pi; sentence assembly via local rule match then DeepSeek polish; edge-tts output; WeChat mini-program UI; systemd-managed boot in 30s with offline AP recovery. Design thesis: wearable sensors dodge the three vision flaws (light, angle, privacy) — the privacy argument now appears in three separate projects (this, SparkSafe, SeaEcho).

**2. NearMeet badge (IOT/17661) — e-paper image transfer with calibrated-RSSI proximity.** 8× WS63 boards as 4 master+slave pairs around a Node.js hub (WebSocket chunked text/binary framing; 4-color EPD 768×528 masters, monochrome 240×416 slaves). Transport discipline is explicit in ARCHITECTURE.md: MCU↔hub WebSocket, **master↔slave UART (0xAA 0xBB [CMD] [LEN] [payload] [CS] 0x55 0xCC)**, **SLE lazy-loaded only in radar mode** (hub-controlled broadcastSleStart/Stop), browser HTTP POST as fallback channel for two-phase image delivery. The radar mode triggers AI image generation when 4 people come within 1m — proximity judged from SLE RSSI with **per-board 1m calibration constants (`g_my_1m_calibrated_rssi`)** baked into firmware, plus identical slave firmware differentiated only by `g_local_mac[5]` (0x01-0x04). Six AI party-game modes run on the same transport skeleton.

**3. OROS (IOT/17664) — commodity env node.** WS63 master (AHT20/BMP280/NEO-6M GPS) + BearPi H3863 body-temp node, edge-AI anomaly detection, MQTT `$oc/devices/{device_id}/sys/properties/report` to Huawei IoT. Nothing new at SLE level; the OHOS property-report topic format is the only reusable detail.

## Boundaries

- 18384's frame-loss figure is self-reported; the retransmit/aggregation code behind it (firmware_ws63/) was verified only at directory level.
- NearMeet's RSSI proximity has no published accuracy envelope; per-device calibration is the right idea but the 1m decision boundary is game-grade, not instrumentation-grade.
- 17661's "SLE image transfer" is actually hub-relayed (WebSocket+EPD) — SLE carries only proximity signals; do not cite it as an SLE image pipeline.
- 17664 read at README level.

## Reusable

- Published link KPIs (25ms interval / 50Hz sync / <0.01% loss) as the sanity targets for a 3-node SLE sensor micro-net.
- Per-device 1m RSSI calibration constant — turns commodity RSSI proximity from ±meters to sub-meter consistency; cheap and portable.
- Lazy SLE activation (radio modes on demand, hub-controlled) — power and interference hygiene for multi-mode devices.
- 0xAA 0xBB ... 0x55 0xCC UART framing with CS — the third independent confirmation of this framing family (NLChat_Web, SparkSafe, NearMeet).
- Identical-firmware MAC-differentiated fleets (07_slave_epd_demo + g_local_mac[5]) — simplest fleet provisioning for WS63-class builds.

## Comparison anchors

- vs. 10102 posture rig (batch 5): both stream dual-hand IMU-class data; XingYu adds published KPIs and flex sensors, 10102 adds cloud consumers.
- vs. 10714 badminton broadcast: connected micro-net (XingYu) vs. connectionless fleet (10714) — same wearable-sensor problem, two link models; XingYu's numbers give the connected side its benchmark.
- vs. batch 4 helmet RSSI hysteresis: NearMeet calibrates RSSI per device for absolute distance; helmet hysteresis avoids switching churn — calibration for semantics, hysteresis for stability.
