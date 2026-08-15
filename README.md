# nearlink-E573H-USBDC

把 USB 上那个 `ffff:3733`（厂商/产品串都写着 `00000000`）的星闪/近场（NearLink, SparkLink/SLE）USB dongle 在 Linux 上跑起来。

## 这是什么

一根插在 USB 上的 **海思 WS73 星闪 dongle**。它当前枚举为：

```
Bus 001 Device 005: ID ffff:3733 00000000 00000000
```

- **VID:PID** `ffff:3733` —— 海思 WS73 全系（WiFi/BT/SLE 组合芯片）USB 标识，SDK 里写死为 `DEIVICE_VENDOR_ID/PRODUCT_ID`
- **接口**：单接口，class `0xE0/0x02/0x02`（WUSB Wire Adapter 伪装），2 个 bulk 端点
- **状态**：**boot 模式** —— 固件还没下载，所以没有任何内核驱动绑定它、没有 /dev 节点、没有网络接口。要它干活，先得把 `ws73.bin` 灌进去（HCC patch 通道），设备会重枚举成 5 端点（kernel 模式），然后才谈得上协议栈。

## 仓库结构

```
.
├── .gitignore                 # 白名单式：只放行源码/文档/配置，二进制与构建产物一律不跟踪
├── docs/
│   ├── DEVICE-INTEL.md        # ffff:3733 设备枚举情报（sysfs/lsusb/描述符/当前绑定）
│   ├── SDK-INTEL.md           # WS73 SDK 1.10.110 结构、构建流程、可复用部件
│   └── USB-PROTOCOL.md        # HCC-over-USB 协议要点（boot/kernel 状态机、EP 布局、固件下载）
└── sdk/ws73_sdk_linux_WS73_1.10.110/   # 海思 WS73 Linux SDK（源码随仓跟踪，二进制被 gitignore 拦住）
```

> SDK 原始分发是 `ws73_sdk_linux_WS73_1.10.110.zip`（48MB，本机位于项目根目录，被 `*.zip` 规则排除出版本库）。`firmware/us/ws73.bin` 等固件 blob 也**不进 git** —— 驱动运行时从 SDK 解压目录或 `/etc/ws73/` 读取。

## 驱动方案（路线图）

SDK 自带完整 host 侧驱动源码（`driver/platform/hcc/host/hcc_usb_host.c` 的 `wireless_usb` usb_driver），但它是按海思 ARM 平台（Hi3518EV300 / kernel 4.9, `arm-himix100`）交叉编译的；预编译的 `sle_chba.ko` 也全是 ARM/aarch64，x86 上 vermagic 对不上。

三条路线：

| 路线 | 做法 | 代价 | 结果 |
|---|---|---|---|
| **A. 移植 SDK 驱动** | 把 `driver/platform` + `driver/bsle/sle_driver` 重编成 x86 模块 | 中（依赖 osal/td 层，工具链是 x86 gcc + 本机内核头，可行但缝多） | `plat_soc.ko`+`sle_soc.ko` → `/dev/hwsle` + `hwslechba` 网卡 |
| **B. 新写精简驱动** | 以 SDK 源码为协议参考，从零写一个干净 x86 `usb_driver`（boot→固件下载→kernel 状态机 + misc 字符设备） | 中（协议已摸清，工作量可控） | 自己的驱动，`/dev/hwsle` 等价物，代码干净 |
| **C. 先 userspace 验证** | libusb/gousb 先跑通「枚举 → 固件下载 → 重枚举」握手，确认协议后再决定内核化 | 低（最快拿到结果） | 验证协议正确性，不产出最终驱动 |

用户态协议栈（`sparklinkd`/`sparklinkchba`/`sparklinkctrl`）SDK 里只有 ARM 预编译二进制、无源码，最终要么用路线 A/B 内核 + 自行实现 SLE HCI 用户态，要么反向这套二进制。

## 快速开始

```bash
# 固件放好（SDK 解压目录已含 firmware/us/）
ls sdk/ws73_sdk_linux_WS73_1.10.110/firmware/us/ws73.bin

# 设备情报
lsusb -v -d ffff:3733
```

（驱动代码尚未落地 —— 见上路线图与 docs/。）

## License

AGPL-3.0（SDK 归海思/CompanyNameMagicTag 所有，仅作驱动开发参考）。
