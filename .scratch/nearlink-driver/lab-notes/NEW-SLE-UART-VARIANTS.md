# New SLE UART Variants: `sle_uart` vs `Hi3863-SLE-2025`

**Inspection date:** 2026-09-09  
**Mode:** Read-only local inspection; no network, build, or hardware access.  
**Allowed source roots:** `/mnt/hdd/nearlink-stuff/sle_uart` and `/mnt/hdd/nearlink-stuff/Hi3863-SLE-2025`.  
**Comparison documents:** `NearLinkSLE-SAMPLES.md`, `WS63-SLE-EXAMPLES.md`, and `SSAP-DIALECT-COMPARISON.md` in the workspace lab-notes directory.

## Source key

- `SU` means `/mnt/hdd/nearlink-stuff/sle_uart`.
- `H3` means `/mnt/hdd/nearlink-stuff/Hi3863-SLE-2025`.
- `NL` means `/home/archivalera/plum/zcode-projects/nearlink/.scratch/nearlink-driver/lab-notes/NearLinkSLE-SAMPLES.md`.
- `W63` means `/home/archivalera/plum/zcode-projects/nearlink/.scratch/nearlink-driver/lab-notes/WS63-SLE-EXAMPLES.md`.
- `SSAP` means `/home/archivalera/plum/zcode-projects/nearlink/.scratch/nearlink-driver/lab-notes/SSAP-DIALECT-COMPARISON.md`.
- Citations use the form `[alias/file:line-line]`; the aliases above expand to absolute paths.

## Executive findings

1. `SU` is a genuine UART-to-SLE bridge sample, while `H3` splits the functionality into an EPD-oriented SLE client and an ExBoard-oriented SLE server application. `[SU/CMakeLists.txt:4-17] [H3/EPD/CMakeLists.txt:1-15] [H3/ExBoard/CMakeLists.txt:1-23]`
2. Neither inspected implementation defines an explicit UART frame header, length field, sequence number, or checksum around the application bytes. `[SU/sle_uart.c:100-107] [SU/sle_uart.c:204-212] [H3/ExBoard/ExBoard_sample.c:53-76] [H3/EPD/EPD_Sample.c:29-45]`
3. `SU` passes raw UART bytes directly into SSAP writes or notifications; `H3` instead gives the bytes a small text protocol based on leading `E` and `A` characters. `[SU/sle_uart.c:89-97] [SU/sle_uart.c:184-202] [H3/ExBoard/ExBoard_sample.c:62-76] [H3/EPD/EPD_Sample.c:35-45]`
4. The two trees do not agree on the service UUID: `SU` uses `0x2222`, whereas the `H3` ExBoard server uses `0x2000`; both use property UUID `0x2323`. `[SU/sle_uart_server/sle_uart_server.h:22-27] [H3/ExBoard/sle_uart_server.h:31-36]`
5. Both trees request or advertise an SSAP MTU of 520, but their actual send paths use a 256-byte server stack buffer or fixed 30-byte application buffers rather than a negotiated-MTU-aware framer. `[SU/sle_uart_client/sle_uart_client.c:17-18] [SU/sle_uart_server/sle_uart_server.c:280-292] [H3/EPD/sle_uart_client.c:18-19] [H3/ExBoard/sle_uart_server.c:282-295] [H3/ExBoard/ExBoard_sample.c:105-113]`
6. Both server variants pre-initialize a descriptor with `{0x01, 0x00}` and both clients skip an explicit CCCD write, reproducing the shortcut documented for the production `SLE_UART_HE` sample. `[SU/sle_uart_server/sle_uart_server.c:175-214] [SU/sle_uart_client/sle_uart_client.c:178-187] [H3/ExBoard/sle_uart_server.c:178-218] [H3/EPD/sle_uart_client.c:183-192] [NL:195-256]`
7. `SU` has a single-connection state model; `H3` declares space for eight connections but uses shared discovery/write state and an off-by-one connection lookup in its pair-complete path. `[SU/sle_uart_client/sle_uart_client.c:31-38] [H3/EPD/sle_uart_client.c:31-40] [H3/EPD/sle_uart_client.c:117-146] [H3/EPD/sle_uart_client.c:223-239]`
8. The `H3` ExBoard write parser has a concrete memory-safety defect: it passes an array of uninitialized `uint8_t *` pointers to `sscanf` with `%s`. `[H3/ExBoard/ExBoard_sample.c:29-35] [H3/ExBoard/ExBoard_sample.c:62-70]`
9. The `H3` server copies the message-queue machinery from `SU` but never consumes the queue, so its disconnect control message cannot trigger re-advertising as it does in `SU`. `[SU/sle_uart.c:141-178] [H3/ExBoard/sle_uart_server.c:400-418] [H3/ExBoard/ExBoard_sample.c:128-229]`
10. The safest borrow is not the application text parser; it is the explicit framing, buffering, retry, CCCD, and per-connection state patterns called out in the existing comparison documents. `[NL:40-59] [NL:259-324] [NL:414-451] [W63:172-205] [SSAP:4-27]`

## 1. Repository and build-shape comparison

11. `SU` conditionally compiles either a server or a client from one root `sle_uart.c`, and the selected branch adds the matching server or client source file. `[SU/CMakeLists.txt:4-17]`
12. The `SU` server build includes the advertising source, server source, and common UART source; the client build includes the client source and common UART source. `[SU/CMakeLists.txt:4-17]`
13. `H3/EPD` compiles the EPD driver, EPD sample, GUI code, and its SLE client source, with no common UART bridge source. `[H3/EPD/CMakeLists.txt:1-15]`
14. `H3/ExBoard` compiles the ExBoard sample, PWM, SLE server, advertising, I2C, BH1750, AHT20, WS2812, and motor sources. `[H3/ExBoard/CMakeLists.txt:1-23]`
15. The `H3` README describes an EPD/SSD1683, AHT20, BH1750, SG90, WS2812, and one-to-many SLE server/client collection, which matches the split application layout rather than a reusable UART transport library. `[H3/README.md:1-7]`
16. `SU` exposes UART bus and TX/RX pin choices through Kconfig, with bus 0 and pins 17/18 as defaults. `[SU/Kconfig:5-23]`
17. The `SU` Kconfig choice makes server and client mutually exclusive and enables peripheral or central support from that choice. `[SU/Kconfig:25-43]`
18. `H3` has no corresponding UART Kconfig in the inspected tree; its two applications are selected by their own CMake conditions. `[H3/EPD/CMakeLists.txt:1-15] [H3/ExBoard/CMakeLists.txt:1-23]`
19. `SU` configures UART at 115200 baud, 8 data bits, one stop bit, no parity, and no CTS/RTS flow control. `[SU/sle_uart.c:30-38] [SU/sle_uart.c:51-68]`
20. `SU` allocates one 512-byte UART RX buffer at file scope and gives it to the UART driver. `[SU/sle_uart.c:31-38]`
21. `H3/EPD` initializes the SLE client from an EPD task and never initializes a UART peripheral in the inspected EPD sources. `[H3/EPD/EPD_Sample.c:165-171] [H3/EPD/CMakeLists.txt:1-15]`
22. `H3/ExBoard` initializes SLE, sensors, PWM, motor, I2C, SPI, and a watchdog, but its source list contains no UART driver source. `[H3/ExBoard/ExBoard_sample.c:137-160] [H3/ExBoard/CMakeLists.txt:1-23]`
23. `SU` uses a 0x1200 server task stack and a 0x600 client task stack in the common entry file. `[SU/sle_uart.c:21-25] [SU/sle_uart.c:71-75]`
24. `H3/EPD` uses a 0x2000 EPD task stack, while `H3/ExBoard` requests a much larger 0x20000 task stack for its combined application. `[H3/EPD/EPD_Sample.c:22-23] [H3/ExBoard/ExBoard_sample.c:20-21]`
25. This build-shape difference means `SU` is the closer starting point for a dongle UART transport, whereas `H3` is an application bundle with transport code embedded in product-specific tasks. `[SU/CMakeLists.txt:4-17] [H3/EPD/CMakeLists.txt:1-15] [H3/ExBoard/CMakeLists.txt:1-23]`

## 2. UART transport and framing

26. `SU` registers the server UART RX callback with `UART_RX_CONDITION_FULL_OR_IDLE` and a threshold of one byte. `[SU/sle_uart.c:159-166]`
27. `SU` registers the client UART RX callback with the same full-or-idle condition and threshold. `[SU/sle_uart.c:223-230]`
28. The server RX handler immediately calls `sle_uart_server_send_report_by_handle()` when the pair handle is nonzero. `[SU/sle_uart.c:100-107]`
29. The client RX handler immediately fills the global SSAP write parameter with the UART buffer pointer and byte count, then calls `ssapc_write_req()`. `[SU/sle_uart.c:204-212]`
30. Neither handler adds a magic word, payload length, sequence number, channel identifier, or checksum before invoking SSAP. `[SU/sle_uart.c:100-107] [SU/sle_uart.c:204-212]`
31. The existing `SLE_QT` reference uses a distinct `0x5A 0x5A` header and XOR checksum for multiplexed UART data, showing that the inspected trees omit a known sample framing pattern. `[NL:40-46]`
32. The existing `SLE_UART_HE` reference is described as a transparent UART bridge with 8 KiB ring buffers and 128-byte retry chunks, which is materially more robust than the direct callbacks in `SU`. `[NL:56-60] [NL:259-324]`
33. `SU` server notifications copy the incoming UART bytes into a 256-byte stack array named `receive_buf`. `[SU/sle_uart_server/sle_uart_server.c:279-292]`
34. The `SU` common UART RX buffer is 512 bytes, so a callback length above 256 can overwrite the server notification stack buffer because `memcpy_s` is given `len` as the destination size without a prior cap. `[SU/sle_uart.c:31-38] [SU/sle_uart_server/sle_uart_server.c:280-290]`
35. `SU` server send-by-UUID accepts a `uint8_t len`, inherently limiting that API to 255 bytes even though the negotiated MTU is 520. `[SU/sle_uart_server/sle_uart_server.c:248-276] [SU/sle_uart_client/sle_uart_client.c:17-18]`
36. `SU` client notification and indication callbacks ignore status and pass `data->data_len` directly to UART write. `[SU/sle_uart.c:184-202]`
37. `SU` client notification and indication callbacks print the payload with `%s`, which is not binary-safe for NUL-containing or unterminated UART data. `[SU/sle_uart.c:184-202]`
38. `SU` server write callback also prints the received value with `%s` before writing the declared length to UART. `[SU/sle_uart.c:89-97]`
39. `H3/ExBoard` has no UART RX callback; its SSAP write callback interprets the first byte as an application command. `[H3/ExBoard/ExBoard_sample.c:53-76]`
40. `H3/EPD` has no UART write path; its SSAP notification callback parses text and calls the SLE client send helper. `[H3/EPD/EPD_Sample.c:29-45] [H3/EPD/sle_uart_client.c:223-240]`
41. The `H3` ExBoard sensor path formats a 30-byte stack buffer and sends `sizeof(buffer)`, not the string length. `[H3/ExBoard/ExBoard_sample.c:105-113]`
42. The `H3` EPD response path formats a 30-byte array and sends `sizeof(transmit_message)`, also preserving trailing NUL bytes. `[H3/EPD/EPD_Sample.c:35-45]`
43. The `H3` EPD `A` branch copies `sizeof(data->data)` bytes rather than `data->data_len`, making the copy size depend on the SDK member declaration instead of the received payload length. `[H3/EPD/EPD_Sample.c:41-45]`
44. The `H3` ExBoard command parser uses `sscanf` with `%s` and stores into `message_from_ai`, which is declared as an array of 30 `uint8_t *` pointers rather than a character buffer. `[H3/ExBoard/ExBoard_sample.c:29-35] [H3/ExBoard/ExBoard_sample.c:62-70]`
45. The ExBoard parser does not check `write_cb_para->length` before reading `value[0]`, so a zero-length non-null value can be read out of bounds. `[H3/ExBoard/ExBoard_sample.c:53-69]`
46. The ExBoard parser sends `{0x01, 0x02, 0x03}` after either recognized command branch, even when parsing fails or the command is only logged. `[H3/ExBoard/ExBoard_sample.c:62-76]`
47. `SU` uses the UART buffer pointer directly as the SSAP write data pointer, so an asynchronous stack that retains the pointer can observe reused UART DMA/RX storage. `[SU/sle_uart.c:33-38] [SU/sle_uart.c:204-212]`
48. `H3` allocates a send buffer in `sle_uart_client_send_data()`, calls `ssapc_write_req()`, and immediately frees the buffer without checking the return status. `[H3/EPD/sle_uart_client.c:223-239]`
49. The immediate free is safe only if `ssapc_write_req()` synchronously copies the payload; the code provides no completion-gated lifetime or retry queue to establish that contract. `[H3/EPD/sle_uart_client.c:223-239]`
50. Neither tree implements fragmentation, reassembly, flow control, or backpressure at the UART/application boundary. `[SU/sle_uart.c:100-107] [SU/sle_uart.c:204-212] [H3/EPD/sle_uart_client.c:223-239] [H3/ExBoard/ExBoard_sample.c:53-76]`

## 3. SLE service UUIDs, advertising, and discovery

51. `SU` declares service UUID `0x2222` and notification-property UUID `0x2323`. `[SU/sle_uart_server/sle_uart_server.h:22-27]`
52. `H3/ExBoard` declares service UUID `0x2000` and the same notification-property UUID `0x2323`. `[H3/ExBoard/sle_uart_server.h:31-36]`
53. Both server implementations register the application with the two-byte app UUID `{0x12, 0x34}`. `[SU/sle_uart_server/sle_uart_server.c:217-228] [H3/ExBoard/sle_uart_server.c:220-231]`
54. Both encode 16-bit UUIDs into the standard 128-bit base and place the little-endian 16-bit value in bytes 14 and 15. `[SU/sle_uart_server/sle_uart_server.c:46-79] [H3/ExBoard/sle_uart_server.c:49-80]`
55. This UUID encoding matches the common wire dialect documented for 2-byte standard UUIDs and the base `37 BE A8 80 FC 70 11 EA B7 20 00 00 00 00 00 00 00 00`. `[SSAP:10-12] [W63:157-168]`
56. The existing community samples use service/property UUIDs `0x3333`/`0x3434`, so neither inspected service UUID is the community-sample default. `[NL:81-108]`
57. The existing WS63 samples use service/property UUIDs `0xABCD`/`0x1122`, so a peer hardcoded for WS63 will not match either inspected service UUID without discovery or configuration. `[W63:157-168] [W63:234-249]`
58. `SU` advertises the complete local name `sle_uart_server`; `H3/ExBoard` advertises `hisoc -ExBoard`. `[SU/sle_uart_server/sle_uart_server_adv.c:41-42] [H3/ExBoard/sle_uart_server_adv.c:40-42]`
59. `SU` uses advertising handle 1, while `H3/ExBoard` uses advertising handle 11. `[SU/sle_uart_server/sle_uart_server_adv.c:37-40] [H3/ExBoard/sle_uart_server_adv.c:36-39]`
60. `SU` uses local address `01:02:03:04:05:06`; `H3/ExBoard` uses `03:02:03:04:05:06`. `[SU/sle_uart_server/sle_uart_server_adv.c:128-148] [H3/ExBoard/sle_uart_server_adv.c:128-147]`
61. Both advertising implementations use connectable/scanable mode, the default channel map `0x07`, 25 ms advertising intervals, 12.5 ms connection intervals, 5 s supervision timeout, and 0x1F3 latency. `[SU/sle_uart_server/sle_uart_server_adv.c:128-145] [H3/ExBoard/sle_uart_server_adv.c:128-145]`
62. Both advertising files define `SLE_ADV_TX_POWER` as 10 dBm but assign `param.announce_tx_power = 18`, creating a configuration inconsistency. `[SU/sle_uart_server/sle_uart_server_adv.c:35-40] [SU/sle_uart_server/sle_uart_server_adv.c:128-145] [H3/ExBoard/sle_uart_server_adv.c:34-39] [H3/ExBoard/sle_uart_server_adv.c:128-145]`
63. Both advertising payloads contain discovery-level and access-mode TLVs, while the complete local name is placed in the scan response. `[SU/sle_uart_server/sle_uart_server_adv.c:70-125] [H3/ExBoard/sle_uart_server_adv.c:70-125]`
64. Neither advertising payload includes a service-UUID TLV, so name matching is the only discovery filter in the inspected clients. `[SU/sle_uart_server/sle_uart_server_adv.c:70-125] [H3/ExBoard/sle_uart_server_adv.c:70-125] [SU/sle_uart_client/sle_uart_client.c:77-85] [H3/EPD/sle_uart_client.c:75-89]`
65. `SU` client scans on PHY index 1 with active seek type value 1, interval 100, and window 100. `[SU/sle_uart_client/sle_uart_client.c:49-61]`
66. `H3/EPD` uses the same PHY, seek-type, interval, and window values. `[H3/EPD/sle_uart_client.c:47-59]`
67. The WS63 reference describes seek type 0 as passive and interval/window 100, so the inspected active-seek value should not be copied without checking the target SDK semantics. `[W63:209-230] [SU/sle_uart_client/sle_uart_client.c:49-61] [H3/EPD/sle_uart_client.c:47-59]`
68. `SU` stops scanning after a name substring match and then removes any paired remote device before connecting. `[SU/sle_uart_client/sle_uart_client.c:77-95]`
69. `H3/EPD` stops scanning after a name substring match but connects directly from the seek-disable callback without removing an existing pairing. `[H3/EPD/sle_uart_client.c:75-99]`
70. `H3/EPD` defaults to server name `hisoc`, which is intentionally a substring of the ExBoard name `hisoc -ExBoard`. `[H3/EPD/sle_uart_client.c:27-29] [H3/ExBoard/sle_uart_server_adv.c:40-42]`
71. `SU` defaults to the exact server name `sle_uart_server`, making its match less tolerant of renamed devices. `[SU/sle_uart_client/sle_uart_client.c:26-28] [SU/sle_uart_client/sle_uart_client.c:77-85]`
72. Neither client validates the discovered service UUID before storing the property handle; both ask for property discovery over the full handle range `1..0xFFFF`. `[SU/sle_uart_client/sle_uart_client.c:151-187] [H3/EPD/sle_uart_client.c:156-192]`
73. The existing WS63 client instead discovers the primary service and writes the service start handle, a different handle convention from the property-handle writes used here. `[W63:71-95] [W63:172-189]`

## 4. MTU and payload limits

74. `SU` client requests MTU 520 and version 1 after pairing completes. `[SU/sle_uart_client/sle_uart_client.c:132-142]`
75. `H3/EPD` client also requests MTU 520 and version 1 after pairing completes. `[H3/EPD/sle_uart_client.c:137-147]`
76. `SU` server sets MTU 520 and version 1 in its pair-complete callback. `[SU/sle_uart_server/sle_uart_server.c:313-324]`
77. `H3/ExBoard` server sets the same MTU and version in its pair-complete callback. `[H3/ExBoard/sle_uart_server.c:316-327]`
78. Both server variants log an MTU-change callback, but neither uses the returned MTU to choose a payload chunk size. `[SU/sle_uart_server/sle_uart_server.c:100-108] [H3/ExBoard/sle_uart_server.c:103-111]`
79. `SU` server notification-by-handle has a hard 256-byte stack destination, while the UART RX buffer can supply 512 bytes. `[SU/sle_uart_server/sle_uart_server.c:279-292] [SU/sle_uart.c:31-38]`
80. `H3/ExBoard` retains the same 256-byte notification-by-handle implementation. `[H3/ExBoard/sle_uart_server.c:282-295]`
81. `H3/EPD` send helper accepts an arbitrary `uint16_t len`, allocates that many bytes, and does not compare it with the negotiated MTU. `[H3/EPD/sle_uart_client.c:223-239]`
82. `SU` client reuses one global `ssapc_write_param_t`, so concurrent UART input and SSAP callbacks can overwrite the handle, type, pointer, or length. `[SU/sle_uart_client/sle_uart_client.c:31-47] [SU/sle_uart.c:204-212]`
83. `H3/EPD` also has one global write parameter even though it declares eight connection slots. `[H3/EPD/sle_uart_client.c:31-40] [H3/EPD/sle_uart_client.c:223-239]`
84. The existing `SLE_UART_HE` reference chunks at 128 bytes and retries each chunk for up to 100 iterations with 2 ms sleeps. `[NL:259-324]`
85. The existing WS63 throughput sample uses MTU 512 and 236-byte notification payloads, but it is a controlled throughput test rather than a general UART framer. `[W63:96-121] [W63:195-205]`
86. The existing dialect note says the wire dialect is shared, but WS73 has an older API subset and lacks newer descriptor-configuration operation bits. `[SSAP:4-17]`
87. A local-stack adapter should therefore treat 520 as a negotiation result, not as permission to pass arbitrary UART lengths into a 256-byte stack buffer. `[SU/sle_uart_server/sle_uart_server.c:280-290] [H3/ExBoard/sle_uart_server.c:283-293]`
88. A local-stack adapter should also preserve binary lengths explicitly instead of using C string functions or `sizeof` a fixed application buffer. `[SU/sle_uart.c:89-97] [H3/ExBoard/ExBoard_sample.c:105-113] [H3/EPD/EPD_Sample.c:35-45]`

## 5. Connection and pairing order

89. `SU` client initialization sleeps 5000 ms, registers seek/connect/SSAP callbacks, and enables SLE. `[SU/sle_uart_client/sle_uart_client.c:219-229]`
90. The `SU` SLE-enable callback calls `sle_uart_client_init()` again and then starts scanning. `[SU/sle_uart_client/sle_uart_client.c:63-68]`
91. That callback path can register the same global callback structures twice and sleep another 5000 ms before scanning, because the task already called `sle_uart_client_init()` before enabling SLE. `[SU/sle_uart.c:214-228] [SU/sle_uart_client/sle_uart_client.c:63-68] [SU/sle_uart_client/sle_uart_client.c:219-229]`
92. `H3/EPD` client initialization sleeps 1000 ms, although its comment says it is waiting five seconds. `[H3/EPD/sle_uart_client.c:243-253]`
93. The `H3/EPD` SLE-enable callback also calls `sle_uart_client_init()` again, creating the same duplicate-registration and repeated-delay hazard as `SU`. `[H3/EPD/sle_uart_client.c:61-66] [H3/EPD/sle_uart_client.c:243-253]`
94. In `SU`, a matching scan result copies the address and stops scanning; the seek-disable callback removes the paired remote and connects. `[SU/sle_uart_client/sle_uart_client.c:77-95]`
95. In `H3/EPD`, a matching scan result copies the address and stops scanning; the seek-disable callback connects without pair cleanup. `[H3/EPD/sle_uart_client.c:75-99]`
96. `SU` marks a connection, stores one global connection ID, and initiates pairing only when `pair_state == SLE_PAIR_NONE`. `[SU/sle_uart_client/sle_uart_client.c:107-120]`
97. `H3/EPD` stores the connection ID in an array at index `g_sle_uart_conn_num`, immediately requests SSAP exchange with client ID 1, and increments the count. `[H3/EPD/sle_uart_client.c:117-125]`
98. `H3/EPD` then calls `sle_start_seek()` at the end of every connection-state callback, including the connected branch. `[H3/EPD/sle_uart_client.c:117-135]`
99. Restarting scanning while a connection is active is a state-machine hazard for a multi-connection central and differs from `SU`, which scans again only after disconnect. `[SU/sle_uart_client/sle_uart_client.c:123-127] [H3/EPD/sle_uart_client.c:117-135]`
100. In `SU`, pair completion requests SSAP exchange with client ID 0 and the global connection ID. `[SU/sle_uart_client/sle_uart_client.c:132-142]`
101. In `H3/EPD`, pair completion requests exchange with client ID 0 but indexes `g_sle_uart_conn_id[g_sle_uart_conn_num]` after the connected callback incremented the count. `[H3/EPD/sle_uart_client.c:117-146]`
102. With one connection, the `H3/EPD` pair-complete path therefore reads slot 1 even though the connection was stored in slot 0; with eight connections it can also run past the array. `[H3/EPD/sle_uart_client.c:31-40] [H3/EPD/sle_uart_client.c:117-146]`
103. `SU` exchange completion starts `SSAP_FIND_TYPE_PROPERTY` discovery from handle 1 through `0xFFFF`. `[SU/sle_uart_client/sle_uart_client.c:151-163]`
104. `H3/EPD` exchange completion starts the same property-discovery request. `[H3/EPD/sle_uart_client.c:156-168]`
105. Both property-found callbacks store the discovered property handle and `SSAP_PROPERTY_TYPE_VALUE`, but neither writes a CCCD descriptor. `[SU/sle_uart_client/sle_uart_client.c:178-187] [H3/EPD/sle_uart_client.c:183-192]`
106. `SU` disconnect handling removes the paired device and restarts scanning, but it does not clear the global connection ID. `[SU/sle_uart_client/sle_uart_client.c:123-127]`
107. Because the `SU` UART TX path uses the global connection ID without a connected-state check, stale-ID writes are possible after disconnect. `[SU/sle_uart.c:204-212] [SU/sle_uart_client/sle_uart_client.c:123-127]`
108. `H3/EPD` disconnect handling decrements the connection count and restarts scanning, but it does not clear the corresponding connection slot or remove a pairing. `[H3/EPD/sle_uart_client.c:127-135]`
109. Repeated or out-of-order disconnect callbacks can therefore underflow `g_sle_uart_conn_num` or leave stale IDs in the array. `[H3/EPD/sle_uart_client.c:31-40] [H3/EPD/sle_uart_client.c:127-135]`
110. `SU` server connection callback stores the connection ID on `CONNECTED`, clears it and the pair handle on `DISCONNECTED`, and enqueues the control string `sle_dis_connect`. `[SU/sle_uart_server/sle_uart_server.c:294-311]`
111. `H3/ExBoard` server has the same connection-state implementation, including the disconnect control enqueue. `[H3/ExBoard/sle_uart_server.c:297-314]`
112. `SU` server pair completion sets the pair handle to `conn_id + 1` and calls `ssaps_set_info()` with MTU 520/version 1. `[SU/sle_uart_server/sle_uart_server.c:313-324]`
113. `H3/ExBoard` server does the same. `[H3/ExBoard/sle_uart_server.c:316-327]`
114. Both server MTU-change callbacks also set the pair handle when it is zero, so the UART TX gate can become true before pair completion. `[SU/sle_uart_server/sle_uart_server.c:100-108] [H3/ExBoard/sle_uart_server.c:103-111]`
115. Both servers expose `sle_uart_client_is_connected()` as a test of the pair handle rather than the actual connection-state enum. `[SU/sle_uart_server/sle_uart_server.c:341-344] [H3/ExBoard/sle_uart_server.c:344-347]`
116. The existing community samples distinguish connection state, pairing, exchange, discovery, and CCCD subscription as separate stages; the inspected trees collapse or skip several of those stages. `[NL:149-190] [NL:195-256]`

## 6. SSAP operation coverage

117. `SU` server registers add-service, add-property, add-descriptor, start-service, delete-service, MTU, read, and write callbacks. `[SU/sle_uart_server/sle_uart_server.c:140-160]`
118. `H3/ExBoard` server registers the same callback set. `[H3/ExBoard/sle_uart_server.c:143-163]`
119. `SU` client registers exchange, find-service, find-property, find-complete, write-confirmation, notification, and indication callbacks. `[SU/sle_uart_client/sle_uart_client.c:205-216]`
120. `H3/EPD` client registers the same callback set. `[H3/EPD/sle_uart_client.c:210-221]`
121. Neither inspected client registers or calls a read-request operation. `[SU/sle_uart_client/sle_uart_client.c:205-216] [H3/EPD/sle_uart_client.c:210-221]`
122. Neither inspected client writes the descriptor/CCCD after property discovery. `[SU/sle_uart_client/sle_uart_client.c:178-187] [H3/EPD/sle_uart_client.c:183-192]`
123. Neither inspected client calls `READ_BY_UUID`; this is consistent with the documented WS73 API subset, which lacks a read-by-UUID server handler. `[SSAP:14-17] [SU/sle_uart_client/sle_uart_client.c:205-216] [H3/EPD/sle_uart_client.c:210-221]`
124. Both server property declarations combine read/write permissions with operation indications containing read and notify, but omit the write operation bit. `[SU/sle_uart_server/sle_uart_server.c:182-185] [H3/ExBoard/sle_uart_server.c:185-188]`
125. The headers define a write-capable operation macro, but neither server uses that macro in the property declaration. `[SU/sle_uart_server/sle_uart_server.h:28-33] [H3/ExBoard/sle_uart_server.h:37-42]`
126. This is inconsistent with the clients, which set `SSAP_PROPERTY_TYPE_VALUE` and issue write requests to the discovered property. `[SU/sle_uart_client/sle_uart_client.c:178-187] [H3/EPD/sle_uart_client.c:183-192]`
127. The existing dialect note defines write operation bit `0x04`, notify `0x08`, and indicate `0x10`, so omitting `0x04` changes the advertised operation semantics even though the permission bits include write. `[SSAP:8-9]`
128. Both servers add a descriptor with read/write permissions, `SSAP_DESCRIPTOR_USER_DESCRIPTION`, and value `{0x01, 0x00}`. `[SU/sle_uart_server/sle_uart_server.c:200-207] [H3/ExBoard/sle_uart_server.c:203-210]`
129. The existing community analysis identifies this descriptor as the notification gate and recommends an explicit client write of little-endian `0x0001`. `[NL:195-227]`
130. The existing community analysis also records that `SLE_UART_HE` deliberately skips the CCCD write and relies on the pre-enabled server descriptor. `[NL:242-256]`
131. Both inspected servers call the combined `ssaps_notify_indicate()` API for handle-based egress. `[SU/sle_uart_server/sle_uart_server.c:279-292] [H3/ExBoard/sle_uart_server.c:282-295]`
132. Both also provide a UUID-based notify/indicate helper, although the inspected application paths use the handle-based helper. `[SU/sle_uart_server/sle_uart_server.c:248-276] [H3/ExBoard/sle_uart_server.c:251-280]`
133. Neither server callback explicitly calls a SSAP response-sending API after read or write; the server callbacks only log and perform application-side UART or notification work. `[SU/sle_uart.c:83-98] [H3/ExBoard/ExBoard_sample.c:44-77]`
134. The WS63 comparison calls this an open response-semantics question because its UUID server similarly registers callbacks without explicitly sending responses. `[W63:65-70] [W63:292-301]`
135. The local-stack adapter must therefore verify whether its SSAP layer auto-responds or requires an explicit response before treating these callbacks as interoperable. `[W63:292-301] [SSAP:21-27]`
136. Both inspected clients discover properties rather than the primary service, unlike the WS63 UUID client sequence documented in `W63`. `[SU/sle_uart_client/sle_uart_client.c:151-187] [H3/EPD/sle_uart_client.c:156-192] [W63:71-95]`
137. Both inspected clients ignore the service UUID returned by find callbacks and do not validate the property operation bits before using the handle. `[SU/sle_uart_client/sle_uart_client.c:165-187] [H3/EPD/sle_uart_client.c:170-192]`
138. The shared SSAP dialect means opcode and PDU layout work can be reused, but the application-level handle and descriptor choices still require explicit compatibility tests. `[SSAP:4-13] [NL:414-451]`

## 7. Callbacks and state machines

139. `SU` server initialization enables SLE, registers advertising callbacks, connection callbacks, SSAP callbacks, adds the service, and starts advertising. `[SU/sle_uart_server/sle_uart_server.c:346-386]`
140. `H3/ExBoard` server initialization follows the same sequence but also creates and registers its message queue. `[H3/ExBoard/sle_uart_server.c:349-392]`
141. `SU` server task consumes queue messages, recognizes the disconnect control string, and calls `sle_start_announce()` when it appears. `[SU/sle_uart.c:141-178]`
142. `H3/ExBoard` defines queue write/delete/create functions but its application task never calls a queue receive function. `[H3/ExBoard/sle_uart_server.c:400-418] [H3/ExBoard/ExBoard_sample.c:128-229]`
143. Consequently, the `H3` disconnect callback enqueues `sle_dis_connect`, but no local task converts that message into re-advertising. `[H3/ExBoard/sle_uart_server.c:306-313] [H3/ExBoard/sle_uart_server.c:414-418]`
144. `SU` server task sleeps 2000 ms after each queue read, so re-advertising is not immediate even when the control message is consumed. `[SU/sle_uart.c:167-178]`
145. `SU` has no explicit announce-state lock or watchdog around re-advertising; the existing community comparison identifies those as enhancements in `SLE_QT`. `[NL:374-410]`
146. `SU` client task returns immediately after UART initialization and callback registration, leaving all later behavior to SDK callbacks. `[SU/sle_uart.c:214-235]`
147. `H3/EPD` runs SLE client initialization and EPD rendering in the same long-lived task loop. `[H3/EPD/EPD_Sample.c:165-216]`
148. `H3/ExBoard` runs a combined task that initializes the watchdog and peripherals, starts sensor/servo threads, and then remains in an LED loop. `[H3/ExBoard/ExBoard_sample.c:128-229]`
149. `H3/ExBoard` sensor work runs in a separate thread once per second and sends a notification when the pair handle is nonzero. `[H3/ExBoard/ExBoard_sample.c:94-124]`
150. `H3/ExBoard` servo work runs in three threads and uses shared angle globals written by the SSAP callback. `[H3/ExBoard/ExBoard_sample.c:29-35] [H3/ExBoard/ExBoard_sample.c:79-92] [H3/ExBoard/ExBoard_sample.c:161-192]`
151. Shared globals make the `H3` server application sensitive to callback/thread races even when the SSAP transport itself is healthy. `[H3/ExBoard/ExBoard_sample.c:29-35] [H3/ExBoard/ExBoard_sample.c:53-76] [H3/ExBoard/ExBoard_sample.c:94-124]`
152. `SU` server read callback logs the request but does not inspect status before logging the handle. `[SU/sle_uart.c:83-88]`
153. `SU` server write callback checks only that length is positive and value is non-null; it does not check the callback status. `[SU/sle_uart.c:89-97]`
154. `H3/ExBoard` read callback logs the request and status but performs no read response. `[H3/ExBoard/ExBoard_sample.c:44-50]`
155. `H3/ExBoard` write callback checks the pointer but not status or length before parsing. `[H3/ExBoard/ExBoard_sample.c:53-69]`
156. `SU` notification callbacks ignore the SSAP status argument. `[SU/sle_uart.c:184-202]`
157. `H3/EPD` notification callback ignores status and data length before indexing `data->data[0]`. `[H3/EPD/EPD_Sample.c:29-45]`
158. `H3/EPD` indication callback only prints the payload as a C string and does not forward it to the EPD or another transport. `[H3/EPD/EPD_Sample.c:48-55]`
159. `SU` client connection callback stores the connection ID but has no separate connected boolean or lock around UART TX. `[SU/sle_uart_client/sle_uart_client.c:107-130] [SU/sle_uart.c:204-212]`
160. `H3/EPD` client connection callback has an array count but no per-slot state object, pairing handle, MTU, or write-queue state. `[H3/EPD/sle_uart_client.c:31-40] [H3/EPD/sle_uart_client.c:110-146]`
161. The existing community comparison recommends connection-state gating and retry/backpressure because SSAP send calls can fail while the stack TX buffer is full. `[NL:321-324] [NL:427-449]`
162. Neither inspected tree implements that recommended gating around UART RX or application-generated sensor data. `[SU/sle_uart.c:100-107] [SU/sle_uart.c:204-212] [H3/ExBoard/ExBoard_sample.c:105-113]`

## 8. Sensor and data-frame semantics

163. `SU` has no sensor driver or application payload schema; it treats UART as an opaque byte stream. `[SU/CMakeLists.txt:4-17] [SU/sle_uart.c:89-97] [SU/sle_uart.c:184-202]`
164. `H3/ExBoard` reads AHT20 temperature/humidity and BH1750 light once per second. `[H3/ExBoard/ExBoard_sample.c:94-107]`
165. The AHT20 driver uses I2C address `0x38`, sends initialization command bytes `BE 08 00`, and waits 10 ms. `[H3/ExBoard/aht20.c:13-29]`
166. The AHT20 read path sends measurement command `AC 33 00`, waits 80 ms, reads six bytes, checks the busy bit, and applies 20-bit humidity and temperature formulas. `[H3/ExBoard/aht20.c:31-95]`
167. The ExBoard application ignores the AHT20 return code and publishes the global temperature/humidity values even after a failed read. `[H3/ExBoard/ExBoard_sample.c:99-113] [H3/ExBoard/aht20.c:31-95]`
168. The BH1750 driver uses I2C address `0x23`, issues a one-time high-resolution mode command, reads two bytes, and computes lux as `(raw * 10) / 12`. `[H3/ExBoard/bh1750.c:13-60] [H3/ExBoard/bh1750.h:6-18]`
169. The ExBoard application does not attach a sensor timestamp, sequence number, validity flag, unit label, or checksum to the formatted sensor frame. `[H3/ExBoard/ExBoard_sample.c:105-113]`
170. The ExBoard sensor frame is text: `E t:%.2f h:%.2f l:%d`, followed by NUL padding because `sizeof(buffer)` is sent. `[H3/ExBoard/ExBoard_sample.c:105-113]`
171. The EPD client recognizes a leading `E`, parses temperature, humidity, and light with `sscanf`, and displays those globals. `[H3/EPD/EPD_Sample.c:35-39] [H3/EPD/EPD_Sample.c:147-155]`
172. The EPD client responds to an `E` frame with the text `E %.2f`, using only temperature and discarding humidity and light. `[H3/EPD/EPD_Sample.c:35-40]`
173. The EPD client recognizes a leading `A`, copies up to its compile-time member size into a 30-byte array, and sends that array to connection index 1. `[H3/EPD/EPD_Sample.c:41-45]`
174. The ExBoard server recognizes a leading `E` only for logging; it does not update sensor state from that command. `[H3/ExBoard/ExBoard_sample.c:62-65]`
175. The ExBoard server recognizes a leading `A` as a servo/weather/message command and then sends a fixed three-byte response. `[H3/ExBoard/ExBoard_sample.c:66-76]`
176. The `A` format string repeats `e:%d` four times while the destination is `sg90_angles`, so the command labels and intended angle fields are ambiguous. `[H3/ExBoard/ExBoard_sample.c:66-69]`
177. The `message_from_ai` destination is especially unsafe because `%s` writes a string through an uninitialized pointer element rather than into a fixed character array. `[H3/ExBoard/ExBoard_sample.c:32-35] [H3/ExBoard/ExBoard_sample.c:68-70]`
178. The ExBoard application starts the motor forward when `sg90_angles[0]` is nonzero and stops it otherwise. `[H3/ExBoard/ExBoard_sample.c:115-121]`
179. The ExBoard application also drives WS2812 LEDs in the main loop and kicks a watchdog from the sensor thread. `[H3/ExBoard/ExBoard_sample.c:121-124] [H3/ExBoard/ExBoard_sample.c:212-226]`
180. The existing `SLE_QT_AD` sample uses a compact three-byte ADC frame `[status, hi, lo]`, which is easier to validate than the variable text frames in `H3`. `[NL:48-54]`
181. The existing `SLE_QT` sample uses a checksum-protected frame for multiple slaves, while `H3` uses unframed text commands and fixed-size padded responses. `[NL:40-46] [H3/ExBoard/ExBoard_sample.c:53-76] [H3/EPD/EPD_Sample.c:35-45]`
182. A portable sensor schema should carry an explicit type, length, sequence, units/scale, validity, and CRC instead of relying on `sscanf` and leading characters. `[H3/ExBoard/ExBoard_sample.c:105-113] [H3/EPD/EPD_Sample.c:35-45] [NL:40-54]`

## 9. Exact implementation differences

183. `SU` has one common UART driver used by both roles; `H3` has no common UART driver and embeds role-specific behavior in EPD and ExBoard applications. `[SU/sle_uart.c:1-256] [H3/EPD/CMakeLists.txt:1-15] [H3/ExBoard/CMakeLists.txt:1-23]`
184. `SU` service UUID is `0x2222`; `H3/ExBoard` service UUID is `0x2000`. `[SU/sle_uart_server/sle_uart_server.h:22-27] [H3/ExBoard/sle_uart_server.h:31-36]`
185. Both inspected servers use property UUID `0x2323`, but that does not make the services interchangeable because the service handle range and discovery expectations differ. `[SU/sle_uart_server/sle_uart_server.h:22-27] [H3/ExBoard/sle_uart_server.h:31-36]`
186. `SU` advertising handle is 1; `H3/ExBoard` advertising handle is 11. `[SU/sle_uart_server/sle_uart_server_adv.c:37-40] [H3/ExBoard/sle_uart_server_adv.c:36-39]`
187. `SU` local address is `01:02:03:04:05:06`; `H3/ExBoard` local address is `03:02:03:04:05:06`. `[SU/sle_uart_server/sle_uart_server_adv.c:128-148] [H3/ExBoard/sle_uart_server_adv.c:128-147]`
188. `SU` advertises `sle_uart_server`; `H3/ExBoard` advertises `hisoc -ExBoard`. `[SU/sle_uart_server/sle_uart_server_adv.c:41-42] [H3/ExBoard/sle_uart_server_adv.c:40-42]`
189. `SU` client defaults to `sle_uart_server`; `H3/EPD` client defaults to `hisoc`. `[SU/sle_uart_client/sle_uart_client.c:26-28] [H3/EPD/sle_uart_client.c:27-29]`
190. `SU` client has one scalar connection ID; `H3/EPD` declares an eight-entry connection-ID array and a count. `[SU/sle_uart_client/sle_uart_client.c:31-38] [H3/EPD/sle_uart_client.c:31-40]`
191. `SU` removes a paired device before connecting; `H3/EPD` does not. `[SU/sle_uart_client/sle_uart_client.c:88-95] [H3/EPD/sle_uart_client.c:92-99]`
192. `SU` restarts scanning only on disconnect; `H3/EPD` calls `sle_start_seek()` at the end of every connection-state callback. `[SU/sle_uart_client/sle_uart_client.c:123-127] [H3/EPD/sle_uart_client.c:117-135]`
193. `SU` pair completion uses client ID 0 and the scalar connection ID; `H3/EPD` pair completion uses client ID 0 and an incremented array index. `[SU/sle_uart_client/sle_uart_client.c:132-142] [H3/EPD/sle_uart_client.c:137-147]`
194. `SU` server consumes its disconnect queue in a dedicated task; `H3/ExBoard` never consumes its copied queue. `[SU/sle_uart.c:141-178] [H3/ExBoard/sle_uart_server.c:400-418]`
195. `SU` server sends raw UART bytes to the peer; `H3/ExBoard` sends sensor text and responds to parsed commands. `[SU/sle_uart.c:100-107] [H3/ExBoard/ExBoard_sample.c:94-124]`
196. `SU` client writes raw UART bytes to SSAP; `H3/EPD` client parses `E`/`A` payloads and generates application responses. `[SU/sle_uart.c:204-212] [H3/EPD/EPD_Sample.c:29-45]`
197. `SU` server send-by-handle uses a 256-byte stack buffer; `H3/ExBoard` retains the same implementation. `[SU/sle_uart_server/sle_uart_server.c:279-292] [H3/ExBoard/sle_uart_server.c:282-295]`
198. `SU` has no sensor/actuator code; `H3/ExBoard` includes AHT20, BH1750, motor, SG90, PWM, WS2812, I2C, SPI, and watchdog code. `[SU/CMakeLists.txt:4-17] [H3/ExBoard/CMakeLists.txt:1-23]`
199. `H3/EPD` includes a large EPD image/font stack and display loop; `SU` has no display code. `[H3/EPD/CMakeLists.txt:1-15] [H3/EPD/EPD_Sample.c:165-216] [SU/CMakeLists.txt:4-17]`
200. `SU` and `H3` both use MTU/version 520/1, but `H3` adds a multi-connection attempt without adding per-connection write state. `[SU/sle_uart_client/sle_uart_client.c:17-18] [H3/EPD/sle_uart_client.c:18-19] [H3/EPD/sle_uart_client.c:31-40]`
201. `SU` and `H3` both pre-enable the descriptor and skip the client CCCD write. `[SU/sle_uart_server/sle_uart_server.c:175-214] [H3/ExBoard/sle_uart_server.c:178-218] [SU/sle_uart_client/sle_uart_client.c:178-187] [H3/EPD/sle_uart_client.c:183-192]`
202. `SU` and `H3` both omit the property write operation bit despite accepting client writes. `[SU/sle_uart_server/sle_uart_server.c:182-185] [H3/ExBoard/sle_uart_server.c:185-188]`
203. `SU` and `H3` both use standard 16-bit UUID encoding over the same base, so their UUID encoding is portable even though their selected UUID values differ. `[SU/sle_uart_server/sle_uart_server.c:46-79] [H3/ExBoard/sle_uart_server.c:49-80] [SSAP:10-12]`
204. `SU` is closer to a reusable transport skeleton; `H3` is closer to a product demo that should be mined for application semantics, not copied as a transport layer. `[SU/CMakeLists.txt:4-17] [H3/EPD/CMakeLists.txt:1-15] [H3/ExBoard/CMakeLists.txt:1-23]`

## 10. Wire bugs and portability hazards

205. **Critical, `SU` server:** a UART RX length above 256 can exceed the 256-byte notification stack buffer because the length is not capped before `memcpy_s`. `[SU/sle_uart.c:31-38] [SU/sle_uart_server/sle_uart_server.c:280-290]`
206. **Critical, `H3` ExBoard:** `%s` writes through uninitialized `uint8_t *` elements in `message_from_ai`, which can corrupt arbitrary memory. `[H3/ExBoard/ExBoard_sample.c:32-35] [H3/ExBoard/ExBoard_sample.c:68-70]`
207. **Critical, both servers:** on descriptor-add failure, the code calls `osal_vfree()` on `descriptor.value`, but that pointer is the stack array `ntf_value`, not an allocation. `[SU/sle_uart_server/sle_uart_server.c:180-211] [H3/ExBoard/sle_uart_server.c:183-214]`
208. **High, both servers:** property operation indications omit WRITE (`0x04`) while the clients issue property writes, creating a possible advertisement/dispatch mismatch. `[SU/sle_uart_server/sle_uart_server.c:182-185] [H3/ExBoard/sle_uart_server.c:185-188] [SSAP:8-9]`
209. **High, both servers:** notification gating relies on a pre-enabled descriptor and a pair-handle boolean rather than an explicit, verified CCCD subscription state. `[SU/sle_uart_server/sle_uart_server.c:175-214] [H3/ExBoard/sle_uart_server.c:178-218] [NL:195-256]`
210. **High, `SU` client:** the SLE-enable callback calls `sle_uart_client_init()` a second time after the task already initialized the client, causing duplicate callback registration and another delay. `[SU/sle_uart.c:214-228] [SU/sle_uart_client/sle_uart_client.c:63-68] [SU/sle_uart_client/sle_uart_client.c:219-229]`
211. **High, `H3/EPD` client:** the same duplicate initialization pattern exists in the EPD client. `[H3/EPD/sle_uart_client.c:61-66] [H3/EPD/sle_uart_client.c:243-253]`
212. **High, `H3/EPD` client:** pair completion indexes the connection array after incrementing the count, producing an off-by-one lookup and a wrong/stale connection ID. `[H3/EPD/sle_uart_client.c:117-146]`
213. **High, `H3/EPD` client:** scanning is restarted from the connected branch, so the central can scan while it is already connected. `[H3/EPD/sle_uart_client.c:117-135]`
214. **High, both clients:** UART/SSAP payloads are not copied into a per-operation owned buffer before an asynchronous request, and no retry queue preserves payload lifetime. `[SU/sle_uart.c:204-212] [H3/EPD/sle_uart_client.c:223-239]`
215. **High, `H3/EPD`:** `sle_uart_client_send_data()` treats its `conn_id` parameter as an array index, offers no bounds check, and always calls SSAP with client ID 0. `[H3/EPD/sle_uart_client.c:223-239]`
216. **High, `H3/EPD`:** one global find-service result and one global write parameter are shared across all declared connections. `[H3/EPD/sle_uart_client.c:31-40] [H3/EPD/sle_uart_client.c:170-192] [H3/EPD/sle_uart_client.c:223-239]`
217. **Medium, `SU` client:** disconnect does not clear the global connection ID, while UART RX can still issue a write using that stale ID. `[SU/sle_uart_client/sle_uart_client.c:123-127] [SU/sle_uart.c:204-212]`
218. **Medium, `H3/EPD` client:** disconnect decrements the count without clearing the slot or removing pairing state, enabling stale IDs and count underflow. `[H3/EPD/sle_uart_client.c:127-135]`
219. **Medium, `H3/ExBoard`:** the server queue is created and written on disconnect but never read, so the copied re-advertise mechanism is dead code. `[H3/ExBoard/sle_uart_server.c:306-313] [H3/ExBoard/sle_uart_server.c:400-418]`
220. **Medium, both servers:** binary payloads are printed with `%s`, which can overread at a NUL terminator and corrupt logs. `[SU/sle_uart.c:89-97] [SU/sle_uart.c:184-202] [H3/ExBoard/ExBoard_sample.c:64-70]`
221. **Medium, `H3/EPD`:** notification parsing indexes `data->data[0]` without checking status, pointer validity, or `data_len`. `[H3/EPD/EPD_Sample.c:29-45]`
222. **Medium, `H3/ExBoard`:** the write parser reads `value[0]` without checking `length`, and it ignores callback status. `[H3/ExBoard/ExBoard_sample.c:53-69]`
223. **Medium, both application paths:** fixed 30-byte sends include NUL padding and do not communicate the actual semantic payload length. `[H3/ExBoard/ExBoard_sample.c:105-113] [H3/EPD/EPD_Sample.c:35-45]`
224. **Medium, both servers:** the advertising power macro says 10 dBm while the actual parameter is 18, so configuration and wire behavior can diverge. `[SU/sle_uart_server/sle_uart_server_adv.c:35-40] [SU/sle_uart_server/sle_uart_server_adv.c:128-145] [H3/ExBoard/sle_uart_server_adv.c:34-39] [H3/ExBoard/sle_uart_server_adv.c:128-145]`
225. **Medium, both clients:** discovery matches a name substring but never validates the service UUID, so a similarly named device can be selected. `[SU/sle_uart_client/sle_uart_client.c:77-85] [H3/EPD/sle_uart_client.c:75-89]`
226. **Medium, both servers:** `ssaps_register_server()` return status is ignored before service addition proceeds. `[SU/sle_uart_server/sle_uart_server.c:217-239] [H3/ExBoard/sle_uart_server.c:220-242]`
227. **Medium, both servers:** advertising parameter/data setup return values are ignored before `sle_start_announce()` is called. `[SU/sle_uart_server/sle_uart_server_adv.c:235-245] [H3/ExBoard/sle_uart_server_adv.c:234-244]`
228. **Medium, `H3` sensor path:** AHT20 read failures are ignored by the publisher, so stale or uninitialized globals can be transmitted. `[H3/ExBoard/ExBoard_sample.c:99-113] [H3/ExBoard/aht20.c:31-95]`
229. **Medium, local-stack port:** WS73 lacks the read-by-UUID handler and newer descriptor-configuration operation bits documented for other dialects, so blindly copying OHOS/WS63 operations is unsafe. `[SSAP:14-17] [SSAP:21-27]`
230. **Medium, local-stack port:** the existing local-stack analysis reports missing descriptor support, descriptor discovery, and retry/backpressure, all of which are required to make the pre-enabled shortcut reliable. `[NL:414-451]`
231. **Low, `SU`:** the UART pinmux branches for bus 0 and bus 1 perform the same pin-mode operation and rely entirely on Kconfig pin values, so a wrong board pin configuration is not detected in code. `[SU/sle_uart.c:40-48] [SU/Kconfig:13-23]`
232. **Low, `H3/EPD`:** the client header declares `get_g_sle_uart_conn_id()`, but the inspected client source does not define it, creating a linker hazard for any caller that uses the helper. `[H3/EPD/sle_uart_client.h:15-21] [H3/EPD/sle_uart_client.c:1-253]`
233. **Low, both trees:** hardcoded addresses, names, UUIDs, and advertising handles make the samples non-portable across boards or coexistence scenarios without configuration changes. `[SU/sle_uart_server/sle_uart_server_adv.c:128-148] [H3/ExBoard/sle_uart_server_adv.c:128-147] [SU/sle_uart_server/sle_uart_server.h:22-27] [H3/ExBoard/sle_uart_server.h:31-36]`

## 11. Concrete borrow/adapt checklist

234. Borrow the `SLE_QT` idea of an explicit frame header, payload length, logical endpoint/channel, sequence number, and checksum before placing UART bytes into SSAP. `[NL:40-46]`
235. Borrow the `SLE_UART_HE` 8 KiB ring-buffer and 128-byte chunk pattern instead of passing UART ISR storage directly to SSAP. `[NL:56-60] [NL:259-324]`
236. Borrow the 2 ms, 100-retry backpressure loop, but retain the unsent bytes in an owned queue rather than retrying a pointer to reused storage. `[NL:259-324] [NL:321-324]`
237. Add a real CCCD/descriptor model and make the client write little-endian `0x0001` after property discovery; do not depend only on a preinitialized descriptor. `[NL:195-227] [NL:414-451]`
238. If server-side pre-enabling is retained for compatibility, gate notifications with an explicit connection-plus-subscription state and test peers that clear the descriptor. `[NL:229-256]`
239. Add the WRITE operation bit to the property declaration when the application accepts property writes, and verify the resulting operation-indication byte on the wire. `[SU/sle_uart_server/sle_uart_server.c:182-185] [H3/ExBoard/sle_uart_server.c:185-188] [SSAP:8-9]`
240. Align service UUIDs through configuration or discovery validation; do not assume `0x2222`, `0x2000`, `0x3333`, and `0xABCD` are interchangeable. `[SU/sle_uart_server/sle_uart_server.h:22-27] [H3/ExBoard/sle_uart_server.h:31-36] [NL:81-108] [W63:157-168]`
241. Preserve the standard UUID base and little-endian bytes 14/15 when adapting 16-bit UUIDs, because that encoding is shared across the documented dialects. `[SSAP:10-12] [W63:157-168]`
242. Enforce `min(requested_MTU, transport_buffer_capacity, framed_payload_limit)` before every SSAP write or notification. `[SU/sle_uart_server/sle_uart_server.c:280-290] [H3/EPD/sle_uart_client.c:223-239] [W63:195-205]`
243. Replace the 256-byte server stack buffer with a DMA-safe heap or ring-buffer segment and reject or fragment oversized input explicitly. `[SU/sle_uart_server/sle_uart_server.c:279-292] [H3/ExBoard/sle_uart_server.c:282-295]`
244. Give every connection its own state object containing connection ID, pair state, MTU, discovery handles, CCCD state, TX queue, and retry counters. `[H3/EPD/sle_uart_client.c:31-40] [NL:374-410]`
245. Fix the `H3` pair-complete index by capturing the slot before incrementing the connection count, and use the actual connection ID rather than an array index in the public send API. `[H3/EPD/sle_uart_client.c:117-146] [H3/EPD/sle_uart_client.c:223-239]`
246. Stop restarting scans from the connected callback; restart only from a validated disconnected state and clear the relevant slot/pairing first. `[H3/EPD/sle_uart_client.c:117-135] [SU/sle_uart_client/sle_uart_client.c:123-127]`
247. Remove the duplicate `sle_uart_client_init()` call from SLE-enable callbacks and make initialization idempotent. `[SU/sle_uart_client/sle_uart_client.c:63-68] [H3/EPD/sle_uart_client.c:61-66]`
248. Make UART RX copy bytes into an owned frame buffer before invoking asynchronous SSAP APIs, and never free a payload until write confirmation or a documented synchronous copy. `[SU/sle_uart.c:204-212] [H3/EPD/sle_uart_client.c:223-239]`
249. Make all callbacks check status, pointer validity, and length before dereferencing or parsing payloads. `[SU/sle_uart.c:83-97] [H3/ExBoard/ExBoard_sample.c:53-76] [H3/EPD/EPD_Sample.c:29-45]`
250. Replace `%s` logging of transport data with length-bounded hex or binary-safe logging. `[SU/sle_uart.c:89-97] [SU/sle_uart.c:184-202]`
251. Replace the ExBoard `%s` parser with a length-bounded binary or TLV parser and store messages in a fixed character array or owned buffer. `[H3/ExBoard/ExBoard_sample.c:32-35] [H3/ExBoard/ExBoard_sample.c:68-70]`
252. Define sensor frames with explicit type, length, sequence, units/scale, validity, and CRC; include an explicit error path when AHT20 or BH1750 reads fail. `[H3/ExBoard/ExBoard_sample.c:99-113] [H3/ExBoard/aht20.c:31-95] [H3/ExBoard/bh1750.c:32-60]`
253. Consume the server control queue in `H3/ExBoard` or remove the copied queue and implement a direct, state-locked re-advertise path with a watchdog. `[H3/ExBoard/sle_uart_server.c:306-313] [H3/ExBoard/sle_uart_server.c:400-418] [NL:374-410]`
254. Verify explicit SSAP read/write response behavior against the target firmware instead of assuming the SDK auto-responds. `[W63:65-70] [W63:292-301]`
255. Keep WS73 adaptation conservative: avoid READ_BY_UUID, avoid unsupported descriptor-configuration bits, and negotiate version 1 before attempting newer features. `[SSAP:14-17] [SSAP:21-27]`
256. Add wire tests for service/property UUID bytes, handle discovery, CCCD write, MTU exchange, write operation bits, binary NUL payloads, oversize payloads, disconnect/re-advertise, and multi-connection slot cleanup. `[NL:195-256] [NL:414-451] [W63:172-205] [SSAP:4-27]`

## 12. Comparison against the existing lab notes

257. Against `NearLinkSLE-SAMPLES.md`, both inspected trees are closer to the transparent-bridge idea than to the checksummed multi-slave `SLE_QT` framer, but neither copies its 8 KiB ring and retry design. `[NL:40-60] [NL:259-324]`
258. Against `NearLinkSLE-SAMPLES.md`, both inspected clients follow the `SLE_UART_HE` shortcut of relying on a pre-enabled descriptor rather than performing the explicit CCCD write shown in `SLE_HELLO`. `[NL:195-256]`
259. Against `NearLinkSLE-SAMPLES.md`, neither inspected tree has the `SLE_QT` disconnect pair cleanup, announce lock, or watchdog state machine. `[NL:374-410] [SU/sle_uart_server/sle_uart_server.c:294-311] [H3/ExBoard/sle_uart_server.c:297-314]`
260. Against `WS63-SLE-EXAMPLES.md`, the inspected clients discover properties and write property handles, whereas the WS63 UUID client discovers a primary service and writes the service start handle. `[W63:71-95] [SU/sle_uart_client/sle_uart_client.c:151-187] [H3/EPD/sle_uart_client.c:156-192]`
261. Against `WS63-SLE-EXAMPLES.md`, the inspected trees use UUIDs `0x2222`/`0x2323` or `0x2000`/`0x2323`, not the WS63 `0xABCD`/`0x1122` table. `[W63:157-168] [SU/sle_uart_server/sle_uart_server.h:22-27] [H3/ExBoard/sle_uart_server.h:31-36]`
262. Against `WS63-SLE-EXAMPLES.md`, the inspected trees do not implement the 236-byte throughput burst, PHY/MCS setup, or flow-controlled million-packet test. `[W63:96-121] [W63:195-205]`
263. Against `WS63-SLE-EXAMPLES.md`, the inspected trees use MTU 520 rather than the WS63 UUID client MTU 300 or speed-sample MTU 512. `[W63:172-205] [SU/sle_uart_client/sle_uart_client.c:17-18] [H3/EPD/sle_uart_client.c:18-19]`
264. Against `SSAP-DIALECT-COMPARISON.md`, the inspected trees use the shared opcode/UUID/version dialect but expose WS73-relevant gaps: no READ_BY_UUID use, no newer descriptor-configuration bits, and no explicit descriptor write. `[SSAP:4-17] [SU/sle_uart_client/sle_uart_client.c:178-187] [H3/EPD/sle_uart_client.c:183-192]`
265. Against `SSAP-DIALECT-COMPARISON.md`, the inspected servers use the common connection-state model but collapse pairing and connection gating into a synthetic pair handle. `[SSAP:21-24] [SU/sle_uart_server/sle_uart_server.c:100-108] [SU/sle_uart_server/sle_uart_server.c:313-344]`
266. Against `SSAP-DIALECT-COMPARISON.md`, the local-stack adaptation should preserve the common SSAP codec while adding the missing descriptor and backpressure behavior identified in the community comparison. `[SSAP:4-13] [NL:414-451]`

## Summary

267. Output path: `/home/archivalera/plum/zcode-projects/nearlink/.scratch/nearlink-driver/lab-notes/NEW-SLE-UART-VARIANTS.md`.
268. `SU` is the reusable UART-bridge skeleton; `H3` is a split EPD/ExBoard product demo with embedded SLE behavior.
269. Neither tree supplies a robust explicit UART frame, negotiated-MTU framer, retry queue, or binary-safe payload lifecycle.
270. The most important interoperability mismatches are service UUID, missing WRITE operation bit, skipped CCCD write, and inconsistent connection-state handling.
271. The most important concrete defects are the 256-byte server overflow, invalid descriptor-value free, duplicate client initialization, `H3` off-by-one connection lookup, dead server queue, and unsafe `%s` parser.
272. The existing `SLE_QT`, `SLE_UART_HE`, WS63, and SSAP notes provide the correct borrowing targets for framing, buffering, CCCD, MTU, and dialect adaptation.
273. A local driver should adopt explicit framing, owned per-connection queues, descriptor support, bounded payloads, status/length checks, and deterministic reconnect state.
274. The report is read-only local analysis and does not claim hardware validation or build success.
275. Scope was limited to the two allowed source trees and the three requested comparison documents.
