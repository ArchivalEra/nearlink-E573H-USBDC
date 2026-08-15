# USB Protocol Intel: HCC over USB (WS73 `ffff:3733`)

> Source: `driver/platform/hcc/host/hcc_usb_host.{c,h}`, `hcc_usb_host_ops.c`, `plat_firmware.c`, `hcc_bus_usb_comm.h` in the WS73 SDK

## Two-stage state machine

`oal_usb_probe()` dispatches by **interface endpoint count**:

```
enum bNumEndpoints == 5  → kernel mode  oal_usb_system_init_config()
enum bNumEndpoints == 2  → boot mode    oal_usb_boot_init_config()   (needs CONFIG_HCC_SUPPORT_PATCH_OPT)
```

```
boot (2 EP)                      kernel (5 EP)
  │                                  ▲
  │ oal_usb_boot_init_config         │ hcc_usb_reload() etc. waits for re-enum
  ▼                                  │
firmware download (HCC patch        │
channel, bulk-out, ≤200KB, 32KB     │
buffer)                             │
  │ QUIT command at end of FW       │
  ▼──────────────────────────────────┘
device reboots, re-enumerates as 5 EP
```

Bus state machine (`bus_usb_state`): OFF → INIT → BOOT_PROBE → BOOT → PROBE → WORK (plus SUSPEND/RESUME/DISCONNECT branches).

## EP layout (kernel mode)

```c
#define DEVICE_KERNEL_EP_NUM 5
#define DEVICE_BOOT_EP_NUM   2
#define BULK_EP_IN_IND      0   // data IN
#define BULK_EP_OUT_IND     1   // data OUT
#define INT_EP_IN_IND       2   // device event notification (usb_dev_notification, 8 bytes)
#define RW_REG_EP_OUT_IND   3   // register write
#define RW_REG_EP_IN_IND    4   // register read
```

## Key constants

```c
// hcc_bus_usb_comm.h
#define DEIVICE_VENDOR_ID   0xFFFF
#define DEIVICE_PRODUCT_ID  0x3733
#define HIUSB_PACKAGE_HEARDER_SIZE 92   // scatter data packet header
#define HIUSB_DEV2HOST_SCATT_MAX 24     // aggregation limit

// hcc_usb_host.h
#define USB_RX_MAX_SIZE   (20 * 1024)
#define USB_TX_MAX_SIZE   (20 * 1024)
#define URB_RX_MAX_NUM    3
#define URB_TX_MAX_NUM    8
#define USB_DOWNLOAD_FW_TIMEOUT 30000    // ms

// plat_firmware.h
#define MAX_FIRMWARE_FILE_TX_BUF_LEN (32 * 1024)
#define MAX_FIRMWARE_FILE_RX_BUF_LEN (128 * 1024)
#define FIRMWARE_FILESIZE_MAX (200 * 1024)
```

## Firmware download (HCC patch channel)

Firmware is transferred over the bulk channel via an ASCII command protocol (`plat_firmware.c`):

```
FW_BIN_DOWNLOAD_CMD     "1,0x400000,/etc/ws73/ws73.bin"
FW_ROMBIN_DOWNLOAD_CMD  "1,0x106400,/etc/ws73/ws73_rom.bin"
FW_WIFICALI_DOWNLOAD_CMD "1,0x430000,/etc/ws73/wifi_cali.bin"
FW_BSLECALI_DOWNLOAD_CMD "1,0x440000,/etc/ws73/btc_cali.bin"
```

Command words: `WRITEM` / `WMEM` / `RMEM` / `FILES` / `QUIT`, with `MSG_FROM_DEV_*` acknowledgements; the firmware header carries SHA256; `firmware_quit_func()` → `hcc_usb_reload()` handles re-enumeration.

## Register R/W (RW_REG endpoints)

```c
#define RW_REG_RETRY_TIMES  3
#define RW_REG_TIMEOUT_MS   2000
#define RW_REG_STAGE_SETUP  0x01
#define RW_REG_STAGE_DATA   0x02
#define RW_REG_ADDR_LEN     4
// rw_reg_setup_bytes: reg_off:16 | dir:1 | reg_len:11 | cur_stage:2
```

## Device notification

Interrupt IN endpoint delivers `usb_dev_notification` (8 bytes: notification + dev_mem_highpri_pool).

## User-space interfaces (as shipped in the SDK)

- `/dev/hwsle` — misc char device (`sle_dev.c`): write = send SLE HCI frame → `hcc_bt_tx_data(HCC_CHANNEL_AP,...)`; open triggers `pm_sle_enable()` → `H2D_MSG_SLE_OPEN`(=29)
- `/dev/hwslechba` — CHBA data netdev (`sle_chba.ko`)
- In HCI data frames the TCID lives at byte 5 (`HCI_DATA_TCID_POS 5`); CHBA filters its own frames (`HCI_DATATYPE_SLE_ACB 0xA3` / `HCI_DATATYPE_SLE_ICB 0xA4`) from the link data

## Cross-reference to the open-source stack

The OpenHarmony `communication_nearlink_service` stack exposes an HCI-like **DLI** command set (`DLI_CREATE_CONNECTION = 0x1401` etc.) and its own HDLC-style framing. Whether the WS73 controller's HCC dialect answers those exact opcodes is **unverified** — see [ECOSYSTEM.md](ECOSYSTEM.md) risks.
