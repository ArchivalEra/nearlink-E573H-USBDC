# Ecosystem Map: 星闪/NearLink 开源生态（2026-08 摸底）

> 基于对 13 个仓库的深挖 + 海思 WS73 SDK 源码分析。目的: 搞清楚「海思全量开源星闪协议栈」到底指什么、咱们的 ffff:3733 驱动能从哪拼出来。

## 一句话结论

**「全量开源」的正主是 OpenHarmony `communication_nearlink_service`（Apache-2.0，36 万行 C，完整 SLE host 协议栈源码）**。但它只到「HCI 之上」，字节传输/内核驱动/固件都不在里面；而 WS73 USB 的底层（HCC、ffff:3733、固件下载）源码在咱手里的海思 SDK 里。**两者拼起来 + 一条自建传输缝 = 完整方案**。

## 分层地图

| 层 | 状态 | 来源 |
|---|---|---|
| SLE 应用 profile（SSAP 服务/发现/连接/安全/测距/传输） | ✅ 开源 (C, Apache-2.0) | `openharmony/communication_nearlink_service` → `services/stack/{cp,dp}` |
| HCI 式命令集 + HDLC 帧（DLI 层） | ✅ 开源 (C) | 同上 → `services/stack/dli/` |
| 字节传输 HAL 缝 | ✅ 开源但仅 6 函数 | 同上 → `SleDliLayerAdapter.cpp`（依赖 OHOS HDI `ISleHciInterface`） |
| 底层字节传输（UART/USB） | ❌ 不在开源栈内 | OHOS HDF 驱动（外部）/ 需自建 |
| WS73 USB 驱动 + 固件下载（ffff:3733 / HCC） | ✅ 源码在手 | 本仓库 `sdk/.../driver/platform/hcc/host/hcc_usb_host.c` |
| 芯片侧固件/协议栈（Hi2821/BS21/WS63 等） | ⚠️ 闭源预编译 .a + 开源应用层 | Hi2821 SDK 仓库（tools+docs+headers 开源） |
| host 用户态栈（sparklinkd 等） | ❌ 闭源 ARM 二进制 | WS73 SDK `application/bin/` |
| 芯片 AT 命令 / SLE-Link 二进制协议规范 | ✅ 文档开源 | Hi2821 SDK `docs/software/`（456K SLE-Link + 816K AT 手册） |

## 各仓库清点

### 已克隆到 /mnt/hdd/nearlink-stuff/

| 仓库 | 实质 | 对本项目价值 |
|---|---|---|
| `communication_nearlink_service` (24M) | **OpenHarmony 官方 SLE host 协议栈全源码** | ★★★ 用户态栈唯一开源来源 |
| `ili9320-i80-hi2821e-spi-bridge` (757M) | BearPi-Pico H2821E 全量 SDK（CFBB 0.9.0.5）：RISC-V 工具链 349M + LiteOS + 闭源协议库 269 个 .a + 31 份中文文档 | ★★ 芯片侧参考 + **SLE-Link/AT 协议规范** |
| `FlashKeyboard` (246M) | 同系 CFBB SDK（BS21E）+ **TIoT host 驱动**（W33 星闪无线电，0x7e 帧 + 固件加载状态机）+ SLP 定位协议 | ★★ TIoT = "host 驱动星闪无线电"的最佳移植参考 |
| `ws73v100-wifi` (189M) | 咱 SDK 的近亲改版（KO 重命名共存加载）；HCC 代码同一家族，咱的反而更新 | ★ 弃用；唯一可抄: `sle_socket` netlink 通道 |
| `NLChat` (191M) | Android 串口聊天 App（CH34x USB-UART 115200 8N1 连星闪板） | ★ 佐证 SLE_UART 透传生态；无代码价值 |
| `tp78_v3_open` / `NearLink_controller` / `NearLinkSLE` / `LinkNebula` / `NLChat_Web` / `SparkLink-FallDetection` | Hi2821 键盘固件包 / 手柄参考 / SSAP 示例 / Rust 固件 / Web 前端 / WS63 学习文档 | ★ SSAP 调用序列与 HID-over-SLE 设计参考 |
| `NearLink-RSSI-Fingerprinting-Dataset` | 定位数据集 | 无关（3733 命中是浮点假阳性） |

### 未拉（注意）

- `gitee.com/HiSpark/fbb_ws63` —— WS63 SDK 官方源（HiSpark），NLChat/跌倒检测都引用它
- `github.com/Hny0305Lin/Bearpi_Hi2821_Pico_NLChat` / `Bearpi_Hi3863_Pico` / `Hihope_WS63_NearLink_SDK` —— BearPi/HiHope 设备侧固件仓库
- OpenHarmony `drivers/interface/nearlink` —— 栈依赖的 HDI 接口定义（外部仓库）

## 关键协议知识（已确认）

- **WS73 USB**: boot 态 2 bulk EP → HCC patch 通道 ASCII 命令（`WRITEM/WMEM/RMEM/FILES/QUIT`）灌 `ws73.bin`（SHA256 校验, ≤200KB）→ 设备重枚举 5 EP（BULK_IN/OUT + INT_IN + RW_REG×2）→ HCC 数据通道（92B 散射头, credit 流控）
- **DLI 层**（OpenHarmony 栈）: HCI 式 opcode（`DLI_CREATE_CONNECTION=0x1401` 等）+ 5B 头 HDLC 帧 + 命令队列 10s 超时；与 WS73 hcc 的对应关系**未验证**（风险点）
- **TIoT**（FlashKeyboard/W33）: `0x7e | subsys | len | payload | 0x7e`（SLE_MSG=0x15），2M 波特 UART，固件加载 3 次重试
- **SLE-Link**（Hi2821 SDK 文档）: Header/Service ID/Command ID/TLV/MIC 帧，host ↔ 芯片 SLE host 的二进制协议
- **SSAP**: 星闪服务协议（GATT 类似），UUID 0x3333 属性 0x3434 示例、MTU 520、连接间隔 6.25–12.5ms
- **SLP 定位**: 6.4896–7.9872 GHz S 波段、AES-128/SM4、测距/Air-Mouse 模式

## 定稿路线（Phase 化）

```
Phase 0  现状: ffff:3733 处于 boot 态, 无驱动绑定, 无 /dev, 无网络接口
   │
Phase 1  用户态验证握手 (最快, 风险最低)
   │      libusb/gousb 工具: 枚举 2EP → HCC patch 灌 ws73.bin → 观察重枚举 5EP
   │      产物: tools/ws73_probe (协议验证器, 留仓库)
   ▼
Phase 2  内核传输驱动
   │      以 sdk hcc_usb_host.c 为协议参考, 写精简 x86 usb_driver "ws73usb":
   │      boot 下载 + kernel 通道 + misc 字符设备 /dev/ws73hci
   │      产物: driver/ws73usb/ (本仓库)
   ▼
Phase 3  用户态协议栈 (正道, 不局限于厂商 AT 驱动)
   │      移植 OpenHarmony nearlink_service: 剥离 OHOS 依赖(hilog/samgr/napi...)
   │      + 把 SleDliLayerAdapter 的 6 函数 HAL 缝接到 /dev/ws73hci
   │      产物: stack/ (本仓库) ← Apache-2.0 合规
   ▼
Phase 4  应用: 点对点星闪通讯 / HID 外设 / UART 透传 (对齐 SLE_UART 生态)
```

## 风险点

1. **DLI opcode 方言 vs WS73 hcc** 是否互通 —— 未验证（Phase 1 用协议探针确认）
2. OpenHarmony 栈剥离工程量（bundle.json 列了 ~50 个 OHOS 依赖，多数可 stub）
3. WS73 固件 blob 加密/签名（`firmware/us/ws73.bin` 是 data，未知内部格式；hcc 下载流程内含 SHA256，未必加密）
4. 许可: OpenHarmony 栈 Apache-2.0 ✅；海思 SDK 源码归 CompanyNameMagicTag，作参考不整体入仓
