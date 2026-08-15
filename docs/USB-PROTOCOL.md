# USB Protocol Intel: HCC over USB (WS73 `ffff:3733`)

> 来源: `driver/platform/hcc/host/hcc_usb_host.{c,h}`、`hcc_usb_host_ops.c`、`plat_firmware.c`、`hcc_bus_usb_comm.h`

## 两阶段状态机

`oal_usb_probe()` 按 **接口端点数量** 分派：

```
enum bNumEndpoints == 5  → kernel 模式  oal_usb_system_init_config()
enum bNumEndpoints == 2  → boot 模式    oal_usb_boot_init_config()   (需 CONFIG_HCC_SUPPORT_PATCH_OPT)
```

```
boot (2 EP)                      kernel (5 EP)
  │                                  ▲
  │ oal_usb_boot_init_config         │ hcc_usb_reload() 等重枚举
  ▼                                  │
固件下载 (HCC patch 通道,           │
bulk-out, ≤200KB, 32KB 缓冲)        │
  │ 固件尾 QUIT 命令                 │
  ▼──────────────────────────────────┘
设备重启, 重枚举为 5 EP
```

bus 状态机（`bus_usb_state`）: OFF → INIT → BOOT_PROBE → BOOT → PROBE → WORK（含 SUSPEND/RESUME/DISCONNECT 支路）。

## EP 布局（kernel 模式）

```c
#define DEVICE_KERNEL_EP_NUM 5
#define DEVICE_BOOT_EP_NUM   2
#define BULK_EP_IN_IND      0   // 数据 IN
#define BULK_EP_OUT_IND     1   // 数据 OUT
#define INT_EP_IN_IND       2   // 设备事件通知 (usb_dev_notification, 8 字节)
#define RW_REG_EP_OUT_IND   3   // 寄存器写
#define RW_REG_EP_IN_IND    4   // 寄存器读
```

## 关键常量

```c
// hcc_bus_usb_comm.h
#define DEIVICE_VENDOR_ID   0xFFFF
#define DEIVICE_PRODUCT_ID  0x3733
#define HIUSB_PACKAGE_HEARDER_SIZE 92   // 散射数据包头
#define HIUSB_DEV2HOST_SCATT_MAX 24     // 聚合项上限

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

## 固件下载（HCC patch 通道）

固件通过 ASCII 命令协议在 bulk 通道上传输（`plat_firmware.c`）:

```
FW_BIN_DOWNLOAD_CMD     "1,0x400000,/etc/ws73/ws73.bin"
FW_ROMBIN_DOWNLOAD_CMD  "1,0x106400,/etc/ws73/ws73_rom.bin"
FW_WIFICALI_DOWNLOAD_CMD "1,0x430000,/etc/ws73/wifi_cali.bin"
FW_BSLECALI_DOWNLOAD_CMD "1,0x440000,/etc/ws73/btc_cali.bin"
```

命令字: `WRITEM` / `WMEM` / `RMEM` / `FILES` / `QUIT`，配 `MSG_FROM_DEV_*` 应答；固件头带 SHA256 校验；`firmware_quit_func()` → `hcc_usb_reload()` 处理重枚举。

## 寄存器 R/W（RW_REG 端点）

```c
#define RW_REG_RETRY_TIMES  3
#define RW_REG_TIMEOUT_MS   2000
#define RW_REG_STAGE_SETUP  0x01
#define RW_REG_STAGE_DATA   0x02
#define RW_REG_ADDR_LEN     4
// rw_reg_setup_bytes: reg_off:16 | dir:1 | reg_len:11 | cur_stage:2
```

## 设备通知

中断 IN 端点收 `usb_dev_notification`（8 字节: notification + dev_mem_highpri_pool）。

## 用户态接口（SDK 既有）

- `/dev/hwsle` — misc 字符设备（`sle_dev.c`），写 = 发 SLE HCI 帧 → `hcc_bt_tx_data(HCC_CHANNEL_AP,...)`；open 触发 `pm_sle_enable()` → `H2D_MSG_SLE_OPEN`(=29)
- `/dev/hwslechba` — CHBA 数据网卡（`sle_chba.ko` 的 netdev）
- HCI 数据帧里 TCID 在字节 5（`HCI_DATA_TCID_POS 5`）；CHBA 用自己的帧（`HCI_DATATYPE_SLE_ACB 0xA3` / `HCI_DATATYPE_SLE_ICB 0xA4`）过滤链路数据
