# SDK Intel: ws73_sdk_linux_WS73_1.10.110

> 版本: WS73_1.10.110（构件日期 2024-10-28，解压 2025-03-12）· 海思 WS73（"fregata" 家族）**host 侧** Linux 驱动 SDK
> 作用: host CPU 通过 USB/SDIO/UART（HCC 总线）驱动 WS73 星闪芯片

## 顶层结构

```
ws73_sdk_linux_WS73_1.10.110/
├── Makefile                     # 顶层构建（Kconfig 风格，需 build/scripts/hconfig.py）
├── application/
│   ├── bin/<平台>/              # 预编译用户态守护进程（ARM/uClibc 或 aarch64/glibc）
│   │   └── 3518_usb/{ble,sle}/ sparklinkd, sparklinkchba, sparklinkctrl, sle_chba.ko
│   ├── lib/<平台>/{sle,ble}/    # 静态库 libsle_host.a / libble_host.a（仅 ARM 预编译）
│   ├── dft/                     # DFT 通道: bp_test.ko + bp_channel
│   ├── sample/{ble,sle}/        # 示例程序（sle_uuid_server/client 等）
│   └── sle_android/             # Android 9/11/12 patch + NearlinkDemo app
├── build/
│   ├── config/                  # 配置模板: ws73_default.config(SDIO), ws73_light.config,
│   │                            #   ws73_usb_light.config, ws73_usb_light_v2.config, ws73_cfg_default.ini
│   └── scripts/                 # hconfig.py, build_host_hso.py 等
├── driver/
│   ├── bsle/{ble_driver,sle_driver}/  # ble_soc.ko / sle_soc.ko 源码
│   │   └── sle_driver/
│   │       ├── sle_dev/         # misc 设备 /dev/hwsle
│   │       ├── sle_hcc/         # HCC service 粘合
│   │       ├── sle_chba/        # CHBA: sle_hci_chba_proc.c, sle_tm_chba_proc.c
│   ├── platform/
│   │   ├── hcc/{comm,cfg,host,slave,inc,octty,build}
│   │   │   └── host/hcc_usb_host.c   # ★ 关键: ffff:3733 的 usb_driver "wireless_usb"
│   │   ├── firmware_download/   # plat_firmware.c 固件下载框架
│   │   ├── main/ pm/ drv/ osal/ diag/ cfg/ exce/ libc_sec/
│   └── wifi/                    # 完整 802.11 host 驱动源码 → wifi_soc.ko
├── firmware/{e,us}/             # ws73.bin(143KB), wifi_cali.bin, btc_cali.bin, wow.bin
├── include/bsle/{ble,common,sle}/  # 公开 SLE/BLE 用户态 API 头
├── open_source/                 # wpa_supplicant / kernel 补丁
└── output/                      # 构建输出（空）
```

## 构建流程（顶层 Makefile）

- `make` → `wifi_soc.ko` + `plat_soc.ko`（+ 可选 `ble_soc.ko`/`sle_soc.ko`/`bp_test.ko`）
- 配置注入: `build/scripts/hconfig.py <Kconfig> <config> -a output/bin/autoconfig.h`（Kconfig/.config 不在本解压包，会回退 `ws73_default.config`）
- 交叉编译: `make -C $(WSCFG_KERNEL_DIR) ARCH=$(WSCFG_ARCH_NAME) CROSS_COMPILE=$(WSCFG_CROSS_COMPILE) M=... modules`
- **USB 参考配置** `build/config/ws73_usb_light.config` 要点:

```
WSCFG_CROSS_COMPILE="arm-himix100-linux-"
WSCFG_KERNEL_DIR=.../Hi3518_SDK_0110_USB/osdrv/opensource/kernel/linux-4.9.y   # Hi3518EV300
WSCFG_BUS_USB=y
CONFIG_FIRMWARE_BIN_PATH="/etc/ws73/ws73.bin"
CONFIG_FIRMWARE_WIFICALI_PATH="/etc/ws73/wifi_cali.bin"
CONFIG_FIRMWARE_BSLECALI_PATH="/etc/ws73/btc_cali.bin"
CONFIG_INI_FILE_PATH="/etc/ws73_cfg.ini"
```

## 守护进程（仅 ARM 预编译，无源码）

- **sparklinkd** — SLE 协议栈守护（GAP/GATT 类 API、低时延、HID 上报）
- **sparklinkchba** — CHBA（Channel Bridge Adapter）守护: 打开 `/dev/hwslechba` + `/dev/hwsle`，桥接 HCI 数据
- **sparklinkctrl** — 控制/测试 CLI
- **bluetoothd/bluetoothctrl** — BLE 等价物

## 可直接复用的部件（按价值排序）

| 部件 | 路径 | 用途 |
|---|---|---|
| **USB host 驱动** | `driver/platform/hcc/host/hcc_usb_host.c`(1793 行) + `hcc_usb_host_ops.c`(1581 行) | 绑 `ffff:3733` 的完整 usb_driver，boot/kernel 状态机，固件下载 |
| **固件下载框架** | `driver/platform/firmware_download/plat_firmware.c` | 分段传输、SHA256 校验、`WRITEM/WMEM/RMEM/FILES/QUIT` 命令协议 |
| **SLE 内核模块** | `driver/bsle/sle_driver/` | `/dev/hwsle` misc 设备、HCC service、CHBA 钩子 |
| **协议头** | `driver/platform/hcc/comm/hcc_bus_usb_comm.h`, `hcc_usb_host.h` | VID/PID、92 字节包头、EP 布局、缓冲尺寸 |
| **固件 blob** | `firmware/us/*.bin` | 必须灌给 boot 阶段设备的镜像 |
| **用户态 API 头** | `include/bsle/sle/` | SLE 应用层接口参考 |

## 平台与 USB

- `3518_usb` = Hi3518EV300（ARM）+ USB 传输；`7205_usb`/`7205_sdio` 为另一 host 芯片的变体
- WS73 是 USB **device**，host 端跑 `wireless_usb` usb_driver
- 两阶段枚举: boot(2 EP) → 固件下载 → 重枚举 kernel(5 EP)
- CHBA 概念跨 WiFi/SLE 共用（mac_chba_common.h）

## 局限

1. 预编译守护进程无源码（ARM/uClibc），x86 需自行实现 SLE HCI 用户态
2. `libsle_host.a` 仅 ARM，无法直接链 x86
3. 参考内核是 Hi3518EV300 的 4.9.y；但 USB/HCC/SLE 代码用的是标准 Linux USB API，**架构可移植**（改 `WSCFG_ARCH_NAME`/`WSCFG_CROSS_COMPILE`/`WSCFG_BUS_USB` 即可）
4. 文档几乎为零（仅几个 sample ReadMe）
