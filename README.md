# nearlink-E573H-USBDC

> [English README](README.en.md) · 中文（当前）

把 USB 上那个 `ffff:3733`（厂商/产品串都写着 `00000000`）的**海思 WS73 三模 dongle**（蓝牙 + WiFi6 + 星闪 SLE）变成一台 Linux 电视盒的**三模无线适配器**——提供真正的高速 WiFi / 蓝牙 / 星闪，同时保留控制接口。核心成果：**从零逆向的 WS73 星闪控制面 + 自研 SSAP 用户态协议栈**（x86 可编译、资源感知自适应）。

> **⚠️ push 前必读 —— 文档维护清单**：见下方 [文档索引与维护](#文档索引与维护)。
> pre-push 钩子（`scripts/check-docs.sh`）自动检查：README 双语互链、文档存在性、docs 英文-only、白名单 gitignore、**README 双语同步**。

## 项目状态（2026-08）

### ✅ 硬件侧（真机验证）
| 里程碑 | 状态 |
|---|---|
| boot 固件下载握手（WRITEM/FILES/QUIT） | ✅ 双 dongle 验证 |
| 星闪 SLE 控制面（广播/扫描/连接/测距/数据链路/安全） | ✅ 全命令 accepted |
| 蓝牙（hci1，LE 扫描发现设备） | ✅ 实测 |
| WiFi（wifi_soc 加载） | ⚠️ 懒初始化 PM 死锁（已记录，需独占 PM） |
| SLB 能力 | ❌ WS73 无（SDK/固件零 SLB，标准会员制） |

### ✅ 软件侧（自研）
```
stack/ssap/                          # SSAP 用户态协议栈（Apache-2.0 移植 + 自研）
├── ssap_codec   PDU 编解码（0x01-0x14，字节级）
├── hwsle_transport  /dev/hwsle ACB 帧适配（tcid 0x0A）
├── ssap_server  服务表 + 请求分发（EXCHANGE/FIND/READ/WRITE/NOTIFY）
├── ssap_link    DLI 连接状态机（0x1401→0x0015→0x1802/1804）
└── feature_mgr  启发式功能切换（容量档案/RAM 压力/状态/对端能力）
```
测试：codec + server + feature 三套单元测试全绿，x86 零依赖可编译。

### 📚 情报库（17 份研究文档）
星闪协议全套深挖：SSAP 方言对照（**OHOS 与设备固件同协议，移植路线实证**）、连接管理、数据面（DTAP/SDR）、安全配对、测距、标准参数（SLE 12Mbps/250µs/256 用户）。

## 仓库结构

```
.
├── stack/ssap/          # 自研 SSAP 用户态栈（核心资产）
├── scripts/             # ws73-probe×3 + load-driver/flash-dongle + 检查脚本
├── docs/                # 英文情报（DEVICE/SDK/USB-PROTOCOL/ECOSYSTEM）
├── sdk/                 # 海思 WS73 SDK（源码 + x86 移植，二进制 gitignored）
└── .scratch/            # wayfinder 决策图 + 17 份研究报告 + 师傅清单
```

## 文档索引与维护

> **维护规则**：docs/ 全英文；README 双语互链；改文档后更新下方表格再 push。

| 文件 | 说明 | 语言 |
|---|---|---|
| `README.md` / `README.en.md` | 项目总览（中英互链） | 中/英 |
| `docs/DEVICE-INTEL.md` | ffff:3733 设备枚举情报 | 英文 |
| `docs/SDK-INTEL.md` | WS73 SDK 结构/构建/可复用部件 | 英文 |
| `docs/USB-PROTOCOL.md` | HCC-over-USB 协议要点 | 英文 |
| `docs/ECOSYSTEM.md` | 星闪开源生态地图 + 定稿路线 | 英文 |
| `stack/ssap/` | SSAP 用户态栈源码（codec/transport/server/link/feature） | — |
| `scripts/` | 测试/验证/检查脚本 | — |
| `.scratch/nearlink-driver/lab-notes/` | **17 份研究报告**（SSAP 方言/CM/DTAP/SDR/SM/HADM/标准…） | 中英混合 |
| `.scratch/nearlink-driver/lab-notes/SHIFU-BUILD-LIST.md` | 电视盒交叉编译清单（hi3798 SDIO/USB 变体） | 中文 |

## Roadmap

1. **SSAP 栈实连验证**：双 dongle 或星闪手机 → 广播/扫描/连接/SSAP 握手（栈已就绪）
2. **WiFi 死锁修复**：PM 独占后加载 wifi_soc → wlan0 → wpa_supplicant
3. **电视盒落地**：hi3798mv310 + SDIO 3.0（师傅按清单交叉编译）
4. **SLB**：WS73 不支持（需专用芯片）；SLE 是当前可做的高速低功耗线

## License

AGPL-3.0（SDK 归海思；SSAP 栈含 Apache-2.0 移植部分）。
