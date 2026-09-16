---
type: harvest
title: "WANG-XU-create/DS10-TTL — commercial NearLink SLE DTU with third-generation AT dialect, measured latency/reliability characterization, and ROS2 driver"
language: en
created: 2026-09-13
tags: [sle, dtu, at-commands, modbus, latency, fragmentation, ros2, robot, tier, harvest]
sources:
  - "/mnt/hdd/nearlink-stuff/DS10-TTL"
trust: verified
stale_after: 2026-12-13
---

# WANG-XU-create/DS10-TTL — commercial NearLink SLE DTU with third-generation AT dialect, measured latency/reliability characterization, and ROS2 driver

- Inspection date: 2026-09-13 (staleness check: pushed 2026-09-07, not archived — ALIVE)
- Source root: `/mnt/hdd/nearlink-stuff/DS10-TTL`
- Mode: read-only local program/doc inspection; no network, build, hardware, or PCB access
- Scope: DS10-TTL SLE transparent serial bridge — AT dialect, measured transport limits, robotics integration

## Executive findings

1. DS10-TTL is a commercial NearLink **SLE transparent serial bridge** (TTL UART side): bytes in one end appear unmodified at the peer, verified bit-exact with random-filled Modbus 0x10 frames (station/function/CRC untouched) — the transport is fully byte-transparent, so any upper framing works. [`docs/DS10-TTL-tech-report:21-64`]
2. **First public measured latency/reliability profile of an SLE serial link** (single-clock Jetson testbed): 32-byte payload ≈ 14 ms one-way; 1400-byte payload ≈ 419 ms with only 44% success — the nonlinearity comes from reliable broadcast's fragmentation + per-fragment ACK accumulation. Practical consequences: uplink frame ceiling ≈ 1100 B vs downlink ≈ 4095 B (link asymmetric), and the master must pace consecutive downlink frames ≥ 5 ms apart. [`docs/DS10-TTL-tech-report:30-32, 65-79`]
3. Specs: SLE with GFSK or Polar coding selectable, ~200 m line-of-sight, topology 1 master ≤ 15 slaves, SLE power levels (tested at 0x07, GFSK high-power tier, reliable broadcast mode). [`docs/DS10-TTL-tech-report:46-56`]
4. The AT dialect is a third distinct design vs our other captures (HHD-01 OH-dialect, YL63 vendor family): a **two-plane transactional AT** — runtime AT (transparent mode via AT+ENTM) vs config-state AT entered with `AT+CFG_NEW`, where edits land in a RAM draft committed atomically by `AT+CFG_SAVE` (save+reboot) or dropped by `AT+CFG_DISCARD`, with `CFG_STATE/CFG_SHOW` draft inspection. [`docs/DS10-AT-shiling-ji:5-20`]
5. SLE parameters are exposed as config values: `AT+CFG_SLE=<frameType>,<tier>` (frame type AND mesh-style tier as first-class config), preset vs expert SLE params, plus business modes `AT+CFG_WORK` covering broadcast/Modbus/rule-routing with per-slave channel config (`AT+CFG_CH` carrying SN/Modbus-ID/route) — industrial Modbus-over-SLE with station-addressed routing is a product feature, not a hack. [`docs/DS10-AT-shiling-ji:13-17`]
6. A ROS 2 driver package wraps the DTU for robotics: master/slave role param, `ds10_interfaces/Frame` messages framed into Modbus RTU over the serial link, station-addressed point-to-point routing, and diagnostics exposing link status, frame counters, and resync byte-drop counts as the link's noise floor. [`src/ds10_driver/README.md:1-22`]

## Boundaries and gaps

- Measurements are near-field desktop (<2 m), 115200 8N1; the latency curve at 200 m range is not characterized.
- The 15-slave cap and tier semantics come from the vendor docs; tier behavior (routing vs grouping) was not exercised in code on our side.
- The config mini-program manual (537 lines) covers a WeChat-mini-program configurator — app-side UX, not inspected in depth.

## Reusable for our stack

- The measured SLE serial transport envelope (small frames fast; large frames degrade through per-fragment ACKs; asymmetric limits; 5 ms pacing) is directly transferable to our SLE UART/transport design — it empirically justifies small-frame + credit-based pacing over bulk streaming, consistent with our split `write`/`write_all` and pending-frame gating work.
- The transactional draft/commit AT design (RAM draft → atomic save+reboot) is a robust configuration-plane pattern for our dongle's NV config handling.
- Modbus-over-SLE with station routing + tier config shows the industrial control vocabulary (frameType, tier, channel, station) that a commercial SLE ecosystem standardized on — useful dialect reference for our own command surface.

## Comparison anchors (vs existing reports)

- `HHD01-BOARD.md` / `WS63-AT-FRAMEWORK.md` / `NEW-YL63-FORK-VERDICT.md`: third AT dialect archived (OH-dialect vs vendor-family vs transactional-industrial); DS10's CFG draft model is the most engineered of the three.
- `SLE-UART-VARIANTS.md`: DS10 is the commercial endpoint of the transparent-serial-over-SLE family this report surveyed, with the only public measured numbers.
- `SLE-MEASURE-QOS.md`: adds empirical latency/success-rate data points to our QoS knowledge; the fragmentation-ACK latency blowup aligns with our credit-based send design.
