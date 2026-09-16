---
type: harvest
title: "HopeRun WS63 SLE demo curriculum: reusable announce, connect, SSAP, UART, and sensor patterns"
language: en
created: 2026-09-16
tags: [harvest, hope-run, ws63, ssap, uart, sensor]
sources:
  - "https://github.com/HopeRunORG/NearLink/blob/main/demo/23_sle_uart/README.md"
  - "https://github.com/HopeRunORG/NearLink/blob/main/demo/23_sle_uart/sle_uart_client.c"
  - "https://github.com/HopeRunORG/NearLink/blob/main/demo/23_sle_uart/sle_uart_server.c"
  - "https://github.com/HopeRunORG/NearLink/blob/main/demo/23_sle_uart/sle_uart_server_adv.c"
  - "https://github.com/HopeRunORG/NearLink/blob/main/demo/24_sle_humi/sle_uart_server.c"
trust: A
stale_after: 2027-03-16
---

# HopeRun WS63 SLE demo curriculum: reusable announce, connect, SSAP, UART, and sensor patterns

## Executive findings

1. The repository is a WS63E/WS63V100 curriculum rather than one monolithic product. Its SLE lessons are split into a transparent UART bridge (`demo/23_sle_uart`) and a one-second AHT20 temperature/humidity publisher (`demo/24_sle_humi`), with the same announce, connection, SSAP, and UART skeleton copied into later LED, gas, and OLED demos. [`demo/23_sle_uart/README.md:52-97`] [`demo/24_sle_humi/README.md:99-100`]
2. The client implements the complete discovery-to-service sequence: enable SLE, seek a name, stop seeking, connect, exchange MTU/version, pair, find the service/property, and write the selected value handle. [`demo/23_sle_uart/sle_uart_client.c:113-217`] [`demo/23_sle_uart/sle_uart_client.c:234-309`]
3. The server builds a 16-bit base UUID plus a two-byte suffix, registers a service/property/CCCD descriptor, starts the service, advertises as connectable and scannable, and notifies the connected peer. [`demo/23_sle_uart/sle_uart_server.c:127-165`] [`demo/23_sle_uart/sle_uart_server.c:231-341`] [`demo/23_sle_uart/sle_uart_server_adv.c:140-269`]
4. The sensor demo adds a useful application pattern: gate sampling on the connection-state flag, read AHT20 over I2C, format text, and notify once per loop while retaining a 100-tick delay. [`demo/24_sle_humi/sle_uart_server.c:89-139`] [`demo/24_sle_humi/sle_uart_server.c:360-399`] [`demo/24_sle_humi/sle_uart_server.c:476-495`]
5. This is strong WS63 OH-API reference code, but it is not a hardened transport: text is NUL-terminated in callbacks, length accounting adds one byte, buffers are global, and there is no binary framing, checksum, replay protection, or bounded reconnect backoff.

## 1. Client connection and SSAP sequence

`SleUartStartScan()` configures seek interval/window/PHY and starts discovery. The seek-result callback copies the peer address only after matching `sle_uart_server`, then stops seeking; the seek-disable callback initiates the connection. [`demo/23_sle_uart/sle_uart_client.c:113-170`]

On connection, the client immediately requests an MTU of 520 and version 1, pairs the peer, and starts service discovery after the exchange callback. [`demo/23_sle_uart/sle_uart_client.c:173-217`] The structure search covers handles `1..0xFFFF`, stores the returned UUID/handle range, and the property callback selects the value handle and `SSAP_PROPERTY_TYPE_VALUE`. [`demo/23_sle_uart/sle_uart_client.c:220-250`]

The outgoing path copies at most the caller length into a 520-byte global buffer but sets `data_len = len + 1`, then sends with `SsapWriteReq()`. [`demo/23_sle_uart/sle_uart_client.c:294-317`] UART RX uses a 256-byte buffer and a 100 ms delay before each write. [`demo/23_sle_uart/sle_uart_client.c:55-71`] [`demo/23_sle_uart/sle_uart_client.c:82-110`] [`demo/23_sle_uart/sle_uart_client.c:312-317`]

## 2. Server service construction and advertising

The server derives a 16-bit service/property UUID by replacing bytes 14 and 15 of a fixed base UUID. [`demo/23_sle_uart/sle_uart_server.c:127-165`] It registers:

- one service with `SsapsAddServiceSync()`;
- one writable/notifiable property with `SsapsAddPropertySync()`;
- one client-characteristic-configuration descriptor with `SsapsAddDescriptorSync()`.

[`demo/23_sle_uart/sle_uart_server.c:231-326`]

The server registers callbacks before enabling SLE, adds the service only from the SLE-enable callback, and then initializes advertising. [`demo/23_sle_uart/sle_uart_server.c:409-469`] Advertising is connectable and scannable, carries discovery-level/access-mode fields, and places the local name in scan response data. [`demo/23_sle_uart/sle_uart_server_adv.c:59-168`] [`demo/23_sle_uart/sle_uart_server_adv.c:171-210`]

Incoming client writes are treated as NUL-terminated text, while server-to-client data uses `SsapsNotifyIndicate()` with `valueLen = len + 1`. [`demo/23_sle_uart/sle_uart_server.c:329-342`] [`demo/23_sle_uart/sle_uart_server.c:390-407`]

## 3. Sensor publisher delta

The humidity demo keeps the same transport skeleton and adds an AHT20 I2C path at 400 kHz. [`demo/24_sle_humi/sle_uart_server.c:42-43`] [`demo/24_sle_humi/sle_uart_server.c:97-139`] A global `connect_success_flag` is set on connection and cleared on disconnect; the task samples only while connected and sends `temp:%.2f,humi:%.2f` as a notification. [`demo/24_sle_humi/sle_uart_server.c:89-95`] [`demo/24_sle_humi/sle_uart_server.c:360-399`] [`demo/24_sle_humi/sle_uart_server.c:476-495`]

This is a compact reference for a periodic SLE sensor service, but the loop is intentionally infinite (`while (c)`) and has no shutdown, error backoff, sample sequence number, or calibration metadata.

## Boundaries

- The curriculum targets the WS63V100/Oniro build layout and is not a desktop-buildable host stack. [`demo/23_sle_uart/README.md:52-93`]
- The server address is fixed in source; deploying multiple servers requires address allocation outside this sample. [`demo/23_sle_uart/sle_uart_server_adv.c:140-168`]
- The README claims two-board serial forwarding and one-second sensor publication, but this harvest performed source inspection only and did not build or run hardware.
- No SLE security policy, pairing secret handling, version negotiation beyond `version = 1`, or application protocol version is implemented.
- The copied demo family is useful as a curriculum map, not as proof that every later sensor demo has independent end-to-end validation.

## Reusable

1. Use the client sequence as a WS63 smoke-test checklist: enable -> seek/name filter -> stop seek -> connect -> MTU exchange -> pair -> FIND -> write/notify.
2. Reuse the base-UUID-plus-16-bit-suffix convention only as a local namespace pattern; assign stable service/property suffixes deliberately.
3. Keep connection state as an explicit gate around periodic sampling, as shown by `connect_success_flag`.
4. Preserve the separation between announce construction, connection callbacks, SSAP service construction, and UART I/O; it makes failures easier to isolate.
5. Use the 520-byte MTU and 256-byte UART buffer as a starting point, then replace global text buffers with bounded, length-aware frames.

## Comparison anchors

- `NEW-TEKI128-MINIMAL-PAIR.md` is a 564-line fixed-address pair; HopeRun adds OHOS-style callbacks, service construction, MTU exchange, and a sensor publisher.
- `NEW-OHOS-TETHERING-SERVICE.md` implements a production port/data-plane service; HopeRun is the educational subset that exposes the individual SSAP calls in a small firmware sample.
- `NEW-NLCHAT-WEB.md` is browser Web Serial only; HopeRun is the device-side SLE/SSAP half that a future browser or Linux serial UI would need to interoperate with.
- `NEW-SLE-UART-VARIANTS.md` compares small UART demos; HopeRun contributes the clearest WS63V100 announce/connect/SSAP callback sequence in the local corpus.

## Source references

- `https://github.com/HopeRunORG/NearLink/blob/main/demo/23_sle_uart/README.md`
- `https://github.com/HopeRunORG/NearLink/blob/main/demo/23_sle_uart/sle_uart_client.c`
- `https://github.com/HopeRunORG/NearLink/blob/main/demo/23_sle_uart/sle_uart_server.c`
- `https://github.com/HopeRunORG/NearLink/blob/main/demo/23_sle_uart/sle_uart_server_adv.c`
- `https://github.com/HopeRunORG/NearLink/blob/main/demo/24_sle_humi/README.md`
- `https://github.com/HopeRunORG/NearLink/blob/main/demo/24_sle_humi/sle_uart_server.c`
