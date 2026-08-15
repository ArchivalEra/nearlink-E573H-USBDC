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
├── .gitignore                 # 白名单式：默认忽略一切，仅 !pattern 放行源码/文档/配置
├── docs/
│   ├── DEVICE-INTEL.md        # ffff:3733 设备枚举情报（sysfs/lsusb/描述符/当前绑定）
│   ├── SDK-INTEL.md           # WS73 SDK 1.10.110 结构、构建流程、可复用部件
│   ├── USB-PROTOCOL.md        # HCC-over-USB 协议要点（boot/kernel 状态机、EP 布局、固件下载）
│   └── ECOSYSTEM.md           # ★ 星闪开源生态地图（13 仓库摸底）+ 定稿路线
└── sdk/ws73_sdk_linux_WS73_1.10.110/   # 海思 WS73 Linux SDK（源码随仓跟踪，二进制被 gitignore 拦住）
```

> SDK 原始分发是 `ws73_sdk_linux_WS73_1.10.110.zip`（48MB，本机位于项目根目录，被 `*.zip` 规则排除出版本库）。`firmware/us/ws73.bin` 等固件 blob 也**不进 git** —— 驱动运行时从 SDK 解压目录或 `/etc/ws73/` 读取。

## 驱动方案（定稿路线，详见 docs/ECOSYSTEM.md）

**核心洞察**：海思 2026 年「全量开源星闪协议栈」= OpenHarmony `communication_nearlink_service`（Apache-2.0，36 万行 C 的 SLE host 栈源码），但它不含字节传输/内核驱动/固件；WS73 USB 底层（HCC、ffff:3733、固件下载）源码在咱手里的 SDK。**两半拼起来 + 一条自建传输缝 = 完整方案**，无需受限于厂商闭源的 AT 驱动。

```
Phase 1  userspace 握手验证: libusb/gousb 灌 ws73.bin (HCC patch) → 观察重枚举 5EP
Phase 2  内核传输驱动: 精简 x86 usb_driver (参考 hcc_usb_host.c) → /dev/ws73hci
Phase 3  用户态协议栈: 移植 OpenHarmony nearlink_service (剥离 OHOS 依赖, HAL 缝接 /dev/ws73hci)
Phase 4  应用: 点对点通讯 / HID / UART 透传
```

旁路参考: 海思 SDK 自带 `wireless_usb` 驱动源码（ARM 预编译、x86 需重编）；FlashKeyboard 里的 TIoT host 驱动（0x7e 帧 + 固件加载）是「host 驱动星闪无线电」的另一移植范本。

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
