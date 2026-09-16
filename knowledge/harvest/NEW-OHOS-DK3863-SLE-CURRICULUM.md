---
type: harvest
title: "openharmony/vendor_hihope nearlink_dk_3863 — official OHOS NearLink DK SLE curriculum (5 application samples over the UART skeleton)"
language: en
created: 2026-09-13
tags: [ohos, h3863, ws63, dk-3863, sle, samples, pairing, ohos-build, harvest]
sources:
  - "https://github.com/openharmony/vendor_hihope"
trust: A
stale_after: 2026-12-13
---

# openharmony/vendor_hihope nearlink_dk_3863 — official OHOS NearLink DK SLE curriculum (5 application samples over the UART skeleton)

- Inspection date: 2026-09-13 (staleness check: pushed 2026-09-12 — freshest upstream of this whole session)
- Source root: `https://github.com/openharmony/vendor_hihope` (full clone, 12MB)
- Scope: the NearLink DK-3863 (HiHope) 28-sample curriculum, focus on the five SLE applications (23-27)

## Executive findings

1. The OHOS vendor tree carries a complete NearLink DK-3863 teaching curriculum: `nearlink_dk_3863/ws63_sample/` with 28 numbered samples from OS primitives (00_thread…05_message) through GPIO/PWM/ADC/sensors (06-13), WiFi networking (14-18), BLE UART (19), MQTT (20-22), and a five-part **SLE application series** (23_sle_uart, 24_sle_humi, 25_sle_led, 26_sle_gas, 27_sle_oled). [`nearlink_dk_3863/ws63_sample/`]
2. The five SLE samples share one client/server skeleton (the sle_uart pair, ~1467 lines per sample) and differ only in application payload: 24 attaches an AHT20 I2C humidity sensor as report source, 25 drives traffic-light-board LEDs from received writes (actuator direction), 26 adds an ADC gas sensor + PWM patch, 27 renders to OLED. Sensor-report-up and actuator-command-down both ride the same single-property dialect. [`24-27_sle_*` file sets, `sle_uart_server.c`/`sle_uart_client.c`]
3. The connect callback explicitly tracks `SlePairStateType pair_state` alongside connection and disconnect-reason — pairing state is a first-class signal in the DK samples, corroborating the pairing prerequisite documented in `NEW-BEARPI-H3863-DOCS.md`. [`25_sle_led/sle_uart_server.c:343`]
4. Actuator control uses the plain write path: client fills `ssapc_write_param_t` (handle + `SSAP_PROPERTY_TYPE_VALUE`) and sends buffer payloads; the server's traffic-light LED board (the traffic-light board red LED) reacts via GPIO in the write callback. No dedicated opcodes — semantic framing is application-level bytes on the UART-dialect property. [`25_sle_led/sle_uart_client.c:52-92, 254-255`]
5. OHOS integration is real: every sample has a `BUILD.gn` wired into the OHOS build (plus `hal_iot_gpio_ex` shims), i.e., these SLE samples compile as OHOS components on the DK-3863 — the concrete device-side counterpart of the OHOS service layer we digested from `communication_nearlink_service`. [`BUILD.gn` per sample]

## Boundaries and gaps

- Samples are teaching-grade: no encryption beyond the pairing step, no error recovery beyond disconnect callbacks, no power management.
- The SLE series rides the single-property UART dialect only; no mesh, no multi-property designs at this layer.
- READMEs per sample are short usage notes; the deeper manual for this family is the BearPi docs repo digested in `NEW-BEARPI-H3863-DOCS.md`.

## Reusable for our stack

- The 5-sample payload taxonomy (UART passthrough / sensor report / actuator command / analog report / display stream) is a ready test matrix for our WS73 SSAP stack: each maps to a different server-side callback + payload semantics we can implement as acceptance tests.
- Pairing-state tracking in the connect callback is a state-machine input our SSAP link layer should surface alongside connection state.
- This is the reference for how HiSilicon expects SLE applications to be structured inside an OHOS build — useful if our dongle work ever ships an OHOS-friendly sample package.

## Comparison anchors (vs existing reports)

- `NEW-BEARPI-H3863-DOCS.md`: the manual for this exact curriculum; pairing prerequisite now confirmed in code (pair_state callback).
- `NEW-TEKI128-MINIMAL-PAIR.md` / `NEW-PET-COLLAR-GATEWAY.md`: same skeleton lineage; the DK series is its official OHOS-packaged form.
- `NEW-OHOS-DEVICE-SOC-WS63.md`: device_soc supplies the SDK headers; vendor_hihope supplies the application curriculum on top — together the full OHOS device-side NearLink story.
