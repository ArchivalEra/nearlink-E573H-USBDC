---
type: harvest
title: "hbu-dragon WS63 case library enumerated: trusted starBeacon, hybrid relay node, DICSOSS distributed collection; nearlink-contrib still MPU6050-only"
language: en
created: 2026-09-15
tags: [harvest, cases, beacon, hybrid, dicsoss, ohos, standards]
sources:
  - ""/mnt/hdd/nearlink-stuff/openharmony-nearlink-ws63-cases (applications/sample/wifi-iot/app/*)""
  - ""/mnt/hdd/nearlink-stuff/nearlink-contrib""
trust: A
stale_after: 2027-03-15
---

# hbu-dragon case library enumeration

## Executive findings

**1. The full case list is 25 apps** under `applications/sample/wifi-iot/app/`: radar_demo, radar_led, radar_linger_warn, sle_announce_customize, sle_delay_optimize, sle_demo_1v1, sle_demo_1vn, sle_dicsoss, sle_hybrid_demo, sle_hybrid_mode, sle_hybrid_n_demo, sle_hybrid_n_mode, sle_light_control, sle_multi_point_gather, sle_one_to_many, sle_radar_surveillance, sle_set_wifi, sle_starBeacon(+_test), sle_throughput_optimize, sle_uart_demo_1v1, sle_uart_demo_1vn, sle_wifi_coexist, sle_wifi_radar_coexist. Targets BearPi H3863/BM-H63, NearLink_DK_WS63(E), built on OH SDK 20240628/20241021. The README also records the **SparkLink group-standard bibliography**: T/XS 30001-2023, T/XS 20002-2022/-2023, T/XS 20004-2022, T/XS 20003-2022, T/XS 20001-2022, T/XS 30002-2023, T/XS 30003-2023, T/XS 00002-2022, TR/XS 00001-2021 — the definitive citation list for SLE standards compliance.

**2. sle_starBeacon — trusted beacon with anti-spoofing, range 3km.** The beacon sample (with its own HTTP server: fsdata.c, httpd_proc.c, makefsdata.exe — beacon + config web UI on one chip) documents: SLE beacon range **cm to 3km**; on 2821E-class silicon with ranging, cm-level precise indoor positioning; and a **"trusted beacon" mode** against beacon spoofing/air-interface data leaks that BLE iBeacon lacks. This is the vendor-side answer to the beacon-security gap and a distinct mechanism to study for our pairing/security work.

**3. sle_hybrid_mode — master-slave-in-one node.** One node running both SLE server and client roles simultaneously (`sle_server.c` + `sle_client.c` in the same app), explicitly "for building linear SLE networks" — a chain of hybrid nodes = the simplest linear multi-hop without routing tables. `sle_hybrid_n_mode`/`_demo` extend the same idea.

**4. sle_dicsoss — distributed open-space collection with MQTT egress.** Multiple Data nodes collect via SLE distributed network into one Receiver node that is the SLE gateway + WiFi uplink, publishing to an MQTT broker under topic `OpenHarmony/DICSOSS/DICSOSS00001/Message`; rich devices subscribe. Module set: sle_dicsoss + oled_ssd1306 + easy_wifi + easy_httpd + paho-embed-mqtt3c/cc — a full reference IoT module bundle on the OH SDK.

**5. Measurement-optimized standard cases.** `sle_delay_optimize`: server-initiated data, client echoes back, server computes RTT latency — the canonical delay-measurement loop. `sle_throughput_optimize`: HiSilicon throughput sample ported to OH SDK with optimizations. Both are the OH-side twins of the fbb-side `sle_throughput` firmware family in hinearlink/firmware_repo (batch 15) — same measurement methodology across two SDK dialects.

**6. nearlink-contrib verdict: still embryonic.** The community "third-party port collection" (GitCode HiNearLink org, mirrored locally) contains exactly one sensor driver — MPU6050 (I2C0/I2C1 handles, complementary filter, gyro auto-calibration, Step1/Step2 doc convention) plus a samples dir. Everything in it we already covered at sync 70 via the competition copy. The two-step doc convention (integration guide + test-case guide) is its only new artifact — a good template for our own driver publications.

## Boundaries

- Case bodies were read at README/intro level (each file opens with ~20 lines of restrictive license — He Bei University copyright forbidding competition/teaching reuse of the cases themselves; **code cannot be copied into our repos**, only API/architecture facts recorded).
- sle_starBeacon's "trusted" mechanism is claimed but its crypto implementation was not inspected.
- nearlink-contrib license not checked in this pass.

## Reusable

- T/XS standard numbers list as the canonical SLE-standards citation block for our docs.
- Linear-network hybrid node pattern (server+client in one) — zero-routing-table option for chains/strings of nodes.
- RTT delay-measurement loop (server timestamps → client echo → server computes) as the standard latency harness for our dongle testing.
- Beacon range/positioning claims (cm-3km, 2821E cm-level) + trusted-beacon anti-spoof concept for our ranging security notes.
- Step1/Step2 doc convention from nearlink-contrib for publishing our drivers.

## Comparison anchors

- vs. helmet DAG mesh (batch 4) and sle_mesh_new optimizer: hybrid linear chain is the third mesh topology type — simplest, no tables, no bitmap.
- vs. sle_throughput (hinearlink firmware, batch 15): same test family in fbb and OH dialects; use both to cross-validate dongle benchmarks.
- vs. 10714 connectionless broadcast: starBeacon is the one-to-many broadcast positioning variant; 10714 the sensor-streaming variant.
