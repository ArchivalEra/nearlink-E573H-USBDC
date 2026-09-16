---
type: intel
title: "NearLinkSLE Community Samples -- Deep Dive"
language: en
created: 2026-08-17
tags: [intel, nearlinksle, community, samples]
sources:
  - "https://github.com/QTDS138/NearLinkSLE"
trust: B
stale_after: 2027-02-17
---

# NearLinkSLE Community Samples -- Deep Dive

**Date:** 2026-08-17
**Analyst:** Research sub-agent
**Classification:** Read-only local analysis, no builds/hardware

## Sources

All paths under `https://github.com/QTDS138/NearLinkSLE/tree/main/`:

| File | Role |
|---|---|
| `README.md` | Project overview (zh), describes SLE_UART_HE as main product |
| `SLE_HELLO/sle_server/sle_hello_server.c` | Simplest SSAP server: periodic "hello world" notification |
| `SLE_HELLO/sle_client/sle_hello_client.c` | Simplest client: scan, connect, CCCD write, log notifications |
| `SLE_QT/sle_server/sle_hello_server.c` | Multi-slave UART transparent server (SLAVE_ID-based) |
| `SLE_QT/sle_client/sle_hello_client.c` | Multi-slave client: 3-node star topology, frame parser |
| `SLE_QT_AD/sle_server/sle_hello_server.c` | ADC sensor (MQ gas sensor) server, UART removed, ADC polling |
| `SLE_QT_AD/sle_client/sle_hello_client.c` | Identical multi-slave client to SLE_QT |
| `SLE_UART_HE/sle_server/sle_hello_server.c` | Production UART transparent server |
| `SLE_UART_HE/sle_client/sle_hello_client.c` | Production UART transparent client |
| All `sle_hello_adv.c` (server-side) | Advertising setup: addr, name, TLV data, scan response |
| All `sle_uart.c` (both sides) | UART1 ISR driver: 4 KiB RX buffer, interrupt-driven, 128B threshold |

---

## 1. Project Overview

### SLE_HELLO -- Minimal connection + notification demo

**Purpose:** Bare-minimum SSAP server+client. Server sends "hello world" every 2 seconds via notification. Client logs received data.

- Server has no UART, no ring buffer -- purely demonstrates SSAP service registration and notification send.
- Client demonstrates the full discovery + CCCD-enabling workflow.
- No data transfer in either direction beyond the periodic hello.

**Build:** CMakeLists.txt collects `sle_hello_server.c` + `sle_hello_adv.c` (server) or `sle_hello_client.c` (client). No external libraries beyond SDK headers.

**Key files:** `SLE_HELLO/sle_server/sle_hello_server.c:166-183` (send loop), `SLE_HELLO/sle_client/sle_hello_client.c:92-123` (CCCD write).

### SLE_QT -- Multi-slave UART transparent transfer

**Purpose:** One client (master) connects to up to 3 servers (slaves) simultaneously. Each slave has a unique SLAVE_ID that drives its MAC address and broadcast name. Data flows over UART in both directions with a custom framing protocol (0x5A 0x5A header, XOR checksum).

**Build:** Same CMake structure plus `sle_uart.c` on both sides. SLAVE_ID is `#define`d in `sle_hello_adv.h` (line 20: `#define SLAVE_ID 3`).

**Key differentiator from SLE_UART_HE:** This is a star topology (1 client, N servers) with a packet framing protocol for multiplexing slave data over a single UART.

### SLE_QT_AD -- ADC sensor server (gas sensor)

**Purpose:** Same multi-slave architecture as SLE_QT but the server replaces UART transparent transfer with ADC polling. Reads MQ gas sensor on GPIO7 (ADC channel 0) every 1000ms, packages the 16-bit voltage as 3 bytes `[status, hi, lo]`, and sends via notification.

**Build:** Adds `adc.h` and `adc_porting.h` (SDK ADC driver). No UART on server side.

**Key files:** `SLE_QT_AD/sle_server/sle_hello_server.c:158-209` (ADC init + polling loop).

### SLE_UART_HE -- Production UART transparent transfer (flagship)

**Purpose:** The "final product" -- a clean, two-board UART-to-UART bridge over SLE. Server and client each have identical UART drivers. Bidirectional: server sends via notification, client sends via write request. Both sides have 8 KiB ring buffers and 128B+2ms x100 retry queues.

**Build:** CMake collects `sle_hello_server.c` + `sle_hello_adv.c` + `sle_uart.c` (server) or `sle_hello_client.c` + `sle_uart.c` (client).

---

## 2. SSAP Server Flow (SLE_HELLO as canonical example)

### 2.1 Registration and Service Addition

The server initialization follows a strict sequential chain:

```
sle_hello_server_entry()          -- app_run() entry point
  -> osal_kthread_create()       -- create kernel task, 4 KiB stack
  -> osal_msleep(1000)           -- wait for system stability
  -> sle_hello_server_init()     -- register all callbacks + enable SLE
  -> (sle_enable_cbk fires)      -- protocol stack ready callback
    -> sle_enable_server_cbk()   -- service + advertising init
      -> sle_hello_server_add()  -- SSAP service registration
      -> sle_uuid_server_adv_init() -- advertising setup + start
```

### 2.2 Service Registration Step-by-Step

From `SLE_HELLO/sle_server/sle_hello_server.c:44-101`:

**Step 1: Register server with app UUID**
```c
sle_uuid_t app_uuid = {.len = 2, .uuid = {0x12, 0x34}};
ssaps_register_server(&app_uuid, &g_server_id);    // line 51
```
App UUID `{0x12, 0x34}` is arbitrary -- just identifies the application.

**Step 2: Add service (16-bit UUID = 0x3333)**
```c
sle_uuid_setu2(0x3333, &service_uuid);             // line 55
ssaps_add_service_sync(g_server_id, &service_uuid, 1, &g_service_handle);  // line 56
```
`1` = number of properties in this service. `g_service_handle` is assigned by the stack.

**Step 3: Add property (16-bit UUID = 0x3434)**
```c
property.permissions = SSAP_PERMISSION_READ | SSAP_PERMISSION_WRITE;        // line 60
property.operate_indication = SSAP_OPERATE_INDICATION_BIT_READ
                            | SSAP_OPERATE_INDICATION_BIT_WRITE
                            | SSAP_OPERATE_INDICATION_BIT_NOTIFY;          // line 61-62
property.uuid = {0x3434};
property.value = osal_vmalloc(6);  // initial 6-byte zeroed value           // line 67
ssaps_add_property_sync(..., &property, &g_property_handle);               // line 74
```
The property supports read, write, AND notification -- the NOTIFY bit is what enables `ssaps_notify_indicate()` to work.

**Step 4: Add descriptor (user description, pre-enables notification)**
```c
uint8_t ntf_value[] = {0x01, 0x00};     // little-endian 0x0001              // line 84
descriptor.type = SSAP_DESCRIPTOR_USER_DESCRIPTION;                         // line 88
descriptor.permissions = SSAP_PERMISSION_READ | SSAP_PERMISSION_WRITE;      // line 86
ssaps_add_descriptor_sync(..., g_property_handle, &descriptor);             // line 92
```
This is the CCCD -- see section 4 for details.

**Step 5: Start service**
```c
ssaps_start_service(g_server_id, g_service_handle);                         // line 98
```

### 2.3 Handle Assignment Pattern

| Element | Handle | Assignment |
|---|---|---|
| Service start | auto-assigned by stack | `g_service_handle` |
| Property | service_start + 1 (typically) | `g_property_handle` |
| Descriptor | property_handle + 1 (client computes this) | client-side: `desc_hdl = property->handle + 1` |

The client discovers the property handle via `ssapc_find_structure()`, then computes the CCCD descriptor handle as `property_handle + 1`.

### 2.4 Advertising Setup

From `SLE_HELLO/sle_server/sle_hello_adv.c:43-123`:

1. **Set local MAC:** `sle_set_local_addr()` with hardcoded `50:5A:0A:83:11:22` (line 56)
2. **Set local name:** `sle_set_local_name("sle_hello", 9)` (line 64)
3. **Configure advertising params:** connectable+scanable, 200ms interval, 3 channels (0x07), conn_interval=20, supervision_timeout=500 (lines 70-81)
4. **Set advertising data (TLV format):**
   - Adv data: `[01 01 01] [01 02 00]` -- discovery_level=1, access_mode=0
   - Scan response: `[0A 0B] [s l e _ h e l l o]` -- type 0x0B = complete local name
5. **Start:** `sle_start_announce(SLE_ADV_HANDLE_DEFAULT)` (line 118)

---

## 3. SSAP Client Flow (SLE_HELLO as canonical example)

### 3.1 Full Sequence

From `SLE_HELLO/sle_client/sle_hello_client.c`:

```
sle_hello_client_entry()
  -> osal_kthread_create()
  -> osal_msleep(1000)
  -> sle_hello_client_init()     -- register callbacks + enable SLE
  -> (sle_enable_cbk fires)
    -> ssapc_register_client()   -- get g_client_id
    -> sle_set_seek_param()      -- scan interval=100ms, window=100ms
    -> sle_start_seek()          -- begin scanning
```

### 3.2 Scan -> Connect -> Discover -> Subscribe

**Scan result** (line 152-173): `parse_adv_name()` walks TLV advertising data looking for name type 0x09/0x0B/0x08. On match, copies address and calls `sle_stop_seek()`.

**Seek disable** (line 175-181): After scan stops, calls `sle_connect_remote_device(&g_remote_addr)`.

**Connected** (line 183-206): Stores `conn_id`, sets `g_client_connected = true`, initiates MTU exchange:
```c
ssap_exchange_info_t info = {0};
info.mtu_size = 520;
info.version = 1;
ssapc_exchange_info_req(g_client_id, conn_id, &info);
```

**MTU exchange complete** (line 135-150): Triggers service discovery:
```c
find_param.type = SSAP_FIND_TYPE_PROPERTY;
find_param.start_hdl = 1;
find_param.end_hdl = 0xFFFF;
ssapc_find_structure(g_client_id, conn_id, &find_param);
```

**Property found** (line 92-123): This is where CCCD write happens -- see section 4.

**Disconnected** (line 200-205): Resets state, restarts scan via `sle_start_seek()`.

---

## 4. CCCD Enable -- The Critical Notification Gate

### 4.1 The Problem

If the client does not write `0x0001` to the CCCD descriptor handle after discovering the property, the server's `ssaps_notify_indicate()` calls will silently fail. The server may return success from the API, but no notification PDU will actually be transmitted over the air.

### 4.2 How SLE_HELLO Client Enables Notifications

From `SLE_HELLO/sle_client/sle_hello_client.c:92-123`:

```c
static void sle_hello_client_find_property_cbk(...) {
    if (status == ERRCODE_SLE_SUCCESS && property != NULL) {
        uint16_t desc_hdl = property->handle + 1;   // line 99: CCCD = prop_handle + 1

        ssapc_write_param_t write_param = {0};
        write_param.handle = desc_hdl;               // line 102
        write_param.type = SSAP_PROPERTY_TYPE_VALUE;  // line 103
        write_param.data_len = 2;                     // line 106
        write_param.data = osal_vmalloc(2);           // line 108
        write_param.data[0] = 0x01;                   // line 114: enable notifications
        write_param.data[1] = 0x00;                   // line 115: (little-endian 0x0001)

        ssapc_write_req(g_client_id, conn_id, &write_param);  // line 117
        osal_vfree(write_param.data);                          // line 121
    }
}
```

Key observations:
- **Handle computation:** CCCD handle = property_handle + 1. This is consistent across all four samples.
- **Value:** `{0x01, 0x00}` = little-endian `0x0001` = notifications enabled. `{0x00, 0x00}` would disable.
- **Memory:** Uses `osal_vmalloc` for DMA-accessible SRAM heap, freed immediately after the write call returns.
- **No confirmation wait:** The write is fire-and-forget; the `write_cfm_cb` callback is registered but the SLE_HELLO client does not block on it.

### 4.3 How the Server Pre-enables Notifications

All server variants add a descriptor with pre-set value `{0x01, 0x00}`:

```c
// SLE_HELLO/sle_server/sle_hello_server.c:84
uint8_t ntf_value[] = {0x01, 0x00};  // pre-enabled
descriptor.type = SSAP_DESCRIPTOR_USER_DESCRIPTION;
ssaps_add_descriptor_sync(..., g_property_handle, &descriptor);
```

This is a pragmatic shortcut: the server-side descriptor starts with notifications enabled, so even if the client forgets to write the CCCD, the server-side state may still allow notifications (implementation-dependent). However, the proper flow is still for the client to explicitly write `0x0001`.

### 4.4 Variants -- SLE_UART_HE Skips CCCD Write

In `SLE_UART_HE/sle_client/sle_hello_client.c:102-109`, the property-found callback does NOT write the CCCD:

```c
static void sle_hello_client_find_property_cbk(...) {
    if (status == ERRCODE_SLE_SUCCESS && property != NULL) {
        g_property_hdl = property->handle;
        osal_printk("[SLE Client] Server notifications are pre-enabled. Ready for UART transmission.\r\n");
    }
}
```

The comment "Server notifications are pre-enabled" confirms that SLE_UART_HE relies on the server-side descriptor pre-enabling. This is a deliberate design choice for the production UART bridge -- it simplifies the client at the cost of depending on server-side cooperation.

---

## 5. Retry Queue -- 128B + 2ms x 100

### 5.1 Server-Side Retry (SLE_UART_HE)

From `SLE_UART_HE/sle_server/sle_hello_server.c:160-186`:

```c
static int sle_hello_server_task(void *arg) {
    while (1) {
        uint16_t bytes_to_send = 0;

        // 1. Drain ring buffer, max 128 bytes per packet
        while (g_trans_queue_head != g_trans_queue_tail && bytes_to_send < 128) {
            send_temp_buf[bytes_to_send++] = g_trans_queue_buf[g_trans_queue_head];
            g_trans_queue_head = (g_trans_queue_head + 1) % TRANS_QUEUE_SIZE;
        }

        if (bytes_to_send > 0) {
            if (g_sle_connected) {
                errcode_t ret;
                uint32_t retries = 0;
                do {
                    ret = sle_hello_server_send_data(send_temp_buf, bytes_to_send, g_sle_conn_hdl);
                    if (ret == 0) break;       // success
                    osal_msleep(2);              // backoff: 2ms
                    retries++;
                } while (retries < 100);        // max 100 retries = 200ms total
            }
        } else {
            osal_msleep(2);  // no data: sleep 2ms to yield CPU
        }
    }
}
```

### 5.2 Client-Side Retry (SLE_UART_HE)

From `SLE_UART_HE/sle_client/sle_hello_client.c:228-264`:

Identical pattern but uses `ssapc_write_req()` instead of `sle_hello_server_send_data()`:

```c
do {
    ret = ssapc_write_req(g_client_id, g_conn_id, &write_param);
    if (ret == 0) break;
    osal_msleep(2);
    retries++;
} while (retries < 100);
```

### 5.3 Retry Parameters Summary

| Parameter | Value | Source |
|---|---|---|
| Max packet size | 128 bytes | `bytes_to_send < 128` condition |
| Retry interval | 2 ms | `osal_msleep(2)` |
| Max retries | 100 | `retries < 100` |
| Total max retry window | 200 ms | 100 x 2ms |
| Ring buffer size | 8192 bytes | `TRANS_QUEUE_SIZE` |
| Idle sleep | 2 ms | prevents busy-spin |
| Send buffer | 256 bytes (static) | `static uint8_t send_temp_buf[256]` |

### 5.4 Why This Pattern

The retry loop addresses a fundamental SLE stack behavior: `ssaps_notify_indicate()` and `ssapc_write_req()` can return non-zero if the stack's internal TX buffer is full (air-interface backpressure). The 2ms sleep gives the stack time to complete the previous transmission and free the buffer. The 100-iteration cap prevents infinite blocking -- if the link is truly dead, the connection-state callback will fire and set `g_sle_connected = false`, causing the main loop to skip sending entirely.

### 5.5 Multi-Slave Variant (SLE_QT)

In SLE_QT, the retry is reduced to 50 iterations (100ms max) for the multi-slave case:

```c
// SLE_QT/sle_client/sle_hello_client.c:377-382
do {
    ret = ssapc_write_req(g_client_id, slave->conn_id, &write_param);
    if (ret == ERRCODE_SLE_SUCCESS) break;
    osal_msleep(2);
    retries++;
} while (retries < 50);
```

---

## 6. Disconnect Re-Announce (Re-Advertising)

### 6.1 SLE_HELLO / SLE_UART_HE -- Simple Re-Advertise

From `SLE_UART_HE/sle_server/sle_hello_server.c:117-130`:

```c
static void sle_hello_connect_state_changed_cbk(...) {
    if (conn_state == SLE_ACB_STATE_DISCONNECTED) {
        g_sle_conn_hdl = 0;
        g_sle_connected = false;
        sle_start_announce(SLE_ADV_HANDLE_DEFAULT);   // line 128: immediate re-advertise
    }
}
```

On disconnect, the server immediately restarts advertising with the same handle. No cleanup of the SSAP service -- it remains registered and active.

### 6.2 Client-Side Re-Scan

From `SLE_UART_HE/sle_client/sle_hello_client.c:160-165`:

```c
else if (conn_state == SLE_ACB_STATE_DISCONNECTED) {
    g_conn_id = 0;
    g_client_connected = false;
    g_property_hdl = 0;        // reset property handle
    sle_start_seek();           // restart scanning
}
```

Client resets its property handle (which will be re-discovered on next connection) and starts scanning again.

### 6.3 SLE_QT -- Enhanced with Pair Cleanup + State Lock

From `SLE_QT/sle_server/sle_hello_server.c:130-148`:

```c
else if (conn_state == SLE_ACB_STATE_DISCONNECTED) {
    g_sle_conn_hdl = 0;
    g_sle_connected = false;
    g_is_announcing = false;     // clear announce lock
    (void)sle_remove_all_pairs();  // LINE 147: clean pair list
}
```

Additional features in SLE_QT:
- **`sle_remove_all_pairs()`**: Clears the pairing database, necessary for re-connecting to a device that was previously paired.
- **`g_is_announcing` state lock**: Prevents double-advertise. The `my_sle_start_announce()` wrapper (line 28-40) checks both `g_sle_connected` and `g_is_announcing` before calling the real `sle_start_announce()`.
- **Watchdog timer**: The main loop has a `watchdog_ticks` counter (line 210-216). Every 1000 iterations (2 seconds at 2ms sleep), if disconnected, it attempts re-advertise. This catches cases where the disconnect callback's `sle_start_announce()` failed.

### 6.4 Client-Side Watchdog (SLE_QT)

From `SLE_QT/sle_client/sle_hello_client.c:405-421`:

```c
watchdog_ticks++;
if (watchdog_ticks >= 1000) {    // every 2 seconds
    watchdog_ticks = 0;
    bool any_disconnected = false;
    for (int i = 0; i < MAX_SLAVES; i++) {
        if (!g_slaves[i].connected) { any_disconnected = true; break; }
    }
    if (any_disconnected && g_pending_slave_idx == -1) {
        (void)my_sle_start_seek();   // self-healing scan restart
    }
}
```

Plus a connection-timeout watchdog (line 393-403): if `g_pending_slave_idx` stays non-null for 5000 ticks (10 seconds), it force-resets the state machine.

---

## 7. Gap Analysis -- What NearLinkSLE Has That Our ssap_server.c Lacks

### 7.1 Our Current Stack

From `stack/ssap/src/ssap_server.c`:

Our `ssap_server.c` implements:
- Service registration (`ssap_server_add_service`)
- Property registration with read/write callbacks (`ssap_server_add_property`)
- PDU dispatch: EXCHANGE_INFO, FIND_STRUCTURE (primary service + property), READ_REQ, WRITE_CMD, WRITE_REQ
- Notify/indicate send (`ssap_server_notify`)
- Feature-aware config (`ssap_server_apply_config`)

### 7.2 Missing Pieces (Directly Copyable)

| # | Gap | NearLinkSLE Reference | Priority |
|---|---|---|---|
| 1 | **No descriptor (CCCD) support** | All servers: `ssaps_add_descriptor_sync()` with type `SSAP_DESCRIPTOR_USER_DESCRIPTION` and value `{0x01,0x00}`. Client writes `0x0001` to `prop_handle+1`. Our FIND_STRUCTURE_RSP always returns `descCount=0`. | HIGH -- notifications silently fail without this |
| 2 | **No FIND_STRUCTURE for descriptors** | SLE_HELLO client discovers property, then writes to `handle+1` for CCCD. Our dispatch only handles `SSAP_FIND_PRIMARY_SERVICE` and `SSAP_FIND_PROPERTY` -- no `SSAP_FIND_DESCRIPTOR`. | HIGH |
| 3 | **No `ssap_server_t.connected` state tracking** | NearLinkSLE servers track `g_sle_connected` and gate all sends on it. Our struct has `connected` field (line 78 of header) but nothing writes to it. | MEDIUM |
| 4 | **No retry/backpressure logic** | All NearLinkSLE variants have the 2ms x 100 retry loop. Our `ssap_server_notify()` is fire-and-forget. | MEDIUM |
| 5 | **No `sle_remove_all_pairs()` equivalent** | SLE_QT calls this on disconnect. Our stack has no pairing management. | LOW (USB driver scope) |
| 6 | **No advertising state machine** | NearLinkSLE has `g_is_announcing` lock, `my_sle_start_announce()` wrapper, watchdog re-advertise. Our stack delegates this to the kernel driver. | LOW (kernel handles this) |
| 7 | **No property value storage** | NearLinkSLE servers allocate 6 bytes via `osal_vmalloc` for initial property value. Our properties have no value buffer -- reads go to callbacks only. | MEDIUM -- cannot serve READ_REQ without a live callback |
| 8 | **No WRITE_RSP with error items** | Our `ssap_server_dispatch` WRITE_REQ handler calls `ssap_encode_write_rsp` but NearLinkSLE's SDK handles this at a lower level. The pattern is worth noting for protocol compliance. | LOW |

### 7.3 Directly Adaptable Code Patterns

1. **CCCD descriptor registration** -- The `ssaps_desc_info_t` pattern from `SLE_HELLO/sle_server/sle_hello_server.c:83-93` should be modeled in our property struct. Add a `has_cccd` flag and a `cccd_value` field to `ssap_property_t`.

2. **Client-side CCCD write** -- The `property_handle + 1` convention from `SLE_HELLO/sle_client/sle_hello_client.c:99` should be documented as the expected handle layout.

3. **Ring buffer + retry queue** -- The 8 KiB ring buffer + 128B max packet + 2ms x 100 retry pattern from `SLE_UART_HE` is battle-tested and should be the reference implementation for our data bridge layer.

4. **Connection state gating** -- Every send in NearLinkSLE checks `g_sle_connected` before calling the SSAP send function. Our `ssap_server_notify()` should do the same using the `srv->connected` field.

5. **FIND_STRUCTURE response with descriptor count** -- Currently hardcoded to `0x00` at `ssap_server.c:154`. Should be computed from the property's actual descriptor count when CCCD support is added.

### 7.4 What We Already Do Better

- **Clean separation:** Our `ssap_server.c` is a pure protocol-layer implementation with no hardware dependencies. NearLinkSLE mixes SSAP, UART, advertising, and GPIO into single files.
- **Callback-based architecture:** Our read/write callbacks are cleaner than NearLinkSLE's inline logic.
- **Feature-aware versioning:** `ssap_server_apply_config()` with `SSAP_VERSION_1_0` vs `SSAP_VERSION_1_3` branching is more maintainable.
- **Codec layer:** Our `ssap_codec.h` encode/decode functions are separated from business logic. NearLinkSLE relies on SDK-provided encode/decode.
