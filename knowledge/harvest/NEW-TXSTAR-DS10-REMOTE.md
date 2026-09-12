---
type: harvest
title: La-OHV/tx_star — DS10 control-loop companion: Android 20 Hz binary-frame joystick over NearLink DTU to STM32 receiver
language: en
created: 2026-09-13
tags: [ds10, dtu, stm32, android, ch340, crc8, remote-control, frame-sync, harvest]
sources:
  - url: https://github.com/La-OHV/tx_star
    note: cloned 2026-09-13, pushed 2026-09-09 (fresh, not archived), 7.6M; STM32F427 + Android app tree
trust: verified
stale_after: 2026-12-13
---

# La-OHV/tx_star — DS10 control-loop companion: Android 20 Hz binary-frame joystick over NearLink DTU to STM32 receiver

- Inspection date: 2026-09-13 (staleness check: pushed 2026-09-09, not archived — ALIVE)
- Source root: `/mnt/hdd/nearlink-stuff/tx_star`
- Mode: read-only local program inspection; no network, build, hardware, or PCB access
- Scope: the phone-to-robot control loop built on the DS10 NearLink DTU digested in `NEW-DS10-SLE-DTU.md`

## Executive findings

1. Same ecosystem as DS10-TTL (same DS10 module, related authorship): an Android dual-joystick remote connects over USB OTG + CH340 serial (115200 8N1) to a DS10 NearLink DTU; an STM32F427 receiver parses the wireless stream — the full phone → NearLink → MCU robot-control chain. [`README.md:1-8`]
2. The control framing is fixed-length binary at 20 Hz with **sequence numbers for loss detection and CRC-8 for integrity**; the master switch forces all joystick/switch values to zero — a safety default rather than stop-frames. [`README.md:16-23`]
3. The STM32 receiver side carries the resilience logic: data struct definition, CRC verification, **automatic re-synchronization after byte misalignment**, and dropout detection (timeout-based) — the receive task starts via `UART_Start_Receive_IT` on huart6. [`Core/Inc/remote_control.h:33`], [`Core/Src/main.c:93`], [`README.md:23`]
4. App structure is a two-tree monorepo: `MyDemo260908/` Android app (UI + USB-serial + protocol) and the STM32 CMake/Keil-style firmware tree with the HAL UART IT receive path. [`README.md:25-29`]

## Boundaries and gaps

- The wireless link quality (latency/jitter at 20 Hz) is not measured in-repo; the DS10 transport envelope applies (see `NEW-DS10-SLE-DTU.md`).
- 20 Hz fixed-frame control is well below the DS10 latency knee (32 B ≈ 14 ms one-way) — the design is conservative and safe for its payload size.
- Security: none (no pairing enforcement documented on the control path).

## Reusable for our stack

- The seq + CRC-8 + resync + timeout dropout pattern is the minimal robust framing for command streams over a transparent SLE serial link — directly matches our SLE UART framing discussions and the DS10 noise-floor diagnostics.
- Force-zero-on-master-switch is a clean safety invariant for actuator control over wireless.

## Comparison anchors (vs existing reports)

- `NEW-DS10-SLE-DTU.md`: this repo is the control-loop consumer of that DTU's transport envelope.
- `SLE-UART-VARIANTS.md`: another member of the framed-serial-over-SLE family, from the robotics side.
- `NEW-SLE-1V8-VEHICLE.md`: both are vehicle/robot control links over SLE serial; vehicle uses SBUS passthrough, tx_star uses app-generated binary frames.
