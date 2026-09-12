# 11 — 仓库猎收（harvest）未完成事项

Type: task
Status: open
Blocked by:

## Question

token 驱动的持续猎收（2026-08-19 会话）中断后，哪些仓已克隆未消化、哪些方向还没搜？

## 已完成（会话内）

- 本地仓 39 个 / 9.4G（/mnt/hdd/nearlink-stuff/），含 hispark-rs 24 子仓全家桶
- NEW-* 报告 17 份归队（含 hispark-rs 9 月增量）：BearPi-NLChat / BS21-WTSL / HiSilicon-Assessment / WS63FLASH-GHIDRA / XF-BURN / XFUSION / HISPARK-RS 生态 / SLE Mesh + BS2X Rust / nearLinKernel 甄别 / SLE UART 变体 / Xinghongpai WS63 固件程序知识 / NearLink Toolbox 网站程序知识 / NLChat Web 端程序知识 / NearLink Assembly Optimization（汇编/编译器/链接器优化资源与通用优化指令集）/ NearLink UWB-Like Ranging（首个公开 SLE Channel Sounding 多锚点测距全栈参考）/ Playjoy HID Keyboard（Sparklink Playjoy 星闪外设 vendor 配置协议逆向 + 五 crate 分层 + 回放测试）/ hispark-rs 9 月增量（FRW ROM 槽 261 ABI、NET0 RX 终止所有权契约、fwpkg 0.3.3 load-address、KM flush 寄存器）

## 已克隆仓库消化状态

| 仓 | 归档结果 | 结论 |
|---|---|---|
| nearLinKernel (29K, Julia) | **已完成**：`NEW-NEARLINKERNEL-CLASSIFICATION.md` | 名字像 NearLink，实际是 near-linear kernel 数值线性代数论文代码，属于名称碰撞 |
| NLChat_Web (636K) | **已完成**：`NEW-NLCHAT-WEB.md` | 浏览器端 Web Serial 串口终端/聊天 UI；没有 SLE/SSAP/HADM/DLI 协议实现，仅可借鉴浏览器串口生命周期与诊断界面 |
| nearlink-toolbox-website (71 files) | **已完成**：`NEW-NEARLINK-TOOLBOX-WEBSITE.md` | 实际是 Next.js 静态宣传页；下载只取远端 JSON 并打开 Windows URL，不是 Tauri/Rust/串口/烧录实现 |
| xinghongpai-nearlink-dev-board (178 files) | **程序知识已完成**：`NEW-XINGHONGPAI-FIRMWARE-KNOWLEDGE.md`；硬件/PCB 按要求不纳入本轮 | 固件示例的 SDK API、AHT20/SSD1306、ADC、Wi-Fi/lwIP 与缺陷边界；引脚/PCB 留给后续硬件 session |
| sle_uart + Hi3863-SLE-2025 (小) | **已完成**：`NEW-SLE-UART-VARIANTS.md` | SLE 透传变体、传感器-over-SLE 数据帧；已对比 UUID、MTU、CCCD 和显式 framing 缺口 |
| hispark-rs/* 24 子仓 (771M) | **已完成**：`NEW-HISPARK-RS-ECOSYSTEM.md` | WS63/BS2X Rust 全家桶（QEMU/rt/pac/crypto/nvs/rtos/rf-facade/fwpkg/flash），可作为 rust-ws73 参考地基 |
| sle_mesh v4.4.9 (19M) | **已完成**：`NEW-SLEMESH-RUST.md` | 2026-06 推送；已记录 leader-rooted tier routing、directed relay、无 flooding/dedup cache 等边界 |
| fbb_bs2x_rust (659M) | **已完成**：`NEW-SLEMESH-RUST.md` | BS2x Rust fork；已记录 Rust `no_std` staticlib 与 C/LiteOS 共存模式 |
| NearLink Assembly Optimization（网络汇编/编译器/链接优化） | **已完成**：`NEW-NEARLINK-ASSEMBLY-OPTIMIZATION.md` | 汇编 inline/寄存器访问、GCC/Clang/LLVM 标志分级、LTO/gc-sections 边界、Rust release profile、通用优化指令集与度量门禁 |
| nearlink-uwb-like-ranging (2026-09-12 推送，5.8M) | **已完成**：`NEW-NEARLINK-UWB-LIKE-RANGING.md` | 首个公开 SLE Channel Sounding 多锚点测距全栈（Anchor/Client/Collector 三角色 + 332B IQ 结构 + 主机 GnUls 定位）；**WS73 sle_hadm_manager.h API 同构，dongle 可做测距端** |

## 未搜方向

- 厂商官方 org（hisilicon/HiHope-Official 在 GitHub 无 nearlink 仓，gitee API 需登录）
- SLE 配对/加密实现（P0-12 SM 层参考）——只搜到应用层调用，无 host 栈实现
- openharmony/communication_dsoftbus（软总线，46M，与 nearlink 竞合）未评估
- openharmony/device_soc_hisilicon（gh code search 出现过 ws63v100/sdk 路径）未克隆
- gitee 侧需 token，GitHub API 匿名限流（10 次/分钟搜索）
- 2026-09-12 热点扫描后续队列：openharmony/communication_nearlink_service 9-11 推送（本地克隆早于更新）需 pull+diff；cxl0928/hi3863-sle-1v8-vehicle（SLE 1 客户端对 8 服务器拓扑）待克隆；yanlinkos/fbb_ws63 + fbb_bs2x（YL63 厂商 fork，472M/217M）需 fork-diff 判定；xingkaiyueying/tethering_nearlink（星闪网络共享）待克隆；Eironax/Qwac（Playjoy 开源替代，80KB）小仓可顺手；Heebu/NearLinkChat（Flutter 星闪聊天）低优先级

## Answer

（待接手会话填写）
