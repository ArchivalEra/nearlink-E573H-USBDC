---
type: intel
title: SLE Measure SDK — QoS Parameters, PHY Tuning & ACB Credit Gating
language: en
created: 2026-08-17
tags: []
---

# SLE Measure SDK — QoS Parameters, PHY Tuning & ACB Credit Gating

**Date:** 2026-08-17
**Source:** `/mnt/hdd/nearlink-stuff/sle_measure_sdk/` (external reference, read-only)

---

## Sources Examined

| File | Key Content |
|---|---|
| `README.md` | Build examples: PHY/MCS/CI/pkt-data-len CLI flags |
| `sle/Kconfig` (lines 29-43) | PHY/MCS/frame-format/pkt-data-len config definitions |
| `sle/sle_speed_server/src/sle_speed_server.c` | PHY config, QoS flow control, credit gating, send loop |
| `sle/sle_ce_server/src/sle_speed_server.c` | Identical QoS pattern (CE variant) |
| `sle/sle_latency_server/src/sle_speed_server.c` | Latency variant with GPIO toggle + variable delay send |
| `sle/sle_speed_client/src/sle_speed_client.c` | MTU negotiation, PHY callback, RSSI averaging |
| `sle/sle_speed_client_1_vs_n/src/sle_speed_client.c` | Multi-pair (1-vs-N) with per-MAC allocation |
| `sle/sle_speed_server/src/sle_speed_server_adv.c` | Advertising params, conn-interval adv defaults |
| `scripts/build_server.sh` | CLI-driven Kconfig update + firmware naming |
| `ble/` directory | Parallel BLE speed server/client + WiFi config (not SLE-relevant) |

---

## 1. PHY Tuning

### Supported PHY Modes

Defined via Kconfig at `sle/Kconfig:33-35`:

```
config PHY
    int "Set PHY (0:SLE_PHY_1M, 1:SLE_PHY_2M, 2:SLE_PHY_4M)."
    default 2
```

Three PHY modes:
- `0` = `SLE_PHY_1M` (GFSK, 1 Mbps baseline)
- `1` = `SLE_PHY_2M`
- `2` = `SLE_PHY_4M` (highest throughput, default for large-throughput configs)

### Frame Formats

`sle/Kconfig:29-31`:
- `0` = `SLE_RADIO_FRAME_1` (legacy/basic)
- `1` = `SLE_RADIO_FRAME_2` (advanced, used with 4M PHY)

### MCS (Modulation and Coding Scheme)

`sle/Kconfig:36-37`:
- Default `6`; commented-out code in CE server (`sle_ce_server/src/sle_speed_server.c:180`) shows `DEFAULT_SLE_SPEED_MCS 10` for max throughput.
- Range: 0-10 (inferred from the commented-out max and Kconfig int type).

### Pilot Density

Hardcoded in all server variants (e.g. `sle_speed_server/src/sle_speed_server.c:190-191`):
- `tx_pilot_density = SLE_PHY_PILOT_DENSITY_16_TO_1`
- `rx_pilot_density = SLE_PHY_PILOT_DENSITY_16_TO_1`

Feedback fields are zeroed: `g_feedback = 0`, `t_feedback = 0`.

### Connection Interval (CI)

`sle/Kconfig:23-25`:
```
config SPEED_DEFAULT_CONN_INTERVAL
    int "Set default connection interval."
    default 20
```

Unit: 125 us. So `20 * 0.125 ms = 2.5 ms`. This is the CI used in high-throughput builds.

The non-throughput default is `0xA0 = 160 -> 20 ms` (see `sle_speed_server.c:63`).

`sle_speed_server_adv.c:18-19` defines the advertisement-side CI separately at `0xA = 10 -> 1.25 ms`.

### How PHY/MCS/CI Are Configured

**Two-phase approach:**

1. **Build-time (Kconfig):** `scripts/build_server.sh` takes CLI flags (`--phy`, `--mcs`, `--conn-interval`, `--pkt-data-len`, `--frame-format`) and writes them into the Kconfig config file. The firmware is then built with these values baked into `CONFIG_*` macros.

2. **Runtime (API):** Inside `send_data_thread_function()`, the server calls:
   - `sle_set_data_len(conn_hdl, 1500)` — set data length per PDU
   - `sle_set_phy_param(conn_hdl, &phy_parm)` — set frame format, PHY, pilot density
   - `sle_set_mcs(conn_hdl, mcs)` — set MCS

   These are HiSilicon vendor SDK APIs (`sle_transmition_manager.h`).

3. **CI at connection setup:** `sle_default_connection_param_set(&param)` sets initial CI; `sle_update_connect_param(&parame)` renegotiates after connection.

---

## 2. Payload

### Maximum Payload: 1370 Bytes

`sle/Kconfig:26-28`:
```
config PKT_DATA_LEN
    int "Set packet data length."
    default 1370
```

The 1370 B default is used in the README build example (`--pkt-data-len 1370`).

### Relationship to MTU

- MTU is negotiated to **1500 B** (`DEFAULT_SLE_SPEED_MTU_SIZE` = 1500, `sle_speed_server.c:68`).
- The SSAP data length is set to **1500 B** (`DEFAULT_SLE_SPEED_DATA_LEN` = 1500, `sle_speed_server.c:67`).
- The *application payload* (`PKT_DATA_LEN`) is the actual data written into the notification PDU, which is capped at 1370 B to fit within the MTU after SSAP/ACB headers.

### Payload vs. Segmentation

No explicit fragmentation/segmentation is implemented in the SDK sample. Each `ssaps_notify_indicate()` call sends one PDU of `PKT_DATA_LEN` bytes. The firmware handles:
- ACB frame encapsulation (5-byte header: `[0xA3][tcid u16][len u16]`)
- Link-layer segmentation into air packets based on PHY and CI

### Retransmission

Retransmission is handled transparently by the SLE link layer (ARQ). The SDK samples do not expose or control retransmission parameters. The client-side `sle_sample_set_phy_cbk` callback (`sle_speed_client.c:194-199`) logs PHY negotiation results but does not configure retransmission.

### Latency Variant Payload

The latency server (`sle_latency_server/src/sle_speed_server.c:60`) uses a smaller `PKT_DATA_LEN = 244` to reduce per-packet latency at the cost of throughput.

---

## 3. ACB Credit Gating

### Two Flow Control Mechanisms

The SDK provides two alternative approaches, selected at compile time:

#### Approach A: `SLE_QOS_FLOWCTRL_FUNCTION_SWITCH` (Preferred)

`sle_speed_server.c:51-53`:
```c
#ifdef SLE_QOS_FLOWCTRL_FUNCTION_SWITCH
static sle_link_qos_state_t g_sle_link_state = 0;  /* sle link state */
#endif
```

`sle_speed_server.c:148-161`:
```c
static void sle_send_data_cbk(uint16_t conn_id, sle_link_qos_state_t link_state)
{
    g_sle_link_state = link_state;
    printf("%s enter, gle_tx_acb_data_num_get:%d.\n", __FUNCTION__, link_state);
}

static void sle_transmission_register_cbks(void)
{
    sle_transmission_callbacks_t trans_cbk = {0};
    trans_cbk.send_data_cb = sle_send_data_cbk;
    sle_transmission_register_callbacks(&trans_cbk);
}
```

Registration happens at init time (`sle_speed_server.c:424-426`):
```c
#ifdef SLE_QOS_FLOWCTRL_FUNCTION_SWITCH
    sle_transmission_register_cbks();
#endif
```

**`sle_link_qos_state_t`** is an opaque type (typedef from SDK headers, not defined in sample code). The callback receives a state value. The gating logic (`sle_speed_server.c:166-173`):

```c
uint8_t sle_flow_ctrl_flag(void)
{
#ifdef SLE_QOS_FLOWCTRL_FUNCTION_SWITCH
    return (g_sle_link_state <= SLE_QOS_FLOWCTRL) ? 1 : 0;
#else
    return gle_tx_acb_data_num_get();
#endif
}
```

So `SLE_QOS_FLOWCTRL` is an enum/define where values <= it mean "safe to send" and values > it mean "backpressure active."

#### Approach B: Legacy `gle_tx_acb_data_num_get()`

`sle_speed_server.c:163`:
```c
extern uint8_t gle_tx_acb_data_num_get(void);
```

This is an extern function from the HiSilicon firmware. It returns the number of pending ACB data frames in the controller's transmit queue. When it returns 0, the controller is full; when > 0, there is room.

### Send Loop with Credit Gating

`sle_speed_server.c:205-211`:
```c
while (1) {
    if (sle_flow_ctrl_flag() > 0) {
        i++;
        data[0] = (i >> 8) & 0xFF;
        data[1] = i & 0xFF;
        sle_uuid_server_send_report_by_handle_id(data, PKT_DATA_LEN, g_sle_conn_hdl);
    }
    ...
}
```

The loop checks `sle_flow_ctrl_flag()` **before every single send**. If the flag is 0 (no credit / backpressure), the iteration is skipped and the loop sleeps or retries next iteration.

### Credit Replenishment

Credit is **ACK-based**: the controller replenishes credit as the peer acknowledges received air packets. The `send_data_cb` callback fires when the controller's credit state changes, updating `g_sle_link_state`. This is a callback-driven model, not a polling model.

In the latency variant (`sle_latency_server/src/sle_speed_server.c:218-234`), the send loop also incorporates a variable delay tied to the connection interval, demonstrating that credit replenishment is synchronized with the CI:
```c
int conn_interval_ms = (int)(SPEED_DEFAULT_CONN_INTERVAL * 0.125);
int conn_interval_us = (int)(SPEED_DEFAULT_CONN_INTERVAL * 125);
```

---

## 4. Comparison with OHOS-DTAP-Layer Credit Mechanism

### SDK Measure (Firmware-side, HiSilicon vendor)

| Aspect | Detail |
|---|---|
| Layer | Controller firmware (`gle_tx_acb_data_num_get`) or link QoS (`sle_link_qos_state_t`) |
| Granularity | Per-connection credit; callback-driven state change |
| Gate check | `sle_flow_ctrl_flag() > 0` before each `ssaps_notify_indicate` |
| Credit source | Firmware ARQ ACK replenishment |
| API | `sle_transmission_register_callbacks({send_data_cb})` |

### OHOS-DTAP (Host-side, HarmonyOS)

The DTAP layer (referenced in our `DTAP-DATA-PLANE.md` and `COMMUNITY-PROJECTS.md`) operates at a higher level:
- It sends via the HCC (Host-Controller Communication) transport layer
- HCC has its own flow control: `hcc_flow_ctrl.c` in the WS73 SDK (`sdk/ws73_sdk_linux_WS73_1.10.110/driver/platform/hcc/comm/hcc_flow_ctrl.c`)
- HCC supports two flow control types: `HCC_FLOWCTRL_DATA` (watermark-based backpressure) and `HCC_FLOWCTRL_CREDIT` (credit-based, with `hcc_bus_get_credit` and `hcc_service_update_credit`)
- Credit register/unregister: `hcc_flow_ctrl_credit_register(cb)` / `hcc_flow_ctrl_credit_unregister()` (`hcc_flow_ctrl.c:317-326`)

### Consistency Assessment

**The two credit models are complementary, not contradictory:**

1. **Firmware-side (SDK measure):** Controls SLE air-interface credit (ACB frame queue depth in the controller). This is the "last mile" — whether the controller can accept another ACB frame for transmission over the air.

2. **Host-side (HCC/DTAP):** Controls the host-to-controller USB bus credit. This is whether the USB endpoint can accept another HCC packet.

3. **They stack:** A host application must pass BOTH gates:
   - HCC credit (can I push data over USB to the controller?)
   - SLE air credit (will the controller accept this ACB frame for over-the-air transmission?)

The SDK measure samples expose the firmware-side gate explicitly. Our Linux driver (`hwsle_transport.c`) currently has **no gate at all** — it writes directly to `/dev/hwsle` with `O_NONBLOCK` and no credit check.

---

## 5. Implications for Our Stack

### Current State: Fire-and-Forget

`hwsle_transport.c:51-71` — `hwsle_transport_send_acb()`:
```c
ssize_t w = write(g_fd, hdr, sizeof(hdr));
// ...
w = write(g_fd, payload, len);
```

No credit check. No flow control. If the USB endpoint or controller buffer is full, `write()` returns -1 with `errno = EAGAIN` (non-blocking) or blocks (blocking mode). The caller does not distinguish "controller busy" from "permanent error."

### Minimum-Viable Credit Gate for `hwsle_transport`

Based on the SDK pattern, the simplest approach is:

1. **Parse the credit event from `/dev/hwsle`:** The `hwsle_transport_run()` event parser already handles `HCI_DATATYPE_EVENT` frames (0xA2). The SLE QoS state change arrives as an event. We need to extract the `sle_link_qos_state_t` (or equivalent credit count) from the event payload.

2. **Add a credit state variable:**
   ```c
   static uint8_t g_acb_credit = 0;  /* 0 = blocked, > 0 = send allowed */
   ```

3. **Gate `hwsle_transport_send_acb()`:**
   ```c
   if (g_acb_credit == 0) {
       errno = EAGAIN;
       return -1;
   }
   ```

4. **Update credit from event parsing** in `hwsle_transport_run()`: when a QoS event is received, update `g_acb_credit`.

5. **Add a public query:** `int hwsle_transport_get_credit(void)` so callers can check before submitting.

This mirrors the SDK's `sle_flow_ctrl_flag()` pattern exactly but operates at the host USB transport level rather than the firmware ACB level. For a minimal first cut, we can also implement a simpler watermark approach: track pending writes (increment on `send_acb`, decrement on event ACK) and cap at a configurable `MAX_INFLIGHT` (e.g., 4-8 frames).

---

## 6. Conclusion: Value for TV-Box Coexistence (WiFi6 + BLE + SLE)

### PHY/QoS Tuning Value

In a TV-box with WiFi6 (2.4/5 GHz), BLE, and SLE sharing the same antenna/radio resource:

- **PHY selection matters for coexistence:** PHY_1M (GFSK) is more robust under interference from WiFi6 OFDMA; PHY_4M (polar) requires cleaner spectrum. A dynamic PHY downgrade strategy (start at 4M, fall back to 1M when WiFi6 is active) would maximize SLE throughput during WiFi quiet periods.

- **CI tuning is critical:** At CI=2.5ms (the SDK default for throughput), SLE occupies the air frequently and competes with BLE connection events. Widening CI to 10-20ms during heavy WiFi/BLE traffic reduces SLE air occupancy by 4-8x while maintaining acceptable latency for non-time-critical data.

- **MCS adaptation:** MCS 10 (max) only works at short range with clean spectrum; MCS 6 (default) is the practical sweet spot. Combining MCS + PHY selection gives a 2D adaptation matrix.

- **TX power control:** The SDK exposes `MAX_TX_POWER_LEVEL` (NV 0x20A0, values 0-7). Lower power during WiFi6 activity reduces near-band interference.

### Credit Gating Value

Without credit gating:
- Host-side buffer bloat: ACB frames pile up in `/dev/hwsle` userspace buffers
- Controller-side packet loss: firmware drops frames when ACB queue is full
- Wasted air time: retransmissions of lost frames consume CI slots

With credit gating:
- Backpressure propagates cleanly from controller to host to application
- Application can make intelligent decisions: drop stale data, reduce send rate, defer to WiFi
- Enables coexistence-aware scheduling: "if SLE credit is low AND WiFi is active, defer SLE sends"

### Recommended Priority

1. **Immediate:** Add credit gating to `hwsle_transport_send_acb()` (minimal: pending-frame counter + MAX_INFLIGHT cap)
2. **Short-term:** Parse QoS events from HCI event stream to drive credit state
3. **Medium-term:** Implement PHY/CI/MCS runtime adaptation API (wrapping `sle_set_phy_param`/`sle_set_mcs` DLI commands) for coexistence-aware operation
4. **Long-term:** Full 2D adaptation matrix (PHY x MCS) with WiFi6/BLE traffic sensing

---

*Report generated from local SDK sources only. No network, build, or hardware access used.*
