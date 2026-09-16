---
type: harvest
title: "HiSpark 2026 embedded competition corpus survey — ~65 NearLink student projects (4.7G IOT + 1.8G AIOT); classification, chip census, and deep-dive queue"
language: en
created: 2026-09-13
tags: [competition, ws63, ws63e, bs21e, survey, classification, corpus, harvest]
sources:
  - "/mnt/hdd/nearlink-stuff/2026_embedded_competition"
trust: verified
stale_after: 2026-12-13
---

# HiSpark 2026 embedded competition corpus survey — ~65 NearLink student projects (4.7G IOT + 1.8G AIOT); classification, chip census, and deep-dive queue

- Inspection date: 2026-09-13 (staleness check: pushed 2026-09-07 — ALIVE)
- Source root: `/mnt/hdd/nearlink-stuff/2026_embedded_competition` (8.6G; AIOT = AI edge direction, IOT = **NearLink IoT direction**)
- Mode: read-only structural census; per-project digests queued, none executed this pass
- Scope: register the corpus, classify by domain, census the chip usage, and mark the highest-value deep-dive targets

## Executive findings

1. Corpus scale: **~65 team projects in the NearLink IOT track** plus 12 AIOT projects (SS928 vision direction), 62 of 65 IOT projects carry READMEs per the submission convention (`team_number_work_name/code/README.md`). This is the largest single source of diverse WS63/SLE application knowledge we hold — each project is a full firmware + docs artifact. [`IOT/` listing, README.md submission rules]
2. **Chip census (from project names)**: WS63 dominates; WS63E appears in 4 projects (vision-guiding glasses, electric-safety control, eldercare monitor, smart pill box); BS21E in 1 (SLE location); H3863 in 2 + BearPi H3863/HH-D02 pair; WS62 in 1. [IOT/ name census]
3. Domain classification of the IOT track (approximate clusters from project names):
   - **Safety/eldercare**: fall detection ×2, non-camera fall detection, eldercare home guardian ×2, Spinal/orthopedic detector, varicose-veed pressure socks, diabetic seniors monitoring, Smart Helmet, NearLink Shield
   - **Positioning/location**: SLE_Indoor_Locate, BS21E_sle_location — the two dedicated indoor-positioning entries
   - **Audio/voice**: WS63_SLE_DLNA_sound, LingLunHiSound, h3863_smart_voice_road_sign
   - **Wearables/wellness**: SignLanguage glove, Smart_glasses_platform, Health_Monitor, sleep_analysis, TennisPalBot
   - **Robotics/vehicles**: inspection robot, smart car ×3, quadcopter, material-transport workstation, dual-mode env patrol car, Wall-detection knock-echo system
   - **Industrial/instrumentation**: AI laser marking, plateau water monitor, sewage monitor (StarEye), pipe-inspection soft robot, product-quality check, MEDBOX, SmartPillBox
   - **Infrastructure**: ws63_data_transfer_unit (DTU), smart home gateway, distributed multi-node human-motion detection, self-organizing network collaborative control
4. The `nearlink-vip`-style submission README convention (code/ + docs) makes each project self-describing — the corpus is directly harvestable project-by-project.

## Deep-dive queue (priority order for future sessions)

1. `18600_SLE_Indoor_Locate` + `11706_BS21E_sle_location` — the two dedicated SLE positioning projects (method comparison: anchors? RSSI? ToF?)
2. `18884_WS63_SLE_DLNA_sound` — audio streaming over SLE (DLNA bridge — audio is our out-of-scope boundary but the transport design is relevant)
3. `24897_WS63_Distributed_Multi-Node_HumanMotionDetectionSystem` — multi-node sensing topology
4. `23778_ws63_data_transfer_unit` — another DTU design to compare with DS10
5. `18384_WS63_XingYu_SLE_SignLanguage_Glove` — sensor-glove over SLE (wearable data path)
6. `16781_WS63E_Electric_Safety_Control_System` — WS63E usage pattern

## Boundaries and gaps

- Structural census only; no project code was read this pass.
- 8.6G locally — full clone retained; per-project code quality is competition-grade by definition.
- AIOT track (SS928 vision) is adjacent (12 projects) — queued behind the NearLink track.

## Reusable for our stack

- The corpus is a **taxonomy of what people actually build with WS63/SLE** — domain distribution is market signal for our tri-mode use cases (safety/eldercare and location are the biggest clusters).
- Competition code shows the real-world SDK usage patterns (which APIs students successfully use), a practical complement to vendor docs.

## Comparison anchors (vs existing reports)

- `NEW-SLE-1V8-VEHICLE.md` / `NEW-SMART-CABINET-FULLCHAIN.md`: previous single-competition-project digests; this corpus generalizes that genre.
- `NEW-PET-COLLAR-GATEWAY.md`: same student-project lineage.
- `NEW-DS10-SLE-DTU.md`: `23778_ws63_data_transfer_unit` is the student counterpart of the commercial DTU.
