# Kernel (5-EP) Mode SLE Channel Bring-Up: Host Init Sequence

SDK root: `sdk/ws73_sdk_linux_WS73_1.10.110/`
Investigated: 2026-08-15. All paths below are relative to the SDK root unless absolute.

## Why this document exists

A test harness sent `H2D_MSG_SLE_OPEN` (29) as a USB control transfer
(`bmRequestType 0x21, bRequest 0, data=uint32(29)`) immediately after firmware
download, and the device **rebooted back to boot mode**. The control transfer
itself is byte-for-byte what the real driver sends — the failure is that the
device-side SLE stack expects two **data-channel** prerequisites first
(customize/INI push + status acks), which the test skipped. Full sequence below.

---

## 1. `hbsle_hcc_custom_ini_data_buf` — customize/INI push

Definition: `driver/platform/cfg/customize_bsle.c:682`
Export: `customize_bsle.c:768` (EXPORT_SYMBOL). Declared `customize_bsle_ext.h:171`.

Callers:
- `driver/platform/cfg/customize_bsle.c:758` — inside `hbsle_hcc_customize_h2d_data_cfg()` (which also does `hcc_service_init(HCC_CHANNEL_AP, HCC_ACTION_TYPE_BSLE_MSG, &g_hcc_bsle_msg_adapt)` at `:743`).
- `driver/platform/pm/plat_pm_wlan.c:502` — `pm_bsle_open()` calls `hbsle_hcc_customize_h2d_data_cfg()`.
- `driver/bsle/sle_driver/sle_host_register.c:145` — `sle_recovery()` (DFR path).
- `driver/bsle/ble_driver/linux/ble_host_hcc.c:711` — BT DFR recovery.
- `driver/bsle/ble_driver/android/ble_host.c:67`.

### Wire bytes it sends

Buffer layout (`customize_bsle.c:682-737`):
- Allocates `BSLE_LARGE_NETBUF_SIZE` = **800** bytes (`customize_bsle_ext.h:19`).
- `data_buf = tx_buf + hcc_get_head_len()` (5 bytes, `driver/platform/hcc/comm/hcc.c:78-81`, `sizeof(hcc_header)`).
- `msg_tag_buf = data_buf` (offset 5), then `data_buf += sizeof(bsle_msg_tag)` (4 bytes, offset 9).
- `bsle_msg_tag.type = BSLE_MSG_HCC_TYPE_CUSTOMIZE_DATA` (= **0**), `.len = sizeof(bfgn_bt_customization_stru)` (= **140**, see below). `customize_bsle.c:714-715`.
- Customize struct memcpy'd at offset 9: `customize_bsle.c:723`.
- TX params (`hbsle_hcc_custom_param_init`, `customize_bsle.c:650-657`):
  - `service_type = HCC_ACTION_TYPE_BSLE_MSG` = **5**
  - `sub_type = 0`
  - `queue_id = BSLE_MSG_QUEUE` = **10**
  - `fc_flag = 0`
- Send: `hcc_tx_data(HCC_CHANNEL_AP, tx_buf, 800, &param)` (`customize_bsle.c:731`).

### BSLE message-channel framing (`hcc_send_message` format)

The **hcc_header** is written into the first 5 bytes of the netbuf by
`hcc_header_init()` (`driver/platform/hcc/comm/hcc.c:950-959`); note it is only
filled when service_type != BT && != SLE, so BSLE_MSG gets a header:

```
struct hcc_header (5 bytes, packed — driver/platform/hcc/inc/hcc_comm.h:121-129):
  byte 0: sub_type:4 | service_type:4     => 0x05 (BSLE_MSG)
  byte 1: queue_id                        => 0x0A (BSLE_MSG_QUEUE=10)
  byte 2-3: pay_len (u16 LE)              => 800 (0x320)
  byte 4: padding (0)
```

Netbuf payload (offsets from start of tx_buf):

| offset | size | content |
|--------|------|---------|
| 0      | 5    | hcc_header (service_type=5, queue_id=10, pay_len=800) |
| 5      | 4    | bsle_msg_tag: `type` u16 LE = **0** (CUSTOMIZE_DATA), `len` u16 LE = **140** |
| 9      | 140  | `bfgn_bt_customization_stru` bytes |
| 149..  | 651  | zero padding (netbuf is 800) |

`bsle_msg_tag` struct: `driver/platform/cfg/customize_bsle_ext.h:109-112`
(`{ uint16_t type; uint16_t len; }`, 4 bytes).

`sizeof(bfgn_bt_customization_stru)` = **140** (`customize_bsle_ext.h:121-165`;
29×u32 + int32 + 2×i16 + 2×u8 + 2×6-byte MAC, 4-byte aligned → 138→140).

### There is no H2D_MSG for INI/customize data

Customize data is NOT a `H2D_MSG_*` control-transfer message. It is a normal
**HCC data packet on BSLE_MSG_QUEUE**. `h2d_msg_type` has no customize entry —
the enum (`driver/platform/drv/device/romable/include/hcc_cfg_comm.h:93-127`)
runs WLAN_OPEN=0 … BT_OPEN=25, BT_CLOSE=26, WOW_SYNC=27, HEART_BEAT=28,
**SLE_OPEN=29**, **SLE_CLOSE=30**, `H2D_MSG_COUNT=32`.

The `bfgn_bt_customization_stru` carries the 6-byte BLE/SLE MACs
(`ble_mac_bdaddr`, `sle_mac_bdaddr`), TX-power calibration, SRRC table, flash
calibration — i.e. the identity/config the device SLE stack needs before any
radio open.

---

## 2. Full host init sequence (probe → open)

Entry: `sle_host_register.c:59 sle_host_init()` (module_init, `:181`).

1. **`pm_sle_open()`** (`sle_host_register.c:64`) = `pm_svc_open(PM_SVC_SLE, TRUE)`
   (`driver/platform/pm/plat_pm_wlan.c:737-740`).
2. **First-service power-on + firmware download** (only if no WLAN/BLE/SLE svc
   open yet, `plat_pm_wlan.c:588-598`): `pm_init_n_firmware_download()` →
   `pm_svc_power_on()` (`driver/platform/pm/plat_pm.c:243`) →
   `ws73_board_service_enter()` (`driver/platform/pm/plat_pm_board_ws73.c:368`)
   → `firmware_download_enter()` → `firmware_download_etc()`
   (`driver/platform/firmware_download/plat_firmware.c:1253`) which runs the
   boot-mode text protocol (WRITEM / FILES / **QUIT**) over bulk EP
   (see `assets/01`). After QUIT the device re-enumerates to 5-EP WORK mode;
   host sees `D2H_MSG_BSP_READY` (bit 0) on the INT EP →
   `plat_hcc_msg_dev_bsp_ready_cb` (`driver/platform/main/plat_main.c:124`).
3. **BSLE pre-open, inside `pm_svc_open` for a BLE/SLE service**
   (`plat_pm_wlan.c:604-607`): `pm_bsle_open()` (`plat_pm_wlan.c:498-515`):
   - `hbsle_hcc_customize_h2d_data_cfg()` → **hcc_service_init for
     HCC_ACTION_TYPE_BSLE_MSG** + **customize INI push** (section 1).
   - Polls `hbsle_hcc_customize_get_device_status(BSLE_STATUS_CUSTOMIZE_RECEIVED)`.
4. **`bsle_open_close_cmd(PM_SVC_SLE, TRUE)`** (`plat_pm_wlan.c:608`,
   function at `:459-487`) → `msg_type = H2D_MSG_SLE_OPEN` (29) →
   `hcc_send_message(HCC_CHANNEL_AP, 29, HCC_ACTION_TYPE_TEST)` — the control
   transfer the test replicated.
5. Back in `sle_host_init`: poll `BSLE_STATUS_BOOT_FINISH`
   (`sle_host_register.c:71`).
6. **`sle_hcc_init()`** (`sle_host_register.c:76`,
   `driver/bsle/sle_driver/sle_hcc/sle_hcc_proc.c:143`) → `hcc_service_init(HCC_CHANNEL_AP, HCC_ACTION_TYPE_SLE(0xA), &g_sle_hcc_adapt)`.
7. **`sle_dev_init()`** (`sle_host_register.c:82`) → registers misc device
   **`/dev/hwsle`** (`sle_dev.c:266-271`) + platform driver.
8. Userspace `open("/dev/hwsle")` → `sle_misc_dev_open` (`sle_dev.c:90`):
   - `pm_sle_enable()` (`sle_dev.c:102`) → `bsle_open_close_cmd(PM_SVC_SLE, TRUE)`
     → sends **H2D_MSG_SLE_OPEN (29) again** (`plat_pm_wlan.c:478-481`).
   - Waits (2 s timeout) for SLE state → `SLE_ON` (`sle_dev.c:108-115`).

**Ack path for the open:** device→host packet on BSLE_MSG_QUEUE with
`bsle_msg_tag.type = BSLE_MSG_HCC_TYPE_DEVICE_ACTION_STATUS` (4) and payload
u32 = `BSLE_STATUS_MSG_SLE_OPEN` (2) → host `hcc_adapt_bsle_msg_rx_proc`
(`customize_bsle.c:335-343`) → `pm_sle_enable_reply_cb_host_get()` →
`sle_open_close_btc_finish_handle` (`sle_dev.c:237`) → `set_sle_state(SLE_ON)`.

`bsle_msg_hcc_type` enum (`customize_bsle_ext.h:92-99`):
CUSTOMIZE_DATA=0, FSM_DATA=1, DEVICE_STATUS=2, DATA_COLLECTION=3,
DEVICE_ACTION_STATUS=4.
`bsle_hcc_msg_c2h` (`customize_bsle_ext.h:114-118`): BOOT_FINISH=1,
CUSTOMIZE_RECEIVED=2.

---

## 3. `hcc_send_message` wire format over USB (WORK mode)

`hcc_send_message` (`driver/platform/hcc/comm/hcc_bus.c:97-111`) ignores
`service_type` (`uapi_unused`) and calls
`bus_ops->send_and_clear_msg(bus, msg_id)` → for USB =
`hcc_usb_send_msg` (`driver/platform/hcc/host/hcc_usb_host_ops.c:743-775`,
installed in `g_usb_opt_ops` at `:1402`):

```
oal_usb_control(usb, HOST_TO_DEV_SEND_MSG, USB_DIR_OUT, &val, sizeof(val));
val = 4-byte little-endian u32 (e.g. 0x0000001D = 29)
```

`oal_usb_control` (`hcc_usb_host_ops.c:75-96`) → `oal_usb_control_msg`
(`:29-73`) with:

```
requesttype = USB_TYPE_CLASS | USB_RECIP_INTERFACE | USB_DIR_OUT
            = 0x20 | 0x01 | 0x00 = 0x21
request     = HOST_TO_DEV_SEND_MSG = 0     (enum, hcc_usb_host.h:231-239)
wValue = 0, wIndex = 0, timeout = USB_CTRL_SET_TIMEOUT
data (OUT) = 4-byte u32 = the message id
```

So the test's `0x21/0x00/val=29` is **exactly** the driver's control transfer —
that part is not the bug. (PM variant uses `HOST_TO_DEV_SEND_PM_MSG`=2,
`hcc_usb_host_ops.c:794`.)

Data-channel packets (customize push, SLE frames) go out as **bulk OUT frames**,
not control transfers:
`hcc_usb_tx_proc` (`hcc_usb_host_ops.c:671-741`) → per-queue
`hcc_usb_tx_control_data` sends a `HCC_NETBUF_QUEUE_SWITCH` descriptor frame
(`:646-666`) then the netbuf frame `hcc_usb_xfer_write_func` (`:443-499`):

```
TX bulk frame:
  offset 0: usb_package { u32 msg_type; u32 len; u32 reserve }   (12 bytes,
             hcc_usb_host.h:272-276)
             msg_type = USB_SINGLE_MSG(0) | USB_ARRG_GET_DATA(1) | USB_HCC_CONTROL_MSG(3)
             len     = total data length (incl. padding, min USB_MIN_PACKAGE_LEN=0x40)
  offset 12: data payload(s), each padded to >= 0x40 bytes
```
Queue-switch descriptor frame is `usb_package(12) + hcc_descr_header{u32 descr_type=1}
+ queue_id byte` (`hcc_usb_host_ops.c:524-560`, `hcc_usb_xfer_write_desc :374-391`).

There is **no `hcc_send_message_wait`** in this SDK — the only wait is the
host-side polling of device status flags.

---

## 4. Device→host ack format

Two distinct inbound paths:

**(a) D2H messages via INT EP** (`driver/platform/hcc/host/hcc_usb_host.c`):
`usb_read_device_event` (`:504-542`) polls `int_in_ep` with
`usb_wlan_interupt_in` (`:479-501`), `INT_TRANS_MAX_PACKAGE_SIZE = 8`
(`hcc_usb_host.h:92`):
```
struct usb_dev_notification (8 bytes, hcc_usb_host.h:278-281):
  u32 notification;        // bitmask of d2h_msg_type (hcc_cfg_comm.h:58-90)
  u32 dev_mem_highpri_pool;
```
Bits relevant: D2H_MSG_BSP_READY=0, D2H_MSG_WLAN_READY=1, D2H_MSG_DEVICE_PANIC=6.
Each set bit → `hcc_bus_call_rx_message` → registered cb. `BSP_READY` cb =
`plat_hcc_msg_dev_bsp_ready_cb` (`plat_main.c:124`). **Device panic/reboot
surfaces here as D2H_MSG_DEVICE_PANIC**, and the USB device re-enumerates.

**(b) BSLE status/ack via bulk IN on BSLE_MSG_QUEUE** — this is how the SLE open
is acked. Device frame = 92-byte scatter header + netbuf(s). Host RX
`usb_read_data_header` (`hcc_usb_host.c:348-377`) + `usb_wlan_xfer_read_func`
(`:382-406`) skip the first `HIUSB_PACKAGE_HEARDER_SIZE` = 92 bytes, then the
netbuf starts with the 5-byte hcc_header (service_type=5, queue_id=10),
then `bsle_msg_tag`, then a u32 `device_msg`:

```
netbuf:
  offset 0:  hcc_header (5 bytes)
  offset 5:  bsle_msg_tag { u16 type; u16 len }
  offset 9:  u32 device_msg
```
Host parser: `hcc_adapt_bsle_msg_rx_proc` (`customize_bsle.c:296-353`):
- type=2 (DEVICE_STATUS): sets `bsle_device_msg[device_msg]=true`
  (device_msg=1 → BOOT_FINISH, 2 → CUSTOMIZE_RECEIVED).
- type=4 (DEVICE_ACTION_STATUS): `device_msg == BSLE_STATUS_MSG_SLE_OPEN(2)`
  → `pm_sle_enable_reply_cb` (→ SLE_ON) ; BLE_OPEN(0)/BLE_CLOSE(1)/SLE_CLOSE(3) similar.

So the **SLE_OPEN ack is NOT a D2H INT-EP message and NOT a control IN**; it is
a bulk-IN HCC data packet on BSLE_MSG_QUEUE.

---

## 5. 92-byte scatter header & queue/service ids

`HIUSB_PACKAGE_HEARDER_SIZE` = 92 (`driver/platform/hcc/comm/hcc_bus_usb_comm.h:19`).
RX (device→host) header layout (`hcc_usb_host.c:348-377`):

| offset | size | field |
|--------|------|-------|
| 0  | u32 | `usb_package.msg_type` = **USB_ARRG_DEV_HOST_DATA** (2) |
| 4  | u32 | `usb_package.len` = total xfer count |
| 8  | u32 | `usb_package.reserve` = **hcc queue id** |
| 12 | 48  | `aggr_len[0..23]` (u16 each, `HIUSB_DEV2HOST_SCATT_MAX=24`) — per-packet lengths |
| 60 | 32  | reserved/padding to 92 |

Data starts at offset 92 (`hcc_usb_host.c:394`). The first netbuf's hcc_header
carries message type (byte0 low nibble = sub_type, high nibble = service_type),
queue id (byte 1), length (bytes 2-3, u16 LE).

Queue/service ids (`driver/platform/drv/device/romable/include/hcc_cfg_comm.h`):
- `BSLE_MSG_QUEUE = 10` (`:52`)  — customize data + status/action acks
- `SLE_DATA_QUEUE = 8` (`:50`)   — SLE HCI frames
- `HCC_ACTION_TYPE_BSLE_MSG = 5` (`:32`), `HCC_ACTION_TYPE_SLE = 0xA` (`:37`),
  `HCC_ACTION_TYPE_TEST = 2` (used for the H2D msg control transfers, `:29`)
- Queue cfg table: `driver/platform/hcc/cfg/hcc_cfg.c:121-141` (BSLE_MSG_QUEUE
  TX/RX, service_type=HCC_ACTION_TYPE_BSLE_MSG, SINGLE_MODE).

---

## Ordered host init checklist

| # | Action | Wire bytes | Expected ack |
|---|--------|-----------|--------------|
| 1 | Power-on + firmware download (boot mode) | ASCII lines `WRITEM …`/`FILES 1 <addr> <len> <state>`/`QUIT` over bulk (see assets/01) | `WRITEM OK`, `READY`, `FILES OK`; device re-enumerates to 5-EP |
| 2 | Wait for device up | — | `D2H_MSG_BSP_READY` bit on INT EP (8-byte `usb_dev_notification`) |
| 3 | `hcc_service_init(HCC_CHANNEL_AP, HCC_ACTION_TYPE_BSLE_MSG, …)` | — | — |
| 4 | Push customize/INI data | bulk OUT frame: `usb_package{msg_type, len, reserve}` + netbuf `[hcc_header 05 0A 20 03 00][tag type=0 len=0x008C][140-byte bfgn_bt_customization_stru]` | bulk IN on BSLE_MSG_QUEUE: `[hcc_header 05 0A …][tag type=2 len=4][u32=2 CUSTOMIZE_RECEIVED]` |
| 5 | `hcc_send_message(HCC_CHANNEL_AP, H2D_MSG_SLE_OPEN=29, …)` | ctrl OUT: `0x21 / req=0 / wValue=0 / wIndex=0 / data=1D 00 00 00` | bulk IN on BSLE_MSG_QUEUE: `[tag type=4 len=4][u32=2 BSLE_STATUS_MSG_SLE_OPEN]` |
| 6 | `hcc_service_init(HCC_CHANNEL_AP, HCC_ACTION_TYPE_SLE, …)`; misc device `/dev/hwsle`; open → SLE_ON | SLE frames go to SLE_DATA_QUEUE | — |

## Verdict: why the device rebooted

`H2D_MSG_SLE_OPEN` (29) is the **second half** of a two-step bring-up. The real
driver first registers the BSLE_MSG service and pushes the **customize/INI data
(140-byte `bfgn_bt_customization_stru`, incl. SLE MAC, TX-power cali)** as a
**bulk-OUT HCC data packet on BSLE_MSG_QUEUE (10)**, and waits for
`BSLE_STATUS_CUSTOMIZE_RECEIVED` and `BSLE_STATUS_BOOT_FINISH` device status
acks before ever sending SLE_OPEN. The test sent only the control transfer
(which is otherwise byte-identical to `hcc_usb_send_msg`); the device-side SLE
stack therefore opened unconfigured, faulted/panicked, surfaced
`D2H_MSG_DEVICE_PANIC`, and re-enumerated to boot mode. The missing pieces are
the **customize-data push on BSLE_MSG_QUEUE** and the **status-ack handshake**,
not the control transfer itself. If the test *did* push customize data, the next
suspects are the 4-byte LE `tag.len`/`type` fields, the hcc_header
(`service_type=5, queue_id=10, pay_len=800`), and queue-switch descriptor
frames that must precede the first bulk data frame.
