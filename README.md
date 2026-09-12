# nearlink-E573H-USBDC

> [English README](README.en.md) · 中文（当前）

把 USB 上那个 `ffff:3733`（厂商/产品串都写着 `00000000`）的**海思 WS73 三模 dongle**（蓝牙 + WiFi6 + 星闪 SLE）变成一台 Linux 电视盒的**三模无线适配器**——提供真正的高速 WiFi / 蓝牙 / 星闪，同时保留控制接口。核心成果：**从零逆向的 WS73 星闪控制面 + 自研 SSAP 用户态协议栈**（x86 可编译、资源感知自适应）。

> **⚠️ push 前必读 —— 文档维护清单**：见下方 [文档索引与维护](#文档索引与维护)。
> pre-push 钩子（`scripts/check-docs.sh` + `scripts/check-harvest-archive.sh`）自动检查：README 双语互链、文档存在性、docs 英文-only、白名单 gitignore、**README 双语同步**，并阻止未在同一 push 中更新双 README 的新 knowledge/harvest 知识归档。

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
assets/stack/ssap/                    # SSAP 用户态协议栈（Apache-2.0 移植 + 自研）
├── ssap_codec   PDU 编解码（0x01-0x14，字节级）
├── hwsle_transport  /dev/hwsle ACB 帧适配（tcid 0x0A）
├── ssap_server  服务表 + 请求分发（EXCHANGE/FIND/READ/WRITE/NOTIFY）
├── ssap_link    DLI 连接状态机（0x1401→0x0015→0x1802/1804）
└── feature_mgr  启发式功能切换（容量档案/RAM 压力/状态/对端能力）
```
测试：codec + server + feature 三套单元测试全绿，x86 零依赖可编译。

### 📚 情报库（122 份研究文档，OKF bundle）
`knowledge/` 是 OKF v0.2 知识束（根 `knowledge/index.md`/`knowledge/log.md` + harvest/intel/decisions 三域）：122 份概念 = 47 份猎收报告（含 Playjoy vendor 协议逆向与 hispark-rs 9 月增量：FRW ROM 槽 261 ABI、NET0 RX 终止所有权契约、fwpkg 0.3.3）+ 74 份协议/芯片/SDK/驱动深挖，另含架构决策记录。覆盖：SSAP 方言对照（**OHOS 与设备固件同协议，移植路线实证**）、连接管理、数据面（DTAP/SDR）、安全配对、测距（含首个公开 UWB-like 多锚点 SLE Channel Sounding 全栈参考）、标准参数（SLE 12Mbps/250µs/256 用户）、6 份 OpenSparklink 契约（DLI/UAPI/CONN-FSM/SSAP/PHY/USB 传输）、汇编/编译器/链接器优化资源与通用指令集，以及 OHOS 生态批次（AT 框架/SLE mesh/framework/HDI/SSAP 权威引擎/客户端/HID 遥控/SA 服务/测距全链路/雷达/DLI 传输/DTAP 数据面/NAI 管理/设备管理/GLE/BGTP/SM 安全/QoS 信用/计划审计/NearLinkSLE 示例/LinkNebula mesh）。

## 仓库结构

```
.
├── knowledge/           # OKF v0.2 知识束（harvest/intel/decisions 三域 + index/log）
├── assets/              # 产物面（stack/ssap 自研栈 + WS73 SDK 参考源码）
├── scripts/             # ws73-probe×3 + load-driver/flash-dongle + check 脚本
├── docs/agents/         # agent 操作约定
└── .scratch/            # wayfinder 追踪器（nearlink-driver / rust-ws73-tri-mode / knowledge-restructure）
```

## 文档索引与维护

> **维护规则**：docs/ 全英文；README 双语互链；改文档后更新下方表格再 push。

| 文件 | 说明 | 语言 |
|---|---|---|
| `README.md` / `README.en.md` | 项目总览（中英互链） | 中/英 |
| `knowledge/` | **OKF v0.2 知识束**：harvest/intel/decisions 三域 + index/log（根 index.md 为 agent 渐进披露面） | 中英混合（legacy zh 带 language 标注，新增英文） |
| `knowledge/intel/DEVICE-INTEL.md` | ffff:3733 设备枚举情报 | 英文 |
| `knowledge/intel/SDK-INTEL.md` | WS73 SDK 结构/构建/可复用部件 | 英文 |
| `knowledge/intel/USB-PROTOCOL.md` | HCC-over-USB 协议要点 | 英文 |
| `knowledge/intel/ECOSYSTEM.md` | 星闪开源生态地图 + 定稿路线 | 英文 |
| `knowledge/intel/SHIFU-BUILD-LIST.md` | 电视盒交叉编译清单（hi3798 SDIO/USB 变体） | 中文 |
| `knowledge/decisions/knowledge-assets-split.md` | 知识面/产物面解耦决策记录 | 英文 |
| `assets/stack/ssap/` | SSAP 用户态栈源码（codec/transport/server/link/feature） | — |
| `scripts/` | 测试/验证/检查脚本 | — |

## Roadmap

1. **SSAP 栈实连验证**：双 dongle 或星闪手机 → 广播/扫描/连接/SSAP 握手（栈已就绪）
2. **WiFi 死锁修复**：PM 独占后加载 wifi_soc → wlan0 → wpa_supplicant
3. **电视盒落地**：hi3798mv310 + SDIO 3.0（师傅按清单交叉编译）
4. **SLB**：WS73 不支持（需专用芯片）；SLE 是当前可做的高速低功耗线

## License

AGPL-3.0（SDK 归海思；SSAP 栈含 Apache-2.0 移植部分）。
